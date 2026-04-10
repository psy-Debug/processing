# 编译器
CC = gcc
# 编译选项
CFLAGS = -Wall -g
# 链接选项（需要pthread）
LDFLAGS = -pthread

# 所有目标
TARGETS = myshell cmd1 cmd2 cmd3 pipe_comm msg_queue sender receiver

all: $(TARGETS)

# shell
myshell: myshell.c
	$(CC) $(CFLAGS) -o $@ $<

# cmd程序
cmd1: cmd1.c
	$(CC) $(CFLAGS) -o $@ $<
cmd2: cmd2.c
	$(CC) $(CFLAGS) -o $@ $<
cmd3: cmd3.c
	$(CC) $(CFLAGS) -o $@ $<

# 管道通信
pipe_comm: pipe_comm.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# 消息队列
msg_queue: msg_queue.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# 共享内存
sender: sender.c
	$(CC) $(CFLAGS) -o $@ $<
receiver: receiver.c
	$(CC) $(CFLAGS) -o $@ $<

# 清理
clean:
	rm -f $(TARGETS) *.o

# 伪目标
.PHONY: all clean