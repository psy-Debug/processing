#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/msg.h>
#include <ctype.h>
#include <errno.h>

#define MAX_MSG 256
#define MSG_KEY 1234

// 消息类型定义
#define MSG_TYPE_NORMAL 1
#define MSG_TYPE_END 2
#define REPLY_TYPE_SENDER1 3
#define REPLY_TYPE_SENDER2 4

// 消息结构
struct msgbuf {
    long mtype;        // 消息类型
    char mtext[MAX_MSG]; // 消息内容
};

int msgid;  // 消息队列ID

// 信号量：保护资源
sem_t sem_terminal;  // 保护终端输入（互斥）
sem_t sem_receiver;  // 同步receiver（有消息才唤醒）

// 标志位：记录sender是否已退出
int sender1_exited = 0;
int sender2_exited = 0;
pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;

void* sender1(void* arg) {
    struct msgbuf msg;
    struct msgbuf reply;
    int thread_id = 1;
    
    
    
    while (1) {
        // ----- 互斥访问终端 -----
        sem_wait(&sem_terminal);
        printf("Sender%d: Thread started\n", thread_id);
        printf("Sender%d: Enter message (or 'exit' to quit): ", thread_id);
        fflush(stdout);
        
        if (fgets(msg.mtext, sizeof(msg.mtext), stdin) == NULL) {
            sem_post(&sem_terminal);
            break;
        }
        sem_post(&sem_terminal);
        // 去掉换行符
        msg.mtext[strcspn(msg.mtext, "\n")] = 0;
        
        // 释放终端
        // ----------------------
        
        // 检查是否输入exit
        if (strcmp(msg.mtext, "exit") == 0) {
            // 发送结束消息，mtext中包含发送者标识
            msg.mtype = MSG_TYPE_END;
            strcpy(msg.mtext, "end1"); // sender1
            if (msgsnd(msgid, &msg, strlen(msg.mtext)+1, 0) == -1) {
                perror("Sender1: msgsnd");
                break;
            }
            printf("Sender1: Sent 'end1', waiting for reply...\n");

            // 通知receiver有消息
            if (sem_post(&sem_receiver) == -1) {
                perror("Sender1: sem_post sem_receiver");
            }

            // 等待sender1专属回复（REPLY_TYPE_SENDER1）
            while (1) {
                ssize_t recv_len = msgrcv(msgid, &reply, sizeof(reply.mtext), REPLY_TYPE_SENDER1, 0);
                if (recv_len == -1) {
                    if (errno == EINTR) continue;
                    perror("Sender1: msgrcv");
                    break;
                }
                if (recv_len >= (ssize_t)sizeof(reply.mtext)) recv_len = sizeof(reply.mtext)-1;
                reply.mtext[recv_len] = '\0';
                // trim whitespace
                char *p = reply.mtext;
                while (*p && isspace((unsigned char)*p)) p++;
                if (p != reply.mtext) memmove(reply.mtext, p, strlen(p)+1);
                int l = strlen(reply.mtext);
                while (l>0 && isspace((unsigned char)reply.mtext[l-1])) reply.mtext[--l] = '\0';

                printf("Sender1: Got reply '%s'\n", reply.mtext);

                if (strcmp(reply.mtext, "over1") == 0) {
                    break;
                }
            }

            // 标记sender1已退出
            pthread_mutex_lock(&exit_mutex);
            sender1_exited = 1;
            pthread_mutex_unlock(&exit_mutex);

            /* 不在这里 sem_post：只有在真正发送消息后才 sem_post，
               否则会造成 receiver 被唤醒但没有对应消息，导致 msgrcv 阻塞 */

            break;  // 退出循环
        }
        
        // 发送普通消息（类型1）
        msg.mtype = MSG_TYPE_NORMAL;
        if (msgsnd(msgid, &msg, strlen(msg.mtext)+1, 0) == -1) {
            perror("Sender1: msgsnd");
            break;
        }
        printf("Sender1: Sent '%s'\n", msg.mtext);
        
        // 通知receiver有消息
        sem_post(&sem_receiver);
    }
    
    printf("Sender1: Exiting\n");
    return NULL;
}

