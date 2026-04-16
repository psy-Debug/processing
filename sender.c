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
    union semun sem_union;
    
    // 1. 创建共享内存
    shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    printf("[Sender] Shared memory created, shmid=%d\n", shmid);
    
    // 2. 附加共享内存到进程地址空间
    shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (char *)-1) {
        perror("shmat");
        exit(1);
    }
    printf("[Sender] Shared memory attached at %p\n", shm_ptr);
    
    // 3. 创建互斥信号量（保护共享内存的互斥访问）
    sem_mutex = semget(SEM_MUTEX_KEY, 1, IPC_CREAT | 0666);
    if (sem_mutex == -1) {
        perror("semget mutex");
        exit(1);
    }
    
    // 4. 创建同步信号量（用于应答同步）
    sem_sync = semget(SEM_SYNC_KEY, 1, IPC_CREAT | 0666);
    if (sem_sync == -1) {
        perror("semget sync");
        exit(1);
    }
    
    // 5. 初始化信号量
    // 互斥信号量初始为1（可用）
    sem_union.val = 1;
    if (semctl(sem_mutex, 0, SETVAL, sem_union) == -1) {
        perror("semctl mutex init");
        exit(1);
    }
    
    // 同步信号量初始为0（等待receiver应答）
    sem_union.val = 0;
    if (semctl(sem_sync, 0, SETVAL, sem_union) == -1) {
        perror("semctl sync init");
        exit(1);
    }
    printf("[Sender] Semaphores initialized (mutex=1, sync=0)\n");
    
    // 6. 写入数据到共享内存（互斥保护）
    printf("[Sender] Waiting for mutex...\n");
    P(sem_mutex, 0);  // 获取互斥锁
    
    printf("[Sender] Got mutex. Enter message: ");
    fflush(stdout);
    fgets(shm_ptr, SHM_SIZE, stdin);
    // 去掉换行符
    shm_ptr[strcspn(shm_ptr, "\n")] = 0;
    printf("[Sender] Wrote '%s' to shared memory\n", shm_ptr);
    
    V(sem_mutex, 0);  // 释放互斥锁
    
    // 7. 等待receiver的应答（同步点）
    printf("[Sender] Waiting for receiver's response...\n");
    P(sem_sync, 0);  // 等待同步信号量（阻塞直到receiver V操作）
    
    // 8. 读取应答（需要重新获取互斥锁）
    P(sem_mutex, 0);
    printf("[Sender] Received response: %s\n", shm_ptr);
    V(sem_mutex, 0);
    
    // 9. 清理
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt");
        exit(1);
    }
    printf("[Sender] Shared memory detached\n");
    
    // 注意：sender不删除共享内存和信号量，由receiver删除
    
    return 0;
}