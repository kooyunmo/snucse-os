sudo ./build-rpi3-arm64.sh 
sudo ./scripts/mkbootimg_rpi3.sh
cp boot.img ../tizen-image
cp modules.img ../tizen-image
sudo mount ../tizen-image/rootfs.img ./mountdir
sudo cp test/gpsupdate ./mountdir/root
sudo cp test/file_loc ./mountdir/root
sudo cp test/mount.sh ./mountdir/root
sudo cp test/umount.sh ./mountdir/root
sudo cp proj4.fs ./mountdir/root
sudo umount mountdir