void* sender2(void* arg) {
    struct msgbuf msg;
    struct msgbuf reply;
    int thread_id = 2;
    
    
    while (1) {
        // ----- 互斥访问终端 -----
        sem_wait(&sem_terminal);
        printf("Sender2: Thread started\n");
        printf("Sender%d: Enter message (or 'exit' to quit): ", thread_id);
        fflush(stdout);
        
        if (fgets(msg.mtext, sizeof(msg.mtext), stdin) == NULL) {
            sem_post(&sem_terminal);
            break;
        }
        sem_post(&sem_terminal);
        msg.mtext[strcspn(msg.mtext, "\n")] = 0;
        
        
        // ----------------------
        
        if (strcmp(msg.mtext, "exit") == 0) {
            // 发送结束消息，mtext中包含发送者标识
            msg.mtype = MSG_TYPE_END;
            strcpy(msg.mtext, "end2"); // sender2
            if (msgsnd(msgid, &msg, strlen(msg.mtext)+1, 0) == -1) {
                perror("Sender2: msgsnd");
                break;
            }
            printf("Sender2: Sent 'end2', waiting for reply...\n");

            // 通知receiver有消息
            if (sem_post(&sem_receiver) == -1) {
                perror("Sender2: sem_post sem_receiver");
            }

            // 等待sender2专属回复（REPLY_TYPE_SENDER2）
            while (1) {
                ssize_t recv_len = msgrcv(msgid, &reply, sizeof(reply.mtext), REPLY_TYPE_SENDER2, 0);
                if (recv_len == -1) {
                    if (errno == EINTR) continue;
                    perror("Sender2: msgrcv");
                    break;
                }
                if (recv_len >= (ssize_t)sizeof(reply.mtext)) recv_len = sizeof(reply.mtext)-1;
                reply.mtext[recv_len] = '\0';
                // trim whitespace
                char *p2 = reply.mtext;
                while (*p2 && isspace((unsigned char)*p2)) p2++;
                if (p2 != reply.mtext) memmove(reply.mtext, p2, strlen(p2)+1);
                int l2 = strlen(reply.mtext);
                while (l2>0 && isspace((unsigned char)reply.mtext[l2-1])) reply.mtext[--l2] = '\0';

                printf("Sender2: Got reply '%s'\n", reply.mtext);

                if (strcmp(reply.mtext, "over2") == 0) {
                    break;
                }
            }

            // 标记sender2已退出
            pthread_mutex_lock(&exit_mutex);
            sender2_exited = 1;
            pthread_mutex_unlock(&exit_mutex);

            /* 不在这里 sem_post：只有在真正发送消息后才 sem_post，
               否则会造成 receiver 被唤醒但没有对应消息，导致 msgrcv 阻塞 */

            break;
        }
        
        msg.mtype = MSG_TYPE_NORMAL;
        if (msgsnd(msgid, &msg, strlen(msg.mtext)+1, 0) == -1) {
            perror("Sender2: msgsnd");
            break;
        }
        printf("Sender2: Sent '%s'\n", msg.mtext);
        
        sem_post(&sem_receiver);
    }
    
    printf("Sender2: Exiting\n");
    return NULL;
}

