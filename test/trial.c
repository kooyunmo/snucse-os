#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>

#define SYS_ROTLOCK_READ 399
#define SYS_ROTLOCK_WRITE 400
#define SYS_ROTUNLOCK_READ 401
#define SYS_ROTUNLOCK_WRITE 402
#define BUFFER_SIZE 32

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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: trial <integer_identifier>\n");
        exit(EXIT_FAILURE);
    }

    int identifier = atoi(argv[1]);
    int integer;
    int ret;
    FILE *fp;
    char buffer[BUFFER_SIZE];
    char *bufptr = buffer;
    size_t bufsize = BUFFER_SIZE;

    while (1) {
        if ((ret = syscall(SYS_ROTLOCK_READ, 90, 90)) < 0) {
            printf("rotlock_read error: %s", strerror(ret));
            exit(EXIT_FAILURE);
        }

        fp = fopen("integer", "r");
        if (fp == NULL) {
            printf("No \"integer\" file\n");
            exit(EXIT_FAILURE);
        }
        getline(&bufptr, &bufsize, fp);
        fclose(fp);
        integer = atoi(buffer);
        printf("trial-%d: ", identifier);
        factorize_prime(integer);

        if ((ret = syscall(SYS_ROTUNLOCK_READ, 90, 90)) < 0) {
            printf("rotunlock_read error: %s", strerror(ret));
            exit(EXIT_FAILURE);
        }
    }

}
