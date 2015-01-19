/*	$NecBSD: wdcsubr.c,v 1.9 1999/07/26 06:31:58 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Isao Ohishi. All rights reserved.
 */
/*
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * DMA and multi-sector PIO handling are derived from code contributed by
 * Onno van der Linden.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>

#include <vm/vm.h>

#include <i386/Cbus/dev/atapi/wdcreg.h>
#include <i386/Cbus/dev/atapi/wdcvar.h>
#include <i386/Cbus/dev/atapi/atapireg.h>
#include <i386/Cbus/dev/scsi-atapi/atapivar.h>
#include <i386/Cbus/dev/atapi/wdchw.h>
#include <dev/ata/atareg.h>

static int _wdcprobesubr __P((struct channel_softc *, int, int));
static int wdcreset __P((struct channel_softc *));
static void wdcunwedge __P((struct channel_link *));
static int wdcprint __P((void *, const char *));
static void wdcstart __P((struct channel_softc *));
static void wdcdone __P((struct channel_link *));
static void wdc_switch_mode __P((struct channel_link *));
static void wdc_initialize_chip __P((struct channel_softc *));
static void wdc_release_chip __P((struct channel_softc *));
static __inline void wdc_timeout_start __P((struct channel_link *));
static __inline void wdc_timeout_stop __P((struct channel_link *));
#define	WDC_TIMER_START	1
#define	WDC_TIMER_STOP	0
static void wdc_timer_setup __P((struct channel_softc *, int));
static void wdc_timer __P((void *));

/*********************************************************
 * probe attach
 *********************************************************/
struct wdc_hw wdc_generic_hardware = {
	"ATbus",
	WDC_DTYPE_GENERIC,

	WDC_ACCESS_PLAIN,
	2,

	{ NULL,
	  NULL,
	  NULL,
	  wdc_cr0_read_plain,
	  wdc_cr0_write_plain,
	  wdc_cr1_read_plain,
	  wdc_cr1_write_plain,
	  wdc_data_read_generic,
	  wdc_data_write_generic
	},

	WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_MODE,
	3,
	0
};

void
wdc_init_channel_softc(chp, hwp)
	struct channel_softc *chp;
	struct wdc_hw *hwp;
{
	int i;

	/*
	 * Setup hardware access methods
	 */
	if (hwp == NULL)
		hwp = &wdc_generic_hardware;

	chp->ch_hwfuncs = hwp->hw_funcs;
	chp->ch_hw = hwp;
	chp->ch_delay = WDCDELAY;

	/*
	 * Setup bank control number
	 */
	for (i = 0; i < ATABUS_MAXCHAN; i ++)
	{
		chp->ch_drvbank[i] = WDC_CHAN_TO_BANKNO(i);
		chp->ch_drvno[i] = WDC_CHAN_TO_DRVNO(i);
	}

	/*
	 * Each bus should setup io and mem space handles.
	 */
}

static int
_wdcprobesubr(chp, drive, flags)
	struct channel_softc *chp;
	int drive, flags;
{
	int rv = 0;

	wdc_select_bank(chp, drive);
	if (wdc_cr0_read(chp, wd_status) == (u_int8_t) -1)
		goto out;

	if (wdcreset(chp) != 0)
		goto out;

	wdc_select_bank(chp, drive);
	wdc_cr0_write(chp, wd_sdh, WDSD_IBM | (WDC_DRIVE_NO(chp, drive) << 4));
	if (wait_for_unbusy(chp) < 0)
		goto out;

	if (wdc_inquire_idedrv(chp, drive) == ENXIO)
		goto out;
	rv = 1;

out:
	wdc_select_bank(chp, WDC_BANK_UNKNOWN);
	return rv;
}

int
wdcprobe(chp)
	struct channel_softc *chp;
{

	wdc_init_channel_softc(chp, NULL);

	chp->data16iot = chp->cmd_iot;
	chp->data16ioh = chp->cmd_ioh;

	return wdcprobesubr(chp);
}

int
wdcprobesubr(chp)
	struct channel_softc *chp;
{
	int rv = 0;

	wdc_initialize_chip(chp);
	if ((rv = _wdcprobesubr(chp, 0, 0)) == 0 && chp->ch_bank != 0)
		rv = _wdcprobesubr(chp, WDC_SECOND_BANK, 0);
	wdc_release_chip(chp);
	return rv;
}

static int
wdcprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct wdc_attach_args *wa = aux;
	struct channel_softc *chp = wa->wa_chp;

	printf(" channel %d bank %d", chp->channel, wa->wa_bank);

	return (UNCONF);
}

void
wdcattach(chp)
	struct channel_softc *chp;
{

	wdc_init_channel_softc(chp, NULL);

	chp->data16iot = chp->cmd_iot;
	chp->data16ioh = chp->cmd_ioh;

	printf("%s", chp->wdc->sc_dev.dv_xname); 
	wdcattachsubr(chp);
}

