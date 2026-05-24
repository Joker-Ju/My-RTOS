#ifndef __SEMAPHORE_H
#define __SEMAPHORE_H

#include "task.h"

typedef struct semaphore{
    int32_t count;               // 资源计数
    TCB_t  *wait_list_head;      // 等待该信号量的任务链表（BLOCKED 状态挂在这里）
}sem_t;

void sem_create(sem_t *sem, int32_t initial_count);
void sem_take(sem_t *sem);
void sem_give(sem_t *sem);
uint32_t sem_give_from_isr(sem_t *sem);
uint32_t sem_take_from_isr(sem_t *sem);
typedef struct mutex {
    TCB_t *holder;           // 当前持有锁的任务（NULL=无人持有）
    TCB_t *wait_list_head;   // 等锁的任务链表
    uint32_t holder_prio;    // 持有者的原始优先级（继承后要恢复用）
    uint32_t lock_count;     // 锁计数（支持递归锁）
} mutex_t;


void mutex_lock(mutex_t *mutex);

void mutex_unlock(mutex_t *mutex);

#endif
