#include "My_RTOS.h"
//#include RTOS
#include "sched.h"
#include "event_groups.h"
#include "timers.h"

//#include Hardware
#include "Usart.h"
#include "Key.h"
#include "Motor.h"
#include "OLED.h"
#include "ADC1.h"
#include "DMA1.h"
#include "Led.h"
#include "Encoder.h"


//#include Others
#include "PID.h"

#define PID_Task_Stack_Size 50
#define PID_Task_PRIORITY 0
#define PID_Task_PREEMPT_PRIORITY 1
TCB_t  PID_Task_Handler;
uint32_t PID_Task_Stack[PID_Task_Stack_Size + TASK_GUARD_SIZE];

#define Position_Task_Stack_Size 50
#define Position_Task_PRIORITY 1
#define Position_Task_PREEMPT_PRIORITY 2
TCB_t Position_Task_Handler;
uint32_t Position_Task_Stack[Position_Task_Stack_Size + TASK_GUARD_SIZE];

#define ADC_Task_Stack_Size 50
#define ADC_Task_PRIORITY 1
#define ADC_Task_PREEMPT_PRIORITY 1
TCB_t  ADC_Task_Handler;
uint32_t ADC_Task_Stack[ADC_Task_Stack_Size + TASK_GUARD_SIZE];


#define Display_Task_Stack_Size 50
#define Display_Task_PRIORITY 0
#define Display_Task_PREEMPT_PRIORITY 2
TCB_t  Display_Task_Handler;
uint32_t Display_Task_Stack[Display_Task_Stack_Size + TASK_GUARD_SIZE];


#define TimerDaemon_Task_Stack_Size 50
#define TimerDaemon_Task_PRIORITY 0
#define TimerDaemon_Task_PREEMPT_PRIORITY 3
TCB_t  TimerDaemon_Task_Handler;
uint32_t TimerDaemon_Task_Stack[TimerDaemon_Task_Stack_Size + TASK_GUARD_SIZE];


#define Start_Task_Stack_Size 50
#define Start_Task_PRIORITY 0
#define Start_Task_PREEMPT_PRIORITY 0
TCB_t  Start_Task_Handler;
uint32_t Start_Task_Stack[Start_Task_Stack_Size + TASK_GUARD_SIZE];


#define BIT_KEY_EN    (1 << 0)   // 按键允许运行
#define BIT_ADC_EN    (1 << 1)   // ADC 采样完成
#define BIT_RUN       (1 << 2)   // 运行/停止状态


event_group_t ctrl_eg;

#define MiddleAngle 2048
#define ErrorAngle 	800

PID_TypeDef AnglePID = {
	.Kp = 0.15,
	.Ki = 0,
	.Kd = 0.3,
    .Target = MiddleAngle,
    .OutMax = 10.0f,
    .OutMin = -10.0f,
	.Alpha = 0.5,
};


void PID_Task(void *param)
{
    while (1) {
		// 等 ADC 采完 + 按键允许
		event_group_wait(&ctrl_eg, BIT_RUN | BIT_KEY_EN | BIT_ADC_EN, WAIT_MODE_AND);//阻塞1
		event_group_clear(&ctrl_eg, BIT_ADC_EN);
	
		PID_Update(&AnglePID);
		// 如果按键状态改变了，直接停止，进入下一轮
		if ((ctrl_eg.flags & (BIT_RUN | BIT_KEY_EN)) != (BIT_RUN | BIT_KEY_EN)) {
			Led_Off();
			Motor_SetPWM(0);
			event_group_clear(&ctrl_eg, BIT_RUN | BIT_KEY_EN);
			continue;
		}

		// 安全保护
		if (AnglePID.Actual < MiddleAngle - ErrorAngle || AnglePID.Actual > MiddleAngle + ErrorAngle) {
			Led_Off();
			Motor_SetPWM(0);
			event_group_clear(&ctrl_eg, BIT_RUN | BIT_KEY_EN);
		}
		else {
			Led_On();
			Motor_SetPWM((int8_t)AnglePID.Out);
		}
		static uint32_t last_disp = 0;
		if (system_tick - last_disp >= 20) {  // 最多每20ms刷一次
			last_disp = system_tick;
			task_notify(&Display_Task_Handler, 1);
		}
		static uint32_t last_Target = 0;
		if (system_tick - last_Target >= 30){
			last_Target = system_tick;
			task_notify(&Position_Task_Handler, 1);
		}
    }
}

