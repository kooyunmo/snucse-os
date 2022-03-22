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

# 개발 Process

## 1. Syscall 등록

우리가 제작한 sys_ptree 함수를 system call로 등록하기 위해서는 몇 가지 절차가 필요하다.

1. syscall table에 함수 등록
    + syscall이 작동하기 위해선 특정 syscall 번호(여기에선 398번)에 함수명을 할당해야한다.
    + 우리가 사용하는 tizen은 arm 아키텍쳐를 따르기 때문에 `arch/arm64/include/asm/unistd32.h`에 등록을 해주어야 한다. 
    + syscall maximum entry number을 399로 바꿔 시스템 콜이 398번까지 있다는 것을 알린다.
        + 처음엔 당연히 x86이라는 생각만으로 x86 syscall table(`arch/x86/entry/syscalls/syscall_64.tbl`)에 함수 등록을 하는 바람에 오류가 발생했다.
2. `include/linux/prinfo.h` 헤더 파일에 prinfo 구조체를 선언해준다.
3. `include/linux/syscalls.h` 헤더 파일에는 여러 시스콜 함수들이 선언되어 있는데 우리가 등록할 sys_ptree() 함수도 여기에 등록을 해준다. 이 때 헤더파일로도 `<linux/prinfo.h>`를 include 해주어야 한다.
4. `include/uapi/asm-generic/unistd.h`에 398번과 sys_ptree함수를 연결시킨다.
5. 커널 Makefile(`kernel/Makefile`)에 `ptree.o` ojective file 역시 추가해준다.
6. `~/kernel/` 디렉토리에 sys_ptree 함수의 코드를 구현한다.


## 2. Kernel 코드 sys_ptree()

### debugfs를 통한 디버깅
우리가 구현한 system call인 sys_ptree 함수의 몸체 부분이다. 라즈베리파이에 컴파일된 모듈들을 올리기 전에 로컬 linux PC에서 커널 코드 디버깅을 하였다. `~/dbfs_ptree_test/dbfs_ptree.c`가 해당 파일이며, `$ make` 실행시 dmesg를 통해 출력을 볼 수 있다. 여기에서는 `linux/debugfs.h` 모듈을 활용하였는데 make 시에 `$ sudo insmod dbfs_ptree.ko`이 실행되어 리눅스 커널에 mudule을 insert한다. 이 때 dbfs_ptree.c의 `__init dbfs_module_init()` 부분이 호출되어 ptree를 순회하며 출력하는 로직이 동작한다. `make clean` 시엔 `$ sudo rmmod dbfs_ptree.ko`가 실행되어 삽입되었던 모듈을 제거하고 `__exit dbfs_module_exit()` 부분이 실행된다.

### task_struct

우리가 system call을 통해서 출력을 해야하는 것은 각 프로세스의 (1)state, (2)pid, (3)부모 프로세스 pid, (4)첫 번째 자식의 pid, (5) sibling의 pid, (6) uid, (7) 프로세스 이름이다. 그리고 이 7가지 값들은 task_struct라는 구조체를 통해 쉽게 접근할 수 있다. 이를 위해서는 우선 시작 프로세스의 task_struct에 접근을 해야하는데 우리가 가진 정보는 시작 프로세스의 pid가 1(systemd)과 2(kthreadd)라는 것밖에 없다. 따라서 pid를 통해서 task_struct를 구해야하고 이를 가능하게 해주는 커널 함수가 `find_get_pid`와 `pid_task`이다. 이 두 함수를 통해서 systemd와 kthreadd의 task_struct를 얻고 이를 시작점으로 순회를 시작하면된다.

