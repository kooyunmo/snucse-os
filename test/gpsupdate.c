#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>

#define SYS_SET_GPS_LOCATION 398

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

void get_input(int *loc_info) {
    if (scanf("%d", loc_info) != 1) {
        printf("Wrong input format\n");  
    }
}

int main() {
    int lat_integer, lat_fractional, lng_integer, lng_fractional, accuracy; 
    
    printf("lat_integer: ");
    get_input(&lat_integer);
    printf("lat_fractional: ");
    get_input(&lat_fractional);
    printf("lng_integer: ");
    get_input(&lng_integer);
    printf("lng_fractional: ");
    get_input(&lng_fractional);
    printf("accuracy: ");
    get_input(&accuracy);

    struct gps_location loc;
    loc.lat_integer = lat_integer;
    loc.lat_fractional = lat_fractional;
    loc.lng_integer = lng_integer;
    loc.lng_fractional = lng_fractional;
    loc.accuracy = accuracy;

    int ret;
    if ((ret = syscall(SYS_SET_GPS_LOCATION, &loc)) < 0) {
        printf("Error: %s", strerror(ret));
        exit(EXIT_FAILURE);
    }
    
    return 0;
}
