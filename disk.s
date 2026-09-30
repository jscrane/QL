	.text
	.section .text
	.global start

#include "disk_device.h"

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

	moveq   #0x60,d1		| Allocate directory driver linkage block
	moveq   #0x18,d0		| MT.ALCHP = allocate space in common heap
	trap    #1

	tst.l   d0
	bne.s   init_done       	| allocation failed

	lea     0x1c(a0),a3
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

	move.l  a0,a3			| A3 = linkage block

	lea     0x18(a0),a0		| Install driver
	moveq   #0x22,d0		| MT.LDD
	trap    #1

	tst.l   d0
	bne.s   init_done

	nop

init_done:
	movem.l (sp)+,d0-d7/a0-a6
	rts

| Entry/exit
|   D0 - operation type/error code
|   D1 - input/output byte
|   D2 - buffer length?
|   D3 - initial entry
|   A0 - channel def block
|   A1 - pointer to read/write buffer
|   A2 - pointer to physical definition block
|   A3 - assumed start of linkage block
|   A6 - base of system variables
io:
	lea	DISK_IO, a4
	clr.l	d4
	move.b	(a4), d4
	ext.w	d4
	ext.l	d4
	move.l	d4, d0
	rts

| Entry/exit
|   D0 - /error code
|   D3 - open type
|   A0 - channel def block
|   A1 - physical def block
|   A3 - assumed start of linkage block
|   A6 - base of system variables
open:
	lea	DISK_OPEN, a2
	clr.l	d0
	move.b	(a2), d0
	ext.w	d0
	ext.l	d0
	rts

| Entry/exit
|   D0 - /error code
|   A0 - channel def block
|   A1 - physical def block
|   A3 - assumed start of linkage block
|   A6 - base of system variables
close:
	lea	DISK_CLOSE, a2
	clr.l	d0
	move.b	(a2), d0
	ext.w	d0
	ext.l	d0
	rts
