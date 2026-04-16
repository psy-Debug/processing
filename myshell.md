作用
实现一个最简单的 shell，可以：

循环打印提示符 myshell>

读取用户输入的命令

如果是 exit 就退出

否则 fork() 一个子进程

子进程调用 execlp() 执行命令（利用 PATH 搜索）

父进程用 wait() 等待子进程结束

打印子进程的退出状态

关键点（与操作系统实验相关）
fork()：创建新进程（子进程复制父进程的地址空间）

execlp()：在当前进程加载并执行一个新程序（替换地址空间、代码、数据、栈）

wait()：父进程阻塞等待子进程，避免僵尸进程

进程状态：运行 → 就绪 → 等待 → 终止

exec 族函数：理解程序替换不创建新进程，只是把当前进程变成另一个程序

这个实验让你理解：

shell 的本质是一个不断 fork + exec 的循环。




一、execlp 的参数含义
c
execlp(cmd, cmd, NULL);
execlp 的原型是：

c
int execlp(const char *file, const char *arg0, const char *arg1, ..., NULL);
参数位置	含义	在你的代码中
第1个参数 file	要执行的程序文件名	cmd（如 "cmd1"）
第2个参数 arg0	传递给新程序的 argv[0]	cmd（也是 "cmd1"）
第3个参数 arg1	传递给新程序的 argv[1]	NULL（结束标志）
后续参数	argv[2], argv[3]...	没有







        一、fork() 声明
实验核心行为
调用 fork() 后，内核会：

复制当前进程的 PCB（进程控制块）
为子进程分配新的 PID
复制父进程的地址空间（代码、数据、堆、栈）（实际可能是写时拷贝 COW）
子进程和父进程从 fork 返回处继续执行
返回值：

父进程：返回子进程的 PID（>0）

子进程：返回 0

错误：返回 -1



2.1 内核中的入口：sys_fork
在 Linux 内核中，fork() 系统调用最终对应 sys_fork()，定义在 kernel/fork.c：

c
// 简化后的内核代码
SYSCALL_DEFINE0(fork)
{
    return _do_fork(SIGCHLD, 0, 0, NULL, NULL, 0);
}
2.2 核心函数：_do_fork() 和 copy_process()
所有进程创建（fork、vfork、clone）最终都通过 _do_fork() → copy_process() 完成。

c
// kernel/fork.c - 简化版
long _do_fork(unsigned long clone_flags, ...)
{
    // 1. 分配新的 PID
    pid_t pid = alloc_pid();
    
    // 2. 复制进程（核心！）
    struct task_struct *p = copy_process(clone_flags, ...);
    
    // 3. 将子进程加入调度队列
    wake_up_new_task(p);
    
    return pid;
}
2.3 copy_process：复制进程的核心
c
// kernel/fork.c - 简化版
static struct task_struct *copy_process(...)
{
    // 1. 分配新的 task_struct（进程控制块 PCB）
    struct task_struct *p = dup_task_struct(current);
    
    // 2. 复制各种资源（根据 clone_flags 决定是复制还是共享）
    if (copy_files(clone_flags, p))    // 复制文件描述符表
        goto bad_fork;
    if (copy_fs(clone_flags, p))       // 复制文件系统信息（当前目录等）
        goto bad_fork;
    if (copy_mm(clone_flags, p))       // 复制内存地址空间！关键！
        goto bad_fork;
    if (copy_sighand(clone_flags, p))  // 复制信号处理函数
        goto bad_fork;
    
    // 3. 设置子进程的返回值为 0（这是"一次调用两次返回"的关键）
    p->thread.regs.eax = 0;  // x86 架构，返回值寄存器
    
    // 4. 分配新的 PID
    p->pid = alloc_pid();
    
