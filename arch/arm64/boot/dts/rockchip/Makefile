# SPDX-License-Identifier: GPL-2.0
DTC_FLAGS_rk3308bs-unihikerM10 := -@
DTC_FLAGS_rk3308bs-unihikerM10-recovery := -@
dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3308bs-unihikerM10.dtb
dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3308bs-unihikerM10-recovery.dtb

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs) overlay
clean-files	:= *.dtb *.dtbo
