#include "semaphore.h"
#include "sched.h"


void sem_create(sem_t *sem, int32_t initial_count) {
    sem->count = initial_count;
    sem->wait_list_head = NULL; // 初始化等待链表为空
}

void sem_take(sem_t *sem)
{
    uint32_t pm = enter_critical(); // 进入临界区，防止中断干扰
    
    if (sem->count > 0)
    {
        sem->count--;
        exit_critical(pm); // 退出临界区
        return; // 成功获取资源，直接返回
    }
    
    else 
    {
        sem->count--; // 资源不足，先将计数减1，表示当前任务占用一个资源（虽然实际没有，但逻辑上是占用的）
        // 当前任务需要阻塞，加入等待链表
        TCB_t *current_task = current_tcb; // 获取当前任务的 TCB

        current_task->state = TASK_BLOCKED; // 将当前任务状态设置为 BLOCKED
        pend_to_list(&sem->wait_list_head, current_task); // 将当前任务按优先级插入等待链表
        exit_critical(pm); // 退出临界区
        trigger_switch(); // 触发 PendSV 进行任务切换
    }

}

uint32_t sem_take_from_isr(sem_t *sem)
{
    uint32_t taken = 0U; // 标记是否成功获取资源
    vTaskSuspendAll(); // 挂起调度器，防止在 ISR 中被调度器打断
    
    if (sem->count > 0)
    {
        sem->count--;
        xTaskResumeAll(); // 恢复调度器
        taken = 1U; // 标记成功获取资源
    }
    return taken; // 返回是否成功获取资源
}



void sem_give(sem_t *sem)
{
    uint32_t pm = enter_critical(); // 进入临界区，防止中断干扰
    sem->count++;
    if (sem->wait_list_head != NULL) // 如果有任务在等待
    {
        TCB_t *to_ready = sem->wait_list_head; // 获取等待链表头部的任务
        sem->wait_list_head = to_ready->next; // 更新等待链表头指针
        to_ready->state = TASK_READY; // 将该任务状态设置为 READY
        scheduler_add(to_ready); // 将该任务加入就绪队列
    }
    exit_critical(pm); // 退出临界区

}

uint32_t sem_give_from_isr(sem_t *sem)
{
    vTaskSuspendAll();
    sem->count++;
    TCB_t *task = wake_one(&sem->wait_list_head);
    xTaskResumeAll();
    return (task != NULL) ? 1U : 0U;
}

void mutex_lock(mutex_t *mutex)
{
    uint32_t pm = enter_critical();
    if (mutex->holder == NULL)
    {
        mutex->lock_count = 1; // 首次获取锁，计数设为1
        mutex->holder = current_tcb;
        mutex->holder_prio = current_tcb->preempt_priority; // 记录持有者的原始优先级
        exit_critical(pm);
        return; // 成功获取锁，直接返回
    }
    if (mutex->holder == current_tcb)
    {
        mutex->lock_count++; // 增加锁计数
        exit_critical(pm);
        return; // 已经持有锁，直接返回
    }
    //当前任务优先级高，将他放入等待链表，并进行优先级继承
    if (current_tcb->preempt_priority > mutex->holder->preempt_priority)
    {
        scheduler_remove(mutex->holder); // 从就绪队列移除持有者
        mutex->holder->preempt_priority = current_tcb->preempt_priority; // 优先级继承  
        scheduler_add(mutex->holder); // 将持有者重新加入就绪队列
    }
    current_tcb->state = TASK_BLOCKED; // 将当前任务状态设置为 BLOCKED
    current_tcb->next = mutex->wait_list_head; // 将当前任务加入等待链表头部
    mutex->wait_list_head = current_tcb; // 更新等待链表头指针
    exit_critical(pm);
    trigger_switch(); // 触发 PendSV 进行任务切换
}

void mutex_unlock(mutex_t *mutex)
{
    uint32_t pm = enter_critical();
    if (mutex->holder != current_tcb)
    {
        exit_critical(pm);
        return; // 只有持有锁的任务才能解锁，其他任务直接返回
    }
    mutex->lock_count--; // 锁计数减1
    if (mutex->lock_count > 0)
    {
        exit_critical(pm);
        return; // 还有锁未完全释放，直接返回
    }
    //如果当前优先级不等于现在持有锁的优先级，说明发生了优先级继承，需要恢复持有者的原始优先级
    //然后只有当lock_count为0时，才说明现在的任务是优先级继承者，才需要恢复优先级，不需要写if判断，因为如果lock_count不为0，会直接return，不会执行到恢复优先级的代码
    mutex->holder->preempt_priority = mutex->holder_prio; // 恢复持有者的原始优先级 
    if (mutex->wait_list_head != NULL) // 如果有任务在等待
    {
        TCB_t *to_ready = mutex->wait_list_head; // 获取等待链表头部的任务
        mutex->wait_list_head = to_ready->next; // 更新等待链表头指针
        to_ready->state = TASK_READY; // 将该任务状态设置为 READY
        scheduler_add(to_ready); // 将该任务加入就绪队列

    }
    mutex->holder = NULL; // 释放锁，清空持有者信息
    exit_critical(pm);
}

