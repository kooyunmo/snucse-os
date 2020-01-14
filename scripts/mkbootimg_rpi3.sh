#!/bin/bash

BOOT_PATH="rpi3/boot"
USER_ID=`id -u`
GROUP_ID=`id -g`

rm -f boot.img
rm -rf tmp
mkdir tmp

# Create boot.img
mkfs.vfat -F 16 -C -n BOOT boot.img 65536
sudo mount -o loop,uid=$USER_ID,gid=$GROUP_ID,showexec boot.img $(pwd)/tmp
cp -a $BOOT_PATH/config_64bit.txt ./tmp/config.txt
cp -a $BOOT_PATH/LICENCE.broadcom ./tmp
cp -a $BOOT_PATH/bootcode.bin ./tmp
cp -a $BOOT_PATH/start*.elf ./tmp
cp -a $BOOT_PATH/fixup*.dat ./tmp
cp -a arch/arm64/boot/Image ./tmp
cp -a arch/arm64/boot/dts/broadcom/bcm*.dtb ./tmp

# install u-boot files extracted from u-boot-rpi3 rpm package in download.tizen.org.
TMP_UBOOT_PATH=tmp_uboot
mkdir -p ${TMP_UBOOT_PATH}
pushd ${TMP_UBOOT_PATH}
REPO_URL=http://download.tizen.org/snapshots/tizen/unified/latest/repos/standard/packages/aarch64/
rm -f index.html*
wget ${REPO_URL}
UBOOT=`awk -F\" '{ print $2 }' index.html | grep u-boot-rpi3`
wget ${REPO_URL}${UBOOT}
unrpm ${UBOOT}

# install u-boot.img having optee.bin extracted from atf-rpi3 rpm package in download.tizen.org.
ATF=`awk -F\" '{ print $2 }' index.html | grep atf-rpi3`
wget ${REPO_URL}${ATF}
unrpm ${ATF}

popd
cp -a ${TMP_UBOOT_PATH}/boot/* ./tmp
rm -rf ${TMP_UBOOT_PATH}

sync
sudo umount tmp

rm -f modules.img
mkdir -p tmp/lib/modules

# Create modules.img
dd if=/dev/zero of=modules.img bs=1024 count=20480
mkfs.ext4 -q -F -t ext4 -b 1024 -L modules modules.img
sudo mount -o loop modules.img $(pwd)/tmp/lib/modules
make modules_install ARCH=arm64 INSTALL_MOD_PATH=./tmp INSTALL_MOD_STRIP=1 CROSS_COMPILE=aarch64-linux-gnu-
sudo -n chown root:root ./tmp/lib/modules -R

sync
sudo umount tmp/lib/modules

rm -rf tmp
