#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define READ_TYPE 0
#define WRITE_TYPE 1

typedef struct __range_descriptor {
    int degree;
    int range;
    pid_t pid;
    int is_holding_lock;
    char type; // READ_TYPE or WRITE_TYPE
    struct list_head list;
} range_descriptor;

LIST_HEAD(rd_list);
DECLARE_WAIT_QUEUE_HEAD(lock_wait_queue);
DEFINE_MUTEX(rd_list_mutex);
DEFINE_MUTEX(wait_mutex);
atomic_t __read_cnt = ATOMIC_INIT(0);
atomic_t __write_cnt = ATOMIC_INIT(0);

static int curr_degree;

static int onrange(int degree, int range) {
    int start = degree >= range ? degree - range : 360 + degree - range;
    int end = (degree + range) % 360;
    return (start <= end && curr_degree >= start && curr_degree <= end)
            || (start > end && (curr_degree < end || curr_degree > start));
}

static void sleep_on(void) {
    mutex_lock(&wait_mutex);
    DEFINE_WAIT(wait_queue_entry);
    prepare_to_wait(&lock_wait_queue, &wait_queue_entry, TASK_INTERRUPTIBLE);
    mutex_unlock(&wait_mutex);
    schedule();
    mutex_lock(&wait_mutex);
    finish_wait(&lock_wait_queue, &wait_queue_entry);
    mutex_unlock(&wait_mutex);
}

static range_descriptor* init_rd(int degree, int range, int type) {
    range_descriptor *rd = kmalloc(sizeof(range_descriptor), GFP_KERNEL);
    rd->degree = degree;
    rd->range = range;
    rd->pid = current->pid;
    rd->type = type;
    rd->is_holding_lock = 0;
    return rd;
}

void print_read_write_cnt(void) {
    printk(KERN_ALERT "read count: %d, write count: %d\n",
            atomic_read(&__read_cnt), atomic_read(&__write_cnt));
}

// Lock rd_list before calling this function
void __print_list_state(void) {
    range_descriptor *rd_cursor;

    printk(KERN_CONT "list head->");
    list_for_each_entry(rd_cursor, &rd_list, list) {
        printk(KERN_CONT "[pid:%d,%s,degree:%d,range:%d,lock:%d]",
                rd_cursor->pid, (rd_cursor->type == READ_TYPE ? "read" : "write"),
                rd_cursor->degree, rd_cursor->range, rd_cursor->is_holding_lock);
    }
    printk(KERN_CONT "->end\n\n");
}

long sys_set_rotation(int degree) {
    printk(KERN_CONT "set_rotation: %d \n", degree);
    curr_degree = degree;
    mutex_lock(&wait_mutex);
    wake_up_all(&lock_wait_queue);
    mutex_unlock(&wait_mutex);
    return 0;
}

long sys_rotlock_read(int degree, int range) {
    range_descriptor *new_rd, *rd_cursor;
    int has_earlier_writing;

    printk(KERN_ALERT "call lock_read. pid: %u\n", current->pid);
	if(degree<0 || degree>=360 || range<=0 || range>=180){
		printk(KERN_ALERT "read lock failed: invalid range or degree. pid: %u\n", current->pid);
		return -1;
	}

    new_rd = init_rd(degree, range, READ_TYPE);

    mutex_lock(&rd_list_mutex);
    list_add_tail(&new_rd->list, &rd_list);

    while (1) {
        int or, write_cnt, condition;
        has_earlier_writing = 0;
        list_for_each_entry(rd_cursor, &rd_list, list) {
            if (rd_cursor == new_rd) {
                break;
            } else if (rd_cursor->type == WRITE_TYPE
                    && onrange(rd_cursor->degree, rd_cursor->range)
                    && !rd_cursor->is_holding_lock) {
                has_earlier_writing = 1;
                break;
            }
        }

        or = onrange(degree, range);
        write_cnt = atomic_read(&__write_cnt);
//        printk(KERN_ALERT "try read lock. pid: %u, onrange: %d, no holding write: %d, no earlier waiting write: %d\n\n",
//                current->pid, or, write_cnt == 0, !has_earlier_writing);
        condition = !or || write_cnt || has_earlier_writing;
        if (!condition) break;
        mutex_unlock(&rd_list_mutex);
        sleep_on();
        mutex_lock(&rd_list_mutex);
    }

    new_rd->is_holding_lock = 1;
    atomic_add(1, &__read_cnt);

    printk(KERN_ALERT "hold read lock. pid: %u\n", current->pid);
    print_read_write_cnt();
    mutex_unlock(&rd_list_mutex);
    return 0;
}

