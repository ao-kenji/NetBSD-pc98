/*	$NecBSD: wdcreg.h,v 1.6 1998/10/18 23:50:01 honda Exp $	*/
/*	$NetBSD: wdreg.h,v 1.13 1995/03/29 21:56:46 briggs Exp $	*/

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
 *	@(#)wdreg.h	7.1 (Berkeley) 5/9/91
 */

/*
 * Control bits.
 */
#define WDCTL_4BIT	0x08	/* use four head bits (wd1003) */
#define WDCTL_RST	0x04	/* reset the controller */
#define WDCTL_IDS	0x02	/* disable controller interrupts */

/*
 * Status bits.
 */
#define	WDCS_BSY	0x80	/* busy */
#define	WDCS_DRDY	0x40	/* drive ready */
#define	WDCS_DWF	0x20	/* drive write fault */
#define	WDCS_DSC	0x10	/* drive seek complete */
#define	WDCS_DRQ	0x08	/* data request */
#define	WDCS_CORR	0x04	/* corrected data */
#define	WDCS_IDX	0x02	/* index */
#define	WDCS_ERR	0x01	/* error */
#define WDCS_BITS	"\020\010bsy\007drdy\006dwf\005dsc\004drq\003corr\002idx\001err"

/*
 * Error bits.
 */
#define	WDCE_BBK	0x80	/* bad block detected */
#define	WDCE_UNC	0x40	/* uncorrectable data error */
#define	WDCE_MC		0x20	/* media changed */
#define	WDCE_IDNF	0x10	/* id not found */
#define	WDCE_ABRT	0x08	/* aborted command */
#define	WDCE_MCR	0x04	/* media change requested */
#define	WDCE_TK0NF	0x02	/* track 0 not found */
#define	WDCE_AMNF	0x01	/* address mark not found */
#define WDERR_BITS	"\020\010bbk\007unc\006mc\005idnf\004mcr\003abrt\002tk0nf\001amnf"

/*
 * Status register bits
 */
#define	ARS_CHECK	0x01	/* error occured, see sense key/code */
#define	ARS_CORR	0x04	/* correctable error occured */
#define	ARS_DRQ		0x08	/* data request / ireason valid */
#define	ARS_DSC		0x10	/* immediate operation completed */
#define	ARS_DF		0x20	/* drive fault */
#define	ARS_DRDY	0x40	/* ready to get command */
#define	ARS_BSY		0x80	/* registers busy */
 /* for overlap mode only: */
#define	ARS_SERVICE	0x10	/* service is requested */
#define	ARS_DMARDY	0x20	/* ready to start a DMA transfer */
#define	ARS_BITS	"\20\010busy\7ready\6fault\5opdone\4drq\3corr\1check"

/*
 * Error register bits
 */
#define	AER_ILI			0x01	/* illegal length indication */
#define	AER_EOM			0x02	/* end of media detected */
#define	AER_ABRT		0x04	/* command aborted */
#define	AER_MCR			0x08	/* media change requested */
#define	AER_SKEY		0xf0	/* sense key mask */
#define	AER_SK_NO_SENSE		0x00	/* no spesific sense key info */
#define	AER_SK_RECOVERED_ERROR	0x10	/* command succeeded, data recovered */
#define	AER_SK_NOT_READY	0x20	/* no access to drive */
#define	AER_SK_MEDIUM_ERROR	0x30	/* non-recovered data error */
#define	AER_SK_HARDWARE_ERROR	0x40	/* non-recoverable hardware failure */
#define	AER_SK_ILLEGAL_REQUEST	0x50	/* invalid command parameter(s) */
#define	AER_SK_UNIT_ATTENTION	0x60	/* media changed */
#define	AER_SK_DATA_PROTECT	0x70	/* reading read-protected sector */
#define	AER_SK_ABORTED_COMMAND	0xb0	/* command aborted, try again */
#define	AER_SK_MISCOMPARE	0xe0	/* data did not match the medium */
#define	AER_BITS	"\20\4mchg\3abort\2eom\1ili"

/*
 * Feature register bits
 */
#define	ARF_DMA		0x01	/* transfer data via DMA */
#define	ARF_OVERLAP	0x02	/* release the bus until completion */

/*
 * Interrupt reason register bits
 */
#define	ARI_CMD		0x01	/* command(1) or data(0) */
#define	ARI_IN		0x02	/* transfer to(1) or from(0) the host */
#define	ARI_RELEASE	0x04	/* bus released until completion */

/*
 * Commands for Disk Controller.
 */
#define	WDCC_RECAL	0x10	/* disk restore code -- resets cntlr */

#define	WDCC_READ	0x20	/* disk read code */
#define	WDCC_WRITE	0x30	/* disk write code */
#define	WDCC__LONG	0x02	 /* modifier -- access ecc bytes */
#define	WDCC__NORETRY	0x01	 /* modifier -- no retrys */

#define	WDCC_FORMAT	0x50	/* disk format code */
#define	WDCC_DIAGNOSE	0x90	/* controller diagnostic */
#define	WDCC_IDP	0x91	/* initialize drive parameters */

#define	WDCC_READMULTI	0xc4	/* read multiple */
#define	WDCC_WRITEMULTI	0xc5	/* write multiple */
#define	WDCC_SETMULTI	0xc6	/* set multiple mode */

#define	WDCC_READDMA	0xc8	/* read with DMA */
#define	WDCC_WRITEDMA	0xca	/* write with DMA */

#define	WDCC_ACKMC	0xdb	/* acknowledge media change */
#define	WDCC_LOCK	0xde	/* lock drawer */
#define	WDCC_UNLOCK	0xdf	/* unlock drawer */
#define	WDCC_FORCE	0x100	/* do not check RDYBSY! */
#define	WDCC_CMDMASK	0xff

#define	WDCC_POWSAVE	0xe3	/* auto spin down */
#define	WDCC_IDENTIFY	0xec	/* read parameters from controller */
#define	WDCC_FEATURES	0xef	/* feature */
#define	FEAT_RCACHE_ON	0xaa	/* read cache enable */
#define	FEAT_RCACHE_OFF	0x55
#define FEAT_WCACHE_ON	0x02	/* write cache enable */
#define	FEAT_WCACHE_OFF	0x82
#define	FEAT_SET_MODE	0x03

#define	WDSD_IBM	0xa0	/* forced to 512 byte sector, ecc */
#define	WDSD_CHS	0x00	/* cylinder/head/sector addressing */
#define	WDSD_LBA	0x40	/* logical block addressing */
