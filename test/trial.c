#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>

#define SYS_SCHED_SETSCHEDULER_WRR 7
#define SYS_SCHED_SETWEIGHT 398
#define SYS_SCHED_GETWEIGHT 399


void factorize_prime(int n) {
    if(n <= 0)
        return;

    printf("%d's primes: ", n); 
    int j = 0;
    for (int i = 2; i <= n; i++) {
        while (n % i == 0) {
            if (n < i) {  
                printf("\n");
                return;
            }
            n=n/i;
            j=1;
        }
        if (j) {
            j=0;
            printf("%d, ",i);
        }
    }   
    printf("\n"); 
}


void child_run(int n, int weight) {
    struct timespec start, finish;
    double elapsed;
    clock_gettime(CLOCK_MONOTONIC, &start);
    factorize_prime(n);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsed = finish.tv_sec - start.tv_sec;
    elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("pid: %d, weight: %d, time: %f\n", getpid(), weight, elapsed);
    exit(0);
}


int main(int argc, char *argv[]) {
    int child_status;
    int p;
    struct sched_param param;
    param.sched_priority = 100;

    if (argc != 3) {
        printf("Usage: test_sched <fork_num> <n> \n");
        return 0;
    }

    int fork_num = atoi(argv[1]),
        n = atoi(argv[2]);

    pid_t pid[fork_num];

    for (int i = 0; i < fork_num; i++) {
        int weight = 3 * i + 1;

        if ((pid[i] = fork()) == 0) {
            if (sched_setscheduler(getpid(), SYS_SCHED_SETSCHEDULER_WRR, &param) != 0) {
                printf("[child] sched_setscheduler failed. pid: %d, weight: %d\n", getpid(), weight);
                return -1;
            }

            int ret;
            if ((ret = syscall(SYS_SCHED_SETWEIGHT, getpid(), weight)) < 0) {
				if(weight>20 || weight <=0)
					errno=EINVAL;
				else
					errno=-1;

                printf("[child] sched_setweight failes. %s\n", strerror(errno));
                return -1;
            }
			else{
				ret=syscall(SYS_SCHED_GETWEIGHT, 0);
				printf("[child] weight is set as %d\n", ret);
			}
            child_run(n, weight);
        }
    }

    for (int i = 0; i < fork_num; i++) {
        pid_t wpid = waitpid(pid[i], &child_status, 0);
        if (WIFEXITED(child_status))
            printf("child %d is completed with exit status %d.\n", wpid, WEXITSTATUS(child_status));
        else {
            printf("child %d is terminated abnormally.\n", wpid);
        }
    }

    return 0;
}