void
wdcattachsubr(chp)
	struct channel_softc *chp;
{
	struct wdc_attach_args wa;
	struct wdc_hw *hw = chp->ch_hw;
	struct channel_queue *cqp;

	/* allocate bank */
	wdc_initialize_chip(chp);

	/* print info */
	printf(": access (%s)", hw->hw_name);
	if (chp->ch_bank != 0)
		printf("(has bank)");
	if (chp->ch_memh != NULL)
		printf("(mem)");
	printf("\n");

	/* allocate and init queue */
	if ((cqp = chp->ch_queue) == NULL)
	{
		cqp = malloc(sizeof(*cqp), M_DEVBUF, M_NOWAIT);
		if (cqp == NULL)
			return;
		memset(cqp, 0, sizeof(*cqp));
		chp->ch_queue = cqp;
	}
	if (cqp->cq_link.tqh_first == NULL)
	{
		TAILQ_INIT(&cqp->cq_drives);
		TAILQ_INIT(&cqp->cq_link);
	}	

	/* start watch dogs */
	wdc_timer_setup(chp, WDC_TIMER_START);

	/* scan a bus */
	wa.wa_chp = chp;
	wa.wa_maxunit = hw->hw_maxunit;
	if (wa.wa_maxunit > ATABUS_DRIVE_PER_BANK)
		wa.wa_maxunit = ATABUS_DRIVE_PER_BANK;
		
	/* scan a bus for bank 0 */
	wa.wa_bank = 0;
	(void) config_found((struct device *) chp->wdc, (void *) &wa, wdcprint);
	if (chp->ch_bank == 0)
		return;

	/* scan a bus for bank 1 */
	wa.wa_bank = 1;
	(void) config_found((struct device *) chp->wdc, (void *) &wa, wdcprint);
}

/*********************************************************
 * ide drive
 *********************************************************/
int
wdc_inquire_idedrv(chp, unit)
	struct channel_softc *chp;
	int unit;
{
	u_int8_t regv;
	int error;

	/* select bank */
	wdc_select_bank(chp, unit);
	wdc_cr0_write(chp, wd_sdh, WDSD_IBM | (WDC_DRIVE_NO(chp, unit) << 4));

	regv = wdc_cr0_read(chp, wd_status);
	if ((regv & (WDCS_BSY | WDCS_DRQ | WDCS_CORR)) != 0)
		return ENXIO;

	wdc_cr0_write(chp, wd_command, WDCC_RECAL);
	delay(1000);		/* 1ms wait */

	regv = wdc_cr0_read(chp, wd_status);
	if ((regv & (WDCS_BSY | WDCS_DRDY | WDCS_DSC | WDCS_ERR)) == 0)
		return ENXIO;

	error = wait_for_unbusy(chp);
	if (error < 0)
		return ENXIO;
	if (error > 0)
	{
		delay(1000);	/* 1ms wait */
		return EIO;
	}
	error = wait_for_ready(chp);
	if (error < 0)
		return ENXIO;
	return error;
}	

/*********************************************************
 * atapi drive
 *********************************************************/
#define	ATAPI_WAIT_CYCLE 	1000
#define	WDC_MAX_WAIT_AND_DRAIN	3
static int wdc_wait_and_drain __P((struct channel_softc *, int));
static int wdc_wait_data_ready __P((struct channel_softc *, int));

static int
wdc_wait_data_ready(chp, mode)
	struct channel_softc *chp;
	int mode;
{
	int error = 0;

	if (mode == MODE_IDE)
	{
		error = wait_for_drq(chp);
	}
	else
	{
		error = wait_for_unbusy(chp);
		if (error == 0)
			error = wdcwait(chp, WDCS_DRQ);
	}
	return error;
}

static int
wdc_wait_and_drain(chp, mode)
	struct channel_softc *chp;
	int mode;
{
	int i, buf, cnt, error = 0;

	for (i = 0; i < WDC_MAX_WAIT_AND_DRAIN; i ++)
	{
		if (mode == MODE_IDE)
		{
			 error = wait_for_ready(chp);
		}
		else
		{
			delay(ATAPI_WAIT_CYCLE);
			(void) wdcwaitPhase(chp, ATAPI_DRQ_WAITS, 0);
			delay(ATAPI_WAIT_CYCLE);
			error = wdcwait(chp, WDCS_DRDY);
		}

		if ((chp->ch_status & WDCS_DRQ) != 0)
		{
			printf("%s: DRQ still asserted\n",
				chp->wdc->sc_dev.dv_xname);
			for(cnt = 0; cnt < DEV_BSIZE; cnt += 2)
				wdc_data_read(chp, (u_int8_t *) &buf, 2, 0);
		}
		else if (error >= 0)
			break;
	}

	return error;
}

