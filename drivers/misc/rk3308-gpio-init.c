// SPDX-License-Identifier: GPL-2.0
/*
 * Early GPIO initialization for RK3308
 * Sets GPIO to high level during kernel boot, keeps it high, and provides sysfs control
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/sysfs.h>

struct rk3308_gpio_init_data {
	struct gpio_desc *gpio;
	int gpio_num;
	struct device *dev;
};

static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rk3308_gpio_init_data *data = dev_get_drvdata(dev);
	int value;

	if (!data || !data->gpio)
		return -ENODEV;

	value = gpiod_get_value(data->gpio);
	return sprintf(buf, "%d\n", value);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct rk3308_gpio_init_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!data || !data->gpio)
		return -ENODEV;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val > 1)
		return -EINVAL;

	gpiod_set_value(data->gpio, val);
	dev_info(dev, "GPIO %d set to %s\n", data->gpio_num, val ? "high" : "low");

	return count;
}

static DEVICE_ATTR_RW(enable);

static struct attribute *rk3308_gpio_init_attrs[] = {
	&dev_attr_enable.attr,
	NULL,
};

static const struct attribute_group rk3308_gpio_init_attr_group = {
	.attrs = rk3308_gpio_init_attrs,
};

static int rk3308_gpio_init_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rk3308_gpio_init_data *data;
	enum gpiod_flags flags = GPIOD_OUT_HIGH;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;

	/* Get GPIO descriptor */
	/* Look for "enable-gpios" property */
	data->gpio = devm_gpiod_get_optional(dev, "enable", flags);
	if (IS_ERR(data->gpio)) {
		ret = PTR_ERR(data->gpio);
		dev_err(dev, "Failed to get GPIO: %d\n", ret);
		return ret;
	}

	if (!data->gpio) {
		dev_warn(dev, "No GPIO specified\n");
		return 0;
	}

	/* Get GPIO number */
	data->gpio_num = desc_to_gpio(data->gpio);

	dev_info(dev, "GPIO %d set to high level during boot\n", data->gpio_num);

	/* Set GPIO to high and keep the descriptor to maintain the state */
	gpiod_set_value(data->gpio, 1);
	mdelay(10);

	platform_set_drvdata(pdev, data);

	/* Create sysfs interface */
	ret = sysfs_create_group(&dev->kobj, &rk3308_gpio_init_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create sysfs group: %d\n", ret);
		return ret;
	}

	dev_info(dev, "GPIO %d initialized to high, control via /sys/devices/platform/vcc-3v3-out-init/enable\n",
		 data->gpio_num);

	return 0;
}

static int rk3308_gpio_init_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_group(&dev->kobj, &rk3308_gpio_init_attr_group);

	return 0;
}

static const struct of_device_id rk3308_gpio_init_of_match[] = {
	{ .compatible = "rockchip,rk3308-gpio-init" },
	{ }
};
MODULE_DEVICE_TABLE(of, rk3308_gpio_init_of_match);

static struct platform_driver rk3308_gpio_init_driver = {
	.driver = {
		.name = "rk3308-gpio-init",
		.of_match_table = rk3308_gpio_init_of_match,
	},
	.probe = rk3308_gpio_init_probe,
	.remove = rk3308_gpio_init_remove,
};

static int __init rk3308_gpio_init_init(void)
{
	return platform_driver_register(&rk3308_gpio_init_driver);
}
subsys_initcall(rk3308_gpio_init_init);

static void __exit rk3308_gpio_init_exit(void)
{
	platform_driver_unregister(&rk3308_gpio_init_driver);
}
module_exit(rk3308_gpio_init_exit);

MODULE_AUTHOR("RK3308 GPIO Init");
MODULE_DESCRIPTION("Early GPIO initialization for RK3308 with sysfs control");
MODULE_LICENSE("GPL v2");

