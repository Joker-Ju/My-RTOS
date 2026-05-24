
			;AREA	myCode,CODE,READONLY
			;ENTRY
			;EXPORT		__main
;num			EQU		2

;__main
			;LDR		R0,=0xE000ED88
			;LDR		R1,[R0]
			;ORR		R1,R1,#(0xF<<20)
			;STR		R1,[R0]
			
			;;VMOV.F	S0,#3F800000
			;;VMOV.F	S1,S0
			;;VADD.F	S2,S1,S0
			
			
			;MOV		R0,#3
			;MOV		R1,#3
			;MOV		R2,#2
			;BL		afunc
			
;Stop		B		Stop

;afunc		CMP		R0,#num
			;MOVHS	PC,LR
			;ADRL	R3,JumpTable
			;LDR		PC,[R3,R0,LSL #2]
;JumpTable		
			;DCD		DoAdd
			;DCD		DoSub

;DoAdd		ADD		R0,R1,R2
			;BX		LR
			
;DoSub		SUB		R0,R1,R2
			;BX		LR




			;END
		