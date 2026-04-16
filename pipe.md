函数速查表
管道相关
函数	格式	参数	返回值	作用
pipe	int pipe(int pipefd[2])	pipefd：存放两个fd的数组	成功0，失败-1	创建管道，[0]读端，[1]写端
read	ssize_t read(int fd, void *buf, size_t count)	fd：文件描述符
buf：缓冲区
count：要读的字节数	成功返回读到的字节数
0表示EOF
-1表示错误	从文件/管道读取数据
write	ssize_t write(int fd, const void *buf, size_t count)	fd：文件描述符
buf：数据源
count：要写的字节数	成功返回写入的字节数
-1表示错误	向文件/管道写入数据
close	int close(int fd)	fd：要关闭的文件描述符	成功0，失败-1	释放文件描述符，减少内核对象引用计数





进程相关
函数	格式	参数	返回值	作用
fork	pid_t fork(void)	无	子进程返回0
父进程返回子进程PID
失败返回-1	创建子进程，复制调用者的地址空间和文件描述符表
wait	pid_t wait(int *wstatus)	wstatus：子进程退出状态（可NULL）	成功返回子进程PID
失败返回-1	等待任意一个子进程结束，回收僵尸进程




信号量相关（POSIX命名信号量）
函数	格式	参数	返回值	作用
sem_open	sem_t *sem_open(const char *name, int oflag, mode_t mode, unsigned int value)	name：以/开头的名字
oflag：O_CREAT表示创建
mode：权限（如0666）
value：初始值（0或1）	成功返回信号量指针
失败返回SEM_FAILED	创建或打开一个命名信号量
sem_wait	int sem_wait(sem_t *sem)	sem：信号量指针	成功0，失败-1	P操作：信号量值>0则减1；=0则阻塞等待
sem_post	int sem_post(sem_t *sem)	sem：信号量指针	成功0，失败-1	V操作：信号量值加1，唤醒等待进程
sem_close	int sem_close(sem_t *sem)	sem：信号量指针	成功0，失败-1	关闭当前进程与信号量的连接
sem_unlink	int sem_unlink(const char *name)	name：信号量名字	成功0，失败-1	删除信号量的名字，引用计数到0时销毁对象







其他
函数	格式	参数	返回值	作用
snprintf	int snprintf(char *str, size_t size, const char *format, ...)	str：输出缓冲区
size：缓冲区大小
format：格式串	成功返回应该写入的字符数（不含\0）	格式化字符串到缓冲区（安全版本）
printf	int printf(const char *format, ...)	format：格式串	成功返回输出的字符数	打印到标准输出（stdout）
perror	void perror(const char *s)	s：自定义前缀字符串	无返回值	打印最近一次系统调用的错误信息
exit	void exit(int status)	status：退出状态码（0表示成功）	不返回	终止当前进程，刷新缓冲区，关闭打开的文件





关键机制总结
机制	代码体现	内核中发生了什么
管道创建	pipe(pipefd)	内核创建环形缓冲区 + 两个struct file指向它
文件描述符	int pipefd[2]	int是进程文件描述符表的下标，表中存的是内核对象指针
fork复制	fork()	子进程复制父进程的文件描述符表，pipefd值不变但指向同一内核管道
互斥信号量	sem_wait(sem_write)	初值1，保证一次只有一个子进程进入临界区
同步信号量	sem_wait(sem_read)	初值0，父进程等待三个子进程各post一次才唤醒
管道读EOF	read() == 0	当所有写端关闭且缓冲区空时，read返回0




