#!/bin/bash

# Check this system has ccache
check_ccache()
{
	type ccache
	if [ "$?" -eq "0" ]; then
		CCACHE=ccache
	fi
}

check_ccache

rm -f arch/arm64/boot/Image
rm -f arch/arm64/boot/dts/broadcom/*.dtb

# Always override config
rm -f .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- tizen_bcmrpi3_defconfig

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j $(nproc)
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs
if [ ! -f "./arch/arm64/boot/Image" ]; then
	echo "Build fail"
	exit 1
fi

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules_prepare -j $(nproc)
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules -j $(nproc)
