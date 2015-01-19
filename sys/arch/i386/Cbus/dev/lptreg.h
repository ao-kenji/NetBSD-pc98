/*	$NecBSD: lptreg.h,v 1.5 1998/03/14 07:04:29 kmatsuda Exp $	*/
/*	$NetBSD: lptreg.h,v 1.5 1996/11/23 23:22:50 cgd Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)lptreg.h	1.1 (Berkeley) 12/19/90
 */
#ifndef	ORIGINAL_CODE
/*
 * lpt.c dirver for PC-9821 series (after A{p,s}2).
 *	written by N. Honda.
 * Merged into NetBSD-1.0 (Rel) by K. Matsuda, Oct 31, 1994.
 * Small clean up by K. Matsuda, Dec 31, 1994.
 *
 * Import to NetBSD 1.0A (Mar 04, 1995 tarballs) by K. Matsuda,
 * Mar 09, 1995.
 * Updated Mar 25, 1995 current base by K. Matsuda, Mar 27, 1995.
 * Updated Apr 08, 1995 current base by K. Matsuda, Apr 10, 1995.
 *
 * Merge new and old lpt by N. Honda.
 * Clean up by N. Honda.
 */
#endif	/* PC-98 */

/*
 * AT Parallel Port (for lineprinter)
 * Interface port and bit definitions
 * Written by William Jolitz 12/18/90
 * Copyright (C) William Jolitz 1990
 */

#define	lpt_data	0	/* Data to/from printer (R/W) */

#define	lpt_status	1	/* Status of printer (R) */
#define	LPS_NERR		0x08	/* printer no error */
#define	LPS_SELECT		0x10	/* printer selected */
#define	LPS_NOPAPER		0x20	/* printer out of paper */
#define	LPS_NACK		0x40	/* printer no ack of data */
#define	LPS_NBSY		0x80	/* printer no ack of data */

#define	lpt_control	2	/* Control printer (R/W) */
#define	LPC_STROBE		0x01	/* strobe data to printer */
#define	LPC_AUTOLF		0x02	/* automatic linefeed */
#define	LPC_NINIT		0x04	/* initialize printer */
#define	LPC_SELECT		0x08	/* printer selected */
#define	LPC_IENABLE		0x10	/* printer out of paper */

#ifdef	ORIGINAL_CODE
#define	LPT_NPORTS	4
#else	/* PC-98 */
#define	LPT_NPORTS	8
#endif	/* PC-98 */

#ifndef	ORIGINAL_CODE
/* old interface */
#define	LPS_O_NBSY	0x4

#define	IO_LPT_PORTC	0x37
#define	LPC_EN_PSTB	0xc	/* PSTB enable */
#define	LPC_DIS_PSTB	0xd	/* PSTB disable */

#define	LPC_MODE8255	0x82	/* 8255 mode */
#define	LPC_IRQ8	0x6	/* IRQ8 active */
#define	LPC_NIRQ8	0x7	/* IRQ8 inactive */
#define	LPC_PSTB	0xe	/* PSTB active */
#define	LPC_NPSTB	0xf	/* PSTB inactive */

/* new interface */
#define	LPTM_EXT	0x10

#define	LPTEM_MASK	0xe0
#define	LPTEM_STAND	0
#define	LPTEM_PS2	0x20
#define	LPTEM_FIFO	0x40
#define	LPTEM_ECP	0x80
#define	LPTEM_CONFIG	0xe0
#define	LPTEM_EINTR	0x10
#define	LPTEM_EDMA	0x08
#define	LPTEM_FIFOFUL	0x02
#define	LPTEM_FIFOEMP	0x01
#endif	/* PC-98 */