void* receiver(void* arg) {
    struct msgbuf msg;
    int end1_received = 0;
    int end2_received = 0;
    
    printf("Receiver: Thread started\n");
    printf("Receiver: Will process messages until both senders exit\n");
    
    while (1) {
        // 检查是否两个sender都退出了，或接收到两个end消息
        pthread_mutex_lock(&exit_mutex);
        int both_exited = (sender1_exited && sender2_exited);
        pthread_mutex_unlock(&exit_mutex);

        if ((end1_received && end2_received) || both_exited) {
            printf("Receiver: Both senders have exited or both end messages received\n");
            break;
        }
        
        // 等待有消息（信号量同步）
        printf("Receiver: Waiting for message...\n");
        sem_wait(&sem_receiver);

        // 接收消息（类型不限）
        ssize_t recv_len = msgrcv(msgid, &msg, sizeof(msg.mtext), 0, 0);
        if (recv_len == -1) {
            perror("Receiver: msgrcv");
            continue;
        }
        if (recv_len >= (ssize_t)sizeof(msg.mtext)) recv_len = sizeof(msg.mtext)-1;
        msg.mtext[recv_len] = '\0';
        // trim whitespace
        char *p = msg.mtext;
        while (*p && isspace((unsigned char)*p)) p++;
        if (p != msg.mtext) memmove(msg.mtext, p, strlen(p)+1);
        int ml = strlen(msg.mtext);
        while (ml>0 && isspace((unsigned char)msg.mtext[ml-1])) msg.mtext[--ml] = '\0';

        printf("Receiver: Got message type=%ld, content='%s'\n", 
               msg.mtype, msg.mtext);
        
        // 处理普通消息（类型1）
        if (msg.mtype == MSG_TYPE_NORMAL) {
            printf("Receiver: Display: %s\n", msg.mtext);
            continue;
        }

        // 处理结束消息（类型2）
        if (msg.mtype == MSG_TYPE_END) {
            struct msgbuf reply;

            // 根据发送者内容决定回复类型和值
            if (strcmp(msg.mtext, "end1") == 0) {
                end1_received = 1;
                reply.mtype = REPLY_TYPE_SENDER1;
                strcpy(reply.mtext, "over1");
                if (msgsnd(msgid, &reply, strlen(reply.mtext)+1, 0) == -1) {
                    perror("Receiver: msgsnd reply to sender1");
                } else {
                    printf("Receiver: Sent over1 to sender1\n");
                }
            }
            else if (strcmp(msg.mtext, "end2") == 0) {
                end2_received = 1;
                reply.mtype = REPLY_TYPE_SENDER2;
                strcpy(reply.mtext, "over2");
                if (msgsnd(msgid, &reply, strlen(reply.mtext)+1, 0) == -1) {
                    perror("Receiver: msgsnd reply to sender2");
                } else {
                    printf("Receiver: Sent over2 to sender2\n");
                }
            }
            else {
                printf("Receiver: Unknown end message '%s'\n", msg.mtext);
            }
        }
    }
    
    printf("Receiver: Cleaning up message queue\n");
    msgctl(msgid, IPC_RMID, NULL);
    printf("Receiver: Exiting\n");
    return NULL;
}

int main() {
    pthread_t t1, t2, t3;
    
    printf("Main: Creating message queue\n");
    // 创建消息队列
    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }
    
    printf("Main: Initializing semaphores\n");
    // 初始化信号量
    sem_init(&sem_terminal, 0, 1);  // 终端互斥
    sem_init(&sem_receiver, 0, 0);  // receiver等待消息
    
    printf("Main: Creating threads\n");
    // 创建线程
    pthread_create(&t1, NULL, sender1, NULL);
    pthread_create(&t2, NULL, sender2, NULL);
    pthread_create(&t3, NULL, receiver, NULL);
    
    printf("Main: Waiting for threads to finish\n");
    // 等待线程结束
    pthread_join(t1, NULL);
    printf("Main: Sender1 joined\n");
    pthread_join(t2, NULL);
    printf("Main: Sender2 joined\n");
    pthread_join(t3, NULL);
    printf("Main: Receiver joined\n");
    
    // 清理
    sem_destroy(&sem_terminal);
    sem_destroy(&sem_receiver);
    pthread_mutex_destroy(&exit_mutex);
    
    printf("Main: All threads joined, exiting\n");
    return 0;
}