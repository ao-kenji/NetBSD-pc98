/*	$NecBSD: atapidrv.c,v 1.21 1999/07/31 14:22:15 honda Exp $	*/
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
 * Copyright (C) 1995 Cronyx Ltd.
 * Author Serge Vakulenko, <vak@cronyx.ru>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/lock.h>

#include <vm/vm.h>

#include <machine/physio_proc.h>
#include <i386/Cbus/dev/atapi/wdcreg.h>
#include <i386/Cbus/dev/atapi/atapireg.h>
#include <i386/Cbus/dev/atapi/wdcvar.h>
#include <i386/Cbus/dev/scsi-atapi/atapivar.h>
#include <i386/Cbus/dev/atapi/wdchw.h>
#include <dev/ata/atareg.h>

/*****************************************************
 * CONTROL
 *****************************************************/
#define	ATAPIIORETRIES 3	/* number of retries before giving up */

#define	PIOMODE		0
#define	PIOMODE_WAIT	1
#define	DMAMODE		2
#define	DMAMODE_WAIT	3
#define	READY		4

u_int atapi_debug;

#ifndef DDB
#define Debugger() panic("should call debugger here (atapidrv.c)")
#endif /* ! DDB */

/*****************************************************
 * STRUCTURE
 *****************************************************/
static int wdclink_atapiintr __P((void *));
static int wdclink_atapiwdcstart __P((struct channel_link *));
static int wdclink_atapitimeout __P((struct channel_link *));
static int wdclink_atapiactivate __P((struct channel_link *));

static struct channel_link_funcs channel_link_atapi_funcs = {
	wdclink_atapiwdcstart,
	wdclink_atapiintr,
	NULL,
	wdclink_atapitimeout,
	wdclink_atapiactivate,
	NULL,
};

static int atapi_send_cmd __P((struct channel_link *, struct ataccb *));
static int atapi_start_nexus __P((struct channel_link *, struct ataccb *));
static void atapi_err __P((struct channel_link *, struct ataccb *, char *, u_int));
int atapidone __P((struct channel_link *));

static void atapi_drain_data __P((struct channel_softc *, u_int));
static void atapi_shutdown __P((void *));
static int atapictrl __P((struct channel_link *));
/**********************************************************
 * CCB
 **********************************************************/
/* ccb */
GENERIC_CCB_STATIC_ALLOC(atapi, ataccb)
GENERIC_CCB(atapi, ataccb, ac_cmdchain)

/**********************************************************
 * scsi atapi interface
 **********************************************************/
static int atapi_scsipi_start __P((struct scsipi_xfer *));
void atapi_scsipi_minphys __P((struct buf *));

struct ata_atapi_attach *
atabus_scsipi_establish(ata, chp, aap)
	struct atabus_softc *ata;
	struct channel_softc *chp;
	struct ata_atapi_attach *aap;
{
	struct wdc_softc *wdc = chp->wdc;

	aap->aa_openings = 1;
	wdc->sc_adapter.scsipi_cmd = atapi_scsipi_start;
	wdc->sc_adapter.scsipi_minphys = atapi_scsipi_minphys;
	aap->aa_bus_private = &wdc->sc_adapter;
	return aap;
}

int
wdc_atapi_get_params(ab_link, pdrive, flags, id)
	struct scsipi_link *ab_link;
	u_int8_t pdrive;
	int flags;
	struct ataparams *id;
{
	struct atabus_softc *ata = (void *) ab_link->adapter_softc;
	struct channel_link *idec;

	idec = ata->sc_idec[pdrive];
	if (idec == NULL || idec->wk_interface == MODE_IDE)
		return ESCAPE_NOT_SUPPORTED;	/* XXX */

	if (wdc_load_params(idec, id) != 0)
		printf("%s: can not load params\n", ata->sc_dev.dv_xname);

	return COMPLETE;
}

void
atapi_scsipi_minphys(bp)
	struct buf *bp;
{

