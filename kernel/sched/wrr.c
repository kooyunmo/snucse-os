#include "sched.h"
#include <linux/list.h>
#include <linux/slab.h>

#define WRR_TIMESLICE           (10 * HZ / 1000) 

static DEFINE_PER_CPU(cpumask_var_t, local_cpu_mask);

/*
    @ DON'T UN-COMMENT THIS PART
truct sched_wrr_entity {
        struct list_head                run_list;
        unsigned long                   timeout;
        unsigned int                    time_slice;
        unsigned int                    weight;
};
*/
 
void __init init_sched_wrr_class(void) {
	unsigned int i;

	for_each_possible_cpu(i) {
		zalloc_cpumask_var_node(&per_cpu(local_cpu_mask, i), GFP_KERNEL, cpu_to_node(i));
	}
	
	current->wrr.weight = 10;
}

void init_wrr_rq(struct wrr_rq *wrr_rq){
   printk(KERN_ALERT "init_wrr_rq\n");
   wrr_rq->weight_sum=0;
   INIT_LIST_HEAD(&(wrr_rq->run_list));
   wrr_rq->curr=NULL;
   wrr_rq->timeout=2000;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{   
    // empty?
}

static struct task_struct *pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
    struct sched_wrr_entity *next_wrr_se = list_first_entry(&((rq->wrr).run_list),
                                                            struct sched_wrr_entity,
                                                            run_list);
    struct task_struct *pr;
                                                            
    if (!next_wrr_se || !((rq->wrr).weight_sum))
        return NULL;

    pr=container_of(next_wrr_se, struct task_struct, wrr);
	pr->se.exec_start=rq->clock_task;
	next_wrr_se->time_slice=(next_wrr_se->weight)*10;
    //printk(KERN_ALERT "^^^^^^^^^^^^^^^^^^^^^^^^66pick_next_task_wrr %d, %d, \n", pr->pid, next_wrr_se->weight);
	return pr;
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
    //printk(KERN_ALERT "\nenqueue_task_wrr\n\n");
    struct sched_wrr_entity *wrr_se = &(p->wrr);            // get wrr entity to enqueue
    struct wrr_rq *wrr_rq = &rq->wrr;
    //printk(KERN_ALERT "policy: %d\n", p->policy);
    
    rcu_read_lock();
    list_add_tail(&wrr_se->run_list, &wrr_rq->run_list);
    //printk(KERN_ALERT "[Before] %d: list_add_tail: %d is current rq's weightsum", rq->cpu, rq->wrr.weight_sum);
    rq->wrr.weight_sum += wrr_se->weight;
    //printk(KERN_ALERT "[After] %d: list_add_tail: %d is current rq's weightsum", rq->cpu, rq->wrr.weight_sum);
    rcu_read_unlock();
    //printk(KERN_ALERT "%wrr_se: pr %d\n", wrr_se->run_list, wrr_se->weight);
    //printk(KERN_ALERT "runqueue %p, %p, %p\n", &((rq->wrr).run_list), ((rq->wrr).run_list).next, ((rq->wrr).run_list).prev);
    //printk(KERN_ALERT "runqueue %p, %p, %p\n", &(wrr_se->run_list), (wrr_se->run_list).next, (wrr_se->run_list).prev);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
    //printk(KERN_ALERT "\ndequeue_task_wrr\n\n");
    struct sched_wrr_entity *wrr_se = &(p->wrr);
    rcu_read_lock();
	if(rq->wrr.weight_sum<=0){
		printk(KERN_ALERT "///////////////////////// dequeue error!");
		return;
	}
    list_del(&(wrr_se->run_list));
    rq->wrr.weight_sum -= wrr_se->weight;
    //printk(KERN_ALERT "%d: %d is current rq's weightsum", rq->cpu, rq->wrr.weight_sum);
    /*(struct list_head *en;
	struct list_head *wrr=&((rq->wrr).run_list);
	list_for_each(en, wrr){
		printk(KERN_ALERT "in list: %d\n", container_of(en, struct sched_wrr_entity, run_list)->weight);
	}*/
	rcu_read_unlock();
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued){
    //printk(KERN_ALERT "\ntask_tick_wrr\n\n");
    struct sched_wrr_entity *wrr_se;
    
    //rcu_read_lock();

    wrr_se = &(p->wrr);
    
    if((--wrr_se->time_slice)<=0){
       wrr_se->time_slice=wrr_se->weight*10;
       dequeue_task_wrr(rq, p, 0);
       enqueue_task_wrr(rq, p, 0);
       set_tsk_need_resched(p);;
       //resched_curr(rq);
    }
    
    //rcu_read_unlock();
}

