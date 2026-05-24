#include "My_RTOS.h"
#include "sched.h"
#include "semaphore.h"
#include "Led.h"
#include "Usart.h"
#include "queue.h"
#include "string.h"
#include "timers.h"
#include "event_groups.h"
#include "heap.h"


// #define TASK_GUARD_SIZE 4  // 每个任务栈底放4个哨兵，检测栈溢出

#define Start_Task_Stack_Size 50
#define Start_Task_PRIORITY 0
#define Start_Task_PREEMPT_PRIORITY 0
TCB_t   Start_Task_Handler;
uint32_t Start_Task_Stack[Start_Task_Stack_Size + TASK_GUARD_SIZE];

#define CmdTask_Stack_Size 50
#define CmdTask_PRIORITY 2
#define CmdTask_PREEMPT_PRIORITY 3
TCB_t   CmdTask_Handler;
uint32_t CmdTask_Stack[CmdTask_Stack_Size + TASK_GUARD_SIZE]; 

#define Task1_Stack_Size 50
#define Task1_PRIORITY 2
#define Task1_PREEMPT_PRIORITY 2
TCB_t   Task1_Handler;
uint32_t Task1_Stack[Task1_Stack_Size + TASK_GUARD_SIZE];

#define Task2_Stack_Size 50
#define Task2_PRIORITY 2
#define Task2_PREEMPT_PRIORITY 1
TCB_t   Task2_Handler;
uint32_t Task2_Stack[Task2_Stack_Size + TASK_GUARD_SIZE];


#define timers_deamon_Stack_Size 50
#define timers_deamon_PRIORITY 2
#define timers_deamon_PREEMPT_PRIORITY 4
TCB_t   timers_deamon_Handler;
uint32_t timers_deamon_Stack[timers_deamon_Stack_Size + TASK_GUARD_SIZE];

#define NotifyTestTask_Stack_Size 50
#define NotifyTestTask_PRIORITY 2
#define NotifyTestTask_PREEMPT_PRIORITY 5
TCB_t  NotifyTestTask_Handler;
uint32_t NotifyTestTask_Stack[NotifyTestTask_Stack_Size + TASK_GUARD_SIZE];

// uint32_t count = 0;
// mutex_t lock1;
// mutex_t lock2;
// sem_t sem1 = {1, NULL};
// sem_t sem2 = {1, NULL};

queue_t queue1;
// char queue1_buffer[13*10];


extern uint8_t Usart1Over;
extern uint8_t buffer[100];
extern uint8_t size;

//typedef enum { 
//	LED_STOP, LED_ON, LED_OFF,LED_TOGGLE, LED_BLINK 
//} led_mode_t;

//volatile led_mode_t led_mode = LED_ON;
//volatile uint32_t led_blink_period = 500;

//static sem_t start_sem;

//static void cmd_on(void) {
//	led_mode = LED_ON;
//	sem_give(&start_sem);              // ← 给信号，通知 Task1 可以继续
//	printf("LED ON\r\n");
//}

//static void cmd_off(void) {
//	led_mode = LED_OFF;
//	sem_give(&start_sem);              // ← 给信号，通知 Task1 可以继续
//	printf("LED OFF\r\n");
//}

//static void cmd_toggle(void) {
//	led_mode = LED_TOGGLE;
//	sem_give(&start_sem);              // ← 给信号，通知 Task1 可以继续
//	printf("LED TOGGLE\r\n");
//}

//static void cmd_blink(void) {
//	led_mode = LED_BLINK;
//	sem_give(&start_sem);              // ← 给信号，通知 Task1 可以继续
//	printf("BLINK 500\r\n");
//}

//typedef struct {
//	const char *cmd_name;       // 命令字符串
//	void (*handler)(void);      // 对应的处理函数
//} cmd_entry_t;


//static const cmd_entry_t cmd_table[] = {
//	{"ON",      cmd_on},
//	{"OFF",     cmd_off},
//	{"TOGGLE",  cmd_toggle},
//	{"BLINK",  cmd_blink},   
//};

//#define CMD_TABLE_SIZE  (sizeof(cmd_table) / sizeof(cmd_table[0]))

//mutex_t printf_mutex = {NULL, NULL, 0, 0};

//void CmdTask(void *param) 
//{
//	char buf[20];
//	while(1) 
//	{
//		queue_recv(&queue1,buf);
//		int i;
//		int found = 0;

