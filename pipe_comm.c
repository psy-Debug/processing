#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PIPE_SIZE 65536  // 管道默认大小
#define MSG_SIZE 1024    // 每次发送大小

int main() {
    int pipefd[2];
    pid_t pid[3];
    sem_t *sem_write, *sem_read;
    
    // 创建管道——内核里会创建一个pipe对象，包含缓冲区
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }
    
    // 创建POSIX信号量——这是用来互斥访问管道的
    sem_unlink("/write_sem");
    sem_unlink("/read_sem");
    sem_write = sem_open("/write_sem", O_CREAT, 0666, 1);
    sem_read = sem_open("/read_sem", O_CREAT, 0666, 0);
    
    // 创建三个子进程
    for (int i = 0; i < 3; i++) {
        pid[i] = fork();
        
        if (pid[i] == 0) {
            // 子进程代码
            close(pipefd[0]);  // 关闭读端
            
            char msg[MSG_SIZE];
            snprintf(msg, sizeof(msg), "Message from child %d", i+1);
            
            // 互斥访问管道——信号量保证一次只有一个子进程写
            sem_wait(sem_write);
            
            printf("Child %d: Writing to pipe...\n", i+1);
            int bytes = write(pipefd[1], msg, MSG_SIZE);
            printf("Child %d: Wrote %d bytes\n", i+1, bytes);
            
            sem_post(sem_write);
            sem_post(sem_read);  // 通知父进程
            
            close(pipefd[1]);
            exit(0);
        }
    }
    
    // 父进程：等待三个子进程写完
    close(pipefd[1]);  // 关闭写端
    
    for (int i = 0; i < 3; i++) {
        sem_wait(sem_read);  // 等待子进程通知
    }
    
    printf("Parent: All children have written, now reading...\n");
    
    // 测试管道大小——连续读直到读完
    char buffer[MSG_SIZE];
    int total = 0;
    while (1) {
        int bytes = read(pipefd[0], buffer, sizeof(buffer));
        if (bytes <= 0) break;
        total += bytes;
        printf("Parent: Read %d bytes (total %d)\n", bytes, total);
    }
    
    printf("Parent: Total bytes read = %d\n", total);
    
    // 等待所有子进程结束
    for (int i = 0; i < 3; i++) {
        wait(NULL);
    }
    
    // 清理
    close(pipefd[0]);
    sem_close(sem_write);
    sem_close(sem_read);
    sem_unlink("/write_sem");
    sem_unlink("/read_sem");
    
    return 0;
}