![taststruct](https://user-images.githubusercontent.com/17061663/66257440-5cf2a680-e7d4-11e9-9fc4-af179ff91b8e.jpeg)


순회를 하면서 첫 번째 자식 프로세스와 형제 프로세스의 task_struct에 접근을 해야하는데 이를 위해서는 다음과 같은 과정을 거쳐야한다.
- 우선 현재 task_struct의 children 멤버에 접근한다.
- children은 list_head 구조체로 선언되어 있는데 children의 next가 현재 프로세스의 자식 프로세스 task_struct의 sibling 멤버와 연결이 되어있다. 따라서 자식 task_struct에 접근하기 위해서는 container_of나 list_entry 함수를 사용해야한다.
    + `struct task_struct *ch = container_of((&curr->children)->next, struct task_struct, sibling);`
- 형제 프로세스 역시 이와 마찬가지이다.
    + `struct task_struct *sb = container_of((&curr->sibling)->next, struct task_struct, sibling);`


### Process Tree Preorder-Travesal Logic

> **swapper, systemd, kthreadd에 관하여**
>
> Kernel이 처음 initialize 되었을 때 `rest_init()`이라는 함수를 마지막으로 호출하는데, 이 함수에서 `kthread_create()`를 통해서 생성하는 첫 번째 kernel thread가 바로 systemd (pid 1번)이고, 그 다음으로 생성되는 kernel thread가 kthreadd (pid 2번)이다. 그리고 이 두 thread를 생성한 thread가 바로 swapper (pid 0번)이 된다. 이와 같은 이유로 각각 0, 1, 2의 pid를 갖는 것이고 모든 프로세스의 집합은 이들을 root로 하는 general tree의 형태를 가지게 된다. 

process들의 집합은 swapper(pid:0)을 root로 하는 general tree 구조를 가지고 있다. 따라서 general tree를 preorder로 순회하는 로직을 구현하면 된다. 처음에는 general tree 순회 로직을 `list.h`의 `list_for_each` 함수 없이 직접 구현하였는데 `list_for_each`를 활용하는 것이 훨씬 안전하고 효율적임을 알게되어서 이를 활용하도록 코드를 수정하였다. `void preorderTraversal(struct task_struct *curr)` 함수는 root task를 인자로 받아서 그 root를 기점으로 preorder visit을 하도록 재귀적으로 구현이 되었다. 다음은 순회에 사용된 기본적인 구조이다.

```c
static void preorderTraversal(struct task_struct* curr) {
    
    // copy current process info to buf

    list_for_each(list, &curr->children) {
        task = list_entry(list, struct task_struct, sibling);
        preorderTraversal(task);
    }
}
```

이러한 순회 과정에서 curr 프로세스의 정보를 preorderTraversal의 caller 함수의 buf에 저장을 해야하는데 이를 위해서는 memcpy를 사용했다.

```c
memcpy(prinfo_buffer+process_cnt, temp_prinfo, sizeof(struct prinfo));
```

### copy_from_user & copy_to_user

우리가 새롭게 등록한 system call 함수인 `sys_ptree`가 유저 테스트 코드(`test.c`)에서 호출되면 호출시 전달된 파라미터들은 커널에 정의된 `asmlinkage long sys_ptree(struct prinfo *buf, int *nr)`로 전달된다. 그런데 유저 메모리를 커널 메모리에서 가져와서 사용할 수는 없으므로 유저 메모리의 내용을 커널 메모리로 복사하고, 작업을 마친 뒤에는 이를 다시 유저 메모리로 복사 해주는 과정이 필요하다. 이를 위해 커널에서 제공하는 함수가 바로 `copy_from_user`와 `copy_to_user`이다. `copy_to_user` 함수는 유저 모드의 가상주소의 특정 주소 값부터 시작해서 특정 크기만큼의 데이터를 커널 가상 주소로 복사시켜주는 함수이고 `copy_to_user`는 이와 반대의 기능을 하는 함수이다.

```c
if(copy_from_user(temp_nr, nr, sizeof(int)) != 0) {
    perror("copy_from_user ERROR: could not read nr struct from user");
    errno= -EFAULT;
}

...

if(copy_from_user(temp_buf, buf, sizeof(struct prinfo)*(*temp_nr)) != 0) {
    perror("copy_from_user ERROR: could not read buf struct from user");
    errno= -EFAULT;
}


// do the job

if(copy_to_user(buf, temp_buf, sizeof(struct prinfo)*(*temp_nr)) != 0) {
    printk("copy_to_user ERROR: could not copy buf to user\n");
}

...

if(copy_to_user(nr, temp_nr, sizeof(int)) != 0) {
    printk("copy_to_user ERROR: could not copy nr to user\n");
}
```

### *nr update와 return value
우리의 시스템콜의 리턴값은 모든 프로세스의 개수이다. 이를 위해 tree traversal은 끝까지 했으나 mem copy to user은 받은 nr*까지만 했다. 또한 함수의 실행이 끝난 후, process의 개수가 파라메터로 받은 *nr보다 작을 경우, *nr값을 update 시켜주었다. 이는 프로젝트의 조건과 같다.


## 3. User test

### 사용자로부터 input을 받은 후 메모리 할당
테스트 코드는 사용자로부터 프린트 할 프로세스 개수를 입력받아 실행된다. 이를 통해 여러 케이스의 테스트를 진행할 수 있다. 입력받은 숫자 만큼의 메모리를 할당하여 시스템콜을 호출한다. 이를 위해서는 <syscall.h>, <unistd.h>의 헤더가 필요하다. 

```c
    struct prinfo *k=(struct prinfo *)malloc(sizeof(struct prinfo)*nr); 
    int total=syscall(398, k, &nr);
```

system call의 리턴 값은 프로세스 개수 혹은 -1이기 때문에 에러의 유무를 판단할 수 있다.

### Error Handling
system call의 리턴 값이 0보다 작을 경우 경우의 수를 나누어 에러 핸들링을 하였다.
만약 메모리의 주소가 NULL이거나 입력한 수가 1보다 작을 경우 errno를 EINVAL로 설정하여 perror을 통해 에러 메세지를 프린트했다.
다른 경우는 메모리에 접근할 수 없는 경우이므로 errno를 EFAULT로 설정하여 perror을 통해 에러 메세지를 프린트했다. 이하는 해당 코드이다.

```c
    if(total<0){
        if(k==NULL || &nr==NULL || nr<1 ){
            errno=EINVAL;
            perror("Pointer is NULL or nr is less than 1");
        }
        else{
            errno=EFAULT;
            perror("Memory is outside of the accessible address space");
        }    
        return -1;
    }
```

### Printing format and method
성공적으로 양수의 값이 리턴되었다면 받은 값들을 프린트한다.
프린트 포멧은 다음과 같다.

```c
    printf("%s,%d,%lld,%d,%d,%d,%lld\n",k->comm, k->pid, k->state, k->parent_pid, k->first_child_pid, k->next_sibling_pid, k->uid);
```

이는 해당 프로젝트의 조건과 같다.

indent 조건은 다음과 같다.
1. previous task pid ==current task's parent pid
	+ indent ++;
2. previous task pid!= current task's parent and previous task next sibling!= current task's pid
	+ update next task's indent as stacked indent value
3. current task has both child and sibling
	+ save indent number in the stack

1의 경우는 child task일 경우 인덴트를 하기 위하여 설정을 했다. 
2의 경우는 어떤 process가 이전 테스크의 child도 sibling도 아닐 때 indent정보를 stack에서 받아오게 하기 위하여 설정을 했다. 해당 indent정보는 이전의 다른 테스크에서 저장을 한다. 저장 조건은 3번과 같다.
3의 경우는 어떤 process가 child도 있고 sibling도 있을 때 indent를 저장하게 하기 위하여 설정을 했다. child의 process를 먼저 프린트 하게되어 next sibling의 indent정보는 알 수가 없게 되기 때문에 해당 정보를 stack에 저장하여 후에 정보를 다시 찾을 수 있게 했다. 

indent 저장을 stack 방식으로 한 이유는 항상 가장 마지막에 저장한 indent가 가장 처음으로 필요로 하는 process의 indent가 되어 LIFO를 만족하기 때문이다. 구현은 array와 하나의 index integer로 했다. 해당 코드는 다음과 같다. 

```c
      int *ind=(int*)malloc((nr+1)/2*sizeof(int));   // indent stack
      int index=-1;//stack pointer
      int tab=0;//current indent
  
      printf("%s,%d,%lld,%d,%d,%d,%lld\n",k->comm, k->pid, k->state, k->parent_pid, k->first_child_pid, k->next_sibling_pid, k->uid);    //printing format
      if((k->first_child_pid!=0) && (k->next_sibling_pid!=0)){
          index++;
          ind[index]=0;
      }
      for(int i=1; i<nr; i++){
          if(((k+i-1)->first_child_pid==(k+i)->pid))// case 1
              tab++;
          else if((k+i-1)->next_sibling_pid!=(k+i)->pid){//case 2
              tab=ind[index];
              index--;
          }
  
          for(int j=0;j<tab;j++)
              printf("\t");
  
          if(((k+i)->first_child_pid!=0) && ((k+i)->next_sibling_pid!=0)){//case 3
              index++;
              ind[index]=tab;
          }
  
          printf("%s,%d,%lld,%d,%d,%d,%lld\n",(k+i)->comm, (k+i)->pid, (k+i)->state, (k+i)->parent_pid, (k+i)->first_child_pid, (k+i)->next_sibling_pid, (k+i)->uid);
  
      }
```

# 느낀점

시스템프로그래밍과 OS 수업에서 배운 kernel 관련 내용을 실습해보고 여러가지 커널 모듈들을 활용해 볼 수 있어서 좋은 공부가 되었다. 하지만 라즈베리파이와 시리얼 통신을 하는 과정에서 에러가 많아서 코딩 외적으로 힘든 점이 많았던 것같다. 그리고 불운하게 팀원 한 분이 드랍을 하셔서 두 명이서 열심히 그 공백을 채우기 위해 노력을 하였는데 혹시라도 다른 팀에서도 드랍자가 발생한다면 팀을 합칠 수 있는 기회가 있으면 좋겠다.