int
wdc_inquire_device(chp, unit)
	struct channel_softc *chp;
	int unit;
{
	u_int8_t tbuf[DEV_BSIZE];
	struct ataparams *ap = (struct ataparams *) tbuf;
	int error;

	wdc_select_bank(chp, unit);
	wdc_cr0_write(chp, wd_sdh, WDSD_IBM | (WDC_DRIVE_NO(chp, unit) << 4));
	wdc_cr0_write(chp, wd_command, ATAPIC_IDENTIFY);

	error = wdc_wait_data_ready(chp, MODE_ATAPI);
	if (error != 0)
		return ENXIO;

	wdc_data_read(chp, tbuf, DEV_BSIZE, 0);
	wdc_wait_and_drain(chp, MODE_ATAPI);
	if ((ap->atap_config & WDC_CFG_ATAPI_MASK) != WDC_CFG_ATAPI)
		return EIO;
	return 0;
}

int
wdc_reset_atapi(chp, unit)
	struct channel_softc *chp;
	u_int unit;
{

	wdc_select_bank(chp, unit);
	wdc_cr0_write(chp, wd_sdh, WDSD_IBM | (WDC_DRIVE_NO(chp, unit) << 4));
	wdc_cr0_write(chp, wd_command, ATAPIC_SRESET);

	delay(100000);	/* 100 ms */

	if (wait_for_unbusy(chp) < 0)
	{
		printf("%s: atapi reset failed\n", chp->wdc->sc_dev.dv_xname);
		return EBUSY;
	}

	return 0;
}

/*********************************************************
 * loading parameters
 *********************************************************/
static void wdc_shuffle_params __P((struct ataparams *));

static void
wdc_shuffle_params(ap)
	struct ataparams *ap;
{
	int i;
	u_int16_t *p;

#if BYTE_ORDER == LITTLE_ENDIAN
	/*
	 * Shuffle string byte order.
	 * ATAPI Mitsumi and NEC drives don't need this.
	 */
	if ((ap->atap_config & WDC_CFG_ATAPI_MASK) == WDC_CFG_ATAPI &&
	    ((ap->atap_model[0] == 'N' && ap->atap_model[1] == 'E') ||
	     (ap->atap_model[0] == 'F' && ap->atap_model[1] == 'X')))
		return;

	for (i = 0; i < sizeof(ap->atap_model); i += 2) {
		p = (u_int16_t *)(ap->atap_model + i);
		*p = ntohs(*p);
	}

	for (i = 0; i < sizeof(ap->atap_serial); i += 2)
	{
		p = (u_int16_t *)(ap->atap_serial + i);
		*p = ntohs(*p);
	}

	for (i = 0; i < sizeof(ap->atap_revision); i += 2)
	{
		p = (u_short *)(ap->atap_revision + i);
		*p = ntohs(*p);
	}
#endif
}

int
wdc_load_params(idec, ap)
	struct channel_link *idec;
	struct ataparams *ap;
{
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	u_int8_t *tbp;
	int mode, s, error = 0, command;

	tbp = malloc(DEV_BSIZE, M_DEVBUF, M_TEMP);
	if (tbp == NULL)
		return EINVAL;
	memset(tbp, 0, DEV_BSIZE);

	mode = idec->wk_interface;
	command = (mode == MODE_IDE) ?  WDCC_IDENTIFY : ATAPIC_IDENTIFY;

	s = splbio();
	if ((error = wdcsyncstart(idec)) != 0)
		goto out;

	error = EIO;
	if (wdccommandshort(chp, idec->wk_chan, command) != 0)
	{
		printf("wdc_load_params: command failed\n");
	}
	else if (wdc_wait_data_ready(chp, mode) == 0)
	{
		wdc_data_read(chp, tbp, DEV_BSIZE, drvp->drive_flags);
		wdc_wait_and_drain(chp, mode);
		error = 0;
	}

	wait_for_ready(chp);
	wdcsyncdone(idec);

out:
	splx(s);
	bcopy(tbp, ap, sizeof(*ap));
	free(tbp, M_TEMP);
	if (error == 0)
		wdc_shuffle_params(ap);
	return error;
}

/*********************************************************
 * start & done
 *********************************************************/
void
wdccmdstart(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;
	struct channel_queue *cqp = chp->ch_queue;

	if ((idec->wk_flags & DRIVE_QUEUED) == 0)
	{
		TAILQ_INSERT_TAIL(&cqp->cq_drives, idec, wk_drivechain);
		idec->wk_flags |= DRIVE_QUEUED;
	}

	if ((cqp->cq_state & CQ_BUSY) == 0)
		wdcstart(chp);
}

/* really host start */
static void
wdc_switch_mode(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;

	switch (chp->ch_interface)
	{
	case MODE_WATAPI:
		delay(256);
		wait_for_unbusy(chp);
		delay(256);
		wait_for_ready(chp);
		break;

	default:
		break;
	}

	chp->ch_interface = idec->wk_interface;
}

static void
wdcstart(chp)
	struct channel_softc *chp;
{
	struct channel_link *idec;
	struct channel_queue *cqp;
	int error;

#ifdef	DIAGONASTIC
	if ((chp->ch_flags & WDCF_ACTIVE) != 0)
		panic("wdcstart: controller expects wdcintr");
#endif	/* DIAGONASTIC */

