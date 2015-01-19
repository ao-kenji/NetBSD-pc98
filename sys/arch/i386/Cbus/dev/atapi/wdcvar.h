/*	$NecBSD: wdcvar.h,v 1.31 1999/07/23 05:38:58 kmatsuda Exp $	*/
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
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef	_WDCVAR_H_
#define	_WDCVAR_H_
#include <machine/bus.h>
#include <dev/ata/atavar.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>

struct channel_softc;
struct channel_link;
struct wdc_hw;

struct atabus_softc {
	struct device sc_dev;

	u_int sc_maxunit;
	u_int32_t sc_attach;

#define	ATABUS_DRIVE_PER_BANK	2
#define	ATABUS_MAXBANK		2
#define	ATABUS_MAXCHAN	(ATABUS_DRIVE_PER_BANK * ATABUS_MAXBANK)
	struct channel_link *sc_idec[ATABUS_MAXCHAN];
	int sc_bank;
	int sc_nbus;
};

TAILQ_HEAD(drivehead, channel_link);
struct channel_queue {
	struct drivehead cq_drives;
	struct drivehead cq_link;
	
#define	CQ_IDLE		0x0000
#define	CQ_BUSY		0x0001
#define	CQ_WANTED	0x0010
#define	CQ_SYNC		0x0020
	int cq_state;
};

struct wdc_hwfuncs {
	void (*open) __P((struct channel_softc *));
	void (*close) __P((struct channel_softc *));

	void (*banksel) __P((struct channel_softc *, u_int));

	u_int8_t (*cr0_read) __P((struct channel_softc *, bus_addr_t));
	void (*cr0_write) __P((struct channel_softc *, bus_addr_t, u_int8_t));

	u_int8_t (*cr1_read) __P((struct channel_softc *, bus_addr_t));
	void (*cr1_write) __P((struct channel_softc *, bus_addr_t, u_int8_t));

	void (*data_read) __P((struct channel_softc *, u_int8_t *, u_int, int));
	void (*data_write) __P((struct channel_softc *, u_int8_t *, u_int, int));
};

struct wdc_softc {
	struct device sc_dev;
	void *sc_ih;

	struct scsipi_adapter sc_adapter;
	int nchannels;
	struct channel_softc *channels;	/* channel */

#define	WDC_CAPABILITY_DATA16 0x0001    /* can do  16-bit data access */
#define	WDC_CAPABILITY_DATA32 0x0002    /* can do 32-bit data access */
#define WDC_CAPABILITY_MODE   0x0004	/* controller knows its PIO/DMA modes */
#define	WDC_CAPABILITY_DMA    0x0008	/* DMA */
#define	WDC_CAPABILITY_UDMA   0x0010	/* Ultra-DMA/33 */
#define	WDC_CAPABILITY_HWLOCK 0x0020	/* Needs to lock HW */
#define	WDC_CAPABILITY_ATA_NOSTREAM 0x0040 /* Don't use stream funcs on ATA */
#define	WDC_CAPABILITY_ATAPI_NOSTREAM 0x0080 /* Don't use stream f on ATAPI */
#define WDC_CAPABILITY_NO_EXTRA_RESETS 0x0100 /* only reset once */
	int cap;			/* capability */
	int pio_mode;			/* pio mode */
	int dma_mode;			/* dma mode */
	int udma_mode;			/* udma mode */

#define	WDC_DMA_READ	0x01
#define	WDC_DMA_POLL	0x02
	void *dma_arg;
	int (*dma_init) __P((void *, int, int, void *, size_t, int));
	void (*dma_start) __P((void *, int, int, int));
	int (*dma_finish) __P((void *, int, int, int));
};

struct channel_softc {
	int channel;
	struct wdc_softc *wdc;

	struct ata_drive_datas ch_drive[ATABUS_MAXCHAN];
	int ch_drvbank[ATABUS_MAXCHAN];
	int ch_drvno[ATABUS_MAXCHAN];

	struct atabus_softc *ch_ata;

	bus_space_tag_t cmd_iot;
	bus_space_tag_t ctl_iot;
	bus_space_tag_t data16iot;
	bus_space_tag_t data32iot;

	bus_space_handle_t cmd_ioh;
	bus_space_handle_t ctl_ioh;
	bus_space_handle_t data16ioh;
	bus_space_handle_t data32ioh;

	bus_space_tag_t ch_bkbst;
	bus_space_handle_t ch_bkbsh;

	bus_space_tag_t ch_memt;
	bus_space_handle_t ch_memh;
	vsize_t ch_vsize;

	int ch_delay;
	struct channel_queue	*ch_queue;

	u_int ch_flags;
#define	WDCF_ACTIVE		0x0001  /* timeout call active(expect wdcintr)*/
#define	WDCF_IRQ_WAIT		WDCF_ACTIVE
#define	WDCF_SINGLE		0x0002  /* sector at a time mode */
#define	WDCF_ERROR		0x0004  /* processing a disk error */
#define	WDCF_BRESET		0x0010	/* no hardware reset */
#define WDCF_RESETGO		0x0100
#define	WDCF_REMOVAL		0x0200
#define	WDCF_DMASTART		0x1000	/* dma start */