	if(bp->b_bcount > MAXPHYS)
		bp->b_bcount = MAXPHYS;
	minphys(bp);
}

int
atapi_scsipi_start(xs)
	struct scsipi_xfer *xs;
{
	struct scsipi_link *sc_link = xs->sc_link;
	struct atabus_softc *ata = (void *) sc_link->adapter_softc;
	struct channel_link *idec;
	struct atapi_drive *adrv;
	struct channel_softc *chp;
	struct ataccb *ac;
	int s;

	idec = ata->sc_idec[sc_link->scsipi_atapi.drive];
	if (idec == NULL)
	{
		printf("atapidrv: idec null (%d)\n", 
			(int) sc_link->scsipi_atapi.drive);
		return TRY_AGAIN_LATER;
	}

	chp = idec->wk_chp;
 	adrv = idec->wk_isc;
#ifdef	DIAGNOSTIC
	if (adrv == NULL)
	{
		printf("atapidrv: adrv null (%d)\n",
		        (int) sc_link->scsipi_atapi.drive);
		return TRY_AGAIN_LATER;
	}
#endif	/* DIAGNOSTIC */

	if ((ac = atapi_get_ccb(xs->flags & SCSI_NOSLEEP)) == NULL)
		return TRY_AGAIN_LATER;
	
	ac->ac_xs = xs;
	ac->ac_pcd = xs->cmd;
	ac->ac_cmdlen = xs->cmdlen;
	ac->ac_bp = xs->bp;
	ac->ac_data = xs->data;
	ac->ac_datalen = xs->datalen;
	ac->ac_flags = 0;
	if (xs->flags & SCSI_DATA_IN)
		ac->ac_flags |= ATAPIF_READ;
	else
		ac->ac_flags |= ATAPIF_WRITE;

	s = splbio();
	TAILQ_INSERT_TAIL(&adrv->av_cmdtab, ac, ac_cmdchain);
	WDC_DRIVE_HAS_JOB(idec);
	if ((xs->flags & SCSI_POLL) == 0)
	{
		wdccmdstart(idec);
		splx(s);
		return SUCCESSFULLY_QUEUED;
	}
	else
	{
		struct channel_queue *cqp = chp->ch_queue;
		int error = WDCLINK_DONE, acqbus = 0;

		ac->ac_flags |= ATAPIF_POLL;
		if ((cqp->cq_state & CQ_BUSY) != 0)
		{
			error = WDCLINK_RESTART;
			ac->ac_result.code = RES_BUSY;
			goto bad;
		}

		error = wdcsyncstart(idec);
		if (error != 0)
		{
			error = WDCLINK_RESTART;
			ac->ac_result.code = RES_BUSY;
			goto bad;
		}

		acqbus = 1;
		error = wdclink_atapiwdcstart(idec);
		while (error == WDCLINK_CONTINUE)
		{
			delay(100);
			error = wdclink_atapiintr(idec);	
		}

bad:
		if (error != WDCLINK_DONE)
		{
	    		chp->ch_errors = ATAPIIORETRIES;
			atapidone(idec);
		}

		if (acqbus != 0)
			wdcsyncdone(idec);
	}
	splx(s);
	return COMPLETE;
}

int
atapidone(idec)
	struct channel_link *idec;
{
	struct atapi_drive *adrv = idec->wk_isc;
	struct ataccb *ac;
	struct scsipi_xfer *xs;
	struct buf *bp;

	if ((ac = adrv->av_cmdtab.tqh_first) == NULL)
	{
		adrv->av_nextph = ATAPI_FREE;
		return WDCLINK_DONE;
	}

	xs = ac->ac_xs;
	bp = ac->ac_bp;

	if (ac->ac_flags & ATAPIF_SENSE)
	{
		if (ac->ac_result.code == RES_OK)
		{
			xs->error = XS_SENSE;
			bcopy(&ac->ac_sense, &xs->sense.scsi_sense,
			      sizeof(xs->sense.scsi_sense));
		}
		else
		{
			xs->error = XS_SHORTSENSE;
			xs->sense.atapi_sense = ac->ac_shortsense;
		}
		goto out;
	}