    return p;
}
2.4 copy_mm：写时复制（COW）的实现
c
// kernel/fork.c - 简化版
static int copy_mm(unsigned long clone_flags, struct task_struct *tsk)
{
    struct mm_struct *oldmm = current->mm;
    
    // 如果是内核线程（没有用户态地址空间），直接返回
    if (!oldmm)
        return 0;
    
    // 如果设置了 CLONE_VM（线程），共享地址空间，不复制
    if (clone_flags & CLONE_VM) {
        atomic_inc(&oldmm->mm_users);
        tsk->mm = oldmm;
        return 0;
    }
    
    // fork 的情况：复制地址空间（但页表指向相同物理页，标记只读）
    struct mm_struct *mm = dup_mm(oldmm);
    
    // dup_mm 内部：
    // 1. 复制页目录和页表
    // 2. 将所有页面标记为只读（写时复制）
    // 3. 父进程的对应页面也标记为只读
    
    tsk->mm = mm;
    return 0;
}
写时复制的核心思想：

fork() 时不复制物理内存，只复制页表

父子进程的页表项指向相同的物理页

这些页表项被标记为只读

当任何一方尝试写入时，CPU 触发缺页异常

内核在缺页异常中真正复制物理页，然后恢复写入权限

2.5 "一次调用，两次返回" 的原理
这是 fork 最神奇的地方，关键在 copy_thread() 函数：

c
// arch/x86/kernel/process.c - 简化版
int copy_thread(unsigned long clone_flags, ...)
{
    struct pt_regs *childregs = task_pt_regs(p);
    
    // 复制父进程的寄存器状态
    *childregs = *current_pt_regs();
    
    // 关键：把子进程的返回值寄存器设为 0
    childregs->ax = 0;  // x86 的 EAX/RAX 是返回值寄存器
    
    // 设置子进程的指令指针（返回后执行的位置）
    p->thread.ip = (unsigned long)ret_from_fork;
    
    return 0;
}
执行流程：

父进程调用 fork()，陷入内核

内核创建子进程，复制父进程的寄存器状态

但特意把子进程的返回值寄存器（ax）改成 0

父子进程各自返回用户态时：

父进程：ax 中保留的是子进程的 PID（>0）

子进程：ax 中是 0

这就是为什么 fork() 调用一次，但返回两次，且子进程返回 0。



        二、execlp 声明
行为
不创建新进程，而是替换当前进程的地址空间

代码、数据、堆、栈全部被新程序覆盖

如果执行成功：不返回

如果失败：返回 -1，并且原进程继续

你在 shell 中的具体执行过程



一、execlp 的实现（glibc 用户态部分）
1.1 你代码中看到的 execlp 声明
c
extern int execlp (const char *__file, const char *__arg, ...)
     __THROW __nonnull ((1, 2));
这只是一个声明。真正的实现在 glibc 的 posix/execlp.c 中。

1.2 execlp 的 glibc 实现
c
int execlp(const char *file, const char *arg, ...)
{
    // 第一步：遍历变长参数，统计参数个数
    va_start(ap, arg);
    for (argc = 1; va_arg(ap, const char *); argc++);
    va_end(ap);

    // 第二步：在栈上分配参数数组（注意注释：不能用 malloc，因为 vfork 场景会出问题）
    char *argv[argc + 1];

    // 第三步：把变长参数填入数组
    va_start(ap, arg);
    argv[0] = (char *)arg;
    for (i = 1; i <= argc; i++)
        argv[i] = va_arg(ap, char *);
    va_end(ap);

    // 第四步：调用 __execvpe 做真正的工作
    return __execvpe(file, argv, __environ);
}
关键点：

execlp 本质上只是把变长参数打包成 argv 数组，然后调用 __execvpe

注释特别说明：不能用 malloc，因为可能在 vfork() 之后调用，此时父进程地址空间被借用，malloc 会破坏状态

__execvpe 才是真正搜索 PATH 并调用 execve 系统调用的地方



逻辑步骤	在哪里写的	代码示例
打印提示符	你的 myshell.c	printf("myshell> ");
读取用户输入	你的 myshell.c	fgets(cmd, ...)
创建子进程	你的 myshell.c	pid = fork();
执行用户命令	你的 myshell.c	execlp(cmd, cmd, NULL);
等待子进程	你的 myshell.c	wait(&status);
fork 的声明	系统头文件	/usr/include/unistd.h
fork 的实现	glibc + 内核	汇编陷入 + sys_fork