	int ch_bank;			/* has bank switch register */
	int ch_errors;			/* errors during current transfer */
	int ch_dma;			/* dma pass flags */
	u_int8_t ch_status;		/* copy of status register */
	u_int8_t ch_error;		/* copy of error register */

#define	MODE_IDE	0
#define	MODE_ATAPI	1
#define	MODE_WATAPI	2
	u_int ch_interface;		/* current interface mode */
	struct wdc_hw *ch_hw;		/* port select */
	struct wdc_hwfuncs ch_hwfuncs;	/* hw interface functions */
};

/*
 * Linkage info.
 */
#define	WDCLINK_DONE		0	/* current job done, other jobs left */
#define	WDCLINK_RELDONE		1	/* current job done, no jobs left */
#define	WDCLINK_RESTART		2	/* try again withiout sc_errors++ */
#define	WDCLINK_CONTINUE	3	/* current job under process */
#define	WDCLINK_RESET		4	/* reset asserted, continue.. */
#define	WDCLINK_ERROR_RETRY	5	/* try again with sc_errrors++ */

#define	WDC_LINK_FUNC(idec, funcs) ((idec)->wk_funcs->funcs)
#define	WDC_LINK_CALL(idec, funcs) ((*WDC_LINK_FUNC((idec), funcs))((idec)))

struct channel_link_funcs {
	int (*wk_startfunc) __P((struct channel_link *));
	int (*wk_intrfunc) __P((void *));
	int (*wk_done) __P((struct channel_link *));
	int (*wk_timeout) __P((struct channel_link *));
	int (*wk_activate) __P((struct channel_link *));
	int (*wk_deactivate) __P((struct channel_link *));
};

struct channel_link_timeout_count {
	u_int wk_active_tc;
	u_int wk_inactive_tc;
	u_int wk_timeout_tc;
};

struct channel_link {
	struct channel_softc *wk_chp; 		/* my controller */
	struct ata_drive_datas *wk_drvp;

	u_int wk_chan; 				/* channel */

	TAILQ_ENTRY(channel_link) wk_linkchain;	/* link chain */
	TAILQ_ENTRY(channel_link) wk_drivechain; 	/* io chain */

	struct atabus_softc *wk_ata; 		/* atabus interface softc */
	void *wk_isc; 				/* interface softc */
	int wk_interface; 			/* interface mode */
	struct channel_link_funcs *wk_funcs; 	/* interface functions */

#define	DRIVE_QUEUED	0x0001
#define	DRIVE_HASJOB	0x0002
#define	DRIVE_INACTIVE	0x0004
	u_int wk_flags; 			/* control flags */
	struct channel_link_timeout_count wk_tc;	/* timeout counter */

	void *wk_ats;				/* shutdown hook */
};

#define	WDC_DRIVE_HAS_NOJOB(idec)	((idec)->wk_flags &= ~DRIVE_HASJOB)
#define	WDC_DRIVE_HAS_JOB(idec)		((idec)->wk_flags |= DRIVE_HASJOB)

/*
 * Attach args
 */
struct wdc_attach_args {
	struct channel_softc *wa_chp;
	int wa_maxunit;
	int wa_bank;
};

/*
 * Functions
 */
void wdc_init_channel_softc __P((struct channel_softc *, struct wdc_hw *));

void atabusscan __P((struct channel_softc *));

int wdcprobe __P((struct channel_softc *));
void wdcattach __P((struct channel_softc *));
int wdcprobesubr __P((struct channel_softc *));
void wdcattachsubr __P((struct channel_softc *));

int wdc_inquire_idedrv __P((struct channel_softc *, int));
int wdc_inquire_device __P((struct channel_softc *, int));

int wdc_activate __P((struct channel_softc *));
int wdc_deactivate __P((struct channel_softc *));
int wdc_is_busy __P((struct channel_softc *));

int wdcintr __P((void *));
void wdcerror __P((struct channel_link *, char *));
void wdccmdstart __P((struct channel_link *));
int wdcsyncstart __P((struct channel_link *));
void wdcsyncdone __P((struct channel_link *));

int wdccommand __P((struct channel_link *, int, int, int, int, int, int));
int wdccommandshort __P((struct channel_softc *, int, int));
int wdcsetfeature __P((struct channel_softc *, int, int, int, int));
int wdc_set_mode __P((struct channel_link *, int, int, int));
int wdc_exec_cmd __P((struct channel_link *, int));

int wdc_load_params __P((struct channel_link *, struct ataparams *));
void wdc_probe_and_setup_caps __P((struct channel_link *, const char *));
void wdc_setup_hardware_caps __P((struct channel_softc *, struct wdc_hw *));

int wdcwait __P((struct channel_softc *, int));
int wdcwaitPhase __P((struct channel_softc *, int, u_char));
u_int8_t wdcPhase __P((struct channel_softc *));
#endif	/* !_WDCVAR_H_ */