	if (bp && ac->ac_result.code != RES_OK &&
	    ++ idec->wk_chp->ch_errors < ATAPIIORETRIES)
	{
		struct ata_drive_datas *drvp = idec->wk_drvp;

		printf("%s: retry io\n", drvp->drv_softc->dv_xname);
		ac->ac_result.code = RES_OK;
		return WDCLINK_RESTART;
	}

	switch (ac->ac_result.code)
	{
	case RES_OK:
		xs->resid = 0;
		xs->error = 0;
		break;

	case RES_BUSY:
		xs->error = XS_BUSY;
		break;

	default:
		if (ac->ac_result.error != 0)
		{
			xs->error = XS_SHORTSENSE;
			ac->ac_shortsense = ac->ac_result.error;
			xs->sense.atapi_sense = ac->ac_shortsense;

#ifdef	SCSIPI_ATAPI_SUPPORT_SENSE
			ac->ac_flags |= ATAPIF_SENSE;
			ac->ac_result.code = RES_OK;
			idec->wk_chp->ch_errors = 0;
			return WDCLINK_RESTART;
#endif	/* SCSIPI_ATAPI_SUPPORT_SENSE */
		}
		else
		{
			xs->error = XS_DRIVER_STUFFUP;
		}
		break;
	}

out:
	TAILQ_REMOVE(&adrv->av_cmdtab, ac, ac_cmdchain);
	if (adrv->av_cmdtab.tqh_first == NULL)
		WDC_DRIVE_HAS_NOJOB(idec);

	adrv->av_nextph = ATAPI_FREE;
	atapi_free_ccb(ac);

	xs->flags |= ITSDONE;
	scsipi_done(xs);

	return WDCLINK_DONE;
}

/*****************************************************
 * ACTIVATE
 *****************************************************/
static int atapi_initialize_target __P((struct channel_softc *, u_int));

static int
atapi_initialize_target(chp, chan)
	struct channel_softc *chp;
	u_int chan;
{
	int error = 0;

	error = wdc_reset_atapi(chp, chan);
	if (error)
	{
		printf("atapi restart BOTCH I\n");
		return EIO;
	}

	error = wdc_inquire_device(chp, chan);
	if (error)
	{
		printf("atapi restart BOTCH II\n");
		return EIO;
	}
	return error;
}

static int
wdclink_atapiactivate(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	int error;

	error = atapi_initialize_target(chp, idec->wk_chan);

	drvp->state = 0;
	return 0 /* error */;		/* XXX */
}

static int
wdclink_atapitimeout(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	int error;

	error = atapi_initialize_target(chp, idec->wk_chan);

	drvp->state = 0;
	if (drvp->drive_flags & (DRIVE_UDMA | DRIVE_DMA))
	{
		drvp->drive_flags &= ~(DRIVE_UDMA | DRIVE_DMA);
		printf("atapi abort DMA transfer mode, use PIO transfer\n");
	}
	return error;
}

static void
atapi_shutdown(arg)
	void *arg;
{
	struct channel_link *idec = arg;
	struct channel_softc *chp = idec->wk_chp;

	if (idec->wk_flags & DRIVE_INACTIVE)
		return;

	(void) wdc_reset_atapi(chp, idec->wk_chan);
}
/**********************************************************
 * Sub Functions
 **********************************************************/
static void
atapi_err(idec, ac, s, error)
	struct channel_link *idec;
	struct ataccb *ac;
	char *s;
	u_int error;
{
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	struct atapi_drive *adrv = idec->wk_isc;

	ac->ac_result.code = error;
	ac->ac_result.status = chp->ch_status;
	ac->ac_result.error = (chp->ch_status & ARS_CHECK) ? chp->ch_error : 0;
	if (s)
		printf("%s: %s: err 0x%x stat 0x%x cmd 0x%x expph 0x%x\n",
			drvp->drv_softc->dv_xname, s, (u_int) chp->ch_error,
			(u_int) chp->ch_status, (u_int)ac->ac_pcd->opcode,
			adrv->av_nextph);

	/* clear error state */
	(void) wdc_cr0_read(chp, wd_error);
	(void) wdc_cr0_read(chp, wd_seccnt);
}