#ifdef CONFIG_SCHED_DEBUG
void print_wrr_stats(struct seq_file *m, int cpu) 
{
    //printk(KERN_ALERT "\nprint_wrr_stats\n\n");
    struct rq * rq;
    
    rcu_read_lock();
    rq = cpu_rq(cpu);
    print_wrr_rq(m, cpu, &(rq->wrr));
    rcu_read_unlock();
}

#endif /* CONFIG_SCHED_DEBUG */


/****
 * put the wrr_entry to the tail of run Q
 */
static void
requeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity * wrr_se)
{
    //printk(KERN_ALERT "\nrequeueu\n\n");
    rcu_read_lock();
	wrr_se->time_slice=(wrr_se->weight)*10;
    list_move_tail(&wrr_se->run_list, &wrr_rq->run_list);
    rcu_read_unlock();
}

/**
 * requeue the currently running task into the tail of the runQ
>>>>>>> 61b8b9cf3590a8fe4173412ae88e57fe573540c2
 */
static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
    //printk(KERN_ALERT "\nrequeue task\n\n");
    struct sched_wrr_entity *wrr_se = &p->wrr; 
    requeue_wrr_entity(&rq->wrr, wrr_se);
    wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;
}

static void yield_task_wrr(struct rq *rq) {
    printk(KERN_ALERT "\nyield_task_wrr\n\n");
    // rq->curr contains the task_struct address currently running on CPU. 
	requeue_task_wrr(rq, rq->curr, 0);
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    // empty
}

#ifdef CONFIG_SMP
/***
 * @return: laziest cpu number
 */
static int select_task_rq_wrr(struct task_struct *p, int select_cpu, int sd_flag, int wake_flags)
{
    //printk(KERN_ALERT "\nselect_task_rq_wrr\n\n");
    struct rq *rq;
    int cpu;
    
    int lowest;
	int second;
	// weight_sum of the laziest cpu
    int temp_weight_sum;
    int selected_cpu = task_cpu(p);         // get the cpu number of the task_strcut 'p'
	second=selected_cpu;
    if (p->nr_cpus_allowed == 1)
        return selected_cpu;
    
    if(sd_flag != SD_BALANCE_FORK)
        return selected_cpu;
        
    rcu_read_lock();
    
    rq = cpu_rq(selected_cpu);
    //lowest = (&rq->wrr)->weight_sum;        // get the weight_sum of the laziest cpu
    lowest = 10000000;
    
    // By travesing every online cpu, get the laziest CPU number and the weight_sum of it
    for_each_online_cpu(cpu) {
        rq = cpu_rq(cpu);
        temp_weight_sum = (&rq->wrr)->weight_sum;
        
        //Future-safe accessor for struct task_struct's cpus_allowed.
        //#define tsk_cpus_allowed(tsk) (&(tsk)->cpus_allowed)
        if (temp_weight_sum < lowest && cpumask_test_cpu(cpu, &(p->cpus_allowed))) {
            second = selected_cpu;
			lowest = temp_weight_sum;
            selected_cpu = cpu;
        }
    }
	if(second==3){
	    printk(KERN_ALERT "This should not be called!!!!");
		second=0;
	}
	 
    rcu_read_unlock();
    
    return second;
}


// @TODO: Complete this function...!
static void migrate_task_rq_wrr(struct task_struct *p)
{

}


static void rq_online_wrr(struct rq *rq)
{
    // empty
}

static void rq_offline_wrr(struct rq *rq)
{
    // empty
}

static void task_woken_wrr(struct rq *this_rq, struct task_struct *task)
{
    // empty
}
#endif

static void set_curr_task_wrr(struct rq *rq)
{
    // empty
}

