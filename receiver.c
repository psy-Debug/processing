#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>

#define SHM_SIZE 1024
#define SHM_KEY 5678
#define SEM_KEY 8765

int main() {
    int shmid, semid;
    char *shm_ptr;
    struct sembuf sb = {0, -1, 0};
    
    // 1. 获取共享内存
    shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    
    // 2. 附加
    shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (char *)-1) {
        perror("shmat");
        exit(1);
    }
    
    // 3. 获取信号量
    semid = semget(SEM_KEY, 1, 0666);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }
    
    printf("Receiver: Waiting for semaphore...\n");
    // P操作：等待sender写完
    if (semop(semid, &sb, 1) == -1) {
        perror("semop P");
        exit(1);
    }
    
    // 读取共享内存
    printf("Receiver: Read from shared memory: %s\n", shm_ptr);
    
    // 写应答
    strcpy(shm_ptr, "over");
    printf("Receiver: Sent response 'over'\n");
    
    // V操作：释放信号量给sender
    sb.sem_op = 1;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop V");
        exit(1);
    }
    
    // 分离
    shmdt(shm_ptr);
    
    // 删除共享内存
    shmctl(shmid, IPC_RMID, NULL);
    
    return 0;
}