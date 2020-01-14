/***********************8
 * 
 *   1. 입력으로 한줄짜리 file path를 입력받고, 그 파일에는 gps location좌표 정보가 들어있어야 한다.
 *  2. get_gps_location sys call 에서는 file로부터 얻은 정보를 파싱하여 gps 좌표를 stdout으로 출력하고,
 *   3. 그 좌표에 해당하는 Google Map 링크를 출력한다. (consider the URL pattern)  
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_GET_GPS_LOCATION    399

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("invalid input");
        return -1;
    }
    
    struct gps_location loc;
    int len = strlen(argv[1]);
    
    char* pathname = (char *)malloc(sizeof(char) * (len + 1));
    
    strncpy(pathname, argv[1], len);
    pathname[len] = '\0';
    
    
    
    if(syscall(SYS_GET_GPS_LOCATION, pathname, &loc) < 0) {
        perror("[gpsupdate failed]");
        free(pathname);
        return -1;
    }
    printf("longitude: %d.%06d, latitude: %d.%06d, accuracy: %d\n", loc.lat_integer,
                                                                loc.lat_fractional,
                                                                loc.lng_integer,
                                                                loc.lng_fractional,
                                                                loc.accuracy);
    printf("Google Map Link: https://www.google.co.kr/maps/search/?api=1&query=%d.%06d,%d.%06d\n", loc.lat_integer,
                                                                                               loc.lat_fractional,
                                                                                               loc.lng_integer,
                                                                                               loc.lng_fractional);
    free(pathname);
    
    return 0;
}
