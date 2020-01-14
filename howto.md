./build-rpi3-arm64.sh

sudo ./scripts/mkbootimg_rpi3.sh

tar -zcvf tizen-unified_20181024.1_iot-boot-arm64-rpi3.tar.gz boot.img modules.img

-----
## compile test code

arm-linux-gnueabi-gcc -I<your kernel path>/include test.c -o test

-----

##After insert SD card 

sudo ./flash-sdcard.sh /dev/<SDCARD>

sudo mount /dev/sdb2 ${mount_dir}
sudo cp ${file_to_move} ${mount_dir}/root

-----

sudo screen /dev/ttyUSB0 115200
