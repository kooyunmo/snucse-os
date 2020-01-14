#ifndef _PRINFO_H_
#define _PRINFO_H_

typedef     long long     int64_t;

struct prinfo {
    int64_t state;
    pid_t pid;
    pid_t parent_pid;
    pid_t first_child_pid;
    pid_t next_sibling_pid;
    int64_t uid;
    char comm[64];
};

#endif
