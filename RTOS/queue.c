#include "queue.h"
#include <string.h>

static void my_memcpy(void *dst, const void *src, uint32_t n)
{
  for (uint32_t i = 0; i < n; i++) {
	  ((uint8_t*)dst)[i] = ((const uint8_t*)src)[i];
  }
}





int queue_create(queue_t *q, void *buffer, uint32_t item_size, uint32_t queue_len)
{
	if (q == NULL || buffer == NULL) return -1;
	if (item_size == 0 || queue_len == 0) return -1;

	q->storage        = buffer;        // 用户提供的存放消息的缓冲区
	q->item_size      = item_size;     // 每条消息几个字节
	q->queue_len      = queue_len;     // 总共能存几条消息
	q->head           = 0;             // 读指针
	q->tail           = 0;             // 写指针
	q->count          = 0;             // 刚开始一条消息都没有
	q->wait_send_head = NULL;          // 没人等空位
	q->wait_recv_head = NULL;          // 没人等消息
	return 0;

}

 

void queue_send(queue_t *q, void *data)
{
    uint32_t bp = enter_critical();

    while (q->count >= q->queue_len) 
    {
        // 队列满了 → 阻塞当前任务
        current_tcb->state = TASK_BLOCKED;
        // 按优先级插入 wait_send_head
		pend_to_list(&q->wait_send_head, current_tcb);
        exit_critical(bp);
        trigger_switch();  // 切出去等空位
        bp = enter_critical(); // 切回来后重新进入临界区
    }
    my_memcpy((uint8_t*)q->storage + q->tail * q->item_size,
        data, q->item_size);
    q->tail = (q->tail + 1) % q->queue_len;
    q->count++;
    // 唤醒一个正在等消息收的任务
    TCB_t *task = wake_one(&q->wait_recv_head);
    exit_critical(bp);
    if (task != NULL && current_tcb != NULL && need_preempt(task)) 
		{
            trigger_switch();
        }
}



uint32_t queue_send_from_isr(queue_t *q, void *data)
{
    vTaskSuspendAll();         
    uint32_t sent = 0;

    if (q->count < q->queue_len) {
        my_memcpy((uint8_t*)q->storage + q->tail * q->item_size,
                data, q->item_size);
        q->tail = (q->tail + 1) % q->queue_len;
        q->count++;

        wake_one(&q->wait_recv_head);         /* 唤醒等消息的任务 */
        sent = 1;
    }
    /* ISR 版：满了直接放弃，不阻塞 */

    xTaskResumeAll();            
    return sent;
}


void queue_recv(queue_t *q, void *data)
{
    uint32_t bp = enter_critical();

    /* 队列空 → 阻塞当前任务，等待消息 */
    while (q->count == 0) {
        current_tcb->state = TASK_BLOCKED;
        pend_to_list(&q->wait_recv_head, current_tcb);
        exit_critical(bp);
        trigger_switch();
        bp = enter_critical();
    }

    /* 从缓冲区读出 */
    my_memcpy(data,
            (uint8_t*)q->storage + q->head * q->item_size,
            q->item_size);
    q->head = (q->head + 1) % q->queue_len;
    q->count--;

    /* 唤醒一个等空位发送的任务 */
    TCB_t *task = wake_one(&q->wait_send_head);

    exit_critical(bp);

    if (task != NULL && current_tcb != NULL && need_preempt(task)) {
        trigger_switch();
    }
}

uint32_t queue_recv_from_isr(queue_t *q, void *data)
{
    vTaskSuspendAll();
    uint32_t got = 0;

    if (q->count > 0) {
        my_memcpy(data,
                (uint8_t*)q->storage + q->head * q->item_size,
                q->item_size);
        q->head = (q->head + 1) % q->queue_len;
        q->count--;

        wake_one(&q->wait_send_head);         /* 唤醒等空位的任务 */
        got = 1;
    }

    xTaskResumeAll();
    return got;
}

