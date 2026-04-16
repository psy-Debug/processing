#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <errno.h>
#include "common.h"

// 封装P/V操作便于使用
void P(int semid, int sem_num) {
    struct sembuf sb = {sem_num, -1, 0};
    if (semop(semid, &sb, 1) == -1) {
        perror("P operation failed");
        exit(1);
    }
}

void V(int semid, int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};
    if (semop(semid, &sb, 1) == -1) {
        perror("V operation failed");
        exit(1);
    }
}

int main() {
    int shmid, sem_mutex, sem_sync;
    char *shm_ptr;
    
    // 1. 获取已存在的共享内存（不创建）
    shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
    if (shmid == -1) {
        perror("shmget - make sure sender is running first");
        exit(1);
    }
    printf("[Receiver] Shared memory got, shmid=%d\n", shmid);
    
    // 2. 附加共享内存到进程地址空间
    shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (char *)-1) {
        perror("shmat");
        exit(1);
    }
    printf("[Receiver] Shared memory attached at %p\n", shm_ptr);
    
    // 3. 获取已存在的信号量（不创建）
    sem_mutex = semget(SEM_MUTEX_KEY, 1, 0666);
    if (sem_mutex == -1) {
        perror("semget mutex - make sure sender is running first");
        exit(1);
    }
    
    sem_sync = semget(SEM_SYNC_KEY, 1, 0666);
    if (sem_sync == -1) {
        perror("semget sync - make sure sender is running first");
        exit(1);
    }
    printf("[Receiver] Semaphores got\n");
    
    // 4. 读取共享内存（互斥保护）
    printf("[Receiver] Waiting for mutex...\n");
    P(sem_mutex, 0);  // 获取互斥锁
    
    printf("[Receiver] Read from shared memory: %s\n", shm_ptr);
    
    // 5. 写入应答
    strcpy(shm_ptr, "over");
    printf("[Receiver] Sent response 'over' to shared memory\n");
    
    V(sem_mutex, 0);  // 释放互斥锁
    
    // 6. 通知sender：应答已就绪
    printf("[Receiver] Notifying sender...\n");
    V(sem_sync, 0);  // V操作唤醒等待中的sender
    
    // 7. 等待一小会儿确保sender读完了应答（实际项目中可用第二个同步信号量）
    sleep(1);
    
    // 8. 清理
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt");
        exit(1);
    }
    printf("[Receiver] Shared memory detached\n");
    
    // 9. 删除共享内存
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }
    printf("[Receiver] Shared memory removed\n");
    
    // 10. 删除信号量
    if (semctl(sem_mutex, 0, IPC_RMID) == -1) {
        perror("semctl mutex remove");
        exit(1);
    }
    if (semctl(sem_sync, 0, IPC_RMID) == -1) {
        perror("semctl sync remove");
        exit(1);
    }
    printf("[Receiver] Semaphores removed\n");
    
    return 0;
}