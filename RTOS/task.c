
#include "sched.h"



// 按抢占优先级的就绪队列数组：同一抢占优先级使用同一链表，链表内部按响应优先级从高到低排序
TCB_t *ready_queues[MAX_PREEMPT_PRIORITIES] = {0};


static TCB_t *notify_blocked_list = NULL;  // 链表头，等待通知的任务挂在这里


// 初始化任务堆栈，使第一次切换时可直接跳任务
uint32_t* task_stack_init(uint32_t *stack_top, void (*task_func)(void *), void *param)
{
    uint32_t *stack = stack_top;

    // 自动保存区（硬件自动入栈），参考ARMv7-M架构手册，注意逆序入栈
    *(--stack) = (uint32_t)0x01000000;    // xPSR (Thumb state位必须为1)
    *(--stack) = (uint32_t)task_func;     // PC, 任务入口
    *(--stack) = (uint32_t)0xFFFFFFFD;    // LR, 返回使用PSP
    *(--stack) = (uint32_t)0x12121212;    // R12
    *(--stack) = (uint32_t)0x03030303;    // R3
    *(--stack) = (uint32_t)0x02020202;    // R2
    *(--stack) = (uint32_t)0x01010101;    // R1
    *(--stack) = (uint32_t)param;         // R0（任务参数）

    // 手动保存区（RTOS切换时手动入栈）
    *(--stack) = 0x11111111; // R11
    *(--stack) = 0x10101010; // R10
    *(--stack) = 0x09090909; // R9
    *(--stack) = 0x08080808; // R8
    *(--stack) = 0x07070707; // R7
    *(--stack) = 0x06060606; // R6
    *(--stack) = 0x05050505; // R5
    *(--stack) = 0x04040404; // R4

    return stack;
}




// 任务创建：TCB初始化并插入就绪链表
void task_create(TCB_t *tcb, 
                void (*func)(void *),
                void *param, 
                uint32_t *stack_base, 
                uint32_t stack_size, 
                uint32_t resp_priority, 
                uint32_t preempt_priority)
{
    // 物理数组终点 = stack_base + stack_size（传进来的 stack_size 还是原来的值）
    uint32_t *phys_end = stack_base + stack_size + TASK_GUARD_SIZE;

    // 在 2 个哨兵位置写 0xDEADBEEF
    for (int i = 0; i < TASK_GUARD_SIZE; i++) {
        phys_end[-(TASK_GUARD_SIZE - i)] = 0xDEADBEEF;
        // 等价于 phys_end[-2], phys_end[-1]
    }

    // 记录第一个哨兵的地址
    tcb->guard_addr = phys_end - TASK_GUARD_SIZE;
    tcb->stack_base = (uint32_t)stack_base;
    // 堆栈指针初始化（Cortex-M3栈高地址为顶部）
    tcb->stack_ptr = task_stack_init(stack_base + stack_size, func, param);
    // 优先级字段
    if (preempt_priority >= MAX_PREEMPT_PRIORITIES) {
        preempt_priority = MAX_PREEMPT_PRIORITIES - 1;
    }
    tcb->priority = resp_priority; // 响应优先级直接存到 priority
    tcb->preempt_priority = preempt_priority;
    tcb->state = TASK_READY;
    tcb->next = NULL;

    // 插入到对应的抢占优先级队列中，队列内部按响应优先级从高到低排序
    pend_to_list(&ready_queues[preempt_priority], tcb);
}







// 全局系统tick变量，SysTick_Handler中维护
volatile uint32_t system_tick = 0;
static TCB_t *delay_list_head = NULL;

// 挂起/延时任务的插入逻辑
void delay_list_add(TCB_t *tcb, uint32_t now) {
    tcb->wake_time = now + tcb->suspend_ticks;

    TCB_t **p = &delay_list_head;
    // 按从早到晚排列, 找到第一个大于自己的位置
    while (*p && (*p)->wake_time <= tcb->wake_time) {
        p = &((*p)->delay_next);
    }
    tcb->delay_next = *p;
    *p = tcb;
}

