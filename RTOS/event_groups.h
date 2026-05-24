#ifndef __EVENT_GROUPS_H
#define __EVENT_GROUPS_H

#include "sched.h"

#define WAIT_MODE_AND 0
#define WAIT_MODE_OR  1

typedef struct event_group {
    uint32_t flags;                     // 32 位事件标志
    struct event_node *wait_list;       // 等事件的任务链表
} event_group_t;

// 等事件时，每个阻塞任务对应一个节点
typedef struct event_node {
    TCB_t    *tcb;                      // 哪个任务在等
    uint32_t  wait_bits;               // 等哪些位
    uint8_t   wait_mode;               // 0=AND(全满足), 1=OR(任一满足)
    struct event_node *next;
} event_node_t;

// 初始化（flags = 0, wait_list = NULL）
void event_group_init(event_group_t *eg);

// 置位：把指定位设成 1，检查是否有任务的条件满足了
void event_group_set(event_group_t *eg, uint32_t bits);

uint8_t event_group_set_from_isr(event_group_t *eg, uint32_t bits);

// 清零
void event_group_clear(event_group_t *eg, uint32_t bits); 

uint8_t event_group_clear_from_isr(event_group_t *eg, uint32_t bits);

// 等指定位：不满足就阻塞
void event_group_wait(event_group_t *eg, uint32_t bits, uint8_t mode);

#endif /* __EVENT_GROUP_H */

