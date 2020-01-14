mount -o rw,remount /dev/mmcblk0p2 /
./rotd
./selector 743912839 &
sleep 1
./trial 0 &
./selector 3251235 &
sleep 1
./trial 1 &
./trial 2 &
