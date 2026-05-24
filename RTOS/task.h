#ifndef __TASK_H
#define __TASK_H
#include "stm32f10x.h"


#include <stdint.h>
#include <stddef.h>

#ifndef TASK_GUARD_SIZE
    #define TASK_GUARD_SIZE    2  // 每个任务栈底放2个哨兵，检测栈溢出
#endif

// 任务状态定义
typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SUSPENDED
} task_state_t;

// 任务控制块（TCB）结构体
typedef struct tcb {
    uint32_t *stack_ptr;           // 当前任务栈顶指针
    uint32_t priority;             // 任务优先级（即响应优先级，值越大优先级越高）
    uint32_t preempt_priority;     // 抢占优先级（同一抢占优先级放在同一队列）
    uint32_t delay_ticks;          // 挂起剩余时间（ms）
    task_state_t state;            // 任务状态
    struct tcb *next;              // 用于任务链表
    struct tcb *delay_next;        // 用于挂起队列链表
    uint32_t wake_time;            // 唤醒时间
    uint32_t arrive_time;
    uint32_t suspend_ticks;
    // 可根据需要扩展更多成员，如任务名字等
    uint32_t notify_value;    // ← 新增：通知值
    struct tcb *notify_next;     // 通知阻塞链表指针（类似 delay_next）
    uint32_t *guard_addr;       // 哨兵地址（栈底）用于检测溢出
    uint32_t stack_base;         // 任务栈的物理起始地址（用于检测溢出）
} TCB_t;

// 最大抢占优先级数（可按需在编译命令中覆盖）
#ifndef MAX_PREEMPT_PRIORITIES
#define MAX_PREEMPT_PRIORITIES 8
#endif

// 就绪队列（按抢占优先级分桶），由 task 模块定义并可被调度器使用
extern TCB_t *ready_queues[MAX_PREEMPT_PRIORITIES];

extern volatile uint32_t system_tick;
// 当前运行任务指针（由调度器维护）
extern TCB_t *current_tcb;

// 任务创建函数
// 参数说明：最后两个参数分别为响应优先级和抢占优先级
void task_create(TCB_t *tcb, void (*func)(void *), void *param, uint32_t *stack_base, uint32_t stack_size, uint32_t resp_priority, uint32_t preempt_priority);

// 任务栈初始化
uint32_t* task_stack_init(uint32_t *stack_top, void (*task_func)(void *), void *param);

// 让当前任务挂起一段时间（ms），非阻塞延时
void task_delay(TCB_t *tcb, uint32_t delay_ticks);

void delay_list_tick(uint32_t now);

void delay_list_add(TCB_t *tcb, uint32_t now);
 
void task_notify(TCB_t *target, uint32_t value);
 
uint32_t task_notify_from_isr(TCB_t *target, uint32_t value);

uint32_t task_notify_wait(void);

//辅助函数
void pend_to_list(TCB_t **head, TCB_t *task);
TCB_t *wake_one(TCB_t **head);
int need_preempt(TCB_t *task);


#endif  // __TASK_H
