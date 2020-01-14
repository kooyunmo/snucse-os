#!/bin/bash

declare FORMAT=""
declare DEVICE=""

# Binaires array for fusing
declare -a FUSING_BINARY_ARRAY
declare -i FUSING_BINARY_NUM=0

declare CONV_ASCII=""
declare -i FUS_ENTRY_NUM=0

# binary name | part number | bs
declare -a PART_TABLE=(
	"boot.img"			1	512
	"rootfs.img"			2	4M
	"system-data.img"		3	4M
	"user.img"			5	4M
	"modules.img"			6	512
	"ramdisk.img"			7	512
	"ramdisk-recovery.img"		8	512
	"inform.img"			9	512
	)

declare -r -i PART_TABLE_ROW=3
declare -r -i PART_TABLE_COL=${#PART_TABLE[*]}/${PART_TABLE_ROW}

# partition table support
function get_index_use_name () {
	local -r binary_name=$1

	for ((idx=0;idx<$PART_TABLE_COL;idx++)); do
		if [ ${PART_TABLE[idx * ${PART_TABLE_ROW} + 0]} == $binary_name ]; then
			return $idx
		fi
	done

	# return out of bound index
	return $idx
}

function print_message () {
	local color=$1
	local message=$2

	tput setaf $color
	tput bold
	echo ""
	echo $message
	tput sgr 0
}

function fusing_image () {
	local -r fusing_img=$1

	# get binary info using basename
	get_index_use_name $(basename $fusing_img)
	local -r -i part_idx=$?
	local EFFECTIVE_DEVICE=$DEVICE

	if [[ "$DEVICE" =~ [0-9]$ ]]
	then
		EFFECTIVE_DEVICE+="p"
	fi

	if [ $part_idx -ne $PART_TABLE_COL ];then
		local -r device=$EFFECTIVE_DEVICE${PART_TABLE[${part_idx} * ${PART_TABLE_ROW} + 1]}
		local -r bs=${PART_TABLE[${part_idx} * ${PART_TABLE_ROW} + 2]}
	else
		echo "Not supported binary: $fusing_img"
		return
	fi

	local -r input_size=`du -b $fusing_img | awk '{print $1}'`

	print_message 2 "[Fusing $1]"
	umount $device
	dd if=$fusing_img | pv -s $input_size | dd of=$device bs=$bs
	resize2fs -f $device
}

function fuse_image_tarball () {
	local -r filepath=$1
	local -r temp_dir="tar_tmp"

	mkdir -p $temp_dir
	tar xvf $filepath -C $temp_dir
	cd $temp_dir

	for file in *
	do
		fusing_image $file
	done

	cd ..
	rm -rf $temp_dir
	eval sync
}

function fuse_image () {

	if [ "$FUSING_BINARY_NUM" == 0 ]; then
		return
	fi

	for ((fuse_idx = 0 ; fuse_idx < $FUSING_BINARY_NUM ; fuse_idx++))
	do
		local filename=${FUSING_BINARY_ARRAY[fuse_idx]}

		case "$filename" in
		    *.tar | *.tar.gz)
			fuse_image_tarball $filename
			;;
		    *)
			fusing_image $filename
			;;
		esac
	done
	echo ""
}

