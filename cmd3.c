#include <stdio.h>
#include <unistd.h>

int main() {
    printf("cmd3: I'm process %d\n", getpid());
    return 0;
}