 	cqp = chp->ch_queue;
	if ((cqp->cq_state & (CQ_BUSY | CQ_WANTED)) == CQ_WANTED)
	{
		cqp->cq_state &= ~CQ_WANTED;
		wakeup(cqp);
		return;
	}

loop:
	idec = cqp->cq_drives.tqh_first;
	if ((cqp->cq_state & CQ_BUSY) != 0)
	{
		if (idec == NULL)
		{
			printf("%s: missing a target channel_link\n", 
				chp->wdc->sc_dev.dv_xname);
			return;
		}

		if ((cqp->cq_state & CQ_SYNC) != 0)
			return;
	}
	else 
	{
		do
		{
			if (idec == NULL)
				return;
			if ((idec->wk_flags & DRIVE_INACTIVE) == 0)
				break;
			idec = idec->wk_drivechain.tqe_next;
		}
		while (1);

		/* reload all here */
		chp = idec->wk_chp;
#ifdef	DIAGNOSTIC
		if (cqp != chp->ch_queue)
			panic("%s: channel queue inconsistent\n",
			      chp->wdc->sc_dev.dv_xname);
#endif	/* DIAGNOSTIC */
		cqp->cq_state |= CQ_BUSY;
		if (idec != cqp->cq_drives.tqh_first)
		{
			TAILQ_REMOVE(&cqp->cq_drives, idec, wk_drivechain);
			TAILQ_INSERT_HEAD(&cqp->cq_drives, idec, wk_drivechain);
		}
	}

	/* interface mode switch */
	if (idec->wk_interface != chp->ch_interface)
		wdc_switch_mode(idec);

	/* bank select */
	wdc_select_bank(chp, idec->wk_chan);

	/* call start func */
	error = WDC_LINK_CALL(idec, wk_startfunc);
	switch (error)
	{
	case WDCLINK_DONE:
	case WDCLINK_RELDONE:
		wdcdone(idec);
		/* fall */
	case WDCLINK_RESTART:
		goto loop;

	case WDCLINK_ERROR_RETRY:
		chp->ch_errors ++;
		goto loop;

	case WDCLINK_CONTINUE:
		wdc_timeout_start(idec);
		break;

	case WDCLINK_RESET:
		wdcunwedge(idec);
		break;
	}
	return;
}

int
wdcsyncstart(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;
	struct channel_queue *cqp = chp->ch_queue;
	int error, s = splbio();

	while ((cqp->cq_state & CQ_BUSY) != 0)
	{
		cqp->cq_state |= CQ_WANTED;
		if ((error = tsleep(cqp, PRIBIO | PCATCH, "wdcsync", 0)) != 0)
		{
			wdccmdstart(idec);
			splx(s);
			return error;
		}
	}

	if (idec->wk_flags & DRIVE_INACTIVE)
	{
		splx(s);
		return EBUSY;
	}

	cqp->cq_state |= (CQ_BUSY | CQ_SYNC);
	if (idec->wk_flags & DRIVE_QUEUED)
		TAILQ_REMOVE(&cqp->cq_drives, idec, wk_drivechain);
	TAILQ_INSERT_HEAD(&cqp->cq_drives, idec, wk_drivechain);
	idec->wk_flags |= DRIVE_QUEUED;

	if (idec->wk_interface != chp->ch_interface)
		wdc_switch_mode(idec);

	wdc_select_bank(chp, idec->wk_chan);
	splx(s);
	return 0;
}

void
wdcsyncdone(idec)
	struct channel_link *idec;
{

	wdcdone(idec);
	if ((idec->wk_flags & DRIVE_HASJOB) != 0)
		wdccmdstart(idec);
}

static void
wdcdone(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;
	struct channel_queue *cqp = chp->ch_queue;
	int flags = (idec->wk_flags & DRIVE_HASJOB);

	/* clear status */
	chp->ch_errors = 0;
	chp->ch_flags &= ~(WDCF_SINGLE | WDCF_ERROR);

	/* reschedule queue */
	if (idec->wk_drivechain.tqe_next)
	{
		TAILQ_REMOVE(&cqp->cq_drives, idec, wk_drivechain);
		if (flags)
			TAILQ_INSERT_TAIL(&cqp->cq_drives, idec, wk_drivechain);
		else
			idec->wk_flags &= ~DRIVE_QUEUED;
	}
	else if (flags == 0)
	{
		TAILQ_REMOVE(&cqp->cq_drives, idec, wk_drivechain);
		idec->wk_flags &= ~DRIVE_QUEUED;
	}

	/* clear queue status */
	if (cqp->cq_state & CQ_SYNC)
	{
		cqp->cq_state &= ~CQ_SYNC;
		(void) wdc_cr0_read(chp, wd_status);
		(void) wdc_cr0_read(chp, wd_seccnt);
	}
	cqp->cq_state &= ~CQ_BUSY;
}

/*********************************************************
 * intr
 *********************************************************/
