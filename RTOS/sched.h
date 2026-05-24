#ifndef __SCHED_H
#define __SCHED_H

#include "task.h"

#ifndef configMAX_SYSCALL_INTERRUPT_PRIORITY
    #define configMAX_SYSCALL_INTERRUPT_PRIORITY 5
#endif


extern volatile uint32_t scheduler_suspended;

void vTaskSuspendAll(void);
void xTaskResumeAll(void);
// 统一的 PendSV 触发函数
static inline void trigger_switch(void) {
    if (!scheduler_suspended) {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }
}

// 调度器初始化
void scheduler_init(void);
 
// 从就绪队列中取下一个任务（出队）并置为运行状态，返回 NULL 表示无任务可运行
TCB_t* scheduler_get_next(void);

// 将任务加入就绪队列（按 tcb->preempt_priority 和 tcb->priority 排序）
void scheduler_add(TCB_t *tcb);
// 从 ISR 中加入就绪队列，接口稍有不同以适应 ISR 环境
uint8_t scheduler_add_from_isr(TCB_t *tcb);
// 从就绪队列中移除指定任务（例如任务阻塞/删除时）
void scheduler_remove(TCB_t *tcb);

// 启动调度器并切换到第一个任务（不返回）
void start_scheduler(void);

// 初始化 1ms SysTick（用于时间片轮转）
void systick_init_1ms(void);

// SysTick节拍处理（1ms）
void scheduler_tick_1ms(void);
 
// PendSV调度入口：根据时间片与队列选择下一个任务
TCB_t* scheduler_on_pendsv(void);

uint32_t enter_critical(void);

void exit_critical(uint32_t basepri);
 


#endif // __SCHED_H
