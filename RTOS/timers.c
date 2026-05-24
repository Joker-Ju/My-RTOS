#include "timers.h"


static timer_t *timer_head = NULL;


queue_t timer_queue;
uint8_t timer_queue_buf[TIME_QUEUE_LEN * sizeof(timer_event_t)];


void timer_init(timer_t *timer, uint32_t period_ticks, 
    uint8_t is_periodic, void (*callback)(void *), void *arg)
{
    timer->next = NULL;
    timer->is_periodic = is_periodic;
    timer->period_ticks = period_ticks;
    timer->remaining_ticks = 0;
    timer->active = 0;
    timer->callback = callback;
    timer->arg = arg;
}
void timer_start(timer_t *timer)
{
    uint32_t bp = enter_critical();
    if (timer->active) {
        exit_critical(bp);
        return;
    }
    timer->remaining_ticks = timer->period_ticks;
    timer->active = 1;
    timer->next = NULL;

    // 相对时间排序插入（按 remaining 从小到大）
    timer_t **p = &timer_head;
    while (*p != NULL && (*p)->remaining_ticks <= timer->remaining_ticks) {
        timer->remaining_ticks -= (*p)->remaining_ticks;  // 相对化
        p = &(*p)->next;
    }
    if (*p != NULL) {
        (*p)->remaining_ticks -= timer->remaining_ticks;
    }
    timer->next = *p;
    *p = timer;

    exit_critical(bp);
}

void timer_stop(timer_t *timer)
{
    uint32_t bp = enter_critical();

    timer_t **p = &timer_head;
    while (*p != NULL && *p != timer) {
        p = &(*p)->next;
    }
    if (*p == timer) {
        *p = timer->next;
        if (*p != NULL) {
            (*p)->remaining_ticks += timer->remaining_ticks;
        }
        timer->next = NULL;
        timer->active = 0;
    }

    exit_critical(bp);
}

 // SysTick 里调
void timer_tick(void)
{
    if (timer_head == NULL) return;

    timer_head->remaining_ticks--;
    if (timer_head->remaining_ticks > 0) return;

    // 到期了，摘下头节点
    timer_t *expired = timer_head;
    timer_head = expired->next;
    expired->next = NULL;
    expired->active = 0;

    // 把下一个头节点的 remaining 恢复（原先是相对值）
    if (timer_head != NULL) {
        timer_head->remaining_ticks += expired->remaining_ticks;
    }

    // 发给守护任务处理
    timer_event_t evt = {expired};
    queue_send_from_isr(&timer_queue, &evt);   // 直接发事件到队列，守护任务里执行回调
    /*以下为缓冲区若是不够，中断执行回调，重新发送任务，但是得重写一个不阻塞函数
    感觉不如直接增加缓冲区。*/
    
    // if (queue_send_from_isr(&timer_queue, &evt) == 0) {
    //     expired->callback(expired->arg);           // ← 直接在中断里跑
    //     if (expired->is_periodic) timer_start_from_isr(expired);  // 周期性的重新 start（中断里执行）
    // }
}