int
wdcintr(arg)
	void *arg;
{
	struct channel_softc *chp = arg;
	struct channel_queue *cqp;
	struct channel_link *idec;
	int error;

	cqp = chp->ch_queue;
	if ((cqp->cq_state & (CQ_BUSY | CQ_SYNC)) != CQ_BUSY)
	{
		(void) wdc_cr0_read(chp, wd_status);
		(void) wdc_cr0_read(chp, wd_seccnt);
		return 0;
	}

	if ((chp->ch_flags & WDCF_ACTIVE) == 0)
		return 0;
	idec = cqp->cq_drives.tqh_first;
#ifdef	DIAGNOSTIC
	if (idec == NULL)
		panic("%s: missing channel link\n", chp->wdc->sc_dev.dv_xname);
#endif	/* DIAGNOSTIC */
	wdc_timeout_stop(idec);

	wdc_select_bank(chp, idec->wk_chan);

	error = WDC_LINK_CALL(idec, wk_intrfunc);
	switch (error)
	{
	case WDCLINK_DONE:
	case WDCLINK_RELDONE:
		wdcdone(idec);
		wdcstart(idec->wk_chp);
		break;

	case WDCLINK_ERROR_RETRY:
		chp->ch_errors ++;
		/* fall */
	case WDCLINK_RESTART:
		wdcstart(idec->wk_chp);
		break;

	case WDCLINK_CONTINUE:
		wdc_timeout_start(idec);
		break;

	case WDCLINK_RESET:
		wdcunwedge(idec);
		break;
	}

	return 1;
}

/*********************************************************
 * timeout control
 *********************************************************/
#define	WDC_TIMEOUT_CHECK_INTERVAL	4
#define	WDC_TIMEOUT_DEFTC		16		/* 16 sec */
#define	WDC_TIMEOUT_ROUNDUP(tc) 	(((tc) + WDC_TIMEOUT_CHECK_INTERVAL - 1) / WDC_TIMEOUT_CHECK_INTERVAL)

static __inline void
wdc_timeout_start(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;

	chp->ch_flags |= WDCF_ACTIVE;
	idec->wk_tc.wk_inactive_tc = 0;
}

static __inline void
wdc_timeout_stop(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;

	chp->ch_flags &= ~WDCF_ACTIVE;
	idec->wk_tc.wk_active_tc = 0;
}

static void 
wdc_timer_setup(chp, flags)
	struct channel_softc *chp;
	int flags;
{
	struct channel_link *idec;
	struct channel_queue *cqp = chp->ch_queue;
	int s = splbio();

	chp->ch_flags &= ~WDCF_ACTIVE;
	for (idec = cqp->cq_link.tqh_first; idec != NULL; 
	     idec = idec->wk_linkchain.tqe_next)
	{
		if (chp != idec->wk_chp)
			continue;

		idec->wk_tc.wk_active_tc = 0;
		idec->wk_tc.wk_inactive_tc = 0;
		if (idec->wk_tc.wk_timeout_tc == 0)
			idec->wk_tc.wk_timeout_tc = WDC_TIMEOUT_DEFTC;
	}

	if (flags == WDC_TIMER_START)
		timeout(wdc_timer, chp, WDC_TIMEOUT_CHECK_INTERVAL * hz);
	else
		untimeout(wdc_timer, chp);
	splx(s);
}

static void
wdc_timer(arg)
	void *arg;
{
	struct channel_softc *chp = arg;
	struct channel_queue *cqp = chp->ch_queue;
	struct channel_link *idec;
	int s = splbio();

	if (chp->ch_flags & WDCF_RESETGO)
		goto out;

	for (idec = cqp->cq_link.tqh_first; idec != NULL; 
	     idec = idec->wk_linkchain.tqe_next)
	{
		struct channel_link_timeout_count *tcp;

		if (chp != idec->wk_chp)
			continue;

		tcp = &idec->wk_tc;
		if (idec == cqp->cq_drives.tqh_first &&
		    (chp->ch_flags & WDCF_ACTIVE))
		{
			register u_int tc;

			if (tcp->wk_timeout_tc == 0)
				tc = WDC_TIMEOUT_DEFTC;
			else
				tc = tcp->wk_timeout_tc;
			tc = WDC_TIMEOUT_ROUNDUP(tc);
			if (tcp->wk_active_tc ++ > tc)
				break;
		}
		else
			tcp->wk_inactive_tc ++;
	}

	if (idec != NULL)
	{
		wdc_timeout_stop(idec);
		wdcerror(idec, "lost interrupt");
		wdcunwedge(idec);
	}

out:
	splx(s);
	timeout(wdc_timer, chp, WDC_TIMEOUT_CHECK_INTERVAL * hz);
}

/*********************************************************
 * chip command out
 *********************************************************/
/*
 * Wait for the drive to become ready and send a command.
 * Return -1 if busy for too long or 0 otherwise.
 * Assumes interrupts are blocked.
 */