//		for (i = 0; i < CMD_TABLE_SIZE; i++) {
//			if (strcmp(buf, cmd_table[i].cmd_name) == 0) {
//				mutex_lock(&printf_mutex);
//				cmd_table[i].handler();     // 找到就执行对应的函数
//				mutex_unlock(&printf_mutex);
//				found = 1;
//				break;
//			}
//		}
//		if (!found) {
//			mutex_lock(&printf_mutex);
//			printf("未知命令: %s\r\n", buf);
//			mutex_unlock(&printf_mutex);
//		}
//		size = 0;
//	}
//}



//void Task1(void *param)
//{
//	LED0_Init();
//	sem_take(&start_sem);              // ← 等 CmdTask 给信号才继续
//	uint32_t last_tick = 0;

//	while (1) {
//		switch (led_mode) {
//		case LED_ON:
//			LED0_On();
//			mutex_lock(&printf_mutex);
//			printf("LED ON\r\n");
//			mutex_unlock(&printf_mutex);
//			break;
//		case LED_OFF:
//			LED0_Off();
//			mutex_lock(&printf_mutex);
//			printf("LED OFF\r\n");
//			mutex_unlock(&printf_mutex);
//			break;
//		case LED_TOGGLE:
//			LED0_Toggle();
//		
//			led_mode = LED_STOP; // 切换一次后停止
//		
//			mutex_lock(&printf_mutex);
//			printf("LED TOGGLE\r\n");
//			mutex_unlock(&printf_mutex);
//			break;
//		case LED_BLINK:
//			if (system_tick - last_tick >= led_blink_period) {
//				LED0_Toggle();
//				last_tick = system_tick;
//			}
//			mutex_lock(&printf_mutex);
//			printf("LED BLINK\r\n");
//			mutex_unlock(&printf_mutex);
//			break;
//		default:
//			break;
//		}
//		task_delay(&Task1_Handler, 1000);   // 每 10ms 检查一次，不阻塞
//	}
//}

//void Task2(void *param)
//{
//	while(1) 
//	{
////		char data[] = "你好世界";
////		queue_send(&queue1, data);
//		task_delay(&Task2_Handler,500);	

//	}
//}

mutex_t lock;
extern queue_t timer_queue;
extern uint8_t timer_queue_buf[];    // 引用 timers.c 里的缓冲区

event_group_t test_eg;

#define BIT_A   (1 << 0)
#define BIT_B   (1 << 1)

timer_t blink_timer;
void led_blink_cb(void *arg);

void CmdTask(void *param) 
{
	task_delay(&CmdTask_Handler, 5000);   // 刚开始等一秒，确保 Task2 先运行
	LED0_Init();
	while(1) 
	{
		mutex_lock(&lock);
		uint32_t i;
		for (i = 0; i < 10000; i++)
		{
			
		}
		i=0;
		// LED0_Toggle();
		mutex_unlock(&lock);
		task_delay(&CmdTask_Handler,1000);
	}
}

void Task1(void *param)
{
	task_delay(&Task1_Handler, 1000);   // 刚开始等一秒，确保 Task2 先运行
	char buf[8];
	while (1) {
			queue_recv(&queue1, buf);
		if (strcmp(buf, "START") == 0) {
			timer_start(&blink_timer);
			event_group_set(&test_eg, BIT_A);
			task_notify(&NotifyTestTask_Handler, 1);    // ← 亮灯
		}
		else if(strcmp(buf, "STOP") == 0) {
			timer_stop(&blink_timer);
			event_group_set(&test_eg, BIT_B);
			task_notify(&NotifyTestTask_Handler, 0);    // ← 灭灯
		}
		size = 0;
		
	}
}

void Task2(void *param)
{
	while(1) 
	{
		event_group_wait(&test_eg, BIT_A | BIT_B, WAIT_MODE_AND);
		// BIT_A 和 BIT_B 都置位了才执行
		LED0_Toggle();
		task_delay(&Task2_Handler, 200);

		// 清掉标志，这样下次还要两个都发才能再闪
		event_group_clear(&test_eg, BIT_A | BIT_B);
	}
}

void NotifyTestTask(void *param)
{
	while (1) {
		uint32_t val = task_notify_wait();
		printf("NotifyTestTask got: %lu\r\n", val);
		if (val == 1) LED0_On();
		else if (val == 0) LED0_Off();
	}
}

