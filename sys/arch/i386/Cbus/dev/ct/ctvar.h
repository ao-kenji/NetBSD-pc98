/*	$NecBSD: ctvar.h,v 1.4 1999/04/15 01:36:13 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
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

#ifndef	_CTVAR_H_
#define	_CTVAR_H_
/*
 * ctvar.h
 * Generic wd33c93 chip driver's definitions
 */

/*****************************************************************
 * Host adapter structure
 *****************************************************************/
struct ct_softc {
	struct scsi_low_softc sc_sclow;		/* generic data */

	bus_space_tag_t sc_iot;			/* core chip ctrl port tag */
	bus_space_tag_t sc_delayt;		/* delay port tag */
	bus_space_tag_t sc_datat;		/* data port tag (pio) */
	bus_space_tag_t sc_memt;		/* data port tag (shm) */
	bus_dma_tag_t sc_dmat;			/* data DMA tag */

	bus_space_handle_t sc_ioh;
	bus_space_handle_t sc_delaybah;
	bus_space_handle_t sc_datah;
	bus_space_handle_t sc_memh;

	void *sc_ih;
	int sc_wc;			/* weight counter */

	int sc_chiprev;			/* chip version */	
#define	CT_WD33C93_A			0x00000
#define	CT_WD33C93_B			0x10000
#define	CT_WD33C93_C			0x20000

	int sc_xmode;
#define	CT_XMODE_PIO			1
#define	CT_XMODE_DMA			2

	int sc_dma;			/* dma transfer start */
#define	CT_DMA_PIOSTART			1
#define	CT_DMA_DMASTART			2

	int sc_satgo;			/* combination cmd start */
#define	CT_SAT_GOING			1

	int sc_atten;			/* attention */
	u_int8_t sc_creg;		/* control register value */

	int sc_chipclk;			/* chipclk 0, 10, 15, 20 */
	struct ct_synch_data {
		u_int cs_period;
		u_int cs_syncr;
	} *sc_sdp;			/* synchronous data table pt */

	/*
	 * Machdep stuff.
	 */
	void *ct_hw;			/* point to bshw_softc etc ... */
	void (*ct_dma_xfer_start) __P((struct ct_softc *));
	void (*ct_pio_xfer_start) __P((struct ct_softc *));
	void (*ct_dma_xfer_stop) __P((struct ct_softc *));
	void (*ct_pio_xfer_stop) __P((struct ct_softc *));
	void (*ct_bus_reset) __P((struct ct_softc *));
	void (*ct_synch_setup) __P((struct ct_softc *, struct lun_info *));
};

/*****************************************************************
 * Lun information 
 *****************************************************************/
struct ct_lun_info {
	struct lun_info cli_li;
	
	u_int8_t cli_syncreg;
};

/*****************************************************************
 * PROTO
 *****************************************************************/
#include <i386/Cbus/dev/ct/ct_machdep.h>

int ctprobesubr __P((bus_space_tag_t, bus_space_handle_t ioh, u_int, int, u_int));
void ctattachsubr __P((struct ct_softc *));
int ctprint __P((void *, const char *));
int ctintr __P((void *));
#endif	/* !_CTVAR_H_ */