static void task_fork_wrr(struct task_struct *p)
{
    //printk(KERN_ALERT "\ntask_fork_wrr\n\n");
    struct sched_wrr_entity *wrr_se = &p->wrr;      // get the wrr entity of pass task_struct 'p'
    if(p->parent->wrr.weight <= 0){
        printk(KERN_ALERT "[KERN_ALERT] 0 is not correct weight!\n");
        wrr_se->weight=10;
    }
    else
        wrr_se->weight = p->parent->wrr.weight;         // weight of the wrr entity is inherited by parent
    wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;    // set the time_slice by the weight
    
}

static void prio_changed_wrr(struct rq *this_rq, struct task_struct *task, int oldprio)
{
    // empty
}

static void switched_from_wrr(struct rq *this_rq, struct task_struct *task)
{
    // empty
}

static void switched_to_wrr(struct rq *this_rq, struct task_struct *task)
{
    //printk(KERN_ALERT "\switched_to_wrr\n\n");
    struct sched_wrr_entity *wrr_se = &task->wrr;
    wrr_se->time_slice = wrr_se->weight * WRR_TIMESLICE;
}

/****
 * @return: return the the allocated time slice for 'task'
 */
static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
    return task->wrr.weight * WRR_TIMESLICE;
}


/**************************************
 **          Load Balancing          **
 **************************************/ 

/*****
 * This function selects the source cpu (which has the largest runQ weight sum)
 * and target cpu (which has the smallest runQ weight sum) for load balancing
 * @src_cpu: ptr to the source cpu
 * @dst_cpu: ptr to the target cpu
 * 
 */ 
static int find_migrate_cpu_pair(int *src_cpu, int *dst_cpu) {
    //printk(KERN_ALERT "\nfind\n\n");
	struct rq *curr_rq;
	int max_weight, min_weight;
	//int second_min_weight;
	int max_cpu, min_cpu;
	//int second_min_cpu;
	int cpu; 		// declared for for_each_cpu loop (correspond to 'i' of for loop)

	/* Init CPUs to make them accessible */
	// cpu_init() sets the information related to CPU states including 'gs' register.
	// If 'gs' register is set, we can know the current cpu number by calling smp_processor_id()
	int init_cpu = smp_processor_id();		// return the core number of current cpu
	if(!cpumask_test_cpu(init_cpu, cpu_active_mask)) {		// cpumaks_test_cpu(cpu_number, cpumask_pointer)
		/****
		 *  cpu_possible_mask - can exist CPU of the bit
    	 *  cpu_present_mask - exist CPU of the bit
         *  cpu_online_mask - exist CPU of the bit and scheduler can manage it
         *  cpu_active_mask - exist CPU of the bit and it is migratable
		 */ 
		init_cpu = cpumask_any(cpu_active_mask);	// cpumask_any - pick a "random" cpu from *srcp(the input cpumask)
	}

	curr_rq = cpu_rq(init_cpu);					// get run Q of init_cpu
	max_weight = 0;		                        // get the weight_sum of the init_cpu run Q
	min_weight = 10000000;                            // set to possible maximum weight + 1
	//second_min_weight = 10000000;                     // set to possible maximum weight + 1
	min_cpu = curr_rq->cpu;
	//second_min_cpu = min_cpu;
	max_cpu = curr_rq->cpu;
	*src_cpu = max_cpu;
	*dst_cpu = min_cpu;

	// If there is no active cpu which is migratable
	if(unlikely(!cpu_active_mask) && num_active_cpus() < 1) {
	    printk(KERN_ALERT "no active cpus\n");
		return 0;
    }

	rcu_read_lock();
	for_each_online_cpu(cpu) {
	
		curr_rq = cpu_rq(cpu);
	
		// if current cpu runQ's weight_sum is larger than temp_max, update it
		if(max_weight < curr_rq->wrr.weight_sum && curr_rq->cpu != 3) {
			max_weight = curr_rq->wrr.weight_sum;
			max_cpu = curr_rq->cpu;
		}
		// if current cpu runQ's weight_sum is smaller than temp_max, update it
		if(min_weight > curr_rq->wrr.weight_sum && curr_rq->cpu != 3){
				//second_min_cpu = min_cpu;
				//second_min_weight = min_weight;
				min_weight = curr_rq->wrr.weight_sum;
				min_cpu = curr_rq->cpu;
		}
		/*
		else if (curr_rq->wrr.weight_sum <= second_min_weight && curr_rq->wrr.weight_sum != min_weight && curr_rq->cpu != 3) {
            second_min_weight = curr_rq->wrr.weight_sum;
            second_min_cpu = curr_rq->cpu; 
        }*/
	}
	
	/*
	if(second_min_weight == max_weight && min_cpu != 3) {
	    second_min_weight = min_weight;
	    second_min_cpu = min_cpu;
	}
	
	if (second_min_weight == 10000000) {
	    printk(KERN_ALERT "second_min_weight is 10000!!!");
	    return 0;
	}*/
	
	if (min_weight == 10000000) {
	    printk(KERN_ALERT "min_weight is 10000000!!!");
	    return 0;
	}
	
	if (max_weight == 0) {
	    printk(KERN_ALERT "max_weight is 0!!!");
	    return 0;
	}
	
	printk(KERN_ALERT "max_weight:%d max_cpu:%d | min_weight: %d min_cpu: %d\n", max_weight, max_cpu, min_weight, min_cpu);

	
	//printk(KERN_ALERT "max_weight:%d max_cpu:%d | second_min_weight:%d second_min_cpu:%d | min_weight: %d min_cpu: %d) check wheter cpu#3 and #2 is in consideration!\n", max_weight, max_cpu, second_min_weight, second_min_cpu, min_weight, min_cpu);

	// After the cpu traversal, compare the max_weight and min_weight
	// If it is not migratable return
	if(max_weight <= min_weight || max_cpu == min_cpu){
        printk(KERN_ALERT "Since (max_weight:%d <= min_weight:%d || max_cpu:%d == min_cpu:%d) ==> not migratable!!!\n", max_weight, min_weight, max_cpu, min_cpu);
		rcu_read_unlock();
		return 0;
	}

	// If it is migratable return by reference
	*src_cpu = max_cpu;
	*dst_cpu = min_cpu;
	if(*dst_cpu==3) {
	    printk(KERN_ALERT "This shouldn't be entered!!!!");
		*dst_cpu=2;
	}

	rcu_read_unlock();
	return 1;
}

