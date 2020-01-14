#!/bin/bash

sudo ./sd_fusing_rpi3.sh -d $1 --format
sudo ./sd_fusing_rpi3.sh -d $1 -b *_iot-headless-2parts-armv7l-rpi3.tar.gz
sudo ./sd_fusing_rpi3.sh -d $1 -b *_iot-boot-arm64-rpi3.tar.gz 
