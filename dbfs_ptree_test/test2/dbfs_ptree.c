#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/task.h>

#define NR      400


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

static struct prinfo *prinfo_buffer;

static int process_cnt = -1;
static int indent_count = 0;

/***

/*
 * Global variables
 */
static struct task_struct *curr, *syst, *kthr;


static struct pid *pid_struct;

/*struct process_struct {
    char* comm;
    int pid;
    struct list_head list;
};*/

static LIST_HEAD(head);


static void preorderTraversal(struct task_struct* curr) {
    process_cnt += 1;
    struct task_struct *task;
    struct list_head *list;
    struct prinfo* temp_prinfo;
    
    if(curr == NULL) {
        printk("curr is NULL");
        return;
    }
    
    struct task_struct *ch = container_of((&curr->children)->next, struct task_struct, sibling);        // leftmost child
    struct task_struct *sb = container_of((&curr->sibling)->next, struct task_struct, sibling);           // right sibling
    

    temp_prinfo = kmalloc(sizeof(struct prinfo), GFP_KERNEL);
    
    temp_prinfo->state = curr->state;          // current state of process
    temp_prinfo->pid = curr->pid;              // pid
    temp_prinfo->parent_pid = curr->parent->pid;       // pid of parent
    temp_prinfo->first_child_pid = ch->pid;  // pid of oldest child
    //temp_prinfo->next_sibling_pid = sb->pid; // pid of younger sibling
    if(sb->pid == 0)
        temp_prinfo->next_sibling_pid = -indent_count;
    else
        temp_prinfo->next_sibling_pid = sb->pid;
    temp_prinfo->uid = (curr->cred->uid).val;             // user id of process owner
    strncpy(temp_prinfo->comm, curr->comm, sizeof(curr->comm));
    
    
    printk("(#%d):[%d] %s, %d, %lld, %d, %d, %d %d", process_cnt, indent_count, temp_prinfo->comm, temp_prinfo->pid, temp_prinfo->state, temp_prinfo->parent_pid, temp_prinfo->first_child_pid, temp_prinfo->next_sibling_pid, temp_prinfo->uid);
    
    if(process_cnt < NR) {
        memcpy(prinfo_buffer+process_cnt, temp_prinfo, sizeof(struct prinfo));
    }
    
    kfree(temp_prinfo);
    
    list_for_each(list, &curr->children) {
        indent_count += 1;
        task = list_entry(list, struct task_struct, sibling);
        //printk("%s, %d", task->comm, task->pid);
        preorderTraversal(task);
    }
    indent_count -= 1;

}


long sys_ptree(struct prinfo *temp_buf, int *temp_nr){
    printk("syscall is called");
    /*
     * -EINVAL: if buf or nr are null, or if the number of entries is less than 1
     * -EFAULT: if buf or nr are outside the accessible address space. The referenced error codes are defined in
     */    

    /* copy from user */
    /*if(copy_from_user(temp_nr, nr, sizeof(int)) != 0) {
        printk("copy_from_user ERROR: could not read nr struct from user\n");
        return -EFAULT;
    }
    
    if(copy_from_user(temp_buf, buf, sizeof(struct prinfo)*(*temp_nr)) != 0) {
        printk("copy_from_user ERROR: could not read buf struct from user\n");
        return -EFAULT;
    }
    
    if(temp_buf == NULL || temp_nr == NULL) {
        printk("invalid arguments input");
        return -EINVAL;
    }*/
    
    
    pid_struct = find_get_pid(1);       // start from systemd(1)
    syst = pid_task(pid_struct, PIDTYPE_PID);   // Find task_struct of systemd(1).
    pid_struct = find_get_pid(2);       // start from kthread(2)
    kthr = pid_task(pid_struct, PIDTYPE_PID);
    
    if(syst == NULL || kthr == NULL)
    {
        printk("selected process is not running\n");
    }
    
    rcu_read_lock();
 
    preorderTraversal(temp_buf, syst);
    indent_count = 0;
    preorderTraversal(temp_buf, kthr);
    
    rcu_read_unlock();
    
    printk(KERN_INFO "total process: %d", process_cnt+1);
    
    // TODO: check whether to copy the size of nr or process_cnt
    if(copy_to_user(buf, temp_buf, sizeof(struct prinfo)*(*temp_nr)) != 0) {
        printk("copy_to_user ERROR: could not copy buf to user\n");
    }
    
    /* copy to user */
    *temp_nr = process_cnt+1;
    if(copy_to_user(nr, temp_nr, sizeof(int)) != 0) {
        printk("copy_to_user ERROR: could not copy nr to user\n");
    }
    
    return process_cnt+1;
}



static int __init dbfs_module_init(void)
{
    /*********************************************************/
    
    prinfo_buffer = kmalloc(sizeof(struct prinfo)*NR, GFP_KERNEL);

    pid_struct = find_get_pid(1);
    syst = pid_task(pid_struct, PIDTYPE_PID);  // Find task_struct using input_pid.
    pid_struct = find_get_pid(2);
    kthr = pid_task(pid_struct, PIDTYPE_PID);
    
    /* If current pid process is not running or invalid */
    if(syst == NULL || kthr == NULL)
    {
        printk("selected process is not running\n");
    }

    rcu_read_lock();
    preorderTraversal(syst);
    indent_count = 0;
    preorderTraversal(kthr);
    rcu_read_unlock();
    
    printk(KERN_INFO "total process: %d", process_cnt+1);
    
    
    int i = 0;
    while(i <= process_cnt) {
        printk("%s, %d, %lld, %d, %d, %d %d", (prinfo_buffer+i)->comm, (prinfo_buffer+i)->pid, (prinfo_buffer+i)->state, (prinfo_buffer+i)->parent_pid, (prinfo_buffer+i)->first_child_pid, (prinfo_buffer+i)->next_sibling_pid, (prinfo_buffer+i)->uid);
        i++;
    }
    
    
    kfree(prinfo_buffer);
    
    /*********************************************************/
	
	printk("dbfs_ptree module initialize done\n");

    return 0;
}

static void __exit dbfs_module_exit(void)
{	
	printk("dbfs_ptree module exit\n");

    //kfree(prinfo_buffer);
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
