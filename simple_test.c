#include <stdio.h>
#include <unistd.h>
#include <syscall.h>
#include "include/linux/prinfo.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>


int main(void) {
    
    int nr = 20;
    struct prinfo *buf = (struct prinfo*)malloc(sizeof(struct prinfo) * nr);
    
    printf("calling syscall 398\n");
    //fflush(stdout);

    int total = syscall(398, buf, &nr); 

    printf("%s,%d,%lld,%d,%d,%d,%lld\n", buf[0].comm, buf[0].pid, buf[0].state, buf[0].parent_pid, buf[0].first_child_pid, buf[0].next_sibling_pid, buf[0].uid);
    
    printf("syscall is called\n");
    printf("total: %d\n", total);
    
    free(buf);
    return 0;
}
