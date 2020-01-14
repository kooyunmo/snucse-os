#!/bin/sh

echo `cp boot.img ./../tizen-image/`
echo copy boot.img done
echo `cp modules.img ./../tizen-image/`
echo copy modules.img done
echo `arm-linux-gnueabi-gcc -I./include ./test/test_sched.c -o ./test/testfile`
echo testfile generate
echo `mount ./../tizen-image/rootfs.img ./mountfile`
echo mount file
echo `cp ./test/testfile mountfile/root`
echo file copy
echo `umount mountfile`
echo umount
echo `./qemu.sh`