static int
atapi_send_cmd(idec, ac)
	struct channel_link *idec;
	struct ataccb *ac;
{
	struct channel_softc *chp = idec->wk_chp;
	struct atapi_drive *adrv = idec->wk_isc;

	if ((ac->ac_flags & ATAPIF_DMA) != 0)
	{
		if ((ac->ac_flags & ATAPIF_READ) != 0)
			chp->ch_dma = WDC_DMA_READ;
		else
			chp->ch_dma = 0;

		if ((*chp->wdc->dma_init)(chp->wdc->dma_arg,
		    chp->channel, idec->wk_chan, adrv->av_data,
		    adrv->av_datalen, chp->ch_dma) != 0)
			return EIO;
	}

	wdc_data_write(chp, (u_int8_t *) ac->ac_pcd, ac->ac_cmdlen, 0);
	if (adrv->av_datalen != 0)
	{
		if ((ac->ac_flags & ATAPIF_READ) != 0)
			adrv->av_nextph = ATAPI_DATAIN;
		else
			adrv->av_nextph = ATAPI_DATAOUT;
	}
	else
		adrv->av_nextph = ATAPI_DISCON;

	if ((ac->ac_flags & ATAPIF_DMA) != 0)
	{
		chp->ch_flags |= WDCF_DMASTART;
		(*chp->wdc->dma_start)(chp->wdc->dma_arg, chp->channel,
				       idec->wk_chan, chp->ch_dma);
	}
	return 0;
}

int
atapi_start_nexus(idec, ac)
	struct channel_link *idec;
	struct ataccb *ac;
{
	struct atapi_drive *adrv = idec->wk_isc;
	int len, dma = 0;

	memset(&ac->ac_result, 0, sizeof(ac->ac_result));
	adrv->av_nextph = ATAPI_CMDOUT;

	len = ac->ac_datalen;
	if ((ac->ac_flags & ATAPIF_WRITE) != 0)
		len = - len;
	if (ac->ac_flags & ATAPIF_DMA)
		dma = ATAPI_PKT_CMD_FTRE_DMA;

	return wdccommand(idec, ATAPIC_PACKET, len, 0, 0, 0, dma);
}

struct atapi_vendor {
	char *name;
	u_int bus_flags;
	u_int wcd_flags;
	u_int maxblk;
	u_int mode;
} atapi_vendor[] = {
	{
		"NEC                 CD-ROM DRIVE:260",
		AVBF_EMULCOMP | AVBF_WFIFO12,
		1,
		MODE_WATAPI,
	},
	{
		NULL,
	}
};

struct atapi_drive *
wdc_link_atapi(idec)
	struct channel_link *idec;
{
	struct atapi_drive *adrv;
	struct ataparams *ap;
	struct atapi_vendor *vs;
	u_int8_t *mbuf, tbuf[DEV_BSIZE];
	int i;

	/* setup functions & idec structure */
	ap = (struct ataparams *) tbuf;
	idec->wk_funcs = &channel_link_atapi_funcs;
	idec->wk_interface = MODE_ATAPI;

	/* make atapi_drive */
	adrv = (void *) malloc(sizeof(struct atapi_drive), M_DEVBUF, M_NOWAIT);
	if ((idec->wk_isc = adrv) == NULL)
		panic("atapi: no mem\n");

	/* setup the atapi drive info */
	memset((u_char *) adrv, 0, sizeof(struct atapi_drive));
	TAILQ_INIT(&adrv->av_cmdtab);
	adrv->av_maxio = ATAPI_MAX_CCB;

	/* init ccb */
	atapi_init_ccbque(adrv->av_maxio);

