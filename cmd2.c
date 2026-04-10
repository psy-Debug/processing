#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
int main() {
    printf("cmd2: Files in current directory:\n");
    system("ls -la");
    return 0;
}