#ifndef COMMON_H
#define COMMON_H

#define SHM_SIZE 1024
#define SHM_KEY 5678
#define SEM_MUTEX_KEY 8765   // 互斥信号量key
#define SEM_SYNC_KEY 8766    // 同步信号量key（用于应答）

// 信号量操作结构（用于semctl初始化）
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#endif