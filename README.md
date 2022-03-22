```
Linux kernel
============

This file was moved to Documentation/admin-guide/README.rst

Please notice that there are several guides for kernel developers and users.
These guides can be rendered in a number of formats, like HTML and PDF.

In order to build the documentation, use ``make htmldocs`` or
``make pdfdocs``.

There are various text files in the Documentation/ subdirectory,
several of them using the Restructured Text markup notation.
See Documentation/00-INDEX for a list of what is contained in each file.

Please read the Documentation/process/changes.rst file, as it contains the
requirements for building and running the kernel, and information about
the problems which may result by upgrading your kernel.
```

> 구체적인 개발 과정은 `proj2` 브랜치의 commit log와 Pull Request를 확인 확인해주시기 바랍니다. 


# 1. Overview

이번 과제에서는 linux 기반의 tizen 운영체제에 새로운 scheduling policy를 추가하고 소인수 분해를 하는 test 프로그램을 통해 해당 policy의 성능을 평가 및 개선해야한다. 새롭게 추가할 policy는 **Weighted Round Robin**(이하 **WRR**)이며, 이는 RT(real-time) 스케줄링 정책으로 이미 존재하는 Round Robin의 방식과 비슷하다. 다만 RR과는 다르게 각각의 process task마다 서로 다른 weight를 가지며, 이 weight(1~20)에 따라 해당 task의 스케줄링 time slice가 결정된다. time slice는 `weight * 10ms`로 정해지며 weight의 기본 값은 10이다. 이에 더하여 각각의 CPU의 run queue에 새롭게 등록된 WRR run queue들 중 가장 큰 weight sum을 가진 run queue의 task를 가장 작은 weight sum을 가진 run queue로 migrate 해주는 load balancing 작업을 수행해야 한다.

### 주요 수정 대상 파일들
1. `include/linux/sched.h`
2. `include/uapi/linux/sched.h`
3. `kernel/sched/sched.h`
4. `include/linux/init_task.h`
5. `kernel/sched/wrr.c`
7. `kernel/sched/core.c`
8. `kernel/sched/rt.c`
9. `kernel/sched/debug.c`

----
# 2. Data Structures & Functions