# partition format
function mkpart_3 () {
	# NOTE: if your sfdisk version is less than 2.26.0, then you should use following sfdisk command:
	# sfdisk --in-order --Linux --unit M $DISK <<-__EOF__

	# NOTE: sfdisk 2.26 doesn't support units other than sectors and marks --unit option as deprecated.
	# The input data needs to contain multipliers (MiB) instead.
	local version=`sfdisk -v | awk '{print $4}'`
	local major=${version%%.*}
	local version=${version:`expr index $version .`}
	local minor=${version%%.*}
	local sfdisk_new=0

	if [ $major -gt 2 ];  then
		sfdisk_new=1
	else
		if [ $major -eq 2 -a $minor -ge 26 ];  then
			sfdisk_new=1
		fi
	fi

	local -r DISK=$DEVICE
	local -r SIZE=`sfdisk -s $DISK`
	local -r SIZE_MB=$((SIZE >> 10))

	local -r BOOT_SZ=64
	local -r ROOTFS_SZ=3072
	local -r DATA_SZ=512
	local -r MODULE_SZ=20
	local -r RAMDISK_SZ=8
	local -r RAMDISK_RECOVERY_SZ=12
	local -r INFORM_SZ=8
	if [ $sfdisk_new == 1 ]; then
		local -r EXTEND_SZ=8
	else
		local -r EXTEND_SZ=4
	fi

	let "USER_SZ = $SIZE_MB - $BOOT_SZ - $ROOTFS_SZ - $DATA_SZ - $MODULE_SZ - $RAMDISK_SZ - $RAMDISK_RECOVERY_SZ - $INFORM_SZ - $EXTEND_SZ"

	local -r BOOT=boot
	local -r ROOTFS=rootfs
	local -r SYSTEMDATA=system-data
	local -r USER=user
	local -r MODULE=modules
	local -r RAMDISK=ramdisk
	local -r RAMDISK_RECOVERY=ramdisk-recovery
	local -r INFORM=inform

	local EFFECTIVE_DISK=$DISK

	if [[ "$DISK" =~ [0-9]$ ]]
	then
		EFFECTIVE_DISK+="p"
	fi

	if [[ $USER_SZ -le 100 ]]
	then
		echo "We recommend to use more than 4GB disk"
		exit 0
	fi

	echo "========================================"
	echo "Label          dev           size"
	echo "========================================"
	echo $BOOT"		" $EFFECTIVE_DISK"1	" $BOOT_SZ "MB"
	echo $ROOTFS"		" $EFFECTIVE_DISK"2	" $ROOTFS_SZ "MB"
	echo $SYSTEMDATA"	" $EFFECTIVE_DISK"3	" $DATA_SZ "MB"
	echo "[Extend]""	" $EFFECTIVE_DISK"4"
	echo " "$USER"		" $EFFECTIVE_DISK"5	" $USER_SZ "MB"
	echo " "$MODULE"	" $EFFECTIVE_DISK"6	" $MODULE_SZ "MB"
	echo " "$RAMDISK"	" $EFFECTIVE_DISK"7	" $RAMDISK_SZ "MB"
	echo " "$RAMDISK_RECOVERY"	" $EFFECTIVE_DISK"8	" $RAMDISK_RECOVERY_SZ "MB"
	echo " "$INFORM"	" $EFFECTIVE_DISK"9	" $INFORM_SZ "MB"

	local MOUNT_LIST=`mount | grep $DISK | awk '{print $1}'`
	for mnt in $MOUNT_LIST
	do
		umount $mnt
	done

	echo "Remove partition table..."
	dd if=/dev/zero of=$DISK bs=512 count=16 conv=notrunc

	if [ $sfdisk_new == 1 ]; then
		sfdisk $DISK <<-__EOF__
		4MiB,${BOOT_SZ}MiB,0xE,*
		8MiB,${ROOTFS_SZ}MiB,,-
		8MiB,${DATA_SZ}MiB,,-
		8MiB,,E,-
		,${USER_SZ}MiB,,-
		,${MODULE_SZ}MiB,,-
		,${RAMDISK_SZ}MiB,,-
		,${RAMDISK_RECOVERY_SZ}MiB,,-
		,${INFORM_SZ}MiB,,-
		__EOF__
	else
		sfdisk --in-order --Linux --unit M $DISK <<-__EOF__
		4,$BOOT_SZ,0xE,*
		,$ROOTFS_SZ,,-
		,$DATA_SZ,,-
		,,E,-
		,$USER_SZ,,-
		,$MODULE_SZ,,-
		,$RAMDISK_SZ,,-
		,$RAMDISK_RECOVERY_SZ,,-
		,$INFORM_SZ,,-
		__EOF__
	fi

	mkfs.vfat -F 16 ${EFFECTIVE_DISK}1 -n $BOOT
	mkfs.ext4 -q ${EFFECTIVE_DISK}2 -L $ROOTFS -F
	mkfs.ext4 -q ${EFFECTIVE_DISK}3 -L $SYSTEMDATA -F
	mkfs.ext4 -q ${EFFECITVE_DISK}5 -L $USER -F
	mkfs.ext4 -q ${EFFECTIVE_DISK}6 -L $MODULE -F
	mkfs.ext4 -q ${EFFECTIVE_DISK}7 -L $RAMDISK -F
	mkfs.ext4 -q ${EFFECTIVE_DISK}8 -L $RAMDISK_RECOVERY -F
	mkfs.ext4 -q ${EFFECTIVE_DISK}9 -L $INFORM -F

	# create "reboot-param.bin" file in inform partition for passing reboot parameter
	# It should be done only once upon partition format.
	umount ${EFFECTIVE_DISK}9
	mkdir mnt_tmp
	mount -t ext4 ${EFFECTIVE_DISK}9 ./mnt_tmp
	touch ./mnt_tmp/reboot-param.bin
	sync
	umount ./mnt_tmp
	rmdir mnt_tmp
}

