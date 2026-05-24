#include "event_groups.h"



// 初始化（flags = 0, wait_list = NULL）
void event_group_init(event_group_t *eg)
{
    eg->flags = 0;
    eg->wait_list = NULL;
}

// 置位：把指定位设成 1，检查是否有任务的条件满足了
void event_group_set(event_group_t *eg, uint32_t bits)
{
    uint32_t bp = enter_critical();
    eg->flags |= bits;           // OR 写入，不覆盖已有标志

    // 检查链表上每个等待任务
    event_node_t **p = &eg->wait_list;
    while (*p != NULL) {
        uint8_t meet = 0;
        if ((*p)->wait_mode == WAIT_MODE_AND) {    
            meet = ((eg->flags & (*p)->wait_bits) == (*p)->wait_bits);
        } 
        else {                    
            meet = ((eg->flags & (*p)->wait_bits) != 0);
        }
        if (meet) {
            event_node_t *to_wake = *p;
            *p = to_wake->next; // 从等待链表移除
            to_wake->next = NULL;
            to_wake->tcb->state = TASK_READY; // 条件满足，唤醒任务
            scheduler_add(to_wake->tcb);       // 放入就绪队列
        }
        else {
            p = &(*p)->next;
        }
    }
    exit_critical(bp);
}


uint8_t event_group_set_from_isr(event_group_t *eg, uint32_t bits)
{

    eg->flags |= bits;           // OR 写入，不覆盖已有标志

    // 检查链表上每个等待任务
    event_node_t **p = &eg->wait_list;
    while (*p != NULL) {
        uint8_t meet = 0;
        if ((*p)->wait_mode == WAIT_MODE_AND) {    
            meet = ((eg->flags & (*p)->wait_bits) == (*p)->wait_bits);
        } 
        else {                    
            meet = ((eg->flags & (*p)->wait_bits) != 0);
        }
        if (meet) {
            event_node_t *to_wake = *p;
            *p = to_wake->next; // 从等待链表移除
            to_wake->next = NULL;
            to_wake->tcb->state = TASK_READY; // 条件满足，唤醒任务
            scheduler_add_from_isr(to_wake->tcb);       // 放入就绪队列
            return 1;   // 说明成功返回
        }
        else {
            p = &(*p)->next;
        }
    }
    return 0;   // 没有任务被唤醒
}

// 清零
void event_group_clear(event_group_t *eg, uint32_t bits)
{
    uint32_t bp = enter_critical();
    eg->flags &= ~bits;
    exit_critical(bp);
}

uint8_t event_group_clear_from_isr(event_group_t *eg, uint32_t bits)
{
    eg->flags &= ~bits;
    return 1;
}
 
void event_group_wait(event_group_t *eg, uint32_t bits, uint8_t mode)
{
    uint32_t bp = enter_critical();
    event_node_t node;
    node.tcb = current_tcb;
    node.wait_bits = bits;
    node.wait_mode = mode;

    while (1) {
        uint8_t meet = 0;
        if (mode == WAIT_MODE_AND) {
            meet = ((eg->flags & bits) == bits);
        } else {
            meet = ((eg->flags & bits) != 0);
        }

        if (meet) break;      // 条件满足，跳出循环

        // 不满足 → 阻塞
        current_tcb->state = TASK_BLOCKED;
        node.next = eg->wait_list;
        eg->wait_list = &node;
        exit_critical(bp);
        trigger_switch();
        bp = enter_critical();  // 被唤醒后重新拿锁、重试
    }

    exit_critical(bp);
}


