/*
 * Practica1_ejer1.s
 *
 *  Created on: 11/02/2025
 */

/* .equ RESULT, 0x100 */

.global _start
_start:
	movia r9, LIST 		/* r9 points to the start of the list */
	movia r4, N			/* r4 points to the number of entries in the list */
	
	ldw r5, 0(r4)		/* r5 loads the number of entries in the list */
	addi r6, r0, 0		/* r6 holds the current number */
	addi r7, r0, 0		/* r7 holds the number of even numbers found */
LOOP:
	andi r8, r6, 1		/* Check if the number is even */
	bne r8, r0, NEXT_ITER /* If the number is odd, skip to the next iteration */

	addi r7, r7, 1		/* Increment the number of even numbers found */
	stw r6, 0(r9)		/* Store the even number in the result list */
	
	beq r7, r5, STOP 	/* If all the even numbers have been found, stop */
	addi r9, r9, 4		/* Move to the next entry in the result list */

NEXT_ITER:
	addi r6, r6, 1		/* Increment the current number */
	br LOOP				/* Repeat the loop */
	
STOP:
	br STOP				/* Remain here if done */
	
.org 0x0F0				/* Starting address of the list */
N:
.word 8 /* Number of entries in the list */

.org 0x100
LIST:
.skip 32 /* Reserve 32 bytes for the result */

.end 
