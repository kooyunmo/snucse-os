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

# Rotation Lock

> **[NOTE]**
> 1. 과제 진행을 위해서 조사 및 활용했던 자료들은 [wiki](https://github.com/hyojeonglee/osfall2019-team10/wiki/Project3-Useful-Functions)에 정리 되어 있습니다.
> 2. `proj3` 브랜치와 별개로 다른 방식으로 구현된 코드도 `yunmo` 브랜치에 존재합니다. (마찬가지로 잘 동작한다고 판단됩니다.)

## Overview

이번 과제에서는 rotation degree에 따라서 각각 특정한 degree와 range가 주어진 reader와 writer 프로세스들이 올바르게 lock을 잡아 연산을 수행하도록 해야한다.

- rotation degree를 범위 내에 포함하는 reader는 writer가 lock을 잡고 있는 상태가 아니라면 lock을 잡을 수 있다.
- rotation degree를 범위 내에 포함하는 writer는 reader나 다른 writer가 lock을 잡고 있는 상태가 아니라면 lock을 잡을 수 있다.

> 이 프로젝트에서 writer에 해당하는 프로세스는 `selector` 프로세스로, `integer`라는 파일에 특정 숫자를 계속해서 write 한다.
> reader에 해당하는 프로세스는 `trial` 프로세스로, `integer` 파일에 있는 정수값을 읽은 뒤 해당 값에 대해서 소인수분해를 하고 결과를 출력한다.
> rotation degree는 `rotd` 프로세스에 의해서 특정 시간 간격으로 변화한다.

### Algorithm

1. 우리가 구현한 코드에서는 writer starvation 방지와 fairness를 위해서 기본적으로 lock을 시도한 시점의 순서를 고려한다.
2. rotd가 범위에 들어오는 reader들은 동시에 lock을 잡고 풀 수 있다.
3. rotd가 범위에 들어오는 writer들 중엔 가장 빨리 lock을 시도한 프로세스 하나만 lock을 잡을 수 있다.
4. reader와 writer 간에는 lock을 시도한 시점이 빠른 프로세스가 먼저 lock을 잡을 수 있다.
  + 다음과 같은 순서로 reader와 writer가 lock을 시도했다고 해보자. (모든 프로세스는 현재 rotd가 자신의 범위에 포함된다고 가정)
    + R1 ==> W1 ==> R2 ==> R3 ==> W2 ==> R4 ==> R5 ==> R6 ...
  + 현재 R1이 lock을 잡았고, R1이 lock을 잡은 상태에서 소인수분해를 수행하는 동안 W1 ~ R6까지의 프로세스들이 lock을 시도했다고 하자.
  + 2번 조건에 따르면 R2, R3, R4, R5, R6는 동시에 lock을 잡을 수도 있지만, R2, R3 앞에는 W1이 먼저 lock을 시도하였고, R4, R5, R6 앞에는 W2가 먼저 lock을 시도하였다.
  + 따라서 R1이 unlock을 한 직후엔 W1이 가장 먼저 lock을 잡는다.
  + W1이 unlock한 직후에서야 비로소 R2, R3가 lock을 잡는다. (이 때 R2, R3 사이의 순서는 보장 되지 않는다. 다만 둘 다 동시에 lock을 잡을 수 있다.)
  + R2, R3가 unlcok을 하면 기다리던 W2가 lock을 잡는다. R4, R5, R6는 자신보다 먼저 들어온 W2가 unlock한 후에야 lock을 잡을 수 있다.


## Data Structure

### 1. range descriptor

```
typedef struct __range_descriptor {
    int degree;
    int range;
    pid_t pid;
    int is_holding_lock;
    char type; // READ_TYPE or WRITE_TYPE
    struct list_head list;
} range_descriptor;
```

- 각각의 reader와 writer의 정보를 추적 및 관리하기 위해서 선언한 구조체이다.
- 하나의 `range_descriptor`는 linked list에서 하나의 노드가 되어 이어진다.

### 2. List & Mutex & CV / Semaphore

```
LIST_HEAD(rd_list);
DECLARE_WAIT_QUEUE_HEAD(__wait_queue);
DEFINE_MUTEX(rd_list_mutex);
DEFINE_MUTEX(wait_mutex);
atomic_t __read_cnt = ATOMIC_INIT(0);
atomic_t __write_cnt = ATOMIC_INIT(0);
static int curr_degree;
```
- `LIST_HEAD(rd_list)`: 위에서 설명한 `range_descriptor` 구조체들이 연결될 리스트의 헤드이다.
- `DECLARE_WAIT_QUEUE_HEAD(__wait_queue)`: 여러 개의 wait queue를 통해서 구현할 수도 있지만 proj3 브랜치의 코드에서는 wait queue 하나만 사용한다.
- `DEFINE_MUTEX(rd_list_mutex)`: `range_descriptor` 리스트의 mutual exclusion을 위해서 선언
- `DEFINE_MUTEX(wait_mutex)`: wait queue를 위한 mutex
- `atomic_t __read_cnt = ATOMIC_INIT(0)`: 현재 lock을 잡은 reader의 개수 (0 이상의 값)
- `atomic_t __write_cnt = ATOMIC_INIT(0)`: 현재 writer가 lock은 잡은 상태인가? (TRUE: 1, FALSE: 0 두 가지 값만 가능)
- `static int curr_degree`: `set_rotation` 시스템 콜을 통해서 설정되는 현재의 rotation degree


## Primary Auxiliary Functions

### 1. Range Validation
```
static int onrange(int degree, int range) {
    int start = degree >= range ? degree - range : 360 + degree - range;
    int end = (degree + range) % 360;
    return (start <= end && curr_degree >= start && curr_degree <= end)
            || (start > end && (curr_degree < end || curr_degree > start));
}
```

- 시스템 콜을 통해서 전달된 degree와 range가 현재의 rotation degree에 대해서 유효한지 검사
- rotd가 계산된 범위 안에 들어온다면 return 1
- 범위 안에 들어오지 않는다면 return 0

### 2. Initialize
```
static range_descriptor* init_rd(int degree, int range, int type) {
    range_descriptor *rd = kmalloc(sizeof(range_descriptor), GFP_KERNEL);
    rd->degree = degree;
    rd->range = range;
    rd->pid = current->pid;
    rd->type = type;
    rd->is_holding_lock = 0;
    return rd;
}
```

- `range_descriptor`를 초기화하여 메모리 공간 관리

### 3. Sleep
```
static void sleep_on(void) {
    mutex_lock(&wait_mutex);
    DEFINE_WAIT(wait_queue_entry);
    prepare_to_wait(&__wait_queue, &wait_queue_entry, TASK_INTERRUPTIBLE);
    mutex_unlock(&wait_mutex);
    schedule();
    mutex_lock(&wait_mutex);
    finish_wait(&__wait_queue, &wait_queue_entry);
    mutex_unlock(&wait_mutex);
}
```

- lock, unlock을 하는 과정에서 프로세스를 BLOCK할 필요가 있을 때 이 함수를 호출한다.
- `DEFINE_WAIT(wait_queue_entry)`
  + `wait_queue_entry`라는 이름으로 wait queue entry를 생성하고, 해당 구조체의 필드를 현재 CPU에서 돌고 있는 프로세스인 `current`와 `autoremove_wake_function()` 함수의 주소로 초기화한다.
- `prepare_to_wait(&__wait_queue, &wait_queue_entry, TASK_INTERRUPTIBLE)`
  + 현재 프로세스의 wait queue entry인 `wait_queue_entry`가 wait queue head인 `__wait_queue`에 등록되어 있지 않으면 등록하고, `current->state`를 `TASK_INTERRUPTIBLE`로 변경하여 이후 `remove_wait_queue`나 `finish_wait`를 통해서 삭제 가능하도록 한다.
- `schedule()`
  + 새롭게 스케줄링이 됨에 따라서 현재 프로세스는 이제 BLOCK 상태가 되고, 다른 프로세스로 컨트롤이 넘어간다.
- `finish_wait(&__wait_queue, &wait_queue_entry)`
  + 깨어나는 조건을 만족하면, 현재 프로세스는 다시 READY 상태가 되고 wait_queue에서 제거 된다.
- 이 함수에서는 쓰레드 간에 공유되는 wait queue를 다루기 때문에 mutual exclusion이 제대로 이루어져야 한다. 또한 `schedule()`을 호출하기 직전에는 다른 쓰레드로 컨트롤이 넘어가기 때문에 그 전에 미리 unlock을 해서 이후 다른 쓰레드에서 작업이 진행될 수 있도록 해야한다. 시그널을 받고 `schedule()` 뒤의 코드가 실행 될 때는 다시 mutex lock을 잡아서 상호배제를 보장해주어야 한다.
- 관련된 설명과 참고자료는 [wiki](https://github.com/hyojeonglee/osfall2019-team10/wiki/Project3-Useful-Functions)에 매우 자세히 설명 해두었다.

> **[NOTE]**
>
> Sleep과 관련된 auxiliary function은 존재하지만 wake up을 위한 함수는 존재하지 않는 이유는 우리는 항상 `wake_up_all()`을 통해서 BLOCK 상태의 프로세스들을 깨웠기 때문이다. 특정 프로세스를 깨우는 것이 효율적이라고 생각해서 처음에는 `wake_up_process()`와 같은 함수를 활용하고자 하였으나, 잘 동작하지 않았다. 따라서 특정 프로세스를 깨워야하는 상황이 발생한다면 일단 wait queue의 모두를 다 깨우고, 이후에 일어나면 안되는 프로세스들은 다시 sleep 하도록 하였다.


## System Call

> 시스템 콜 등록 과정은 proj1, proj2와 다를바가 없으므로 생략

### 1. set_rotation
```
long sys_set_rotation(int degree) {
    printk(KERN_CONT "set_rotation: %d \n", degree);
    curr_degree = degree;
    mutex_lock(&wait_mutex);
    wake_up_all(&__wait_queue);
    mutex_unlock(&wait_mutex);
    return 0;
}
```

- 특정 시간 간격(우리의 경우 3초)을 기준으로 현재의 rotation degree를 30씩 증가시켜주는 `rotd.c`에서 호출하는 시스템 콜이다.
- 이를 통해서 전역 변수로 선언되어 있는 `curr_degree`에 새로운 rotation degree 값을 입력 시킬 수 있다.
- rotation degree가 새롭게 설정되면 wait queue의 validation이 새롭게 이루어져야하기 때문에 `wake_up_all()`을 통해 잠자고 있는 프로세스들을 깨운다.

### 2. rotlock_read

```
long sys_rotlock_read(int degree, int range) {
    range_descriptor *new_rd, *rd_cursor;
    int has_earlier_writing;

    printk(KERN_ALERT "try to hold read lock. pid: %u\n", current->pid);

    new_rd = init_rd(degree, range, READ_TYPE);

    mutex_lock(&rd_list_mutex);
    list_add_tail(&new_rd->list, &rd_list);

    while (1) {
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

        int or= onrange(degree, range);
        int write_cnt = atomic_read(&__write_cnt);
        int condition = !or || write_cnt || has_earlier_writing;
        if (!condition) break;
        mutex_unlock(&rd_list_mutex);
        sleep_on();
        mutex_lock(&rd_list_mutex);
    }

    new_rd->is_holding_lock = 1;
    atomic_add(1, &__read_cnt);

    printk(KERN_ALERT "success in holding read lock. pid: %u\n", current->pid);
    mutex_unlock(&rd_list_mutex);
    return 0;
}
```
- reader들 끼리는 동시에 lock을 잡을 수 있되, 자신보다 먼저 lock을 시도한 writer가 있다면 (즉, wait queue 상에서 reader 자신보다 head에 가까운 writer가 하나라도 존재한다면) 그 writer가 먼저 lock을 잡고 unlock한 뒤에서야 lock을 잡을 수 있다.
- 자신보다 wait queue 상에서 앞에 존재하는 writer의 존재 여부를 파악하기 위해서 range_descriptor list(`rd_list`)를 순회한다. 각각의 range_descriptor entry는 `type (reader / writer)` 을 멤버로 가지고 있기 때문에 해당 엔트리가 reader인지 writer인지 쉽게 파악할 수 있다.
- 그 밖에도 reader의 range가 rotd에 대해서 유효한지 역시 검사되어야 한다.
- 만약 조건을 만족하지 못하였다면 조건을 만족할 때까지 `sleep_on()`을 통해서 BLOCK된 상태를 유지해야한다.
- lock을 잡았다면 `atomic_add` 함수를 통해 `__read_cnt`를 1 증가 시켜서 현재 lock을 잡고 있는 reader의 수를 업데이트 해준다. 이는 writer가 lock을 잡을 수 있는 조건을 검사하는데 활용이 된다.

### 3. rotlock_write
```
long sys_rotlock_write(int degree, int range) {
    range_descriptor *new_rd, *rd_cursor;
    int has_earlier_reading;

    printk(KERN_ALERT "call write lock. pid: %u\n", current->pid);
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
```
- range에 대한 유효성 검사는 reader와 마찬가지로 이루어져야한다.
- writer는 reader와 다르게 여럿의 writer가 함께 lock을 잡는 것이 불가능하다.
- 한 개의 reader라도 현재 lock을 잡고 있는 상태라면 writer는 기다려야한다.
- 조건을 만족하지 못하였다면 `sleep_on()` 함수를 통해 잠이 든다.
- lock을 잡았다면 `atomic_add` 함수를 통해 `__write_cnt`를 1 증가시킨다.
  + reader의 `__read_cnt`는 1 보다 큰 값도 가질 수 있는 counting variable이지만, `__write_cnt`는 이름과는 다르게 0과 1 두가지 값만 가질 수 있는 boolean variable의 역할을 한다. write는 오직 한 개만이 lock을 잡을 수 있기 때문이다.


### 4. rotunlock_read
```
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
```
- range에 대한 유효성 검사는 unlock 시에도 이루어져야한다.
  + unlock 시에도 range에 대한 유효성을 체크하는 것이 매우 비효율적이라고 판단되어서 제거하려고 하였으나, 과제 스펙에 명시가 되어 있어서 그대로 구현을 하였다.
- unlock 시엔 `atomic_sub`를 통해서 `__read_cnt`를 감소 시켜주어야 한다.

### 5. rotunlock_write
```
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
```
- reader와 마찬가지로 rotd가 올바른 형식이어야하고 rotd가 writer의 범위 내에 들어와야 unlock을 할 수 있다.
- `atomic_sub`를 통해서 `__write_cnt`를 0(false)으로 만들어준다.

## Test Code

> **[NOTE]**
>
> `setting.sh` 파일을 다른 테스트 코드와 함께 마운트 디렉토리의 root에 마운트 시킨 뒤, 에뮬레이터 상에서 `$ sh settings.sh`를 하면 `setting.sh`에 미리 입력되어있는 순서대로 `rotd`, `selector`, `trial`를 백그라운드에서 실행시켜준다. 여러개의 프로세스가 돌아가는 중간에는 쉘에 계속해서 로그 메세지들이 출력되기 때문에 스크립트를 통해 한번에 여러 프로세스를 백그라운드로 실행시키면 보다 용이하게 테스트를 할 수 있다.

### 1. rotd

특정 시간이 지남에 따라서 현재의 rotation degree가 30씩 증가한다. (`0, 30, 60, ... 330, 0, 30, ...`)
rotation degree가 변하는 주기는 처음에 2초로 설정되어 있었으나, 우리는 값의 변화를 좀 더 명확하게 관찰하기 위해서 3초로 해둔 상태이다.

### 2. selector

`integer`라는 이름의 텍스트 파일에 소인수 분해의 대상이 될 숫자를 write한다. write를 한 뒤엔 이전 숫자보다 1이 증가한 숫자를 다시 write한다. 

### 3. trial

`selector`가 write한 `integer` 파일의 숫자를 읽어서 소인수분해를 수행한다.

## Trouble Shooting

1. 특정 프로세스만 깨우는 방법은 무엇인가?
프로젝트를 진행하는 동안 부분을 가장 많이 고민했던 것 같다.(특히 초반에) 프로젝트 진행 초반에는 `wake_up_all`을 쓰는 것이 비효율적이라고 생각해서 `wake_up_process`, `wake_up_interruptible`과 같은 특정 프로세스만 깨우는 함수들을 활용하고자 했다. 하지만 위의 함수들의 조작이 생각처럼 이루어지지 않아서 일단은 `wake_up_all`을 사용하되 깨울 task들만 넣는 wait queue를 하나 더 만들어서 깨울 task는 전체 wait queue에서 제거하고 wake up용 temp wait queue에 삽입한 뒤 temp wait queue에서 `wake_up_all`을 사용하는 방식도 시도해보았다. 하지만 결론적으로는 `wake_up_all`을 통해서 모두 깨우고 깨울 필요가 없는 프로세스는 다시 재우는 방식이 가장 구현하기 편리했던 것 같다. 

2. 여러 개의 reader와 writer가 기다리고 있는 상황에서 reader와 writer 간에 lock을 잡는 순서
프로젝트 진행 후반 부에는 writer starvation을 방지하고 reader와 writer 간에 순서를 고려하여 lock을 잡도록 하는 것이 까다로웠다. 하지만 전체적인 자료구조를 다시 검토함으로써 잘 해결할 수 있었던 것 같다.

## Lessons & Learns

- 멀티 쓰레딩을 활용하는 능력은 이제 필수적으로 요구되는 사항이지만 알고리즘이나 자료구조 수업에서 배우는 내용들은 기본적으로 sequential한 프로그램을 가정하고 가르치는 경우가 많다. 병렬적인 프로그램은 사람들의 직관과는 다른 점이 많기 때문에 이를 효율적이고 정확하게 다루기 위해서는 많은 연습이 필요할 것 같다.
- 지난 스케줄러 프로젝트에서 lock을 통한 synchronization으로 인한 문제점을 해결하는 것이 매우 까다로웠다. 이번 과제를 통해서 병렬적인 프로세스에 대한 이해를 높일 수 있었다.
- 지난 과제에서 스케줄러를 다루면서 주로 run queue 자료구조와 그를 다루는 함수에 대해서 공부를 했다면, 이번 과제에서는 `wait.h`에 있는 wait queue 관련 자료구조 및 관련 함수를 공부할 수 있었고, 그 과정에서 `RUN - READY -BLOCK`으로 이루어진 task의 상태가 구체적으로 어떻게 관리되는지 알 수 있었다. ~~(스케줄러를 먼저 만들고 lock을 구현한 이유를 조금은 알 것 같았다.)~~
- 지난 과제 보고서의 project retrospective에 개발 패턴에 변화를 주겠다고 언급을 하였다. 그리고 이번 프로젝트는 그에 따라서 구현을 했는데 전체적인 프로세스를 다 함께 이해할 수 있고, 코드 내에서 서로가 작성한 다른 부분에 예상치 못한 depency가 있어서 디버깅 하기 어려운 에러가 발생하는 문제는 더 이상 발생하지 않았다. 하지만 좀더 복잡하고 큰 프로젝트라면 이런 방식으로 작업하면 그 속도가 상당히 느려질 것이라고 판단된다. 다음 프로젝트에서는 가능하다면 전체 프로젝트를 independent component들로 나누고 각자 다른 component를 작업하는 방식으로 진행을 해볼까 한다. 