long sys_rotlock_write(int degree, int range) {
    range_descriptor *new_rd, *rd_cursor;
    int has_earlier_reading;

    printk(KERN_ALERT "call lock_write. pid: %u\n", current->pid);
	if(degree<0 || degree>=360 || range<=0 || range>=180){
		printk(KERN_ALERT "write lock failed: invalid range or degree. pid: %u\n", current->pid);
		return -1;
	}
    new_rd = init_rd(degree, range, WRITE_TYPE);
    mutex_lock(&rd_list_mutex);
    list_add_tail(&new_rd->list, &rd_list);

    while (1) {
        int or, write_cnt, read_cnt, condition;

        has_earlier_reading = 0;
        list_for_each_entry(rd_cursor, &rd_list, list) {
            if (rd_cursor == new_rd) {
                break;
            } else if (rd_cursor->type == READ_TYPE
                    && onrange(rd_cursor->degree, rd_cursor->range)
                    && !rd_cursor->is_holding_lock) {
                has_earlier_reading = 1;
                break;
            }
        }

        or = onrange(degree, range);
        write_cnt = atomic_read(&__write_cnt);
        read_cnt = atomic_read(&__read_cnt);
//        printk(KERN_ALERT "try write lock. pid: %u, onrange: %d, no holding write: %d, no holding read: %d, no waiting early read: %d\n\n",
//                current->pid, or, write_cnt == 0, read_cnt == 0, !has_earlier_reading);
        condition = !or || write_cnt > 0 || read_cnt > 0 || has_earlier_reading;
        if (!condition) break;
        mutex_unlock(&rd_list_mutex);
        sleep_on();
        mutex_lock(&rd_list_mutex);
    }

    new_rd->is_holding_lock = 1;
    atomic_add(1, &__write_cnt);
    printk(KERN_ALERT "hold write lock. pid: %u\n", current->pid);
    print_read_write_cnt();
    mutex_unlock(&rd_list_mutex);
    return 0;
}

long sys_rotunlock_read(int degree, int range) {
    range_descriptor *rd_cursor;

    printk(KERN_ALERT "call unlock_read. pid: %u\n", current->pid);
	if (degree < 0 || degree >= 360 || range <= 0 || range >= 180) {
		printk(KERN_ALERT "read unlock failed: invalid range or degree. pid: %u\n", current->pid);
		return -1;
	}

    mutex_lock(&rd_list_mutex);
    while (!onrange(degree, range)) {
        mutex_unlock(&rd_list_mutex);
        sleep_on();
        mutex_lock(&rd_list_mutex);
    }

    list_for_each_entry(rd_cursor, &rd_list, list) {
        if (current->pid == rd_cursor->pid) {
            list_del(&rd_cursor->list);
            kfree(rd_cursor);
            atomic_sub(1, &__read_cnt);
            printk(KERN_ALERT "release read lock. pid: %u\n", current->pid);
            mutex_unlock(&rd_list_mutex);
            return 0;
        }
    }
    mutex_unlock(&rd_list_mutex);
    return -1;
}

long sys_rotunlock_write(int degree, int range) {
    range_descriptor *rd_cursor;

    printk(KERN_ALERT "call unlock_write. pid: %u\n", current->pid);
	if (degree < 0 || degree >= 360 || range <= 0 || range >= 180) {
		printk(KERN_ALERT "write unlock failed: invalid range or degree. pid: %u\n", current->pid);
		return -1;
	}

    mutex_lock(&rd_list_mutex);
    while (!onrange(degree, range)) {
        mutex_unlock(&rd_list_mutex);
        sleep_on();
        mutex_lock(&rd_list_mutex);
    }

    list_for_each_entry(rd_cursor, &rd_list, list) {
        if (current->pid == rd_cursor->pid) {
            list_del(&rd_cursor->list);
            kfree(rd_cursor);
            atomic_sub(1, &__write_cnt);
            printk(KERN_ALERT "release write lock. pid:%u\n", current->pid);
            mutex_unlock(&rd_list_mutex);
            return 0;
        }
    }
    mutex_unlock(&rd_list_mutex);
    return -1;
}
