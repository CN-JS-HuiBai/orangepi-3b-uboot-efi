#compile-uboot
make mrproper
export ROCKCHIP_TPL=/root/rkbin/bin/rk35/rk3566_ddr_1056MHz_v1.18.bin
export BL31=/root/rkbin/bin/rk35/rk3568_bl31_ultra_v2.13.elf
export TEE=/root/optee_os/out/arm-plat-vexpress/core/tee.bin
make CROSS_COMPILE=aarch64-linux-gnu- orangepi-3b-rk3566_defconfig
make CROSS_COMPILE=aarch64-linux-gnu- all -j4

