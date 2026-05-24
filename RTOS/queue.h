#ifndef __QUEUE_H
#define __QUEUE_H

#include "sched.h"


typedef struct queue{
    void       *storage;           // 环形缓冲区（malloc 或静态数组）
    uint32_t    item_size;         // 每个消息的字节数
    uint32_t    queue_len;         // 最多能存几个消息
    uint32_t    head;              // 读出位置
    uint32_t    tail;              // 写入位置
    uint32_t    count;             // 当前消息数

    TCB_t      *wait_send_head;    // 队列满时，发送者阻塞在这里
    TCB_t      *wait_recv_head;    // 队列空时，接收者阻塞在这里
}queue_t;

int queue_create(queue_t *q, void *buffer, uint32_t item_size, uint32_t queue_len);

void queue_send(queue_t *q, void *data);

void queue_recv(queue_t *q, void *data);

uint32_t queue_send_from_isr(queue_t *q, void *data);

uint32_t queue_recv_from_isr(queue_t *q, void *data);



#endif 

