#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
int main() {
    printf("cmd2: Files in current directory:\n");
    system("ls -la");
    return 0;
}
//打印提示信息

//调用 system("ls -la") 执行系统命令

//关键点
//system() 内部会调用 fork() + exec() + wait()，相当于再启动一个子 shell 执行 ls -la

//这里可以对比：第一个 shell 用 execlp 直接执行 cmd2，cmd2 又通过 system 创建新进程