static int task_is_migratable(struct rq *rq, struct task_struct *p, int dst_cpu) {
    //printk(KERN_ALERT "\nmigratable\n\n");
    // @function: task_current
    // If p is running on the current CPU rq, then return 1.
    // else, then return 0
    if(!task_current(rq, p)                 // if task 'p' is not running on current cpu rq 
        && p->nr_cpus_allowed > 1           
        && cpumask_test_cpu(dst_cpu, &(p)->cpus_allowed)) {     // and if dst cpu is allowed
        //printk(KERN_ALERT "task_is_migratable: true!!");
        return 1;
    }
    
    //printk(KERN_ALERT "task_is_migratable: false!!");
    return 0;
}


static int migrate_task_wrr(int src_cpu, int dst_cpu) {
    //printk(KERN_ALERT "\nmigrate_task_wrr\n\n");
    struct sched_wrr_entity *wrr_e;
    struct sched_wrr_entity *temp;
    struct rq *rq_dst;
    struct rq *rq_src;
    struct task_struct *curr_task;
    struct task_struct *migrate_task = NULL;
    int highest_weight, min_weight, max_weight;
    
    rcu_read_lock();
    rq_src = cpu_rq(src_cpu);
    rq_dst = cpu_rq(dst_cpu);
    highest_weight = 0;
    
    // reset before traveral
    min_weight = rq_dst->wrr.weight_sum;
    max_weight = rq_src->wrr.weight_sum;
    list_for_each_entry_safe(wrr_e, temp, &rq_src->wrr.run_list, run_list) {
        curr_task = container_of(wrr_e, struct task_struct, wrr);
        
        if(task_is_migratable(rq_src, curr_task, dst_cpu) &&
           wrr_e->weight > highest_weight &&
           min_weight + wrr_e->weight <= max_weight - wrr_e->weight) {
                highest_weight = wrr_e->weight;
                migrate_task = curr_task;   
        }
    }
    rcu_read_unlock();
    
    // If there exists at least one migratable task, do it!
    if(migrate_task != NULL) {
        printk(KERN_ALERT "[migrating task] weight of migrating target task: %d\n", (migrate_task->wrr).weight);
 
        raw_spin_lock(&migrate_task->pi_lock);
        double_rq_lock(rq_src, rq_dst);
        if(task_cpu(migrate_task) != src_cpu) {
            printk(KERN_ALERT "already migrated\n");
            double_rq_unlock(rq_src, rq_dst);
            raw_spin_unlock(&migrate_task->pi_lock);
            return 1;
        }
        
        if(!cpumask_test_cpu(dst_cpu, &(migrate_task->cpus_allowed))) {
            printk(KERN_ALERT "cpumask_test_cpu failed");
            double_rq_unlock(rq_src, rq_dst);
            raw_spin_unlock(&migrate_task->pi_lock);
            return 0;
        }
        
        if(migrate_task->on_rq) {
            deactivate_task(rq_src, migrate_task, 1);
            set_task_cpu(migrate_task, rq_dst->cpu);
            activate_task(rq_dst, migrate_task, 0);
            check_preempt_curr(rq_dst, migrate_task, 0);
        }
        
        double_rq_unlock(rq_src, rq_dst);
        raw_spin_unlock(&migrate_task->pi_lock);
        
        return 1;
    }
    
    printk(KERN_ALERT "No migratable TASK!");
    return 0;
}

