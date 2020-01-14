#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>

#define SYS_ROTLOCK_READ 399
#define SYS_ROTLOCK_WRITE 400
#define SYS_ROTUNLOCK_READ 401
#define SYS_ROTUNLOCK_WRITE 402

int main(int argc, char* argv[]) {
    int ret;
    if ((ret = syscall(SYS_ROTLOCK_READ, 90, 90)) < 0) {
        printf("rotlock_read error: %s", strerror(ret));
        exit(EXIT_FAILURE);
    }

    int i = 0;
    while (1) {
        if (i++ % 50000000 == 0) {
            printf("test is running!\n");
            fflush(stdout);
        }
        //sleep(1);
    }
}