int
wdccommand(idec, command, cylin, head, sector, count, features)
	struct channel_link *idec;
	int command;
	int cylin, head, sector, count, features;
{
	struct channel_softc *chp = idec->wk_chp;

	/* Select drive, head, and addressing mode. */
	wdc_select_bank(chp, idec->wk_chan);
	wdc_cr0_write(chp, wd_sdh, WDSD_IBM | 
		      (WDC_DRIVE_NO(chp, idec->wk_chan) << 4) | head);

	/* Wait for it to become ready to accept a command. */
	if (wdcwait(chp, WDCS_DRDY) < 0)
		return -1;

	/* Load parameters. */
	wdc_cr0_write(chp, wd_features, features);
	wdc_cr0_write(chp, wd_cyl_lo, cylin);
	wdc_cr0_write(chp, wd_cyl_hi, cylin >> 8);
	wdc_cr0_write(chp, wd_sector, sector);
	wdc_cr0_write(chp, wd_seccnt, count);

	/* Send command. */
	wdc_cr0_write(chp, wd_command, command);

	return 0;
}

int
wdcsetfeature(chp, chan, command, count, features)
	struct channel_softc *chp;
	int chan;
	int command, count, features;
{

	wdc_select_bank(chp, chan);
	wdc_cr0_write(chp, wd_sdh, WDSD_IBM | (WDC_DRIVE_NO(chp, chan) << 4));
	if (wdcwait(chp, WDCS_DRDY) < 0)
		return -1;

	wdc_cr0_write(chp, wd_seccnt, count);
	wdc_cr0_write(chp, wd_features, features);
	wdc_cr0_write(chp, wd_command, command);
	return 0;
}

int
wdccommandshort(chp, chan, command)
	struct channel_softc *chp;
	int chan;
	int command;
{

	/* Select drive. */
	wdc_select_bank(chp, chan);
	wdc_cr0_write(chp, wd_sdh, WDSD_IBM | (WDC_DRIVE_NO(chp, chan) << 4));
	if ((command & WDCC_FORCE) == 0 && wdcwait(chp, WDCS_DRDY) < 0)
		return -1;

	wdc_cr0_write(chp, wd_command, command & WDCC_CMDMASK);
	return 0;
}

/*********************************************************
 * service functions
 *********************************************************/
int
wdc_exec_cmd(idec, command)
	struct channel_link *idec;
	int command;
{
	struct channel_softc *chp = idec->wk_chp;
	int s, error;

	s = splbio();
	if ((error = wdcsyncstart(idec)) != 0)
	{
		splx(s);
		return error;
	}

	error = wdccommandshort(chp, idec->wk_chan, command);
	if (error != 0)
		goto bad;
	error = wait_for_ready(chp);

bad:
	(void) wait_for_ready(chp);
	wdcsyncdone(idec);
	splx(s);

	if (error < 0)
		error = EBUSY;
	else if (error > 0)
		error = EIO;
	return error;
}

int
wdc_set_mode(idec, command, count, features)
	struct channel_link *idec;
	int command, count, features;
{
	struct channel_softc *chp = idec->wk_chp;
	int s, error;

	s = splbio();
	if ((error = wdcsyncstart(idec)) != 0)
	{
		splx(s);
		return error;
	}

	error = wdcsetfeature(chp, idec->wk_chan, command, count, features);
	if (error != 0)
	{
		error = EIO;
		goto out;
	}

	error = wait_for_ready(chp);
	if (error != 0)
		error = EIO;

out:
	(void) wait_for_ready(chp);
	wdcsyncdone(idec);
	splx(s);
	return error;
}

/*********************************************************
 * chip reset
 *********************************************************/
static int
wdcreset(chp)
	struct channel_softc *chp;
{

	if ((chp->ch_flags & WDCF_BRESET) == 0)
	{
		wdc_cr1_write(chp, wd_ctlr, WDCTL_RST | WDCTL_IDS);
		delay(1000);
	}
	wdc_cr1_write(chp, wd_ctlr, WDCTL_IDS);
	delay(1000);

	(void) wdc_cr0_read(chp, wd_error);

	wdc_cr1_write(chp, wd_ctlr, WDCTL_4BIT);
	if (wait_for_unbusy(chp) < 0)
		return 1;
	return 0;
}

/*********************************************************
 * chip timeout recover
 *********************************************************/
/*
 * Unwedge the controller after an unexpected error. We do this by resetting
 * it, marking all drives for recalibration, and stalling the queue for a short
 * period to give the reset time to finish.
 * NOTE: We use a timeout here, so this routine must not be called during
 * autoconfig or dump.
 */
static void wdcrecover_1 __P((void *));
static void wdcrecover_2 __P((void *));

void
wdcerror(idec, msg)
	struct channel_link *idec;
	char *msg;
{
	struct channel_softc *chp = idec->wk_chp;

	printf("%s(chan %d): %s: status %b error %b\n",
	    chp->wdc->sc_dev.dv_xname, idec->wk_chan,
	    msg, chp->ch_status, WDCS_BITS, chp->ch_error, WDERR_BITS);
}

static void
wdcrecover_2(arg)
	void *arg;
{
	struct channel_softc *chp = arg;
	int s;

	s = splbio();
	chp->ch_flags &= ~WDCF_RESETGO;
	wdcstart(chp);
	splx(s);
}