PID_TypeDef PositionPID = {
	.Target = 0,            // 目标位置（编码器计数），调参时再设
	.OutMax = 100.0f,       // 最大偏移 200 个 ADC 值（约 5 度）
	.OutMin = -100.0f,
	.Alpha = 0.5,

};
uint16_t ADC1_Buffer[5]; // 0-2: Kp/Ki/Kd, 3:预留，4:角度
void Position_Task(void *param)
{

	// 初始化编码器
	Encoder_Init();
	static  int32_t Position = 0; 
	while (1) {
		task_notify_wait();   // 等角度环通知

		Position += Encoder_Get();
		PositionPID.Actual = Position;
		PositionPID.Kp = ADC1_Buffer[0] / 4095.0f * 0.5f;
		PositionPID.Ki = ADC1_Buffer[1] / 4095.0f * 0.05f;
		PositionPID.Kd = ADC1_Buffer[2] / 4095.0f * 10.0f;
		PID_Update(&PositionPID);

		// 位置环输出作为角度目标的偏移量
		AnglePID.Target = MiddleAngle + (int16_t)PositionPID.Out;
	}
}



void ADC_Task(void *param)
{
    while (1) {
		event_group_wait(&ctrl_eg, BIT_RUN | BIT_KEY_EN, WAIT_MODE_AND);
		// 重新触发 DMA + ADC
		DMA1_Reset_CNDTR(5);
        ADC1_SWSTART();

        task_notify_wait();// 等 DMA ISR 通知
		AnglePID.Actual = ADC1_Buffer[4];
        event_group_set(&ctrl_eg, BIT_ADC_EN);        
    }
}

void Display_Task(void *param)
{
	OLED_ShowString(1,1,"Kp=0.000");
	OLED_ShowString(2,1,"Ki=0.000");
	OLED_ShowString(3,1,"Kd=0.000");
	OLED_ShowString(1,9,"Tar=0000");
	OLED_ShowString(2,9,"ACT=0000");
	OLED_ShowString(3,9,"Out= 000");
    while (1) {
		task_notify_wait();  // ← 等 PID 通知再刷新
		OLED_ShowNum(1,13, AnglePID.Target, 4);
		OLED_ShowNum(2,13, AnglePID.Actual, 4);
		OLED_ShowSignedNum(3,13, AnglePID.Out, 3);
//		Serial_Printf("%f,%f,%f,%f\r\n",AnglePID.Target,AnglePID.Actual,AnglePID.Out,AnglePID.ErrorInt);

		OLED_ShowNum(1, 4, PositionPID.Kp * 10 / 10, 1);
 		OLED_ShowNum(1, 6, PositionPID.Kp * 10000 / 10, 3);
 		OLED_ShowNum(2, 4, PositionPID.Ki / 1, 1);
 		OLED_ShowNum(2, 6, PositionPID.Ki * 10000 / 10, 3);
 		OLED_ShowNum(3, 4, PositionPID.Kd / 1, 1);
 		OLED_ShowNum(3, 6, PositionPID.Kd * 10000 / 10, 3);
		
    }
}


void key_scan_cb(void *arg)
{
	Key_Tick();
	uint8_t k = Key_GetNum();
	if (k == 2) {
		if ((ctrl_eg.flags & BIT_RUN) != 0) {
			PositionPID.Target += 408;    // 正转一圈
		}
	}
	else if (k == 3) {
		if ((ctrl_eg.flags & BIT_RUN) != 0) {
			PositionPID.Target -= 408;    // 反转一圈
		}
	}
	else if (k == 4) {
		if ((ctrl_eg.flags & BIT_RUN) != 0) {
			// 正在运行 → 停止
			Led_Off();
			Motor_SetPWM(0);
			event_group_clear(&ctrl_eg, BIT_RUN | BIT_KEY_EN);
		} 

		else {
			// 已停止 → 启动
			Led_On();
			PID_Init(&AnglePID);
			PID_Init(&PositionPID);
			PositionPID.Target = 0;
			event_group_clear(&ctrl_eg, BIT_ADC_EN);
			event_group_set(&ctrl_eg, BIT_RUN | BIT_KEY_EN);
	
	}
}
}