	/* get atapi drive info */
	if (wdc_load_params(idec, ap) != 0)
		printf("atapidrv: can not load params\n");

	if ((ap->atap_config & ATAPI_CFG_CMD_MASK) != 0)
		adrv->av_bflags |= AVBF_CMD16;
	if ((ap->atap_config & ATAPI_CFG_IRQ_DRQ) != 0)
		adrv->av_bflags |= AVBF_INTRCMD;
	if ((ap->atap_config & ATAPI_CFG_ACCEL_DRQ) != 0)
		adrv->av_bflags |= AVBF_ACCEL;

	mbuf = ap->atap_model;
	for (i = 0; i < sizeof(ap->atap_model); i++)
		if (mbuf[i] != 0)
			mbuf[i] = ' ';
	mbuf[i] = 0;
	for (i = sizeof(ap->atap_model) - 1; i >= 0 && mbuf[i] == ' '; i --)
		mbuf[i] = 0;

	adrv->av_maxblk = 8;
	for (vs = &atapi_vendor[0]; vs->name != NULL; vs ++)
	{
		if (strncmp(mbuf, vs->name, strlen(vs->name)) == 0)
		{
			idec->wk_interface = vs->mode;
			adrv->av_maxblk = vs->maxblk;
			adrv->av_bflags |= vs->bus_flags;
		}
	}

	idec->wk_ats = shutdownhook_establish(atapi_shutdown, idec);
	return adrv;
}

static void
atapi_drain_data(chp, len)
	struct channel_softc *chp;
	u_int len;
{
	u_int32_t buf;

	for ( ; len > 0; len -= sizeof(u_int16_t))
		wdc_data_read(chp, (u_int8_t*) &buf, sizeof(u_int16_t), 0);
}

/*********************************************************
 * WDC LINK FUNCTIONS
 *********************************************************/
static __inline int _wdclink_atapiwdcstart __P((struct channel_link *, struct ataccb *));

static __inline int
_wdclink_atapiwdcstart(idec, ac)
	struct channel_link *idec;
	struct ataccb *ac;
{
	struct atapi_drive *adrv = idec->wk_isc;
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	int error;

	if (chp->ch_errors >= ATAPIIORETRIES)
	{
		atapi_err(idec, ac, "too many retries", RES_ERR);
		return atapidone(idec);
	}

	if (drvp->state < READY && (ac->ac_flags & ATAPIF_POLL) == 0)
	{
		error = atapictrl(idec);
		if (error != WDCLINK_RESTART)
			return error;
	}
	
	if (ac->ac_flags & ATAPIF_SENSE)
	{
		memset(&ac->ac_sense_cmd, 0, sizeof(ac->ac_sense_cmd));
		ac->ac_sense_cmd.opcode = REQUEST_SENSE;
		ac->ac_data = (u_int8_t *) &ac->ac_sense;
		ac->ac_datalen = sizeof(ac->ac_sense);
		ac->ac_pcd = (struct scsipi_generic *) &ac->ac_sense_cmd;
		/* ac->ac_cmdlen = xs->cmdlen; */
		ac->ac_flags &= ~ATAPIF_WRITE;
		ac->ac_flags |= ATAPIF_READ;
	}

	adrv->av_data = ac->ac_data;
	adrv->av_datalen = ac->ac_datalen;
	if ((drvp->drive_flags & (DRIVE_UDMA | DRIVE_DMA)) != 0 &&
    	    ac->ac_datalen != 0 && chp->ch_errors == 0)
		ac->ac_flags |= ATAPIF_DMA;
	else
		ac->ac_flags &= ~ATAPIF_DMA;

	/* Start packet command. */
	if (atapi_start_nexus(idec, ac) < 0)
	{
		atapi_err(idec, ac, "cmdout timeout", RES_ERR);
		return WDCLINK_RESET;
	}

	if (adrv->av_bflags & AVBF_INTRCMD)
		return WDCLINK_CONTINUE;

