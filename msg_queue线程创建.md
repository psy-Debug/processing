一、用户态编程角度的区别
你的代码使用的是线程（pthread）
c
pthread_create(&t1, NULL, sender1, NULL);  // 创建线程
pthread_create(&t2, NULL, sender2, NULL);
pthread_create(&t3, NULL, receiver, NULL);
如果改成进程（fork）
c
pid_t pid1 = fork();  // 创建进程
if (pid1 == 0) {
    // 子进程
    sender1(NULL);
    exit(0);
}
关键区别表
特性	线程（pthread）	进程（fork）
地址空间	共享同一地址空间	每个进程独立地址空间
全局变量	所有线程共享	各自独立（写时拷贝）
消息队列ID	共享同一个msgid	子进程继承msgid（同一个）
信号量	共享（需创建为共享）	需特殊设置（PSHARED）
创建开销	小（几百微秒）	大（几毫秒）
通信方式	直接读写全局变量	需要IPC（管道、消息队列等）
崩溃影响	一个线程崩溃可能影响整个进程	进程崩溃不影响其他进程
二、你的代码如果用进程会怎样？
假设改成进程版本：

c
int main() {
    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);  // 父进程创建
    
    pid_t pid1 = fork();
    if (pid1 == 0) {
        // 子进程1 - sender1
        // msgid 是父进程的拷贝，指向同一个内核消息队列
        sender1(NULL);
        exit(0);
    }
    
    pid_t pid2 = fork();
    if (pid2 == 0) {
        // 子进程2 - sender2
        sender2(NULL);
        exit(0);
    }
    
    pid_t pid3 = fork();
    if (pid3 == 0) {
        // 子进程3 - receiver
        receiver(NULL);
        exit(0);
    }
    
    // 父进程等待
    wait(NULL); wait(NULL); wait(NULL);
}
问题来了：

信号量失效：sem_init(&sem_terminal, 0, 1) 中的第二个参数0表示线程间共享，进程间无效

全局变量不共享：sender1_exited 等变量各自独立，无法同步状态

需要改用：sem_init(&sem_terminal, 1, 1)（PSHARED标志）或改用信号量文件

三、内核实现机制深度对比
1. 线程创建（pthread_create）
内核调用链：

c
pthread_create()
    └── clone(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND, ...)
        └── do_fork() / kernel_clone()
            └── copy_process()
                ├── 复制task_struct（进程描述符）
                ├── 共享内存描述符（CLONE_VM）  // 关键！
                ├── 共享文件表（CLONE_FILES）
                ├── 共享信号处理（CLONE_SIGHAND）
                └── 不复制页表，指向相同物理内存
关键点：CLONE_VM标志使线程共享同一地址空间

2. 进程创建（fork）
内核调用链：

c
fork()
    └── clone(SIGCHLD, 0)
        └── do_fork() / kernel_clone()
            └── copy_process()
                ├── 复制task_struct
                ├── 复制内存描述符（mm_struct）  // 新页表！
                ├── 复制文件表（复制但指向相同文件）
                ├── 写时拷贝（COW）机制
                └── 复制页表项，标记为只读
四、内存布局对比图
text
线程内存布局：
┌─────────────────┐
│   代码段（共享）  │
├─────────────────┤
│   数据段（共享）  │  ← 所有线程共享
├─────────────────┤
│   堆（共享）      │
├─────────────────┤
│   线程1栈        │
├─────────────────┤
│   线程2栈        │
├─────────────────┤
│   线程3栈        │
└─────────────────┘

进程内存布局（每个进程独立）：
进程A              进程B              进程C
┌─────────┐      ┌─────────┐      ┌─────────┐
│ 代码段   │      │ 代码段   │      │ 代码段   │
├─────────┤      ├─────────┤      ├─────────┤
│ 数据段   │      │ 数据段   │      │ 数据段   │
├─────────┤      ├─────────┤      ├─────────┤
│ 堆       │      │ 堆       │      │ 堆       │
├─────────┤      ├─────────┤      ├─────────┤
│ 栈       │      │ 栈       │      │ 栈       │
└─────────┘      └─────────┘      └─────────┘



想看内核实现，重点看这些：

bash
# 进程/线程创建核心
kernel/fork.c
    - copy_process()    # 复制的核心逻辑
    - copy_mm()         # 内存复制（CLONE_VM是关键）

# clone系统调用
arch/x86/entry/syscalls/syscall_64.tbl  # 看384号clone
kernel/sys.c
    - __do_sys_clone()

# 调度相关
kernel/sched/core.c
    - schedule()        # 调度算法
