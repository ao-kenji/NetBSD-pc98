/*	$NecBSD: fdreg.h,v 3.15 1998/03/14 07:08:23 kmatsuda Exp $	*/
/*	$NetBSD: fdreg.h,v 1.10 1997/09/07 13:03:10 mycroft Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)fdreg.h	7.1 (Berkeley) 5/9/91
 */

/*
 * AT floppy controller registers and bitfields
 */

/* uses NEC765 controller */
#include <dev/ic/nec765reg.h>

/* registers */
#ifdef	ORIGINAL_CODE
#define	fdout	2	/* Digital Output Register (W) */
#define	FDO_FDSEL	0x03	/*  floppy device select */
#define	FDO_FRST	0x04	/*  floppy controller reset */
#define	FDO_FDMAEN	0x08	/*  enable floppy DMA and Interrupt */
#define	FDO_MOEN(n)	((1 << n) * 0x10)	/* motor enable */
#endif	/* !PC-98 */

#ifdef	ORIGINAL_CODE
#define	fdsts	4	/* NEC 765 Main Status Register (R) */
#define	fddata	5	/* NEC 765 Data Register (R/W) */
#else	/* PC-98 */
#define	fdsts	0	/* NEC 765 Main Status Register (R) */
#define	fddata	1	/* NEC 765 Data Register (R/W) */
#endif	/* PC-98 */

#ifdef	ORIGINAL_CODE
#define	fdctl	7	/* Control Register (W) */
#else	/* PC-98 */
#define	fdctl	2	/* FD Control Register */
#endif	/* PC-98 */
#define	FDC_500KBPS	0x00	/* 500KBPS MFM drive transfer rate */
#define	FDC_300KBPS	0x01	/* 300KBPS MFM drive transfer rate */
#define	FDC_250KBPS	0x02	/* 250KBPS MFM drive transfer rate */
#define	FDC_125KBPS	0x03	/* 125KBPS FM drive transfer rate */

#ifdef	ORIGINAL_CODE
#define	fdin	7	/* Digital Input Register (R) */
#define	FDI_DCHG	0x80	/* diskette has been changed */
#endif	/* !PC-98 */

#define	FDC_BSIZE	512
#define	FDC_NPORT	8
#define	FDC_MAXIOSIZE	MAXBSIZE

#ifndef ORIGINAL_CODE
#define	FDC_RST		0x80	/*  FDC RESET */
#define	FDC_RDY		0x40	/*  force READY */
#define	FDC_DD		0x20	/*  FDD Mode Exchange 0:1M 1:640K */
#define	FDC_DMAE	0x10	/*  enable floppy DMA */
#define	FDC_MTON	0x08	/*  MOTOR ON (when EMTON=1)*/
#define	FDC_RESET	(FDC_RST | FDC_RDY | FDC_DMAE | FDC_MTON)
#define	FDP_EMTON	0x04	/*  enable MTON */
#define	FDP_FDDEXC	0x02	/*  FDD Mode Exchange 1:1M 0:640K */
#define	FDP_PORTEXC	0x01	/*  PORT Exchane 1:1M 0:640K */
#endif	/* PC-98 */

/*
 * Copyright (C) 1992-1994 by Joerg Wunsch, Dresden
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * From: FreeBSD: Id: ioctl_fd.h,v 1.7 1994/10/30 19:17:39 joerg Exp 
 */

#define FD_MAX_NSEC 36		/* highest known number of spt - allow for */
				/* 2.88 MB drives */

struct ne7_fd_formb {
	int cyl, head;
	int transfer_rate;	/* fdreg.h: FDC_???KBPS */

	union {
		struct fd_form_data {
			/*
			 * DO NOT CHANGE THE LAYOUT OF THIS STRUCTS
			 * it is hardware-dependant since it exactly
			 * matches the byte sequence to write to FDC
			 * during its `format track' operation
			 */
			u_char secshift; /* 0 -> 128, ...; usually 2 -> 512 */
			u_char nsecs;	/* must be <= FD_MAX_NSEC */
			u_char gaplen;	/* GAP 3 length; usually 84 */
			u_char fillbyte; /* usually 0xf6 */
			struct fd_idfield_data {
				/*
				 * data to write into id fields;
				 * for obscure formats, they mustn't match
				 * the real values (but mostly do)
				 */
				u_char cylno;	/* 0 thru 79 (or 39) */
				u_char headno;	/* 0, or 1 */
				u_char secno;	/* starting at 1! */
				u_char secsize;	/* usually 2 */
			} idfields[FD_MAX_NSEC]; /* 0 <= idx < nsecs used */
		} structured;
		u_char raw[1];	/* to have continuous indexed access */
	} format_info;
};

/* make life easier */
# define fd_formb_secshift   format_info.structured.secshift
# define fd_formb_nsecs      format_info.structured.nsecs
# define fd_formb_gaplen     format_info.structured.gaplen
# define fd_formb_fillbyte   format_info.structured.fillbyte
/* these data must be filled in for(i = 0; i < fd_formb_nsecs; i++) */
# define fd_formb_cylno(i)   format_info.structured.idfields[i].cylno
# define fd_formb_headno(i)  format_info.structured.idfields[i].headno
# define fd_formb_secno(i)   format_info.structured.idfields[i].secno
# define fd_formb_secsize(i) format_info.structured.idfields[i].secsize