function show_usage () {
	echo "- Usage:"
	echo "	sudo ./sd_fusing*.sh -d <device> [-b <path> <path> ..] [--format]"
}

function check_partition_format () {
	if [ "$FORMAT" != "2" ]; then
		echo "-----------------------"
		echo "Skip $DEVICE format"
		echo "-----------------------"
		return 0
	fi

	echo "-------------------------------"
	echo "Start $DEVICE format"
	echo ""
	mkpart_3
	echo "End $DEVICE format"
	echo "-------------------------------"
	echo ""
}

function check_args () {
	if [ "$DEVICE" == "" ]; then
		echo "$(tput setaf 1)$(tput bold)- Device node is empty!"
		show_usage
		tput sgr 0
		exit 0
	fi

	if [ "$DEVICE" != "" ]; then
		echo "Device: $DEVICE"
	fi

	if [ "$FUSING_BINARY_NUM" != 0 ]; then
		echo "Fusing binary: "
		for ((bid = 0 ; bid < $FUSING_BINARY_NUM ; bid++))
		do
			echo "  ${FUSING_BINARY_ARRAY[bid]}"
		done
		echo ""
	fi

	if [ "$FORMAT" == "1" ]; then
		echo ""
		echo "$(tput setaf 3)$(tput bold)$DEVICE will be formatted, Is it OK? [y/n]"
		tput sgr 0
		read input
		if [ "$input" == "y" ] || [ "$input" == "Y" ]; then
			FORMAT=2
		else
			FORMAT=0
		fi
	fi
}

function print_logo () {
	echo ""
	echo "Raspberry Pi3 downloader, version 0.1"
	echo ""
}

print_logo

function add_fusing_binary() {
	local declare binary_name=$1
	FUSING_BINARY_ARRAY[$FUSING_BINARY_NUM]=$binary_name

	FUSING_BINARY_NUM=$((FUSING_BINARY_NUM + 1))
}


declare -i binary_option=0

while test $# -ne 0; do
	option=$1
	shift

	case $option in
	--f | --format)
		FORMAT="1"
		binary_option=0
		;;
	-d)
		DEVICE=$1
		binary_option=0
		shift
		;;
	-b)
		add_fusing_binary $1
		binary_option=1
		shift
		;;
	*)
		if [ $binary_option == 1 ];then
			add_fusing_binary $option
		else
			echo "Unkown command: $option"
			exit
		fi
		;;
	esac
done

check_args
check_partition_format
fuse_image

