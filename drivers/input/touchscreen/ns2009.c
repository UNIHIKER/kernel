/*
 * Nsiway NS2009 touchscreen controller driver
 *
 * Copyright (C) 2017 Icenowy Zheng <icenowy@aosc.xyz>
 *
 * Some codes are from silead.c, which is
 *   Copyright (C) 2014-2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

/* polling interval in ms */
#define POLL_INTERVAL	30

/* this driver uses 12-bit readout */
#define MAX_12BIT	0xfff

#define NS2009_TS_NAME	"ns2009_ts"

#define NS2009_READ_X_LOW_POWER_12BIT	0xc0
#define NS2009_READ_Y_LOW_POWER_12BIT	0xd0
#define NS2009_READ_Z1_LOW_POWER_12BIT	0xe0
#define NS2009_READ_Z2_LOW_POWER_12BIT	0xf0

#define NS2009_DEF_X_FUZZ	32
#define NS2009_DEF_Y_FUZZ	16

/*
 * The chip have some error in z1 value when pen is up, so the data read out
 * is sometimes not accurately 0.
 * This value is based on experiements.
 */
#define NS2009_PEN_UP_Z1_ERR	150

struct ns2009_data {
	struct i2c_client		*client;
	struct input_dev		*input;
	struct timer_list		timer;
	struct work_struct		work;
	
	bool				pen_down;
};

static int ns2009_ts_read_data(struct ns2009_data *data, u8 cmd, u16 *val)
{
	u8 raw_data[2];
	int error;

	error = i2c_smbus_read_i2c_block_data(data->client, cmd, 2, raw_data);
	if (error < 0)
		return error;

	*val = (raw_data[0] << 4) | (raw_data[1] >> 4);

	return 0;
}

static int ns2009_ts_report(struct ns2009_data *data)
{
	u16 x, y, z1;
	int ret;

	/*
	 * NS2009 chip supports pressure measurement, but currently it needs
	 * more investigation, so we only use z1 axis to detect pen down
	 * here.
	 */
	ret = ns2009_ts_read_data(data, NS2009_READ_Z1_LOW_POWER_12BIT, &z1);
	if (ret)
		return ret;
	
	if (z1 >= NS2009_PEN_UP_Z1_ERR) {
		ret = ns2009_ts_read_data(data, NS2009_READ_X_LOW_POWER_12BIT,
					  &x);
		if (ret)
			return ret;

		ret = ns2009_ts_read_data(data, NS2009_READ_Y_LOW_POWER_12BIT,
					  &y);
		if (ret)
			return ret;

		if (!data->pen_down) {
			input_report_key(data->input, BTN_TOUCH, 1);
			data->pen_down = true;
		}

		input_report_abs(data->input, ABS_X, x);
		input_report_abs(data->input, ABS_Y, 4095 - y);
		input_sync(data->input);
	} else if (data->pen_down) {
		input_report_key(data->input, BTN_TOUCH, 0);
		input_sync(data->input);
		data->pen_down = false;
	}
	return 0;
}

/* Work queue handler for touch data */
static void ns2009_ts_work_handler(struct work_struct *work)
{
	struct ns2009_data *data = container_of(work, struct ns2009_data, work);
	int ret;

	ret = ns2009_ts_report(data);
	if (ret)
		dev_err(&data->client->dev, "Poll touch data failed: %d\n", ret);
}

/* Timer polling callback */
static void ns2009_ts_poll(struct timer_list *t)
{
	struct ns2009_data *data = from_timer(data, t, timer);
	
	schedule_work(&data->work);
	mod_timer(&data->timer, jiffies + msecs_to_jiffies(POLL_INTERVAL));
}

static void ns2009_ts_config_input_dev(struct ns2009_data *data)
{
	struct input_dev *input = data->input;

	input_set_abs_params(input, ABS_X, 0, MAX_12BIT, NS2009_DEF_X_FUZZ, 0);
	input_set_abs_params(input, ABS_Y, 0, MAX_12BIT, NS2009_DEF_Y_FUZZ, 0);

	input->name = NS2009_TS_NAME;
	input->phys = "input/ts";
	input->id.bustype = BUS_I2C;
	input_set_capability(input, EV_KEY, BTN_TOUCH);
}

static int ns2009_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct ns2009_data *data;
	struct device *dev = &client->dev;
	int error;

	dev_info(dev, "NS2009 I2C address: 0x%02x\n", client->addr);

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_I2C |
				     I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
		dev_err(dev, "I2C functionality check failed\n");
		return -ENXIO;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);
	data->client = client;

	data->input = devm_input_allocate_device(dev);
	if (!data->input)
		return -ENOMEM;

	ns2009_ts_config_input_dev(data);

	INIT_WORK(&data->work, ns2009_ts_work_handler);

	timer_setup(&data->timer, ns2009_ts_poll, 0);

	error = input_register_device(data->input);
	if (error) {
		dev_err(dev, "Failed to register input device: %d\n", error);
		return error;
	}

	dev_info(dev, "Input device registered: %s\n", data->input->name);

	mod_timer(&data->timer, jiffies + msecs_to_jiffies(POLL_INTERVAL));

	return 0;
}

static void ns2009_ts_remove(struct i2c_client *client)
{
	struct ns2009_data *data = i2c_get_clientdata(client);

	del_timer_sync(&data->timer);
	cancel_work_sync(&data->work);
	dev_info(&client->dev, "NS2009 driver removed\n");
}

static const struct i2c_device_id ns2009_ts_id[] = {
	{ "ns2009", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ns2009_ts_id);

#ifdef CONFIG_OF
static const struct of_device_id ns2009_of_match[] = {
	{ .compatible = "nsiway,ns2009" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ns2009_of_match);
#endif

static struct i2c_driver ns2009_ts_driver = {
	.probe = ns2009_ts_probe,
	.remove = ns2009_ts_remove,
	.id_table = ns2009_ts_id,
	.driver = {
		.name = NS2009_TS_NAME,
		.of_match_table = of_match_ptr(ns2009_of_match),
	},
};
module_i2c_driver(ns2009_ts_driver);

MODULE_AUTHOR("Ouki.Wang <dzdzz@qq.com>");
MODULE_DESCRIPTION("NS2009 Touchscreen Driver for Linux 6.1");
MODULE_LICENSE("GPL");