static void
wdcrecover_1(arg)
	void *arg;
{
	struct channel_softc *chp = arg;
	struct channel_queue *cqp = chp->ch_queue;
	struct channel_link *idec;

	for (idec = cqp->cq_link.tqh_first; idec != NULL;
	     idec = idec->wk_linkchain.tqe_next)
	{
		if (chp != idec->wk_chp)
			continue;

		if (WDC_LINK_FUNC(idec, wk_timeout) != NULL)
			(void) WDC_LINK_CALL(idec, wk_timeout);
	}

	chp->ch_flags |= WDCF_ERROR;
	++ chp->ch_errors;
	timeout(wdcrecover_2, chp, RECOVERYTIME);
}

static void
wdcunwedge(idec)
	struct channel_link *idec;
{
	struct channel_softc *chp = idec->wk_chp;

	wdc_timeout_stop(idec);

	/* anyway shutdown DMA */
	if ((chp->ch_flags & WDCF_DMASTART) != 0)
	{
		(*chp->wdc->dma_finish)(chp->wdc->dma_arg,
		     			chp->channel, idec->wk_chan,
					chp->ch_dma);
		chp->ch_flags &= ~WDCF_DMASTART;
	}

	chp->ch_flags |= WDCF_RESETGO;
	if (wdcreset(chp) != 0)
		printf("%s: reset failed\n", chp->wdc->sc_dev.dv_xname);

	timeout(wdcrecover_1, chp, RECOVERYTIME);
}

/*********************************************************
 * chip phase wait
 *********************************************************/
u_int8_t
wdcPhase(chp)
	struct channel_softc *chp;
{
	register u_int8_t stat;

	stat = wdc_cr0_read(chp, wd_seccnt);
	return (stat & (ARI_CMD | ARI_IN)) | (chp->ch_status & WDCS_DRQ);
}

int
wdcwaitPhase(chp, waits, phase)
	struct channel_softc *chp;
	int waits;
	u_int8_t phase;
{

	for ( ; waits >= 0; waits = waits - 10)
	{
		chp->ch_status = wdc_cr0_read(chp, wd_status);
		if (phase != 0)
		{
			if ((chp->ch_status & WDCS_BSY) == 0 &&
			    wdcPhase(chp) == phase)
				goto done;
		}
		else
		{
			if ((chp->ch_status & (WDCS_BSY | WDCS_DRQ)) == 0)
				goto done;
		}

		delay(10);
	}
	return -1;

done:
	if (chp->ch_status & WDCS_ERR)
	{
		chp->ch_error = wdc_cr0_read(chp, wd_error);
		return WDCS_ERR;
	}
	return 0;
}

int
wdcwait(chp, mask)
	struct channel_softc *chp;
	int mask;
{
	int timeout = 0;
	u_int8_t status;

	for (;;)
	{
		chp->ch_status = status = wdc_cr0_read(chp, wd_status);
		if ((status & WDCS_BSY) == 0 && (status & mask) == mask)
			break;
		if (++ timeout > WDCNDELAY)
			return -1;
		delay(chp->ch_delay);
	}

	if (status & WDCS_ERR)
	{
		chp->ch_error = wdc_cr0_read(chp, wd_error);
		return WDCS_ERR;
	}

	return 0;
}

/*********************************************************
 * chip deactivate & activate
 *********************************************************/
int
wdc_deactivate(chp)
	struct channel_softc *chp;
{
	struct channel_queue *cqp = chp->ch_queue;
	struct channel_link *cc;

	wdc_timer_setup(chp, WDC_TIMER_STOP);

	for (cc = cqp->cq_link.tqh_first; cc; cc = cc->wk_linkchain.tqe_next)
	{
		if (chp != cc->wk_chp)
			continue;

		cc->wk_flags |= DRIVE_INACTIVE;
		if (WDC_LINK_FUNC(cc, wk_deactivate))
			(void) WDC_LINK_CALL(cc, wk_deactivate);

		wdc_select_bank(chp, cc->wk_chan);
		wdc_cr0_write(chp, wd_sdh, WDSD_IBM | 
			      (WDC_DRIVE_NO(chp, cc->wk_chan) << 4));

		delay(1);
		(void) wdc_cr0_read(chp, wd_status);
		(void) wdc_cr0_read(chp, wd_seccnt);
		(void) wdc_cr0_read(chp, wd_error);

	}

	if (chp->ch_hwfuncs.close != NULL)
		(*chp->ch_hwfuncs.close) (chp);
	return 0;
}

int
wdc_activate(chp)
	struct channel_softc *chp;
{
	struct channel_queue *cqp = chp->ch_queue;
	struct channel_link *cc;
	int error = EIO;

	if (chp->ch_hwfuncs.open != NULL)
		(*chp->ch_hwfuncs.open) (chp);

	if (wdcreset(chp) != 0)
		printf("%s: reset failed\n", chp->wdc->sc_dev.dv_xname);