void TimerDaemonTask(void *param)
{
	LED1_Init();
	while (1) {
		timer_event_t evt;
		queue_recv(&timer_queue, &evt);       // 等到期事件

		timer_t *t = evt.timer;
		t->callback(t->arg);                  // 在任务上下文执行回调

		if (t->is_periodic) {
			timer_start(t);                   // 重新插入链表
		}
	}
}




void Start_Task(void *pvParameters)
{
	LED1_Init();
	uint32_t bp = enter_critical();
	task_create(&CmdTask_Handler,
				CmdTask,
				
				NULL,
				CmdTask_Stack, // 这里直接用 Start_Task 的栈顶地址，实际应用中应为独立的栈空间
				CmdTask_Stack_Size,
				CmdTask_PRIORITY,
				CmdTask_PREEMPT_PRIORITY);
	task_create(&Task1_Handler,
				Task1,
				NULL,
				Task1_Stack,
				Task1_Stack_Size,
				Task1_PRIORITY,
				Task1_PREEMPT_PRIORITY);
	task_create(&Task2_Handler,
				Task2,
				NULL,
				Task2_Stack,
				Task2_Stack_Size,
				Task2_PRIORITY,
				Task2_PREEMPT_PRIORITY);
	task_create(&timers_deamon_Handler,
				TimerDaemonTask,
				NULL,
				timers_deamon_Stack,
				timers_deamon_Stack_Size,
				timers_deamon_PRIORITY,
				timers_deamon_PREEMPT_PRIORITY);
	task_create(&NotifyTestTask_Handler,
				NotifyTestTask,
				NULL,
				NotifyTestTask_Stack,
				NotifyTestTask_Stack_Size,
				NotifyTestTask_PRIORITY,
				NotifyTestTask_PREEMPT_PRIORITY);
	exit_critical(bp);
//	/* ── 堆测试 ── */
//	heap_init();
//	
//	// 1) 基本分配
//	void *p1 = heap_malloc(20);
//	void *p2 = heap_malloc(50);
//	printf("p1=0x%x, p2=0x%x, diff=%d\r\n", (uint32_t)p1, (uint32_t)p2, (uint32_t)p2 - (uint32_t)p1);

//	// 2) 写入并读出
//	strcpy((char*)p1, "HELLO");
//	printf("p1 says: %s\r\n", (char*)p1);

//	// 3) 释放 p1，再申请 60 → 应该重用 p1 那块，体现分割
//	heap_free(p1);
//	void *p3 = heap_malloc(60);
//	printf("p3=0x%x, same as p1=%d\r\n", (uint32_t)p3, p3 == p1);

//	// 4) 释放 p2，再申请更大的 → 验证合并
//	heap_free(p2);
//	heap_free(p3);                    // 此时三块并一块
//	void *p4 = heap_malloc(200);
//	printf("p4=0x%x, coalesce ok\r\n", (uint32_t)p4);

//	// 5) 空指针/0 边界
//	heap_free(NULL);
//	void *p5 = heap_malloc(0);
//	printf("p5=%x (should be 0)\r\n", (uint32_t)p5);

//	heap_free(p4);
//	printf("heap test done\r\n");

	while (1) 
    {
         
    }
}

void led_blink_cb(void *arg)
{
	LED1_Toggle();
	printf("Timer Callback: LED TOGGLE\r\n");
}

void My_RTOS_Start(void)
{
    scheduler_init();

    //初始化 SysTick（1ms 心跳），必须在启动调度器之前
    systick_init_1ms();
	timer_init(&blink_timer, 500, IS_PERIODIC, led_blink_cb, NULL);
	// sem_create(&start_sem, 0);
	queue_create(&queue1, buffer, 8,10);
	queue_create(&timer_queue, timer_queue_buf, sizeof(timer_event_t), 10);
    //创建启动任务
    task_create(&Start_Task_Handler,
                Start_Task,
                NULL,
                Start_Task_Stack,
                Start_Task_Stack_Size,
                Start_Task_PRIORITY,
                Start_Task_PREEMPT_PRIORITY);
	Usart_Init();
    //启动调度器
    start_scheduler();

    //不会执行到这里
}
void USART1_IRQHandler(void)
{
	if (USART1->SR & USART_SR_RXNE) {
		buffer[size++] = USART1->DR;
	}
	else if (USART1->SR & USART_SR_IDLE) {
		USART1->DR;
		buffer[size] = '\0';
		queue_send_from_isr(&queue1, buffer);   // 整条发进队列

	}
}

