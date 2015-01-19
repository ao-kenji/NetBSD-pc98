/*	$NecBSD: nepcreg.h,v 1.7.6.1 1999/08/19 09:32:14 root Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pcic98 : PC-9801 original PCMCIA controller code for NS/A, Ne, NX/C, NL/R.
 * by Noriyuki Hosobuchi
 */

#ifndef	_NEPCREG_H_
#define	_NEPCREG_H_

#define	PCIC98_REG0		0x0a8e	/* byte */
#define	PCIC98_REG1		0x1a8e	/* byte */
#define	PCIC98_REG2		0x2a8e	/* byte */
#define	PCIC98_REG3		0x3a8e	/* byte : Interrupt */
#define	PCIC98_REG4		0x4a8e	/* word : PC-98 side IO base */
#define	PCIC98_REG5		0x5a8e	/* word : Card side IO base */
#define	PCIC98_REG6		0x7a8e	/* byte */
#define	PCIC98_REG_WINSEL	0x1e8e	/* byte : win bank select register */
#define	PCIC98_REG_PAGOFS	0x0e8e	/* word */

/* PCIC98_REG_WINSEL */
#define	PCIC98_MAPROM		0x80
#define	PCIC98_MAPRAM		0x81
#define	PCIC98_MAPWIN		0x84	/* map Card on 0xda0000 - 0xdbffff */
#define	PCIC98_UNMAPWIN		PCIC98_MAPROM

/* PCIC98_REG1 */
#define	PCIC98_CARDEXIST	0x08	/* 1:exist 0:not exist */
#define	PCIC98_CARDWP		0x04	/* Write protect */
#define	PCIC98_CARDRAM		0x02	/* Ram rom card */
#define	PCIC98_CARDEMS		0x01	/* EMS card */

/* PCIC98_REG2 */
#define	PCIC98_MAPIO		0x80	/* 1:IO 0:Memory */
#define	PCIC98_IOTHROUGH	0x40	/* 0:IO map 1:IO addr-through */
#define	PCIC98_8BIT		0x20	/* bit width 1:8bit 0:16bit */
#define	PCIC98_IOMAP128		0x10	/* IO map size 1:128byte 0:16byte */
#define	PCIC98_BUSY		0x04	/* Busy state (dubious? ready ?) */
#define	PCIC98_VCC3P3V		0x02	/* Vcc 1:3.3V 0:5.0V */

/* PCIC98_REG3 */
#define	PCIC98_INT0		(0xf8 + 0x0)	/* INT0(IRQ3) */
#define	PCIC98_INT1		(0xf8 + 0x1)	/* INT1(IRQ5) */
#define	PCIC98_INT2		(0xf8 + 0x2)	/* INT2(IRQ6) */
#define	PCIC98_INT4		(0xf8 + 0x4)	/* INT4(IRQ10) */
#define	PCIC98_INT5		(0xf8 + 0x5)	/* INT5(IRQ12) */
#define	PCIC98_INTDISABLE	(0xf8 + 0x7)	/* disable interrupt */

/* PCIC98_REG6 */
#define	PCIC98_ATTRMEM		0x20	/* 1:attr mem 0:common mem */
#define	PCIC98_VPP12V		0x10	/* Vpp 0:5V 1:12V */

/* Abstract register offsets */
#define	nepc_reg0	0
#define	nepc_reg1	1
#define	nepc_reg2	2
#define	nepc_reg3	3
#define	nepc_reg4	4
#define	nepc_reg5	5
#define	nepc_reg6	6
#define	nepc_reg_winsel	7
#define	nepc_reg_pagofs	8
#endif	/* !_NEPCREG_H_ */