	/* Wait for DRQ. */
	if (wdcwaitPhase(idec->wk_chp, ATAPI_BUS_WAITS, PHASE_CMDOUT) < 0)
	{
		atapi_err(idec, ac, "no cmd drq", RES_NODRQ);
		return WDCLINK_RESET;
	}

	/* Send packet command. */
	if (atapi_send_cmd(idec, ac) != 0)
		return WDCLINK_RESET;

	return WDCLINK_CONTINUE;
}

static int
wdclink_atapiwdcstart(idec)
	struct channel_link *idec;
{
	struct atapi_drive *adrv = idec->wk_isc;
	struct ataccb *ac = adrv->av_cmdtab.tqh_first;
	struct physio_proc *pp;
	int error;

	if (ac == NULL)
		return WDCLINK_DONE;

	pp = physio_proc_enter(ac->ac_bp);
	error = _wdclink_atapiwdcstart(idec, ac);
	physio_proc_leave(pp);

	return error;
}

/******************************************************
 * ATAPI PHASE TRACER
 ******************************************************/
/* #define	SCSIPI_ATAPI_SUPPORT_SENSE	"YES" */
#define	SCSIPI_ATAPI_CD_COMPLETELY_BUGGY	"YES"
#define	ATAPI_MAX_PHMISS	40000

static __inline int _wdclink_atapiintr __P((struct channel_link *, struct ataccb *));
static __inline u_int get_xfc __P((struct channel_softc *));
static __inline int atapi_check_ph __P((struct atapi_drive *, u_int));

static __inline u_int
get_xfc(chp)
	struct channel_softc *chp;
{
	register u_int count;

	count = (u_int) wdc_cr0_read(chp, wd_cyl_lo);
	count |= ((u_int) wdc_cr0_read(chp, wd_cyl_hi)) << 8;
	return count;
}

static __inline int
atapi_check_ph(adrv, curph)
	struct atapi_drive *adrv;
	u_int curph;
{

	if (curph == adrv->av_nextph)
		return 0;

	delay(10);
	return EINVAL;
}

static __inline int
_wdclink_atapiintr(idec, ac)
	struct channel_link *idec;
	struct ataccb *ac;
{
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	struct atapi_drive *adrv = idec->wk_isc;
	int dma_err, phmiss, error = WDCLINK_DONE;
	u_int len;

	/*
	 * Shutdown DMA first!
	 */
	dma_err = 0;
	if ((chp->ch_flags & WDCF_DMASTART) != 0)
	{
		int rv;

		rv = (*chp->wdc->dma_finish) (chp->wdc->dma_arg, chp->channel,
					      idec->wk_chan, chp->ch_dma);
		if (rv == 0)
		{
			adrv->av_datalen = 0;
		}
#ifdef	SCSIPI_ATAPI_CD_COMPLETELY_BUGGY
		else if (ac->ac_bp == NULL && rv > 0)
		{
			if ((ac->ac_flags & ATAPIF_SENSE) == 0)
				adrv->av_datalen = 0;
		}
#endif	/* SCSIPI_ATAPI_CD_COMPLETELY_BUGGY */
		else
		{
			atapi_err(idec, ac, "dmaerr", RES_OVERRUN);
			dma_err |= ARS_CHECK;
		}
		chp->ch_flags &= ~WDCF_DMASTART;
	}

