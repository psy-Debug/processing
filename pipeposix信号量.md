不是，POSIX信号量有两种定义和初始化方式，本程序使用的是命名信号量，还有另一种未命名信号量（基于内存的信号量）。
两种方式对比
表格
特性
命名信号量（本程序使用）
未命名信号量
定义方式
 sem_t *sem （指针）
 sem_t sem （变量）
初始化函数
 sem_open() 
 sem_init() 
销毁方式
 sem_close()  +  sem_unlink() 
 sem_destroy() 
跨进程共享
✅ 通过名字在文件系统共享
✅ 需放在共享内存中
使用场景
无亲缘关系进程
线程或有亲缘关系进程
持久化
内核持久，需手动unlink
随进程结束自动销毁
本程序为何用命名信号量？
c
// 命名信号量特点
sem_write = sem_open("/write_sem", O_CREAT, 0666, 1);
//              ↑
//           全局名字，任何进程都能通过这个名字找到它

原因：父子进程虽然有亲缘关系，但本程序通过名字访问信号量，更直观且易于调试（可以用  ls /dev/shm/  查看）。
未命名信号量版本（等效实现）
如果改用未命名信号量，需要配合共享内存：
c
// 1. 创建共享内存
int shm_fd = shm_open("/my_shm", O_CREAT|O_RDWR, 0666);
ftruncate(shm_fd, sizeof(sem_t) * 2);
sem_t *sems = mmap(NULL, sizeof(sem_t)*2, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);

// 2. 在共享内存中初始化信号量
sem_init(&sems[0], 1, 1);  // 第3个参数1表示跨进程共享
sem_init(&sems[1], 1, 0);

// 3. 使用 &sems[0] 代替 sem_write

明显更复杂，所以本程序选择命名信号量是合理的。
总结
表格
问题
答案
只能这么定义吗？
❌ 不是，还有未命名信号量
哪种更简单？
命名信号量（本程序选择正确）
哪种更高效？
未命名信号量（少一次文件系统查找）
哪种更通用？
命名信号量（适合任意进程）