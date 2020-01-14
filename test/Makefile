all: gpsupdate file_loc

gpsupdate:
	arm-linux-gnueabi-gcc -I../include ./gpsupdate.c -o ./gpsupdate

file_loc:
	arm-linux-gnueabi-gcc -I../include ./file_loc.c -o ./file_loc

clean:
	rm -rf ./gpsupdate
	rm -rf ./file_loc
