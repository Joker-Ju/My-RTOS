TCB_GUARD_OFFSET   		EQU  	 48
TCB_STACK_BASE_OFFSET 	EQU 	52 

		AREA    TCB_BSS, DATA, NOINIT, READWRITE, ALIGN=2
		EXPORT  current_tcb
current_tcb
		SPACE   4       ; 为指针分配4字节空间，初值为0
		AREA |.text|,CODE,READONLY
	
		IMPORT  scheduler_get_next
		EXPORT 	start_scheduler
		EXPORT	SVC_Handler
		EXPORT	PendSV_Handler
		EXPORT	HardFault_Handler
		THUMB
start_scheduler
		CPSID   I


		BL      scheduler_get_next
		CBZ     R0, NO_TASK


		LDR     R1, =current_tcb
		STR     R0, [R1]


		LDR     R0, [R0]          
		LDMIA   R0!, {R4-R11}      


		MSR     PSP, R0

		MOVS    R0, #2
		MSR     CONTROL, R0
		ISB
		CPSIE   I 
		
		SVC     0
		B       .

NO_TASK
		BKPT    0
		ALIGN



SVC_Handler

		THUMB

		CPSIE   I                  


		MRS     R0, PSP
		ADDS    R0, R0, #32        
		MSR     PSP, R0


		BX      LR
		ALIGN

HardFault_Handler
		IMPORT	HardFault_Diagnose
		
		THUMB

		TST     LR, #4            ;检查 LR bit 2
		ITE     EQ
		MRSEQ   R0, MSP           ; bit2=0 → 帧在 MSP
		MRSNE   R0, PSP           ; bit2=1 → 帧在 PSP
		B       HardFault_Diagnose


	
PendSV_Handler
		IMPORT  scheduler_on_pendsv
		IMPORT	GuardFault_Handler
			
		THUMB

		PUSH    {R12, LR}

		MRS     R0, PSP
		STMDB   R0!, {R4-R11}
		
		
		
		
		
		LDR     R5, =current_tcb
		LDR     R5, [R5]            ; R5 = current_tcb（整个 PendSV 都不改 R5）
		; ; ── 向上检查：哨兵 ──
		; R5 = 下一个任务的 TCB 指针
		LDR     R1, [R5, #TCB_GUARD_OFFSET]  ; R5 = TCB->guard_addr（用你数的偏移）
		LDR     R2, [R1]                      ; R2 = *guard_addr
		LDR     R3, =0xDEADBEEF
		CMP     R2, R3
		BNE     GUARD_CORRUPTED
		; ── 向下检查：SP < stack_base？──
		LDR     R1, [R5, #TCB_STACK_BASE_OFFSET]  ; R1 = stack_base
		CMP     R0, R1                            ; R0 = PSP-32
		BLO     STACK_UNDERFLOW

		STR     R0, [R5]            ; current_tcb->stack_ptr = R0（R5 就是 TCB 指针）


		BL      scheduler_on_pendsv
		CBZ     R0, PEND_NO_TASK

		LDR     R1, =current_tcb
		STR     R0, [R1]

		LDR     R0, [R0]
		LDMIA   R0!, {R4-R11}
		MSR     PSP, R0
		;为了对齐
		POP     {R12, PC}
GUARD_CORRUPTED
		MOV     R0, R5               ; 备份 R0（下一个任务）
		MOV     R1, #0
		LDR     R2, =GuardFault_Handler
		BLX     R2
		B       .
STACK_UNDERFLOW
		MOV     R0, R5               ; R0 = current_tcb
		MOV     R1, #1               ; R1 = 1，表示"向下溢出"
		LDR     R2, =GuardFault_Handler
		BLX     R2
		B       .

PEND_NO_TASK
		POP     {R12, PC}
		ALIGN
		END


