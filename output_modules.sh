#!/bin/bash
# 一键导出内核模块脚本

# 设置安装路径（可以根据需要修改）
INSTALL_MOD_PATH=./output/modules

# 创建输出目录
mkdir -p $INSTALL_MOD_PATH

# 导出所有内核模块
make modules_install INSTALL_MOD_PATH=$INSTALL_MOD_PATH

echo "内核模块已导出到: $INSTALL_MOD_PATH/lib/modules/$(make kernelrelease)"
