#ifndef LINUX_GPS_H
#define LINUX_GPS_H
#include <linux/fs.h>

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

// function to check whether it is accessible
int nearby(struct inode * inode);

extern struct gps_location latest_location;
extern struct mutex l_gps;

#endif /* LINUX_GPS_H */