void wrr_load_balance(void) {
    //printk(KERN_ALERT "start load balance\n");
    //print_curr_cpu_weights();

    int src_cpu, dst_cpu, max_cpu, min_cpu;
    //rcu_read_lock();
    if(!find_migrate_cpu_pair(&max_cpu, &min_cpu)) {
        printk(KERN_ALERT "[Alert]: find_migrate_cpu_pair : Cannot find migratable cpu pair.\n\n");
        return;
    }
    src_cpu = max_cpu;
    dst_cpu = min_cpu;
    printk(KERN_ALERT "[migrate plan] src cpu#: %d --> dst cpu# : %d\n", src_cpu, dst_cpu);
    
    if(!migrate_task_wrr(src_cpu, dst_cpu)) {
        printk(KERN_ALERT "[Alert] migrate_task_wrr : No migratable task. Just return...\n\n");
    }
    //rcu_read_unlock();
    
    //printk(KERN_ALERT "success load balance\n");
    //print_curr_cpu_weights();
    return;
}

void task_dead_wrr(struct task_struct *p){
	//printk(KERN_ALERT "/////////////////////////////////dead!!!\n");
/*	if(p->wrr.weight==0)
		printk(KERN_ALERT "@@@@@@@@@@@@@@@@@@@@@@2strange");
	if((cpu_rq(p->cpu)->wrr).weight_sum!=0){
		dequeue_task_wrr(cpu_rq(p->cpu), p, 0);
	}*/
}

void update_curr_wrr(struct rq * rq){

}

/*
 * All the scheduling class method:
 */
const struct sched_class wrr_sched_class = {
/*******************************************************************
 * enqueue_task:		Process enters the TASK_RUNNING(ready) state
 * dequeue_task:		Process is not TASK_RUNNING state
 * yield_task:			Process calls yield() syscall by itself
 * check_preempt_curr:	check whether it is possible to preempt
 *                      currently running process
 * pick_next_task:		pick next process to be scheduled
 * put_prev_task:		put currently running task into the run Q again 
 * load_balance:		render load balancing
 * set_curr_task:		set task's sched_class or task group
 * task_tick:			call timer tick function
 *******************************************************************/
	.next			    = &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
    .yield_task		    = yield_task_wrr,
	.check_preempt_curr	= check_preempt_curr_wrr,
	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,
	.update_curr		= update_curr_wrr,
#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
	.migrate_task_rq	= migrate_task_rq_wrr,
	.rq_online		    = rq_online_wrr,
	.rq_offline		    = rq_offline_wrr,
	.task_woken         = task_woken_wrr,
	.set_cpus_allowed	= set_cpus_allowed_common,
#endif
    .task_dead          = task_dead_wrr,
    .set_curr_task      = set_curr_task_wrr,
	.task_tick		    = task_tick_wrr,
	.task_fork		    = task_fork_wrr,
	.prio_changed		= prio_changed_wrr,
	.switched_from		= switched_from_wrr,
	.switched_to		= switched_to_wrr,
	.get_rr_interval	= get_rr_interval_wrr,
    //////////////////////////////////////
    
};
