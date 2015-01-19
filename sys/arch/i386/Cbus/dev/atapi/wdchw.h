/*	$NecBSD: wdchw.h,v 1.30 1999/01/19 06:55:15 honda Exp $	*/
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

#ifndef	_WDCHW_H_
#define	_WDCHW_H_

#include <machine/bus.h>

struct wdc_hw {
	u_char *hw_name;			/* symbolic name */
#define	WDC_DTYPE_GENERIC	0
#define	WDC_DTYPE_IDE98		1		/* XXX */
	u_int d_subtype;			/* geometric subtype */

#define	WDC_ACCESS_PLAIN	0x0100
#define	WDC_ACCESS_INDEXED	0x0200
#define	WDC_ACCESS_MEM		0x0400
#define	WDC_ACCESS_HASBANK	0x0800
#define	WDC_ACCESS_USER		0x00ff
#define	WDC_ACCESS_BRESET	0x0001
	int hw_access;				/* access type */
	int hw_maxunit;				/* max unit */

	struct wdc_hwfuncs hw_funcs;		/* access methods */

	int hw_capability;			/* capability */
	int hw_pio;
	int hw_dma;
}; 

/* local port defs (relocated) */
#define	wd_data		(0)
#define wd_error	(1)
#define	wd_precomp	(1)
#define	wd_features	(1)
#define	wd_seccnt	(2)
#define	wd_sector	(3)
#define	wd_cyl_lo	(4)
#define	wd_cyl_hi	(5)
#define	wd_sdh		(6)
#define	wd_command	(7)
#define	wd_status	(7)

#define	wd_base		(8)
#define	wd_altsts	(0)
#define	wd_ctlr		(0)
#define	wd_digin	(1)
#define	wd_mctlr	(1)

#define	wd_cridx	(2)
#define	wd_crdata	(3)
#define	wd_edata	(4)

/* bank select macro */
#define	WDC_DRIVE_NO(chp, chan)	((chp)->ch_drvno[chan])
#define	WDC_BANK_NO(chp, chan)	((chp)->ch_drvbank[chan])

#define	WDC_CHAN_TO_BANKNO(chan) ((chan) / ATABUS_DRIVE_PER_BANK)
#define	WDC_CHAN_TO_DRVNO(chan)  ((chan) % ATABUS_DRIVE_PER_BANK)
#define	WDC_BANK_TO_CHAN(bank, drive) ((bank) * ATABUS_DRIVE_PER_BANK + (drive))

#define	WDC_SECOND_BANK		ATABUS_DRIVE_PER_BANK
#define	WDC_BANK_UNKNOWN	(-1)

#define	RECOVERYTIME		(hz / 2) /* time to recover from an error */
#define	WDCDELAY		100
#define	WDCNDELAY		100000	 /* 10 sec */

#define	wait_for_drq(d)		wdcwait(d, WDCS_DRDY | WDCS_DSC | WDCS_DRQ)
#define	wait_for_ready(d)	wdcwait(d, WDCS_DRDY | WDCS_DSC)
#define	wait_for_unbusy(d)	wdcwait(d, 0)

/* common access methods */
u_int8_t wdc_cr0_read_plain __P((struct channel_softc *, bus_addr_t));
void wdc_cr0_write_plain __P((struct channel_softc *, bus_addr_t, u_int8_t));
u_int8_t wdc_cr0_read_indexed __P((struct channel_softc *, bus_addr_t));
void wdc_cr0_write_indexed __P((struct channel_softc *, bus_addr_t, u_int8_t));
u_int8_t wdc_cr1_read_plain __P((struct channel_softc *, bus_addr_t));
void wdc_cr1_write_plain __P((struct channel_softc *, bus_addr_t, u_int8_t));
u_int8_t wdc_cr1_read_indexed __P((struct channel_softc *, bus_addr_t));
void wdc_cr1_write_indexed __P((struct channel_softc *, bus_addr_t, u_int8_t));
void wdc_data_write_generic __P((struct channel_softc *, u_int8_t *, u_int, int));
void wdc_data_read_generic __P((struct channel_softc *, u_int8_t *, u_int, int));

#define	wdc_select_bank(chp, bank) 				\
{								\
	if ((chp)->ch_bank != 0)				\
		((*((chp)->ch_hwfuncs.banksel)) ((chp), (bank)));\
}
#define	wdc_cr0_read(chp, offs) 				\
	((*((chp)->ch_hwfuncs.cr0_read)) ((chp), (offs)))
#define	wdc_cr0_write(chp, offs, val) 				\
	((*((chp)->ch_hwfuncs.cr0_write)) ((chp), (offs), (val)))
#define	wdc_cr1_read(chp, offs) 				\
	((*((chp)->ch_hwfuncs.cr1_read)) ((chp), (offs)))
#define	wdc_cr1_write(chp, offs, val) 				\
	((*((chp)->ch_hwfuncs.cr1_write)) ((chp), (offs), (val)))
#define	wdc_data_read(chp, bufp, cnt, bus)			\
	((*((chp)->ch_hwfuncs.data_read)) ((chp), (bufp), (cnt), (bus)))
#define	wdc_data_write(chp, bufp, cnt, bus)			\
	((*((chp)->ch_hwfuncs.data_write)) ((chp), (bufp), (cnt), (bus)))
#endif	/* !_WDCHW_H_ */
