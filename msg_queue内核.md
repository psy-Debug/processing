核心数据结构（Linux内核真实代码）
c
// ipc/msg.c 中的真实结构
struct msg_queue {
    struct kern_ipc_perm q_perm;
    struct list_head q_messages;  // 消息链表头
    struct list_head q_receivers; // 等待接收的进程
    struct list_head q_senders;   // 等待发送的进程
    unsigned long q_qnum;         // 消息数量
    unsigned long q_qbytes;       // 队列最大字节数
    // ...
};

struct msg_msg {
    struct list_head m_list;       // 链表指针（双向）
    long m_type;                   // 消息类型
    size_t m_ts;                   // 消息长度
    struct msg_msgseg *next;       // 大数据分段
    // 数据跟在后面（柔性数组）
};
关键机制
1. 发送消息（msgsnd）

根据msgid找到msg_queue结构

加锁（spin_lock）

检查队列空间，不够则阻塞进程加入q_senders链表

创建msg_msg结构，从用户态拷贝数据

用list_add_tail挂到q_messages链表尾部

唤醒q_receivers中等待的进程

解锁

2. 接收消息（msgrcv）

根据msgid找到msg_queue

加锁

遍历q_messages链表，根据msgtyp找匹配的消息

msgtyp=0：取第一个

msgtyp>0：找type相等的

msgtyp<0：找type <= |msgtyp|的最小值

找到则用list_del摘除，拷贝数据到用户态

找不到则阻塞进程加入q_receivers

解锁

3. 消息类型的作用

不是ID，是过滤器

接收时可以指定要哪种类型

链表遍历时根据m_type字段匹配