// 每次tick或被调度时调用
void delay_list_tick(uint32_t now) {
    // 连续唤醒所有到期的任务（==批量唤醒）
    while (delay_list_head && delay_list_head->wake_time <= now) {
        TCB_t *to_ready = delay_list_head;
        delay_list_head = to_ready->delay_next;
        to_ready->delay_next = NULL;
        to_ready->state = TASK_READY;   // 恢复就绪（可省略，由调度器做）

        scheduler_add_from_isr(to_ready);        // 放入就绪队列
    }
}


// 将某任务挂入定时唤醒队列
void task_delay(TCB_t *tcb, uint32_t delay_ticks) {
    if (!tcb || delay_ticks == 0) return;  // 可做参数保护

    // 若不是当前运行任务，先从就绪队列移除
    if (tcb != current_tcb) {
        scheduler_remove(tcb);
    }

    tcb->arrive_time = system_tick;
    tcb->suspend_ticks = delay_ticks;
    tcb->state = TASK_BLOCKED;
    tcb->delay_next = NULL;
    delay_list_add(tcb, system_tick);

    // 若当前任务主动延时，触发 PendSV 切换任务
    if (tcb == current_tcb) {
        trigger_switch();
    }
}
 //???
void task_notify(TCB_t *target, uint32_t value)
{
    uint32_t bp = enter_critical();
    target->notify_value = value;

    // 在 notify_blocked_list 里找 target（用 notify_next 串的链表）
    TCB_t **p = &notify_blocked_list;
    while (*p != NULL) {
        if (*p == target) {
            *p = target->notify_next;                 // 摘出
            target->notify_next = NULL;
            scheduler_add(target);           // 加入就绪队列
            exit_critical(bp);

            return;
        }
        p = &(*p)->notify_next;
    }
    // target 没在等通知 → 只存值，不唤醒
    exit_critical(bp);

}

uint32_t task_notify_from_isr(TCB_t *target, uint32_t value)
{
    target->notify_value = value;

    // 在 notify_blocked_list 里找 target（用 notify_next 串的链表）
    TCB_t **p = &notify_blocked_list;
    while (*p != NULL) {
        if (*p == target) {
            *p = target->notify_next;                 // 摘出
            target->notify_next = NULL;
            scheduler_add_from_isr(target);           // 加入就绪队列
            return 1;
        }
        p = &(*p)->notify_next;
    }
    // target 没在等通知 → 只存值，不唤醒
    return 0;
}

uint32_t task_notify_wait(void)
{
    uint32_t bp = enter_critical();
    if (current_tcb->notify_value != 0) {
        // 已经有值了，直接消费
        uint32_t val = current_tcb->notify_value;
        current_tcb->notify_value = 0;
        exit_critical(bp);
        return val;
    }
    // 还没收到通知 → 阻塞
    current_tcb->state = TASK_BLOCKED;
    current_tcb->notify_next = notify_blocked_list;
    notify_blocked_list = current_tcb;
    exit_critical(bp);
    trigger_switch();    // 切出去等通知
    // 被唤醒后回来
    bp = enter_critical();
    uint32_t val = current_tcb->notify_value;
    current_tcb->notify_value = 0;
    exit_critical(bp);
    return val;
}





//辅助函数，提高可读性
TCB_t *wake_one(TCB_t **head)
{
    TCB_t *task = *head;
    if (task != NULL) {
        *head = task->next;
        task->next = NULL;
        task->state = TASK_READY;
        scheduler_add_from_isr(task);
    }
    return task;
}

int need_preempt(TCB_t *task) 
{
      return (task->preempt_priority > current_tcb->preempt_priority ||
              (task->preempt_priority == current_tcb->preempt_priority &&
               task->priority > current_tcb->priority));
}


void pend_to_list(TCB_t **head, TCB_t *task)
{
  while (*head != NULL && (*head)->priority >= task->priority) {
	  head = &(*head)->next;
  }
  task->next = *head;
  *head = task;
}
//


