#ifndef __TIMERS_H
#define __TIMERS_H

#include "sched.h"
#include "queue.h"

#define IS_PERIODIC 1
#define IS_ONESHOT 0
#ifndef TIME_QUEUE_LEN
    #define TIME_QUEUE_LEN 10
#endif

typedef struct timer {
    // 链表用
    struct timer *next;

    // 定时参数
    uint8_t  is_periodic;        // 0=一次性, 1=周期
    uint32_t period_ticks;       // 周期（周期定时器 reload 用）
    uint32_t remaining_ticks;    // 倒计数值，每 tick 减 1
    uint8_t  active;             // 0=停止, 1=运行

    // 回调
    void (*callback)(void *arg);
    void   *arg;                 // 回调参数
} timer_t;


typedef struct {
    timer_t *timer;
} timer_event_t;

extern queue_t timer_queue;
extern uint8_t timer_queue_buf[TIME_QUEUE_LEN * sizeof(timer_event_t)];


void timer_init(timer_t *timer, uint32_t period_ticks, uint8_t is_periodic, void (*callback)(void *), void
*arg);
void timer_start(timer_t *timer);     // 插入排序链表
void timer_stop(timer_t *timer);      // 从链表移除
void timer_tick(void);                // 


#endif
