#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <errno.h>

#define SHM_SIZE 1024
#define SHM_KEY 5678
#define SEM_KEY 8765

// 信号量操作结构
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main() {
    int shmid, semid;
    char *shm_ptr;
    struct sembuf sb = {0, -1, 0};  // P操作
    union semun sem_union;
    
    // 1. 创建共享内存——内核分配一段物理内存
    shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    
    // 2. 附加到进程地址空间
    shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (char *)-1) {
        perror("shmat");
        exit(1);
    }
    
    // 3. 创建信号量用于同步
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }
    
    // 初始化信号量为1（可用）
    sem_union.val = 1;
    if (semctl(semid, 0, SETVAL, sem_union) == -1) {
        perror("semctl");
        exit(1);
    }
    
    printf("Sender: Waiting for semaphore...\n");
    // P操作：获取信号量
    if (semop(semid, &sb, 1) == -1) {
        perror("semop P");
        exit(1);
    }
    
    printf("Sender: Got semaphore. Enter message: ");
    fflush(stdout);
    
    // 从终端读取输入
    fgets(shm_ptr, SHM_SIZE, stdin);
    shm_ptr[strcspn(shm_ptr, "\n")] = 0;
    
    printf("Sender: Wrote '%s' to shared memory\n", shm_ptr);
    
    // V操作：释放信号量
    sb.sem_op = 1;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop V");
        exit(1);
    }
    
    // 等待receiver的应答
    printf("Sender: Waiting for response...\n");
    sleep(2);  // 简单等待，实际可用另一个信号量
    
    // 读取应答
    if (strlen(shm_ptr) > 0) {
        printf("Sender: Received response: %s\n", shm_ptr);
    }
    
    // 分离共享内存
    shmdt(shm_ptr);
    
    return 0;
}