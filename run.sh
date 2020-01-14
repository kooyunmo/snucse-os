sudo bash ./build-rpi3-arm64.sh
sudo bash ./scripts/mkbootimg_rpi3.sh
sudo mount ./../tizen-image/rootfs.img ./mountdir
arm-linux-gnueabi-gcc -I./include ./test/selector.c -o ./test/sel
arm-linux-gnueabi-gcc -I./include ./test/trial.c -o ./test/trial
arm-linux-gnueabi-gcc -I./include ./test/trial2.c -o ./test/trial2
arm-linux-gnueabi-gcc -I./include ./test/trial3.c -o ./test/trial3
arm-linux-gnueabi-gcc -I./include ./test/trial4.c -o ./test/trial4
sudo cp test/sel ./mountdir/root
sudo cp test/trial ./mountdir/root
sudo cp test/trial2 ./mountdir/root
sudo cp test/trial3 ./mountdir/root
sudo cp test/trial4 ./mountdir/root
sudo umount mountdir
sudo cp modules.img ./../tizen-image
sudo cp boot.img ./../tizen-image
./qemu.sh