关键结构体：

c
struct task_struct {
    struct mm_struct *mm;        // 进程地址空间
    struct files_struct *files;   // 打开的文件
    pid_t pid;                    // 进程ID
    pid_t tgid;                   // 线程组ID（主线程PID）
    // ...
};







你说到点子上了！这正是线程和进程编程模型最直观的区别——线程创建不需要像fork那样判断返回值，因为线程是"平级"的，而进程是"父子"关系。

一、为什么线程不需要判断返回值？
进程的fork()需要判断：
c
pid_t pid = fork();
if (pid == -1) {
    // 错误处理
} else if (pid == 0) {
    // 子进程代码
    // 这里是全新的执行流，代码从fork()返回处开始执行
    do_child_work();
    exit(0);
} else {
    // 父进程代码
    // pid存储的是子进程的ID
    wait(NULL);
}
为什么需要判断？ fork()调用一次，返回两次：

父进程返回子进程的PID（>0）

子进程返回0

同一个函数调用，在两个不同的执行流中返回不同的值

线程的pthread_create()不需要：
c
pthread_create(&t1, NULL, sender1, NULL);  // 调用一次，返回一次
// 父线程继续执行下面的代码
// 新线程从sender1函数开始执行，不回来
pthread_create(&t2, NULL, sender2, NULL);
为什么不需要？

新线程不是从pthread_create返回处开始，而是从指定的函数（sender1）开始

创建线程的父线程继续执行pthread_create之后的代码

两个执行流在创建时就分道扬镳了

二、对比图
text
fork() 的返回方式：
                
父进程                 子进程
  |                     |
  | fork()              |
  |   ↓                 |
  | 判断返回值(>0)      |
  |   ↓                 | (从fork返回，返回0)
  | 继续执行            | 判断返回值(==0)
  |   ↓                 |   ↓
  | wait()              | 执行子进程逻辑
  |   ↓                 |   ↓
  | 等待子进程          | exit()
  
pthread_create() 的返回方式：

主线程                新线程
  |                     |
  | pthread_create()    |
  |   ↓                 | (立即开始执行sender1)
  | 判断返回值(0/错误)  |   ↓
  |   ↓                 | 执行sender1逻辑
  | 继续执行后续代码    |   ↓
  |   ↓                 | pthread_exit()
  | pthread_join()      |
  |   ↓                 |
  | 等待线程结束        |
三、你的代码中的体现
你的main函数：

c
int main() {
    // 创建消息队列...
    
    // 创建三个线程，不需要判断返回值来区分执行路径
    pthread_create(&t1, NULL, sender1, NULL);  // 线程1开始
    pthread_create(&t2, NULL, sender2, NULL);  // 线程2开始
    pthread_create(&t3, NULL, receiver, NULL); // 线程3开始
    
    // main线程继续执行，等待所有线程结束
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    
    // 清理...
    return 0;
}
main线程做了什么？

创建三个子线程

然后像个"管家"一样，等待所有线程结束

最后清理退出

如果用fork实现同样的功能：

c
int main() {
    pid_t pid1, pid2, pid3;
    
    pid1 = fork();
    if (pid1 == 0) {
        // 必须在这里写sender1的逻辑
        sender1(NULL);
        exit(0);
    }
    
    pid2 = fork();
    if (pid2 == 0) {
        sender2(NULL);
        exit(0);
    }
    
    pid3 = fork();
    if (pid3 == 0) {
        receiver(NULL);
        exit(0);
    }
    
    // 父进程等待
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    waitpid(pid3, NULL, 0);
    
    return 0;
}




fork()的内核实现逻辑：
c
// kernel/fork.c 简化版
long do_fork(...) {
    struct task_struct *p;
    
    p = copy_process(...);  // 复制进程描述符
    
    if (IS_ERR(p))
        return PTR_ERR(p);
    
    // 关键：子进程从这里开始执行
    // 子进程返回0，父进程返回子进程的PID
    if (current->pid == parent_pid) {
        return p->pid;  // 父进程返回子进程PID
    } else {
        return 0;       // 子进程返回0
    }
}
pthread_create()的内核实现逻辑：
c
// glibc的pthread_create最终调用clone
int pthread_create(...) {
    // 准备启动参数
    struct pthread *pd = ...;
    
    // clone系统调用，但指定了起始函数
    int ret = clone(start_thread, 
                    stack, 
                    CLONE_VM | CLONE_FS | ...,  // 共享标志
                    pd);
    
    // 新线程从start_thread开始执行，不是从clone返回
    // 所以这里只需要返回成功或失败
    return ret;
}