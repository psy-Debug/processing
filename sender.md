的，我们聚焦sender.c，只讲核心函数和关键概念。

一、变量定义和初始化
c
int shmid, sem_mutex, sem_sync;
char *shm_ptr;
union semun sem_union;
变量	类型	作用	初始值
shmid	int	共享内存ID	未初始化
sem_mutex	int	互斥信号量ID	未初始化
sem_sync	int	同步信号量ID	未初始化
shm_ptr	char*	指向共享内存的指针	未初始化
sem_union	union semun	信号量初始化辅助	未初始化
二、共享内存的创建和附加
1. shmget() - 创建共享内存
c
shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
参数详解：

SHM_KEY = 5678：键值，类似文件名，不同进程用相同key找到同一块内存

SHM_SIZE = 1024：大小1024字节

IPC_CREAT | 0666：不存在则创建，权限为读写（6=4读+2写）

返回值：

成功：返回shmid（如65538）

失败：返回-1

内核做了什么：

分配一块物理内存（实际4KB，因为页大小）

在内核中建立shmid_kernel结构

返回一个ID给用户态

2. shmat() - 附加到进程空间
c
shm_ptr = shmat(shmid, NULL, 0);
参数详解：

shmid：上面创建的ID

NULL：让内核自己选择虚拟地址

0：读写权限

返回值：

成功：返回虚拟地址（如0x7f1234567000）

失败：返回(void*)-1

"附加到进程空间"是什么意思？

text
附加前：
进程虚拟地址空间          物理内存
┌────────────┐           ┌────────┐
│ 代码段     │           │ 页框1  │
│ 数据段     │           │ 页框2  │
│ 堆         │           └────────┘
│ 空闲区域   │           ┌────────┐
│ 栈         │           │ 共享页 │ ← 内核刚分配的物理页
└────────────┘           └────────┘

附加后：
进程虚拟地址空间          物理内存
┌────────────┐           ┌────────┐
│ 代码段     │           │ 页框1  │
│ 数据段     │           │ 页框2  │
│ 堆         │           └────────┘
│ shm_ptr───┼──────────→┌────────┐
│ 空闲区域   │           │ 共享页 │
│ 栈         │           └────────┘
└────────────┘

本质：修改进程页表，添加一条映射关系
虚拟地址0x7f1234567000 → 物理地址0x12345000
三、信号量的创建和初始化
1. semget() - 创建信号量集
c
sem_mutex = semget(SEM_MUTEX_KEY, 1, IPC_CREAT | 0666);
sem_sync = semget(SEM_SYNC_KEY, 1, IPC_CREAT | 0666);
参数详解：

SEM_MUTEX_KEY = 8765：互斥信号量的键

SEM_SYNC_KEY = 8766：同步信号量的键

1：每个信号量集包含1个信号量

IPC_CREAT | 0666：创建，权限读写

返回值：

成功：返回semid

失败：返回-1

2. semctl() - 初始化信号量值
c
// 初始化互斥信号量为1
sem_union.val = 1;
semctl(sem_mutex, 0, SETVAL, sem_union);

// 初始化同步信号量为0
sem_union.val = 0;
semctl(sem_sync, 0, SETVAL, sem_union);
参数详解：

sem_mutex/sem_sync：信号量集ID

0：操作这个集中的第0个信号量

SETVAL：设置信号量的值

sem_union：包含要设置的值

为什么互斥信号量初始为1？

1表示资源可用

P操作：1→0（获得锁）

V操作：0→1（释放锁）

为什么同步信号量初始为0？

0表示还没有应答

sender执行P操作会阻塞

receiver执行V操作后唤醒sender

四、使用共享内存
写入共享内存
c
P(sem_mutex, 0);  // 获得互斥锁
fgets(shm_ptr, SHM_SIZE, stdin);  // 直接写入共享内存
V(sem_mutex, 0);  // 释放互斥锁
fgets(shm_ptr, SHM_SIZE, stdin)详解：

shm_ptr：目标地址，就是共享内存的虚拟地址

SHM_SIZE：最多读1024字节

stdin：从键盘输入读取

关键：数据直接从用户态缓冲区写入共享内存，不需要中间拷贝。

读取共享内存
c
P(sem_mutex, 0);
printf("Received: %s\n", shm_ptr);  // 从共享内存读
V(sem_mutex, 0);
printf从共享内存读：

shm_ptr指向共享内存

printf直接访问共享内存中的字符串

五、清理
c
shmdt(shm_ptr);
参数：shm_ptr - 要分离的共享内存地址

作用：

清除当前进程页表中的映射

减少内核中shm_nattch计数

不删除共享内存本身

完整流程图
text
1. shmget(5678, 1024, IPC_CREAT|0666)
   ↓
   内核分配物理内存，返回shmid=65538

2. shmat(65538, NULL, 0)
   ↓
   修改页表，映射虚拟地址到物理内存，返回shm_ptr=0x7f...

3. semget(8765, 1, IPC_CREAT|0666)
   ↓
   创建互斥信号量集，返回sem_mutex=65540

4. semget(8766, 1, IPC_CREAT|0666)
   ↓
   创建同步信号量集，返回sem_sync=65541

5. semctl(sem_mutex, 0, SETVAL, {val=1})
   ↓
   互斥信号量初始化为1

6. semctl(sem_sync, 0, SETVAL, {val=0})
   ↓
   同步信号量初始化为0

7. P(sem_mutex, 0)
   ↓
   互斥信号量：1→0，获得锁

8. fgets(shm_ptr, 1024, stdin)
   ↓
   用户输入直接写入共享内存

9. V(sem_mutex, 0)
   ↓
   互斥信号量：0→1，释放锁

10. P(sem_sync, 0)
    ↓
    同步信号量：0→? 值为0，进程阻塞等待

11. (receiver执行V后，sender被唤醒)
    ↓
    P(sem_mutex, 0) → 获得锁
    printf读shm_ptr → 输出应答
    V(sem_mutex, 0) → 释放锁

12. shmdt(shm_ptr)
    ↓
    解除映射，进程不能再访问
现在清楚了吗？需要我继续解释receiver.c吗？