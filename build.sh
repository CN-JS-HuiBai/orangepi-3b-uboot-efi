make mrproper
cp rkbin / -r
export ROCKCHIP_TPL=/rkbin/rk35/rk3566_ddr_1056MHz_v1.10.bin
export BL31=/rkbin/rk35/rk3568_bl31_v1.32.elf
make CROSS_COMPILE=aarch64-linux-gnu- orangepi-3b-rk3566_defconfig
make CROSS_COMPILE=aarch64-linux-gnu- all -j4
rm -rf /rkbin