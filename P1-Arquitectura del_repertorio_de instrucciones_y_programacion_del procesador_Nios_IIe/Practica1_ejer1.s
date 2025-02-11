.equ N, 0x0F0
.equ RESULT, 0x100

.global _start
_start:
	
	movia r9, RESULT
	movia r4, N
	
	ldw r5, 0(r4)
	addi r6, r0, 0
	addi r7, r0, 0

LOOP:

	andi r8, r6, 1
	
	bne r8, r0, NEXT_ITER
	
	addi r7, r7, 1
	stw r6, 0(r9)
	
	beq r7, r5, STOP
	addi r9, r9, 4

NEXT_ITER:
	addi r6, r6, 1
	
	br LOOP
	
STOP:
	br STOP
	
.org 0x0f0

N:
.word 8

.end 