void TimerDaemon_Task(void *param)
{
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



void Start_Task(void *param)
{
	Key_Init();
    // ADC + DMA 初始化（已改为软件触发+单次模式）
    ADC1_Init();
    DMA1_Init();
    DMA1_ADC1_StartConvert((uint32_t *)ADC1_Buffer, 5);   //先启动一次，确保 ADC 和 DMA 都正常工作，后续由 ADC_Task 负责重新触发
	Led_Init();
	//Usart可能有中断，要在调度器启动前初始化
	Usart_Init();
	OLED_Init();
	Motor_Init();

    // 创建软件定时器（按键扫描，20ms）
    timer_t key_timer;
    timer_init(&key_timer, 20, IS_PERIODIC, key_scan_cb, NULL);
    timer_start(&key_timer);
	//timer_queue，timer_queue_buf以及其长度在timers.c和timers.h里面
    queue_create(&timer_queue, timer_queue_buf, sizeof(timer_event_t), 10);
    event_group_init(&ctrl_eg);
	
	uint32_t bp = enter_critical();
	
	task_create(&PID_Task_Handler,
				PID_Task,
				NULL,
				PID_Task_Stack, // 这里直接用 Start_Task 的栈顶地址，实际应用中应为独立的栈空间
				PID_Task_Stack_Size,
				PID_Task_PRIORITY,
				PID_Task_PREEMPT_PRIORITY);	
	task_create(&Position_Task_Handler,
				Position_Task,
				NULL,
				Position_Task_Stack, // 这里直接用 Start_Task 的栈顶地址，实际应用中应为独立的栈空间
				Position_Task_Stack_Size,
				Position_Task_PRIORITY,
				Position_Task_PREEMPT_PRIORITY);			
    task_create(&ADC_Task_Handler,
                ADC_Task,
                NULL,
                ADC_Task_Stack, 
                ADC_Task_Stack_Size,
                ADC_Task_PRIORITY,
                ADC_Task_PREEMPT_PRIORITY);	
    task_create(&Display_Task_Handler,
                Display_Task,               
                NULL,
                Display_Task_Stack, 
                Display_Task_Stack_Size,
                Display_Task_PRIORITY,
                Display_Task_PREEMPT_PRIORITY);	
    task_create(&TimerDaemon_Task_Handler,
                TimerDaemon_Task,               
                NULL,
                TimerDaemon_Task_Stack, 
                TimerDaemon_Task_Stack_Size,
                TimerDaemon_Task_PRIORITY,
                TimerDaemon_Task_PREEMPT_PRIORITY);	
    
    exit_critical(bp);
    while(1){
		
	}
}



void My_RTOS_Start(void)
{
    scheduler_init();
    //初始化 SysTick（1ms 心跳），必须在启动调度器之前
    systick_init_1ms();
    //启动调度器
    //创建启动任务
    task_create(&Start_Task_Handler,
                Start_Task,
                NULL,
                Start_Task_Stack,
                Start_Task_Stack_Size,
                Start_Task_PRIORITY,
                Start_Task_PREEMPT_PRIORITY);
    start_scheduler();

    //不会执行到这里
}


void DMA1_Channel1_IRQHandler(void)
{
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        DMA1->IFCR |= DMA_IFCR_CTCIF1;
        if (task_notify_from_isr(&ADC_Task_Handler, 1)) {
            trigger_switch();
        }
    }
}


void SysTick_Handler(void)
{
    system_tick++;
    scheduler_tick_1ms();
    timer_tick();  // 处理定时器相关的计数和回调
    delay_list_tick(system_tick);  // 唤醒那些到期的任务，system_tick 作为全局时间基准, 好像会到上限？先用着，反正能跑1.36年
	trigger_switch();
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
			printf("  [%d] 0x%08X\r\n", idx, val);
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
		printf("(all garbage - stack corruption detected)\r\n");
	} else {
		printf("(%d valid addresses found)\r\n", good);
	}
    while (1);
}

void GuardFault_Handler(TCB_t *suspect, uint32_t reason)
{
	__disable_irq();
	if (reason == 0) {
		printf("\n\n!!! STACK OVERFLOW (GUARD TRIGGERED) !!!\r\n");
		printf("Guard at:         0x%08X\r\n", (uint32_t)suspect->guard_addr);
		printf("Guard value:      0x%08X (expected 0xDEADBEEF)\r\n",*suspect->guard_addr);
	} 
	else {
		printf("\n\n!!! STACK UNDERFLOW (SP < STACK_BASE) !!!\r\n");
	}
	printf("Suspect task TCB: 0x%08X\r\n", (uint32_t)suspect);
	printf("Stack base:       0x%08X\r\n", suspect->stack_base);
	while (1);
}


