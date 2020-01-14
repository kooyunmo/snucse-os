sudo ./scripts/mkbootimg_rpi3.sh
sudo cp boot.img ../tizen-image
sudo cp modules.img ../tizen-image
sudo mount ../tizen-image/rootfs.img ./mnt_dir
sudo cp proj4.fs ./mnt_dir/root
sudo cp test/gpsupdate ./mnt_dir/root
sudo cp test/file_loc ./mnt_dir/root
sudo cp test/mount.sh ./mnt_dir/root
sudo cp test/umount.sh ./mnt_dir/root
sudo umount mnt_dir