	for (cc = cqp->cq_link.tqh_first; cc; cc = cc->wk_linkchain.tqe_next)
	{
		if (chp != cc->wk_chp)
			continue;

		cc->wk_flags &= ~DRIVE_INACTIVE;
		if (WDC_LINK_FUNC(cc, wk_activate))
		{
			if (WDC_LINK_CALL(cc, wk_activate) == 0)
				error = 0;
			else
				cc->wk_flags |= DRIVE_INACTIVE;
		}
		else
			error = 0;
	}

	if (error != 0)
		return error;

	wdc_timer_setup(chp, WDC_TIMER_START);
	wdcstart(chp);
	return 0;
}

int
wdc_is_busy(chp)
	struct channel_softc *chp;
{
	struct channel_queue *cqp = chp->ch_queue;
	struct channel_link *cc;

	for (cc = cqp->cq_link.tqh_first; cc; cc = cc->wk_linkchain.tqe_next)
	{
		if (chp != cc->wk_chp)
			continue;
		if (cc->wk_flags & DRIVE_INACTIVE)
			continue;
		if ((cc->wk_flags & DRIVE_HASJOB) != 0)
			return EBUSY;
	}
	return 0;
}

/*********************************************************
 * IDE bank control
 *********************************************************/
static void
wdc_initialize_chip(chp)
	struct channel_softc *chp;
{
	struct wdc_hw *hw = chp->ch_hw;

	if ((hw->hw_access & WDC_ACCESS_HASBANK) != 0 &&
	    chp->ch_hwfuncs.banksel != NULL)
		chp->ch_bank = 1;

	if (chp->ch_hwfuncs.open != NULL)
		(*chp->ch_hwfuncs.open) (chp);
}

static void
wdc_release_chip(chp)
	struct channel_softc *chp;
{

	chp->ch_bank = 0;
	if (chp->ch_hwfuncs.close != NULL)
		(*chp->ch_hwfuncs.close) (chp);
}

/*********************************************************
 * data access methods
 *********************************************************/
u_int8_t
wdc_cr0_read_plain(chp, offs)
	struct channel_softc *chp;
	bus_addr_t offs;
{

	return bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, offs);
}


u_int8_t
wdc_cr0_read_indexed(chp, offs)
	struct channel_softc *chp;
	bus_addr_t offs;
{

	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_cridx, offs);
	return bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_crdata);
}
					
void
wdc_cr0_write_plain(chp, offs, data)
	struct channel_softc *chp;
	bus_addr_t offs;
	u_int8_t data;
{

	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, offs, data);
}

void
wdc_cr0_write_indexed(chp, offs, data)
	struct channel_softc *chp;
	bus_addr_t offs;
	u_int8_t data;
{

	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_cridx, (u_int8_t) offs);
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_crdata, data);
}

u_int8_t
wdc_cr1_read_plain(chp, offs)
	struct channel_softc *chp;
	bus_addr_t offs;
{

	return bus_space_read_1(chp->ctl_iot, chp->ctl_ioh, offs);
}


u_int8_t
wdc_cr1_read_indexed(chp, offs)
	struct channel_softc *chp;
	bus_addr_t offs;
{

	offs += wd_base;
	bus_space_write_1(chp->ctl_iot, chp->cmd_ioh, wd_cridx, offs);
	return bus_space_read_1(chp->ctl_iot, chp->cmd_ioh, wd_crdata);
}
					
void
wdc_cr1_write_plain(chp, offs, data)
	struct channel_softc *chp;
	bus_addr_t offs;
	u_int8_t data;
{

	bus_space_write_1(chp->ctl_iot, chp->ctl_ioh, offs, data);
}

void
wdc_cr1_write_indexed(chp, offs, data)
	struct channel_softc *chp;
	bus_addr_t offs;
	u_int8_t data;
{

	offs += wd_base;
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_cridx, offs);
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_crdata, data);
}

/*********************************************************
 * data access methods (multi)
 *********************************************************/
/* generic 16 or 32 bits multi read */
void
wdc_data_read_generic(chp, bufp, cnt, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int cnt;
	int flags;
{

	if ((flags & DRIVE_CAP32) != 0 && (cnt & 0x3) == 0)
		bus_space_read_multi_4(chp->data32iot, chp->data32ioh,
			wd_data, (u_int32_t *) bufp, cnt >> 2); 
	else
		bus_space_read_multi_2(chp->data16iot, chp->data16ioh,
			wd_data, (u_int16_t *) bufp, cnt >> 1);
}

void
wdc_data_write_generic(chp, bufp, cnt, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int cnt;
	int flags;
{

	if ((flags & DRIVE_CAP32) != 0 && (cnt & 0x3) == 0)
		bus_space_write_multi_4(chp->data32iot, chp->data32ioh,
			wd_data, (u_int32_t *) bufp, cnt >> 2);
	else
		bus_space_write_multi_2(chp->data16iot, chp->data16ioh,
			wd_data, (u_int16_t *) bufp, cnt >> 1);
}
