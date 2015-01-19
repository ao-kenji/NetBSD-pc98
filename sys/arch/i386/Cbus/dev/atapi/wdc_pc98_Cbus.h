/*	$NecBSD: wdc_pc98_Cbus.h,v 1.3 1999/07/09 05:51:47 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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
 * Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef	_WDC_PC98_CBUS_H_
#define	_WDC_PC98_CBUS_H_

#include <machine/bus.h>
#include <machine/dvcfg.h>

struct wdc_pc98_hw {
	struct wdc_hw wdc_hw;			/* common hardware defs */

	/* PC98 specific staff */
	bus_size_t hw_iosz;			/* io size */
	int hw_nmaps;				/* map structures */

	bus_space_iat_t hw_iat0;		/* register map */
	size_t hw_ofs0;				/* offset */
	size_t hw_iat0sz;			/* register map sz */

	bus_space_iat_t hw_iat1;		/* register map */
	size_t hw_ofs1;				/* offset */
	size_t hw_iat1sz;			/* register map sz */
}; 

/* PCIDE: memory buffer control */ 
#define	WDMC_BUFSZ	NBPG
#define	WDMC_MEMMAP	0x01		/* map a memory buffer in cpu space */
#define	WDMC_BUSY	0x04		/* buffer access busy */
#define	WDMC_MEMRW	0x10		/* memory buffer rw enable */
#define	WDMC_MEMTRICW	0x20		/* 24 bits tricky access (Whao)! */

/* Ninja: memory buffer control */
#define	WDMC_WB_BUFSZ	NBPG
#define	WDMC_WB_FIFOEN	0x08		/* fifo enable */
#define	WDMC_WB_BUS32	0x04		/* 32bits access */
#define	WDMC_WB_CORER	0x01		/* select core chip registers */
#define	WDMC_WB_SMITR	0x00		/* select smit chip registers */
#define	WDMC_WB_SMITEN	0x10		/* smit enable */
#define	WDMC_WB_COREEN	0x40		/* core enable */
#define	WDMC_WB_ENABLE	(WDMC_WB_COREEN | WDMC_WB_SMITEN)

/* KXL: memory buffer control */
#define	WDMC_KXL_SMITR	0x00		/* select smit chip registers */
#define	WDMC_KXL_CORER	0x01		/* select core chip registers */
#define	WDMC_KXL_BUS32	0x02		/* 32bits access */
#define	WDMC_KXL_SMITEN	0x10		/* smit enable */
#define	WDMC_KXL_COREEN	0x40		/* core enable */
#define	WDMC_KXL_RST	0x80		/* chip reset  */
#define	WDMC_KXL_ENABLE	(WDMC_KXL_COREEN | WDMC_KXL_SMITEN)

/* IDE98 known registers (dubious) */
#define	ide98_bsr	0x00		/* Bank Select Register */
#define	BSR_CH0_RACC	0x01
#define	BSR_CH1_RACC	0x02
#define	BSR_CH0_WACC	0x04
#define	BSR_CH1_WACC	0x08
#define	ide98_mbsr	0x83		/* Memory Bank Select Register */
#define	MBSR_ROM1	0x01
#define	MBSR_R0M2	0x02
#define	MBSR_BM		0x80
#define	ide98_iomr	0x84		/* IO Method Register */
#define	IOM_ENA		0x01
#define	IOM_MEM		0x14
#define IOM_MEMA	0x24
#define	ide98_mwcr	0x85		/* Mem Weight Ctrl Register */
#define	MWCR_READ	0x01
#define	MWCR_WRITE	0x08
#define	ide98_scr	0x86		/* Serial Com Register */
#define	ide98_rbpcr	0x87		/* Read Buffer Pointer Control Reg */
#define	ide98_rblr	0x88		/* Read Buffer Limit Register */
#define	RBLR_LMASK	0x7f
#define	RBLR_RDIS	0x80
#define	IDE98_OFFSET	0x900		/* memory buffer offset (bios area) */

/* misc NEC */
#define	NEC_BANK_PADDR	0x432
#define	NEC_IDE_PADDR	0x640

/* io size */
#define	WDC_IBM_IO0SZ	8
#define	WDC_IBM_IO1SZ	2

extern struct dvcfg_hwsel wdc_pc98_hwsel;
#endif	/* !_WDC_PC98_CBUS_H_ */
