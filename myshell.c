#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CMD 256

int main() {
    char cmd[MAX_CMD];
    
    while (1) {
        printf("myshell> ");
        fflush(stdout);
        
        if (fgets(cmd, MAX_CMD, stdin) == NULL) {
            break;  // Ctrl+D退出
        }
        
        // 去掉换行符
        cmd[strcspn(cmd, "\n")] = 0;
        
        // 检查exit
        if (strcmp(cmd, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        
        // 创建子进程——这就是fork的本质！
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork failed");
            continue;
        }
        
        if (pid == 0) {
            // 子进程：exec变成用户要的程序
            execlp(cmd, cmd, NULL);
            
            // 如果execlp返回，说明执行失败
            printf("Command not found: %s\n", cmd);
            exit(1);
        } else {
            // 父进程：等子进程结束——这就是wait的本质
            int status;
            wait(&status);
            printf("Command completed with status %d\n", status);
        }
    }
    
    return 0;
}