void SysTick_Handler(void)
{
    system_tick++;
    scheduler_tick_1ms();
    timer_tick();  // 处理定时器相关的计数和回调
    delay_list_tick(system_tick);  // 唤醒那些到期的任务，system_tick 作为全局时间基准, 好像会到上限？先用着，反正能跑1.36年
}


void HardFault_Diagnose(uint32_t *frame)
{
    uint32_t pc   = frame[6];    // 崩在哪条指令
    uint32_t lr   = frame[5];    // 返回地址
    uint32_t psr  = frame[7];    // 程序状态
    uint32_t r0   = frame[0];    // 第一个参数（通常是指针/值）

    uint32_t cfsr = *(volatile uint32_t *)0xE000ED28;
    uint32_t hfsr = *(volatile uint32_t *)0xE000ED2C;
    uint32_t bfar = *(volatile uint32_t *)0xE000ED38;

    printf("\n\n=== HARD FAULT ===\r\n");
    printf("PC  = 0x%08X\r\n", pc);
    printf("LR  = 0x%08X\r\n", lr);
    printf("PSR = 0x%08X\r\n", psr);
    printf("R0  = 0x%08X\r\n", r0);
    printf("CFSR= 0x%08X\r\n", cfsr);

    // 解码 CFSR —— UsageFault 段
    if (cfsr & (1UL << 16)) printf("  >> UNDEFINSTR\r\n");
    if (cfsr & (1UL << 17)) printf("  >> INVSTATE (LSB=0?)\r\n");
    if (cfsr & (1UL << 24)) printf("  >> DIVBYZERO\r\n");
    if (cfsr & (1UL << 25)) printf("  >> UNALIGNED\r\n");

    // BusFault 段
    if (cfsr & (1UL << 8))  printf("  >> IBUSERR (inst fetch)\r\n");
    if (cfsr & (1UL << 9))  printf("  >> PRECISERR (data access at BFAR)\r\n");
    if (cfsr & (1UL << 11)) printf("  >> IMPRECISERR\r\n");

    // MemManage 段
    if (cfsr & (1UL << 0))  printf("  >> IACCVIOL\r\n");
    if (cfsr & (1UL << 1))  printf("  >> DACCVIOL\r\n");

    // 有精确地址的
    if (cfsr & (1UL << 15)) printf("  >> BFAR = 0x%08X\r\n", bfar);
/* ── 向下扫（低地址方向）→ 完好的 LR ── */
	printf("\n=== STACK DOWN (good LR) ===\r\n");
	uint32_t *p = frame;
	int idx = 0;
	while ((uint32_t)p > 0x20000000 && idx < 10) {  // 不要读到 SRAM 以下
		p--;
		uint32_t val = *p;
		if (val >= 0x08000000 && val <= 0x0803FFFF) {
			printf("  [%d] 0x%08lX\r\n", idx, val);
			idx++;
		}
	}

	/* ── 向上扫（高地址方向）→ 检查是否被踩 ── */
	printf("\n=== STACK UP (corrupted?) ===\r\n");
	p = frame;
	int good = 0;
	while ((uint32_t)p < 0x20005000 && p < frame + 256) {  // 不要超过 SRAM 上限
		uint32_t val = *p;
		if (val >= 0x08000000 && val <= 0x0803FFFF) good++;
		p++;
	}
	if (good == 0) {
		printf("  (all garbage — stack corruption detected)\r\n");
	} else {
		printf("  (%d valid addresses found)\r\n", good);
	}
    while (1);
}

void GuardFault_Handler(TCB_t *suspect, uint32_t reason)
{
	__disable_irq();
	if (reason == 0) {
		printf("\n\n!!! STACK OVERFLOW (GUARD TRIGGERED) !!!\r\n");
		printf("Guard at:         0x%08lX\r\n", (uint32_t)suspect->guard_addr);
		printf("Guard value:      0x%08lX (expected 0xDEADBEEF)\r\n",*suspect->guard_addr);
	} 
	else {
		printf("\n\n!!! STACK UNDERFLOW (SP < STACK_BASE) !!!\r\n");
	}
	printf("Suspect task TCB: 0x%08lX\r\n", (uint32_t)suspect);
	printf("Stack base:       0x%08lX\r\n", suspect->stack_base);
	while (1);
}
