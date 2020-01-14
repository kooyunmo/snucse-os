#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/task.h>


MODULE_LICENSE("GPL");

/***/

struct prinfo {
    int64_t state;          // current state of process
    pid_t pid;              // pid
    pid_t parent_pid;       // pid of parent
    pid_t first_child_pid;  // pid of oldest child
    pid_t next_sibling_pid; // pid of younger sibling
    int64_t uid;             // user id of process owner
    char comm[64];          // name of process (ex: systemd)
};

static struct prinfo* prinfo_buffer;

static int process_cnt = 0;
static int indent_count = 0;

/***

/*
 * Global variables
 */
static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

static struct pid *pid_struct;

/*struct process_struct {
    char* comm;
    int pid;
    struct list_head list;
};*/

static LIST_HEAD(head);


/* General Tree Preorder Traversal */
static void preorder(struct task_struct* curr) {
    
    struct task_struct *ch = container_of((&curr->children)->next, struct task_struct, sibling);        // leftmost child
    //printk("check 1");
    //struct task_struct *pr = curr->parent;
    //printk("check 2");
    struct task_struct *sb = container_of((&curr->sibling)->next, struct task_struct, sibling);           // right sibling
    //printk("check 3");
    
    
    //pid_t parent_pid;
    pid_t first_child_pid;  // pid of oldest child
    pid_t next_sibling_pid; // pid of younger sibling
    int64_t uid;
 
    /*if(pr == NULL) {
        printk("check a");
        return;
    }
    printk("here1");
    parent_pid = pr->pid;
    printk("here2");*/
    
    if(ch == NULL) {
        printk("check b");
        //printfo_buffer->fisrt_child_pid = 0;
        return;
    }
    first_child_pid = ch->pid;
    //printk("check 4");
        
    if(sb == NULL) {
        printk("check c");
        return;
    }
    next_sibling_pid = sb->pid;
    //printk("check 5");
    
    if(curr == NULL) {
        printk("check d");
        return;
    }
    //printk("check 6");
    
    if(curr->cred == NULL) {
        //printk("check e");
        uid = -200;
        return;
    }
    uid = curr->cred->uid;
    //printk("check 7");
    
    printk(KERN_INFO "indent: %d", indent_count);
    printk(KERN_INFO "[%d] %s, %d, %lld, %d, %d, %d, %lld", process_cnt, curr->comm, curr->pid, curr->state, -100, first_child_pid, next_sibling_pid, curr->cred->uid);
    
    if(ch != NULL && ch->pid != 0) {       // if curr is not a leaf node
        //printk("check 8");
        struct task_struct *temp = ch;
        //printk("check 9");
        indent_count += 1;
        //printk("check 10");
        while(temp != NULL && temp->pid != 0) {
            //printk("check 11");
            preorder(temp);
            //printk("check 12");
            temp = container_of((&temp->sibling)->next, struct task_struct, sibling);           // temp's right sibling
            //printk("check 13");
        }   
        indent_count -= 1;
    }
    
    process_cnt += 1;
}


static int __init dbfs_module_init(void)
{
    /*********************************************************/

    pid_struct = find_get_pid(1);
    curr = pid_task(pid_struct, PIDTYPE_PID);  // Find task_struct using input_pid.
    
    /* If current pid process is not running or invalid */
    if(curr == NULL)
    {
        printk("selected process is not running\n");
    }

    //read_lock(&tasklist_lock);
    rcu_read_lock();
    preorder(curr);
    rcu_read_unlock();
    //read_unlock(&tasklist_lock);
    
    printk(KERN_INFO "total process: %d", process_cnt);
    
    
    /*********************************************************/
	
	printk("dbfs_ptree module initialize done\n");

    return 0;
}

static void __exit dbfs_module_exit(void)
{	
	printk("dbfs_ptree module exit\n");

    //(myblob);
    //kfree(buffer);
    
    //debugfs_remove_recursive(dir);
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
