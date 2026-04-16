#include <stdio.h>
#include <unistd.h>

int main() {
    printf("cmd3: I'm process %d\n", getpid());
    return 0;
}


//getpid() 是内核提供的系统调用，陷入内核后返回当前进程的标识

//可以让实验者观察到：同一个程序多次运行，每次 PID 不同

//父进程（shell）的 PID 固定不变