	/*
	 * Check phase.
	 */
	for (phmiss = ATAPI_MAX_PHMISS; phmiss > 0; phmiss --)
	{
		if (wdcwait(chp, 0) < 0)
		{
			atapi_err(idec, ac, "controller not ready", RES_NOTRDY);
			chp->ch_status |= ARS_CHECK;
		}
		chp->ch_status |= dma_err;

		if (atapi_debug > 1)
			Debugger();

		if (drvp->state < READY && (ac->ac_flags & ATAPIF_POLL) == 0)
			return atapictrl(idec);

		if ((chp->ch_status & ARS_CHECK) != 0)
			goto done;

		switch (wdcPhase(chp))
		{
		default:
			atapi_err(idec, ac, "unknown phase", RES_ERR);
			return WDCLINK_RESET;

		case PHASE_CMDOUT:
			if (atapi_check_ph(adrv, ATAPI_CMDOUT))
				continue;

			if (atapi_send_cmd(idec, ac) != 0)
				return WDCLINK_RESET;

			return WDCLINK_CONTINUE;

		case PHASE_DATAOUT:
			if ((ac->ac_flags & ATAPIF_DMA) != 0)
				continue;
			if (atapi_check_ph(adrv, ATAPI_DATAOUT))
				continue;

			len = get_xfc(chp);
			if ((adrv->av_bflags & AVBF_WFIFO12) && len > 12)
				len = 12;
			if (adrv->av_datalen < len)
			{
				atapi_err(idec, ac, "w_underrun", RES_UNDERRUN);
				goto done;
			}

			wdc_data_write(chp, (u_int8_t *) adrv->av_data, len,
				       drvp->drive_flags);
			adrv->av_data += len;
			adrv->av_datalen -= len;
			if (adrv->av_datalen <= 0)
			{
				adrv->av_nextph = ATAPI_DISCON;
				if (adrv->av_bflags & AVBF_EMULCOMP)
					goto done;
			}
			return WDCLINK_CONTINUE;

		case PHASE_DATAIN:
			if ((ac->ac_flags & ATAPIF_DMA) != 0)
				continue;
			if (atapi_check_ph(adrv, ATAPI_DATAIN))
				continue;

			len = get_xfc(chp);
			if (adrv->av_datalen < len)
			{
				atapi_err(idec, ac, "r_overrun", RES_OVERRUN);
				atapi_drain_data(chp, len);
				goto done;
			}

			wdc_data_read(chp, (u_int8_t *) adrv->av_data, len,
				      drvp->drive_flags);
			adrv->av_data += len;
			adrv->av_datalen -= len;
			if (adrv->av_datalen <= 0)
			{
				adrv->av_nextph = ATAPI_DISCON;
				if (adrv->av_bflags & AVBF_EMULCOMP)
					goto done;
			}
			return WDCLINK_CONTINUE;

		case PHASE_COMPLETED:
		case PHASE_COMPLETED_ALT:
#ifdef	SCSIPI_ATAPI_CD_COMPLETELY_BUGGY
			/* XXX:
			 * The upper layer scsipi requests illegal data size
			 * for cd_read_toc, cd_read_subchannel, cd_read_mode.
			 * I gave up to fix the above stupid mistakes and
			 * give a quick solutions in our lower layer.
			 */
			if (ac->ac_bp == NULL && 
			    (ac->ac_flags & ATAPIF_SENSE) == 0 &&
			    adrv->av_datalen < ac->ac_datalen)
				adrv->av_datalen = 0;
#endif	/* SCSIPI_ATAPI_CD_COMPLETELY_BUGGY */
			/* XXX:
			 * Too many atapi devices never handle 
		 	 * an scsipi sense cmd, give a quick solution
			 * in the lower layer.
			 */
			if ((ac->ac_flags & ATAPIF_SENSE) &&
			    adrv->av_datalen < ac->ac_datalen)
				adrv->av_datalen = 0;
			goto done;
		}

	}

	atapi_err(idec, ac, "too much phase miss", RES_ERR);
	return WDCLINK_RESET;

done:
	if ((chp->ch_status & (ARS_CHECK | ARS_DF)) != 0)
	{
		atapi_err(idec, ac, NULL, RES_ERR);
		error = WDCLINK_DONE;
	}
	else if (adrv->av_datalen != 0)
	{
		atapi_err(idec, ac, "unexpected disc", RES_OVERRUN);
		error = WDCLINK_RESET;
	}
	else
		ac->ac_result.code = RES_OK;

	if ((adrv->av_bflags & AVBF_EMULCOMP) != 0 &&
	    wdcwaitPhase(chp, ATAPI_DRQ_WAITS, 0) < 0)
	{
		atapi_err(idec, ac, "DRQ continue", RES_ERR);
		error = WDCLINK_RESET;
	}

