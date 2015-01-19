/*	$NecBSD: cpureg.h,v 1.13 1999/07/09 05:52:17 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Kouichi Matsuda. All rights reserved.
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
 * XXX: should be in i386/specialreg.h
 */

#ifndef _I386_CPUREG_H_
#define _I386_CPUREG_H_

/*
 * Cyrix Cx486S/S2/D/D2/DX/DX2 and 5x86 special registers,
 * accessable as IO ports.
 *
 * Only Cyrix 5x86 have PCR0, CCR4 and do not have CCR1_N_LOCK.
 * Cyrix Cx486S/S2/D/D2/DX/DX2 have CCR1_RPL same as Cx486SLC/DLC.
 *
 */
#define	PCR0		0x20	/* performance control register 2 */
#define	PCR0_RSTK_EN	0x01	/* return stack enable */
#define	PCR0_BTB_EN	0x02	/* branch target buffer enable */
#define	PCR0_LOOP_EN	0x04	/* loop enable */
#define	PCR0_LSSER	0x80	/* load store serialize enable (reorder disable) */

#define	CCR1_USE_SMI	0x02	/* enables SMM pins (SMI# I/O pin and SMADS# output pin) */
#define	CCR1_SMAC	0x04	/* enables SMM memory accesses with SMADS# active */
#define	CCR1_MMAC	0x08	/* enables main memory accesses when CCR1.SMAC = 1 */
#define	CCR1_N_LOCK	0x10	/* Cx486DX/DX2: negate LOCK# */

#define	CCR2		0xc2	/* configuration control register 2 */
#define	CCR2_USE_WBAK	0x02	/* enables WB cache pins (INVAL, WM_RST, HITM#) */
#define	CCR2_LOCK_NW	0x04	/* prohibits changing the state of the CR0.NW bit */
#define	CCR2_SUSP_HALT	0x08	/* enables entering suspend mode on HLT instructions */
#define	CCR2_WT1	0x10	/* caching for 640K..1M area
				 * 1=force all writes to 640K..1M area that hit
				 *   in cache issued on the external bus
				 * 0=disabled
				 */
#define	CCR2_BWRT	0x40	/* enable (16byte WB) burst write cycle */
#define	CCR2_USE_SUSP	0x80	/* enables SUSP# input pin and SUSPA# output pin */

#define	CCR3		0xc3	/* configuration control register 3 */
#define	CCR3_SMI_LOCK	0x01	/* SMM register lock
				 * 1=CCR1.bit3..1 and CCR3.bit1 can't be changed
				 *   in SMM; CCR3.bit0 can be changed in SMM;
				 *   only RESET clears it.
				 * 0=disabled
				 */
#define	CCR3_NMI_EN	0x02	/* enables NMI during SMM */
#define	CCR3_LINBRST	0x04	/* enables linear address sequence for burst cycles */
#define	CCR3_SMM_MODE	0x08	/* enables Intel compatible SMM (i486SL) */
#define	CCR3_MAPEN0	0x10	/* selects active control register set for 0xd0..0xfd */
#define	CCR3_MAPEN1	0x20	/* selects active control register set for 0xd0..0xfd */
#define	CCR3_MAPEN2	0x40	/* selects active control register set for 0xd0..0xfd */
#define	CCR3_MAPEN3	0x80	/* selects active control register set for 0xd0..0xfd */

#define	CCR4		0xe8	/* configuration control register 4 */
#define	CCR4_IORT0	0x01	/* I/O recovery time */
#define	CCR4_IORT1	0x02	/* I/O recovery time */
#define	CCR4_IORT2	0x04	/* I/O recovery time */
#define	CCR4_MEM_BYP	0x08	/* enables memory bypassing */
#define	CCR4_DTE_EN	0x10	/* enables directory table entry cache */
#define	CCR4_FP_FAST	0x20	/* enables fast FPU exception reporting */

#define	DIR0		0xfe	/* device identification register 1 */
#define	DIR1		0xff	/* device identification register 2
				 * (not on Cyrix Cx486S A-step processors
				 *  but on newer Cx486/SLC/DLC)
				 *	bit7..4: processor revision
				 *	bit3..0: processor stepping
				 */

#define	CPUMSR_K6_WHCR	((register_t) (0xc0000082))
#define	K6_WHCR_WAE15M	0x0001
#define	K6_WHCR_WAELIM	0x00fe
#define	K6_WHCR_PAGESZ	(NBPG * 1024)

#define	CPUMSR_K6_3_WHCR	((register_t) (0xc0000082))
#define	K6_3_WHCR_WAE15M 0x0100
#define	K6_3_WHCR_SHIFT	 22
#define	K6_3_WHCR_WAELIM ((u_long) (~((1 << K6_3_WHCR_SHIFT) - 1)))
#define	K6_3_WHCR_PAGESZ (NBPG * 1024)

#define	CPUMSR_K6_TR12	((register_t) (0x0000000e))
#define	K6_TR12_CI	0x0008

#define	K5_WT_ALLOC_TME	0x40000
#define	K5_WT_ALLOC_PRE	0x20000
#define	K5_WT_ALLOC_FRE	0x10000

#define	C2_MCR_BASE	0x110
#define	C2_MCR_0	C2_MCR_BASE
#define	C2_MCR_1	(C2_MCR_BASE + 1)
#define	C2_MCR_2	(C2_MCR_BASE + 2)
#define	C2_MCR_3	(C2_MCR_BASE + 3)
#define	C2_MCR_4	(C2_MCR_BASE + 4)
#define	C2_MCR_5	(C2_MCR_BASE + 5)
#define	C2_MCR_6	(C2_MCR_BASE + 6)
#define	C2_MCR_7	(C2_MCR_BASE + 7)
#define	C2_MCR_ATTR_WC		0x0001
#define	C2_MCR_ATTR_NC		0x0002
#define	C2_MCR_ATTR_WWO		0x0008
#define	C2_MCR_ATTR_WRO		0x0010

#define	C2_MCR_CTRL		0x120
#define	C2_MCR_PAGE_MASK	0x0fff
#define	C2_MCR_CTRL_WC		0x000f
#define	C2_MCR_CTRL_WWO		0x0010
#define	C2_MCR_CTRL_TRT		0x0040
#define	C2_MCR_CTRL_IMASK	0xe0000
#define	C2_MCR_CTRL_ITRT	0x20000
#endif /* !_I386_CPUREG_H_ */
