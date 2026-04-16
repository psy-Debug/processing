一、头文件和宏定义
c
#include <sys/msg.h>  // 消息队列相关系统调用

#define MAX_MSG 256           // 消息最大长度
#define MSG_KEY 1234          // 消息队列唯一标识符

// 消息类型定义
#define MSG_TYPE_NORMAL 1     // 普通消息
#define MSG_TYPE_END 2        // 结束消息
#define REPLY_TYPE_SENDER1 3  // 回复sender1
#define REPLY_TYPE_SENDER2 4  // 回复sender2
二、消息结构体
c
struct msgbuf {
    long mtype;               // 消息类型（必须>0）
    char mtext[MAX_MSG];      // 消息正文
};
三、全局变量
c
int msgid;                    // 消息队列ID
sem_t sem_terminal;           // 互斥信号量（保护终端）
sem_t sem_receiver;           // 同步信号量（唤醒receiver）
int sender1_exited;           // sender1退出标志
int sender2_exited;           // sender2退出标志
pthread_mutex_t exit_mutex;   // 互斥锁（保护退出标志）
四、核心函数
4.1 消息队列创建（main函数中）
c
msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
// 作用：创建或获取key为1234的消息队列
// IPC_CREAT: 不存在则创建
// 0666: 读写权限
// 返回：消息队列ID（类似文件描述符）
4.2 发送消息（sender中）
c
msgsnd(msgid, &msg, strlen(msg.mtext)+1, 0);
// 参数1: 消息队列ID
// 参数2: 消息结构体指针
// 参数3: 消息正文长度（+1包含\0）
// 参数4: 0表示阻塞发送
4.3 接收消息（receiver中）
c
msgrcv(msgid, &msg, sizeof(msg.mtext), msgtyp, 0);
// 参数1: 消息队列ID
// 参数2: 接收缓冲区
// 参数3: 缓冲区大小
// 参数4: 消息类型选择器
//   0: 取第一个消息
//   >0: 取第一个匹配类型的消息
//   <0: 取类型<=|msgtyp|的最小类型
// 参数5: 0表示阻塞接收
4.4 删除消息队列（receiver中）
c
msgctl(msgid, IPC_RMID, NULL);
// 作用：删除消息队列，释放内核资源
五、消息类型设计
类型值	宏定义	发送者	接收者	用途
1	MSG_TYPE_NORMAL	sender1/sender2	receiver	普通聊天消息
2	MSG_TYPE_END	sender1/sender2	receiver	通知对方要退出
3	REPLY_TYPE_SENDER1	receiver	sender1	确认sender1退出
4	REPLY_TYPE_SENDER2	receiver	sender2	确认sender2退出
六、同步机制
6.1 信号量
c
sem_init(&sem_terminal, 0, 1);  // 初始值1，互斥终端
sem_wait(&sem_terminal);         // P操作，申请资源
sem_post(&sem_terminal);         // V操作，释放资源

sem_init(&sem_receiver, 0, 0);   // 初始值0，receiver等待
sem_post(&sem_receiver);          // 有消息时唤醒
sem_wait(&sem_receiver);          // 无消息时阻塞
6.2 互斥锁
c
pthread_mutex_lock(&exit_mutex);   // 加锁
sender1_exited = 1;                // 修改共享变量
pthread_mutex_unlock(&exit_mutex); // 解锁
七、工作机制总结
7.1 用户态视角
text
创建消息队列(msgget)
       ↓
发送消息(msgsnd) → 内核链表 → 接收消息(msgrcv)
       ↓
删除消息队列(msgctl)
7.2 内核态机制
消息存储：内核维护msg_msg结构体链表

发送：加锁 → 创建节点 → 尾插链表 → 解锁 → 唤醒接收者

接收：加锁 → 遍历链表匹配类型 → 摘除节点 → 解锁 → 拷贝数据

类型匹配：遍历时根据msgtyp参数比较m_type字段

7.3 三种锁的分工
锁	位置	保护对象	作用
内核自旋锁	内核	消息链表	保证msgsnd/msgrcv原子操作
sem_terminal	用户态	终端	防止两个sender同时输入
sem_receiver	用户态	receiver唤醒	避免空转等待
exit_mutex	用户态	exit标志	防止数据竞争
八、完整通信流程
text
1. main: msgget() 创建消息队列
2. main: 创建3个线程
3. sender1获得终端锁 → 输入消息 → 释放锁 → msgsnd()发送
4. sem_post(sem_receiver) 唤醒receiver
5. receiver: sem_wait通过 → msgrcv()接收 → 处理消息
6. 输入"exit"时: sender发MSG_TYPE_END → 等待专属回复
7. receiver收到END → 回复REPLY_TYPE_SENDER → sender收到后退出
8. 两个sender都退出后 → receiver删除队列 → 程序结束







┌─────────────────────────────────────────────────────────────┐
│                          main线程                            │
│  创建消息队列 → 创建3个线程 → 等待线程结束 → 清理信号量        │
└───────┬─────────────────┬─────────────────┬─────────────────┘
        │                 │                 │
        ▼                 ▼                 ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│  sender1线程   │ │  sender2线程   │ │  receiver线程  │
├───────────────┤ ├───────────────┤ ├───────────────┤
│ 无限循环:      │ │ 无限循环:      │ │ 无限循环:      │
│ 1.获取终端锁   │ │ 1.获取终端锁   │ │ 1.检查退出条件 │
│ 2.输入消息     │ │ 2.输入消息     │ │ 2.sem_wait等待 │
│ 3.释放终端锁   │ │ 3.释放终端锁   │ │ 3.msgrcv接收   │
│ 4.判断exit?    │ │ 4.判断exit?    │ │ 4.处理消息     │
│   -是:发END    │ │   -是:发END    │ │   -普通:显示   │
│      等回复    │ │      等回复    │ │   -END:回复    │
│      标记退出  │ │      标记退出  │ │ 5.检查是否都退出│
│   -否:发NORMAL │ │   -否:发NORMAL │ └───────────────┘
│ 5.sem_post唤醒 │ │ 5.sem_post唤醒 │
└───────────────┘ └───────────────┘
        │                 │                 │
        └─────────────────┼─────────────────┘
                          │
                   消息队列（内核）
              ┌─────────────────────────┐
              │ MSG_TYPE_NORMAL (type=1) │
              │ MSG_TYPE_END (type=2)    │
              │ REPLY_TYPE_SENDER1 (3)   │
              │ REPLY_TYPE_SENDER2 (4)   │
              └─────────────────────────┘
六、线程间同步点
同步点	机制	作用
终端输入	sem_terminal	防止两个sender同时输入
消息通知	sem_receiver	receiver无消息时阻塞
退出标志	exit_mutex	保护sender1/2_exited的读写
退出确认	消息类型3和4	sender等待专属回复后再退出
七、线程退出顺序
text
正常退出流程：
1. 用户在sender1输入"exit"
2. sender1发END消息 → 等待回复 → 收到over1 → 标记exited=1 → 线程结束
3. 用户在sender2输入"exit"  
4. sender2发END消息 → 等待回复 → 收到over2 → 标记exited=2 → 线程结束
5. receiver检测到两个都退出 → 跳出循环 → 删除队列 → 线程结束
6. main的pthread_join全部返回 → 程序结束