	if (error == WDCLINK_DONE)
		return atapidone(idec);

	return error;
}

static int
wdclink_atapiintr(arg)
	void *arg;
{
	struct channel_link *idec = arg;
	struct atapi_drive *adrv = idec->wk_isc;
	struct physio_proc *pp;
	struct ataccb *ac;
	int error;

	ac = adrv->av_cmdtab.tqh_first;
#ifdef	DIAGNOSTIC
	if (ac == NULL)
	{
		printf("%s: stray interrupt\n", idec->wk_ata->sc_dev.dv_xname);
		return WDCLINK_DONE;
	}
#endif	/* DIAGNOSTIC */

	pp = physio_proc_enter(ac->ac_bp);
	error = _wdclink_atapiintr(idec, ac);
	physio_proc_leave(pp);

	return error;
}

/******************************************************
 * ATAPI DRIVE SETUP
 ******************************************************/
#define	STATE_ERROR(str)	\
	{ s = (str); goto bad; }
#define	STATE_AGAIN(drvp, stat) \
	{ (drvp)->state = (stat); continue; }
#define	STATE_CONTINUE(drvp, stat) \
	{ (drvp)->state = (stat); return WDCLINK_CONTINUE; }

static int
atapictrl(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	int error = 0;
	u_char *s, *nss;

    while (1)
    {
	switch (drvp->state)
	{
	case PIOMODE:
		if ((chp->wdc->cap & WDC_CAPABILITY_MODE) == 0 ||
		    (drvp->drive_flags & DRIVE_MODE) == 0)
			STATE_AGAIN(drvp, READY);

		if (wdcsetfeature(chp, idec->wk_chan, 
				  WDCC_FEATURES, 0x08 | drvp->PIO_mode,
				  FEAT_SET_MODE) != 0)
			STATE_ERROR("piomode setup (1)");

		STATE_CONTINUE(drvp, PIOMODE_WAIT);

	case PIOMODE_WAIT:
		if (chp->ch_status & (ARS_CHECK | ARS_DF))
			STATE_ERROR("piomode setup (2)");
		/* fall through */

	case DMAMODE:
		if (drvp->drive_flags & DRIVE_UDMA)
		{
			error = wdcsetfeature(chp, idec->wk_chan,
					      WDCC_FEATURES,
			    		      0x40 | drvp->UDMA_mode,
					      FEAT_SET_MODE);
		}
		else if (drvp->drive_flags & DRIVE_DMA)
		{
			error = wdcsetfeature(chp, idec->wk_chan,
					      WDCC_FEATURES,
					      0x20 | drvp->DMA_mode,
					      FEAT_SET_MODE);
		} 
		else
		{
			STATE_AGAIN(drvp, READY);
		}	

		if (error != 0)
			STATE_ERROR("dma setup (1)");

		STATE_CONTINUE(drvp, DMAMODE_WAIT);
		break;

	case DMAMODE_WAIT:
		if (chp->ch_status & (ARS_CHECK | ARS_DF))
			STATE_ERROR("dma setup (2)");
		/* fall through */

	case READY:
		drvp->state = READY;
		return WDCLINK_RESTART;
	}

	return WDCLINK_CONTINUE;
     }

bad:
	nss = NULL;
	if (drvp->drive_flags & DRIVE_UDMA)
	{
		drvp->drive_flags &= ~DRIVE_UDMA;
		nss = "DMA";
	}
	else if (drvp->drive_flags & DRIVE_DMA)
	{
		drvp->drive_flags &= ~DRIVE_DMA;
		nss = "pio";
	}
	else
		drvp->drive_flags &= ~DRIVE_MODE;

	printf("%s: atapictrl %s failed\n", drvp->drv_softc->dv_xname, s);
	if (nss != NULL)
		printf("%s: atapictrl fall to %s mode\n",
		        drvp->drv_softc->dv_xname, nss);

	return WDCLINK_RESET;
}
