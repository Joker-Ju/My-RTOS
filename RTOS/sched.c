#include "sched.h"
#include "stm32f10x.h"


volatile uint32_t slice_ticks = 0;
volatile uint8_t slice_expired = 0;
volatile uint32_t scheduler_suspended = 0;



void vTaskSuspendAll(void) {
    scheduler_suspended = 1;
}

void xTaskResumeAll(void) {
    scheduler_suspended = 0;
}


void scheduler_init(void)
{
    for (int i = 0; i < MAX_PREEMPT_PRIORITIES; ++i) {
        ready_queues[i] = NULL;
    }

    // 配置 PendSV 为最低优先级（0xFF），避免阻塞其他中断
    // PendSV = exception 14，优先级寄存器字节地址 0xE000ED22
    *(volatile uint8_t*)0xE000ED22 = 0xFFU;

    // 配置 SysTick 优先级（0x0F），留够提升空间
    // SysTick = exception 15，优先级寄存器字节地址 0xE000ED23
    // 注：STM32F103 实现 4 位优先级，bits[7:4] 有效，bits[3:0] 忽略
    // 0xF0 = 优先级 15（最低，但高于 PendSV 的 0xFF = 也是 15）
    *(volatile uint8_t*)0xE000ED23 = 0xF0U;
}

TCB_t* scheduler_get_next(void)
{
	uint32_t bp = enter_critical();
    // 从最高抢占优先级开始搜索（数值越大优先级越高）
     for (int p = MAX_PREEMPT_PRIORITIES - 1; p >= 0; --p) {
        TCB_t *head = ready_queues[p];
        if (head != NULL) {
            // 出队头节点
            ready_queues[p] = head->next;
            head->next = NULL;
            head->state = TASK_RUNNING;
			exit_critical(bp);
            return head;
        }
    }
	exit_critical(bp);
    return NULL;
}

void scheduler_add(TCB_t *tcb)
{
    if (tcb == NULL) return;
    uint32_t pre = tcb->preempt_priority;
    if (pre >= MAX_PREEMPT_PRIORITIES) pre = MAX_PREEMPT_PRIORITIES - 1;
	uint32_t bp = enter_critical();
    // 插入到对应的抢占优先级队列中，队列内部按响应优先级从高到低排序  
    pend_to_list(&ready_queues[pre], tcb);
    tcb->state = TASK_READY;
	exit_critical(bp);
    // 如果新任务抢占优先级高于当前任务，触发一次 PendSV 立即抢占
    if (current_tcb != NULL && need_preempt(tcb)) 
    {
        trigger_switch();
    }
}

uint8_t scheduler_add_from_isr(TCB_t *tcb)
{
    if (tcb == NULL) return 0;
    uint32_t pre = tcb->preempt_priority;
    if (pre >= MAX_PREEMPT_PRIORITIES) pre = MAX_PREEMPT_PRIORITIES - 1;
    // 插入到对应的抢占优先级队列中，队列内部按响应优先级从高到低排序  
    pend_to_list(&ready_queues[pre], tcb);
    tcb->state = TASK_READY;
    if (current_tcb != NULL && need_preempt(tcb)) 
    {
        return 1;   // 需要切换
    }
    return 0;
}


void scheduler_remove(TCB_t *tcb)
{
    if (tcb == NULL) return;
    uint32_t pre = tcb->preempt_priority;
    if (pre >= MAX_PREEMPT_PRIORITIES) pre = MAX_PREEMPT_PRIORITIES - 1;
	uint32_t bp = enter_critical();
    
    
    TCB_t **head = &ready_queues[pre];
    TCB_t *curr = *head;
    TCB_t *prev = NULL;

    while (curr != NULL) 
	{
        if (curr == tcb) 
			{
            if (prev == NULL) 
			{
                *head = curr->next;
            } 
			else
			{
                prev->next = curr->next;
            }
            curr->next = NULL;
			exit_critical(bp);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
	exit_critical(bp);
}

void systick_init_1ms(void)
{
    SystemCoreClockUpdate();
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}



 


void scheduler_tick_1ms(void)
{
    //system_tick也作为全局时间基准，调度器每次tick时会检查时间片是否到期
    if (current_tcb == NULL) {
        return;
    }
    slice_ticks++;
    if (slice_ticks >= 10U) {
        slice_ticks = 0U;
        slice_expired = 1U;
        // 触发 PendSV 进行任务切换
        trigger_switch();
    }
}

TCB_t* scheduler_on_pendsv(void)
{
    TCB_t *prev = current_tcb;
    // 先判断是否时间片到期
    uint8_t need_switch = (slice_expired != 0U);
	if (prev != NULL && prev->state != TASK_RUNNING) {
	  need_switch = 1U;
	}
    // 若未到期，但就绪队列存在更高优先级任务，也需要切换
    if (need_switch == 0U && prev != NULL) {
        uint32_t cur_pre = prev->preempt_priority;
        // 先看更高抢占优先级是否有任务
        for (int p = MAX_PREEMPT_PRIORITIES - 1; p > (int)cur_pre; --p) {
            if (ready_queues[p] != NULL) {
                need_switch = 1U;
                break;
            }
        }
    }

    if (need_switch == 0U) {
        return prev;
    }

    slice_expired = 0U;
    if (prev != NULL) {
        if (prev->state == TASK_RUNNING) {
            scheduler_add(prev);
        }
    }

    TCB_t *next = scheduler_get_next();
    if (next == NULL) {
        return prev;
    }

    if (next != prev) {
        slice_ticks = 0U;
    }

    return next;
}


uint32_t enter_critical(void)
{
    uint32_t basepri = __get_BASEPRI();         // 保存当前 BASEPRI
    __set_BASEPRI(configMAX_SYSCALL_INTERRUPT_PRIORITY << (8U - __NVIC_PRIO_BITS));
    return basepri;                             // 返回旧值给 exit 用
}

void exit_critical(uint32_t basepri)
{
    __set_BASEPRI(basepri);                     // 恢复旧值（0 = 全开）
}

