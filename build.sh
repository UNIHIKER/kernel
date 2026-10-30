#export CROSS_COMPILE=/home/fary/rk3308bs_recovery/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- 
#export ARCH=arm64  
#make rk3308_linux_defconfig
#make -j$(nproc)
#make menuconfig
#!/bin/bash
#make ARCH=arm64 CROSS_COMPILE=/home/fary/rk3308bs_recovery/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- "$@"
#cp arch/arm64/boot/Image .
#cp arch/arm66/boot/dts/rockchip/pythonboard.dtb .
#!/bin/bash

# 设置环境变量（可选，如果其他地方需要）
export ARCH=arm64
export CROSS_COMPILE=../prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-

# 执行 make 命令并等待完成
make "$@"

# 检查 make 是否成功执行
if [ $? -eq 0 ]; then
    echo "Make completed successfully, copying files..."
    mkdir -p output
    # 复制内核映像文件
    cp arch/arm64/boot/Image output/
    # 复制设备树文件（修正了拼写错误）
    cp arch/arm64/boot/dts/rockchip/rk3308bs-unihikerM10.dtb output/
    echo "Files copied: Image and rk3308bs-unihikerM10.dtb"
else
    echo "Make failed, skipping file copy"
    exit 1
fi
