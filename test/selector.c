#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYS_ROTLOCK_WRITE 400
#define SYS_ROTUNLOCK_WRITE 402

int main(int argc, char * argv[]) {

	if (argc != 2) {
		printf("Invalid input\n");
		return -1;
	}
	int n = atoi(argv[1]);
	
	FILE *fp;
	int ret;

	while (1) {
		if ((ret=syscall(SYS_ROTLOCK_WRITE, 90, 90)) != 0) {
			printf("write lock failed\n");
			exit(EXIT_FAILURE);
		}

		fp = fopen("integer", "w");
        if (fp == NULL) {
            printf("No \"integer\" file\n");
            exit(EXIT_FAILURE);
        }

        fprintf(fp, "%d\n", n);
        fclose(fp);
        printf("selector: %d\n", n);
        n++;

		if ((ret=syscall(SYS_ROTUNLOCK_WRITE, 90, 90)) != 0) {
			printf("write unlock failed\n");
			exit(EXIT_FAILURE);
		}
	}
	return 0;

}
