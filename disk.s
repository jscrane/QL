	.text
	.section .text

start:
	dc.l	0x4afb0001
	dc.w	proc_def - start
	dc.w	init - start

proc_def:
	dc.w	0
	dc.w	0
	dc.w	0
	dc.w	0

init:
	movem.l d0-d7/a0-a6,-(sp)

	moveq   #40,d1			| Allocate directory driver linkage block
	moveq   #0x18,d0		| MT.ALCHP = allocate space in common heap
	trap    #1

	tst.l   d0
	bne.s   init_done       	| allocation failed

	move.l  a0,a3			| A3 = linkage block
	lea     io(pc),a1		| $1C: I/O routine
	move.l  a1,(a3)+
	lea     open(pc),a1		| $20: Open routine
	move.l  a1,(a3)+
	lea     close(pc),a1		| $24: Close routine
	move.l  a1,(a3)+
	clr.l   (a3)+			| $28: Forced slave -- none
	clr.l   (a3)+			| $2C: Rename -- none
	clr.l   (a3)+			| $30: Truncate -- none
	clr.l   (a3)+			| $34: Format -- none

        move.l  #0x40,(a3)+		| $38: Physical Definition Block size

        move.w  #3,(a3)+		| $3C: Device name length + name
        move.l  #0x44495330,(a3)+	| "DIS0"

        move.w  #3,(a3)+		| $42: second device
        move.l  #0x44495330,(a3)+	| "DIS0"

	lea     0x18(a0),a0		| Install driver
	moveq   #0x22,d0		| MT.LDD
	trap    #1

	tst.l   d0
	bne.s   init_done

	nop

init_done:
	movem.l (sp)+,d0-d7/a0-a6
	rts

io:
	rts

open:
	rts

close:
	rts
