/*	$NecBSD: i82365reg.h,v 1.4 1998/03/14 07:06:13 kmatsuda Exp $	*/
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

#ifndef	_I82365REG_H_
#define	_I82365REG_H_
/* XXX
 * should change registers names.
 * should complete the registers lists and definitions.
 */

#define	i82365_index	0		/* i82365 index reg offset */
#define	i82365_data	1		/* i28365 data reg offset */

#define	REG_ID		0		/* id register offset */
#define	IDR_IIDM	0xc0
#define	IDR_REVM	0x0f

#define	REG_STAT	1		/* status register offset */
#define	STAT_VPPV	0x80
#define	STAT_POWON	0x40
#define	STAT_RDY	0x20
#define STAT_CD		0x0c

#define	REG_POW		2
#define POWR_VPP5V	0x01
#define	POWR_VPP12V	0x02
#define	POWR_VCC5V	0x10
#define	POWR_AUTOPOW	0x20
#define	POWR_RESDRV	0x40
#define	POWR_OUTEN	0x80

#define	REG_IGC		3
#define IGCR_IRQM	0x0f
#define IGCR_INTR	0x10
#define IGCR_IOCARD	0x20
#define IGCR_DISRST	0x40
#define IGCR_RE		0x80

#define	REG_CS		4
#define CS_RDY		0x04
#define	CS_CCHG		0x08

#define	REG_CCS		5
#define	CCS_ANY		0x0f

#define	REG_WE		6
#define WE_MEMBASE	0x01
#define	WE_MEMMASK	0x1f
#define WE_MEMCS16	0x20
#define WE_IOBASE	0x40
#define WE_IOMASK	0xc0

#define	REG_IOC		7

#define	REG_IOWBASE	0x08		/* io windows register base offset */
#define	REG_MWBASE	0x10		/* mem windows register base offset */

#define	REG_SLOT_SKIP	0x40		/* slot gap */

#define	REG_IO_SLOT_SKIP	REG_SLOT_SKIP
#define	REG_IO_WIN_SKIP		4

#define	REG_MEM_SLOT_SKIP	REG_SLOT_SKIP
#define	REG_MEM_WIN_SKIP	8

/* PD672X special registers */
#define	REG_CIRRUS_MC1		0x16	/* cirrus misc control (1) */
#define	CIRRUS_MC1_INTRDIS	0x04	/* status int disenable */
#define	CIRRUS_MC1_SIGNAL	0x10	/* signal enable */
#define	REG_CIRRUS_FC		0x17	/* cirrus fifo control */
#define	REG_CIRRUS_MC2		0x1e	/* cirrus misc control (2) */
#define	REG_CIRRUS_ID		0x1f	/* cirrus id reg */
#define	REG_CIRRUS_SETUP	0x3a	/* setup time */
#define	REG_CIRRUS_CMD		0x3b	/* cmd time */
#define	REG_CIRRUS_RECOV	0x3c	/* recovery time */
#endif	/* !_I82365REG_H_ */
