#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYS_ROTLOCK_READ 399
#define SYS_ROTUNLOCK_READ 401


void factorize_prime(int n) {
    if (n <= 0) {
        printf("factorize_prime: Wrong input number 0");
        return;
    }

    printf("%d = ", n); 
    int is_first_prime = 1;
    int f = 2;
    while (n > 1) {
        if (n % f == 0) {
            if (is_first_prime) {
                printf("%d", f);
                is_first_prime = 0;
            } else {
                printf(" * %d", f);
            }
            n /= f;
        } else {
            f++;
        }
    }
    printf("\n");
}

static volatile int run = 1;

void sigint_handler(int tmp){
	run = 0;
	syscall(SYS_ROTUNLOCK_READ, 0, 90);
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        printf("Usage: trial <integer_identifier>\n");
        exit(EXIT_FAILURE);
    }

	int input = atoi(argv[1]);

	FILE *fp;

	signal(SIGINT, sigint_handler);
	
	int value;
	while(run){
		if(syscall(SYS_ROTLOCK_READ, 0, 90) == 0){
			if(NULL != (fp = fopen("integer", "r"))){
		    	fscanf(fp, "%d", &value);
                fclose(fp);

	            printf("trial-%d: ", input);
		    	factorize_prime(value);
			}
			syscall(SYS_ROTUNLOCK_READ, 0, 90);
	    }	
	    sleep(1);
	}

    return 0;
}