> 본 repo의 [wiki](https://github.com/hyojeonglee/osfall2019-team10/wiki/Useful-Functions)와 [issue](https://github.com/hyojeonglee/osfall2019-team10/issues/1)에 유용한 링크와 함수들을 정리해 두었습니다.

## Process Hierarchy
- RT 프로세스
   + 0~99 사이 우선권을 가진다.
   + 일반 프로세스에 비해 높은 우선권으로 실행하므로 빠르고 신속히 처리하도록 구현 되어있다.
   + 스케줄러 정책은 SCHED_FIFO 이며 우선 순위가 더 높은 RT 프로세스가 없으면 계속 CPU를 점유하여 사용한다. 런큐 내 서브 런큐 중 RT 프로세스를 관리하는 RT 프로세스 런큐가 있으며 `struct rt_rq` 구조체로 구현되어 있다.
- 일반 프로세스
   + 100~139 사이 우선권을 갖고 있습니다. CFS 스케줄러 클래스로 실행되며 대부분 프로세스가 이 범주에 속하게 된다.
   + CFS 스케줄러 정책에 따라 SCHED_NORMAL 스케줄링 정책으로 CFS 스케줄러는 시분할 방식으로 프로세스를 관리한다.

**우리가 이번에 구현할 WRR 정책의 런큐는 RT와 CFS의 사이에 존재한다.** 따라서 `rt_rq`의 next는 WRR policy가 되어야 한다.

```c
// kernel/sched/rt.c

const struct sched_class rt_sched_class = {
   //.next           = &fair_sched_class,   // <--- BEFORE
   .next			= &wrr_sched_class,
```

## Weighted Round Robin (WRR)

### struct sched_class wrr_sched_class

```c
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
    .next               = &fair_sched_class,
    .enqueue_task       = enqueue_task_wrr,
    .dequeue_task       = dequeue_task_wrr,
    .yield_task         = yield_task_wrr,
    .check_preempt_curr = check_preempt_curr_wrr,
    .pick_next_task     = pick_next_task_wrr,
    .put_prev_task      = put_prev_task_wrr,
    .update_curr        = update_curr_wrr,
#ifdef CONFIG_SMP
    .select_task_rq	= select_task_rq_wrr,
    .migrate_task_rq    = migrate_task_rq_wrr,
    .rq_online          = rq_online_wrr,
    .rq_offline         = rq_offline_wrr,
    .task_woken         = task_woken_wrr,
    .set_cpus_allowed   = set_cpus_allowed_common,
#endif
    .task_dead          = task_dead_wrr,
    .set_curr_task      = set_curr_task_wrr,
    .task_tick          = task_tick_wrr,
    .task_fork          = task_fork_wrr,
    .prio_changed       = prio_changed_wrr,
    .switched_from      = switched_from_wrr,
    .switched_to        = switched_to_wrr,
    .get_rr_interval	= get_rr_interval_wrr,
};
```

### 스케줄러 클래스 관련 주요 함수
- enqueue_task: 프로세스가 TASK_RUNNING(ready) 상태에 들어가도록 런큐에 삽입한다.
- dequeue_task: 프로세스는 TASK_RUNNING 상태가 아니므로 런큐에서 삭제한다.
- yield_task: 프로세스가 `yield()` syscall을 호출하였을 경우에 현재 TASK_RUNNING(run) 상태였던 task를 런큐의 tail로 re-enqueue한다.
- check_preempt_curr: 현재 스케줄링 된 프로세스가 preempt 가능한지 체크한다. (구현할 필요 없었음)
- pick_next_task: 다음으로 스케줄링될 프로세스를 뽑는다.
- put_prev_task: 현재 스케줄링 중인 task를 다시 런큐에 삽입한다. (구현할 필요 없었음)
- load_balance: load balancing을 수행한다. 
- task_tick: 매 CPU tick마다 호출되는 함수이다. 이를 통해서 task에게 부여된 time slice가 만료되었는지를 synchronous하게 확인하는 등 다양하게 활용이 가능한 함수이다.

이들 함수들은 모두 `kernel/sched/core.c`에서 사용된다.

> **참고: 프로세스 상태**
> 커널에서 정의한 프로세스의 상태는 다음 3가지 매크로를 통해 정의된다.
> - `#define TASK_RUNNING            0`
> - `#define TASK_INTERRUPTIBLE      1`
> - `#define TASK_UNINTERRUPTIBLE    2`
> 이와 같은 상태들은 task_struct의 `state` 멤버에 저장이 되는데 이 중에서 `TASK_RUNNING` 상태의 경우 실행 대기(ready) 상태와 실행 중(run) 상태를 모두 나타낸다. 따라서 이 둘을 구분하기 위해서는 `task_current` 함수 등을 통해서 `struct rq`의 `curr` 필드에 접근해서 현재 CPU를 점유하며 실행 중인 프로세스를 확인해야만 한다.

### wrr_rq

```c
struct wrr_rq {
    unsigned long weight_sum;
    struct list_head run_list;
    int timeout;
    struct task_struct *curr;
};
```
- CPU 런큐 중 우리가 새롭게 추가한 WRR 런큐를 관리하는 구조체 타입의 정의이다.
- `weight_sum`을 통해 런큐에 있는 task들의 전체 weight 합을 저장한다. 이를 통해 load balancing 시 source 런큐와 destination 런큐를 정할 수 있다.
- `run_list`는 task_struct가 그러하듯이 구조체의 멤버로 linked list 노드를 가짐으로써 entry 순회를 효과적으로 할 수 있도록 한다. 


### sched_wrr_entity

```c
struct sched_wrr_entity {
    struct list_head                run_list;
    unsigned long                   timeout;
    unsigned int                    time_slice;
    unsigned int                    weight;
};
```

- WRR 런큐에 들어가는 task 각각의 entity를 정의한 구조체이다.
- `run_list`: 우리가 구현할 WRR 런큐의 태스크들은 linked list 형태로 관리 되어야하기에 이 멤버가 존재한다.
- `time_slice`: 각 task는 10ms * weight를 time slice로 할당받고, 해당 길이의 시간만큼 스케줄링된다.`
- `weight`: weight는 1~20의 값을 가지며 task가 스케줄링 되는 시간을 정한다.


----
# 3. System Call

```c
// kernel/sched/core.c

SYSCALL_DEFINE2(sched_setweight, pid_t, pid, int, weight){
    return sched_setweight(pid, weight);
}

SYSCALL_DEFINE1(sched_getweight, pid_t, pid){
    return sched_getweight(pid);
}
```

위와 같이 시스템 콜을 정의하고 아래과 같이 함수의 기능을 구현한다.

```c
// kernel/sched/core.c
long sched_setweight(pid_t pid, int weight) {
    if (weight <= 0 || weight > 20)
        return -EINVAL;
    struct task_struct *p;
    if (pid == 0)
        p = current;
    else
        p = pid_task(find_get_pid(pid), PIDTYPE_PID);
    struct rq * rq=task_rq(p);
	if (!p || !rq)
        return -EINVAL;

	rcu_read_lock();
	(rq->wrr).weight_sum-=(p->wrr).weight;
	(rq->wrr).weight_sum+=weight;
	(p->wrr).weight = weight;
    (p->wrr).time_slice = weight*10;
    rcu_read_unlock();
    return p->wrr.weight;
}

long sched_getweight(pid_t pid){

    struct task_struct *p;
    struct pid *t=find_get_pid(pid);
    p=pid_task(t, PIDTYPE_PID);
    if(!p){
        return -EINVAL;
    }
    return p->wrr.weight;
}
```

----
# 4. Load Balancing 

이번 과제의 주요한 스펙 중 하나인 load balancing은 2000ms를 간격으로 가장 바쁜 CPU 런큐의 task를 가장 한가한 CPU 런큐로 migrate시키는 작업이다. 여기에서 '바쁜 정도'는 CPU 런큐에 존재하는 task들의 weight 합을 통해 측정되며, 따라서 가장 바쁜 CPU는 런큐의 weight sum이 가장 크다는 것을 의미한다. 따라서 로드 밸런싱은 다음과 같은 함수들로써 구현된다.
- 2000ms 단위로 `core.c`에서 `wrr.c`에 정의된 load balancing 함수를 호출한다.
    + 이 때 2000ms는 `jiffies`를 이용해서 카운트 하면 구현에 용이하다.
- source 런큐인 가장 바쁜 CPU 런큐와 destination 런큐인 가장 한가한 CPU 런큐를 선택하는 함수를 구현한다.
- 가장 바쁜 런큐의 task 중에서 가장 큰 weight를 가졌으면서, migrate 시 weight sum의 역전이 발생하지 않고, 현재 run 상태는 아닌 task가 존재하는지 확인한다.
- 위의 조건을 만족한다면 enqueue, dequeue를 통해 migrate를 수행한다.


----
# 5. Performance
![image](https://user-images.githubusercontent.com/17061663/68064845-6e43ba00-fd64-11e9-8b2e-47d5b491a806.png)


----
# 6. Improvement

### 1. Considering Preemption
- `check_preempt_curr`: 현재 실행 중인 프로세스가 preempt 되어야 하는지 검사를 하여 여러 조건이 충족되면 `set_tsk_need_resched`를 통해 `schedule` 함수를 호출한다. migration시에 source CPU와 destination CPU 간의 이동이 끝나면 이 함수를 통해서 새롭게 스케줄링을 한다. migration 이후에는 런큐의 상태가 변화했기 때문에 preemptive하게 re-scheduling을 하는 것이 유리할 수 있다.
- `preempt_disable`: 어느 쓰레드가 동작 동작 중일 때 다른 thread가 수행되는 preemption이 발생하지 않도록 한다. (인터럽트는 막지 않지만 다른 쓰레드가 동작하는 것을 막고 현재 쓰레드만 계속 수행되도록 한다.) preemptive multitasking OS에서는 critical section에서 preemption을 disabling하여 현재 thread가 CPU 자원을 독점하도록 하는 것이 좋다.
- `preempt_enable`: `preemption_disable`로 막아두었던 preemption을 다시 풀어준다.

### 2. Load Balancing Threshold

2000ms 단위로 수행하는 로드밸런싱은 상당히 오버헤드가 크고 CPU affinity를 저하할 수 있으므로 전체 프로세스의 개수가 많지 않다면(하나의 CPU만으로 충분히 빠르게 처리할 수 있는 경우라면), 로드 밸런싱을 수행하지 않는다. 따라서 fork의 수가 특정 threshold 이하인 경우에는 로드밸런싱을 수행하지 않는다.




----
# 7. Retrospective

이번 과제는 우리 팀이 예상한 것과는 달리 상당히 고비가 많았다. 과제가 나오자마자 바로 시작을 한 것은 아니지만 상당히 많은 시간을 투자하였고, 현 시점 기준으로 `proj` 브랜치에만 114개의 커밋을 하였다. _(아마 디버깅 용으로 만들고 지우고 했던 브랜치들까지 합치면 150커밋은 넘을 것이다.)_ 이렇게 상당히 많은 시간을 투자하였으나 최초 deadline을 지키지 못하고 delay를 하였다. 따라서 이번 프로젝트의 문제점은 무엇이었는지 왜 기한 내에 성공적으로 완성을 하지 못했는지에 대한 회고가 필요할 것이다.

## 1. 수정은 신중하게
지난 과제인 ptree의 경우 여러 파일들과 함수들 간에 복잡한 dependency가 존재하지 않았기 때문에 코드를 과감하게 수정을 하고 쉽게 쉽게 커밋을 쌓아나가도 무방했다. 하지만 이번 과제와 같이 하나의 소스파일에만 수천 라인의 코드와 수백 개의 함수들이 존재하는 개발 태스크의 경우엔 수정에 매우 신중할 필요가 있다. 또한 개발 원칙에 따라 깃헙의 main 브랜치는 항상 에러없이 돌아갈 수 있는 상황을 유지했어야 했다.

### 개선 방안
- 깃헙의 add rule 기능을 활용해서 master 브랜치 역할을 하는 `projX` 브랜치에는 직접적인 커밋을 하지 못하도록 제한한다. 커밋을 위해서는 반드시 브랜치를 따로 파서 작업을 하고 Pull Request를 통해서 리뷰를 거친 뒤에 merge 시키도록한다.
- 개발의 초반부에는 에러가 하나도 없는 코드를 올리기 어렵겠지만 틀이 한번 갖춰지고 에러가 없어진 시점부터는 에러가 있는 코드는 절대로 깃헙에 올리지 않아야 한다.
 

## 2. 개발 패턴의 정립
이번 과제에서는 3명의 팀원들이 각자 함수 단위로 작게 태스크를 나누어 구현을 하는 식으로 개발을 하였는데 서로의 부분에 대해서 이해가 부족한 상태에서 의존성이 높은 큰 시스템을 개발하려고 하니 에러가 발생하기 쉬웠고, 에러가 발생하였을 때 이를 고치는 것이 정말 힘들었다. 다른 사람이 작성한 코드에 대해서 파악이 제대로 되지 않은 상태에서 내가 짠 코드, 혹은 기존 커널 코드를 수정하여 디버깅을 하다보니 오히려 점점 미궁 속으로 빠지는 기분이었다.

### 개선 방안
- 앞으로는 서브 태스크를 나누어 개발하는 방식보다는 모두가 같은 부분을 함께 개발하며 이야기를 나누고 그 중에서 가장 좋은 코드를 논의를 거쳐 main 브랜치로 병합하는 방식을 택하는 것이 좋을 것 같다.
- 기능의 구현도 중요하지만 해당 프로젝트 구현에 적합한 개발 패턴을 미리 논의하고 들어가는 것이 시간을 아끼는 길이다.

