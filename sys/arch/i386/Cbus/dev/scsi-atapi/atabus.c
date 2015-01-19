/*	$NecBSD: atabus.c,v 1.14 1999/07/26 06:32:04 honda Exp $	*/
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
 * Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>

#include <i386/Cbus/dev/atapi/wdcreg.h>
#include <i386/Cbus/dev/atapi/wdcvar.h>
#include <i386/Cbus/dev/atapi/atapireg.h>
#include <i386/Cbus/dev/scsi-atapi/atapivar.h>
#include <i386/Cbus/dev/atapi/wdchw.h>

#include "locators.h"
#include "atapidrv.h"

static int atabusmatch __P((struct device *, struct cfdata *, void *));
static void atabusattach __P((struct device *, struct device *, void *));
static int atabusprint __P((void *, const char *));

struct cfattach atabus_ca = {
	sizeof(struct atabus_softc), atabusmatch, atabusattach
};

extern struct cfdriver atabus_cd;

#ifdef	ATAPI_SHOW_MSG
#define	PRINTF(msg)	printf((msg))
#else	/* !ATAPI_SHOW_MSG */
#define	PRINTF(msg)
#endif	/* !ATAPI_SHOW_MSG */

/*************************************************************
 * ATTACH PROBE
 ************************************************************/
static struct channel_link *atabus_alloc_channel_link __P((struct  channel_softc *, struct atabus_softc *, struct ata_drive_datas *, int));

static struct channel_link *
atabus_alloc_channel_link(chp, ata, drvp, chan)
	struct channel_softc *chp;
	struct atabus_softc *ata;
	struct ata_drive_datas *drvp;
	int chan;
{
	struct channel_link *idec;
	struct channel_queue *cqp = chp->ch_queue;

	idec = (struct channel_link *) malloc(sizeof(*idec), M_DEVBUF, M_NOWAIT);
	if (idec == NULL)
		panic("%s: no mem\n", ata->sc_dev.dv_xname);

	memset(idec, 0, sizeof(*idec));
	idec->wk_chp = chp;
	idec->wk_ata = ata;
	idec->wk_chan = chan;
	idec->wk_drvp = drvp;
	TAILQ_INSERT_TAIL(&cqp->cq_link, idec, wk_linkchain);

	return idec;
}

static int
atabusmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct wdc_attach_args *wa = aux;
	struct channel_softc *chp = wa->wa_chp;
	struct cfdata *cf = match;

	if (cf->cf_loc[WDC_ATABUSCF_BANK] != WDC_ATABUSCF_BANK_DEFAULT &&
	    cf->cf_loc[WDC_ATABUSCF_BANK] != wa->wa_bank)
		return 0;

	if (cf->cf_loc[WDC_ATABUSCF_CHANNEL] != WDC_ATABUSCF_CHANNEL_DEFAULT &&
	    cf->cf_loc[WDC_ATABUSCF_CHANNEL] != chp->channel)
		return 0;

	if (wa->wa_bank >= ATABUS_MAXBANK || 
	    wa->wa_maxunit > ATABUS_DRIVE_PER_BANK)
		return 0;
	return 1;
}

int
atabusprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct ata_atapi_attach *aa = aux;
	struct ata_drive_datas *drvp = aa->aa_drv_data;
	struct channel_softc *chp = drvp->chnl_softc;

	printf(" channel %d drive %d", chp->channel, drvp->drive);
	return (UNCONF);
}

#define	ATABUS_MAX_LUN	1	/* XXX */

void
atabusscan(chp)
	struct  channel_softc *chp;
{
	struct atabus_softc *ata = chp->ch_ata;
	struct ata_atapi_attach aatab;
	struct ata_atapi_attach *aa = &aatab;
	struct ata_drive_datas *drvp = NULL;
	struct channel_link *idec;
	int error, chan, drive;
	struct cfdata *m = NULL, *atapibus_found = NULL;

	/* search atapibus */
	memset(aa, 0, sizeof(*aa));
	if (ata->sc_nbus == 0)
	{
		aa->aa_type = T_ATAPI;
		aa->aa_channel = chp->channel;
		atapibus_found = config_search(NULL, (struct device *) ata, aa);
		if (atapibus_found != NULL)
			ata->sc_nbus = 1;
	}

	/* 
	 * search devices
	 */ 
	memset(aa, 0, sizeof(*aa));
	aa->aa_channel = chp->channel;
	aa->aa_openings = 1;
	for (drive = 0; drive < ata->sc_maxunit; drive ++)
	{
		chan = WDC_BANK_TO_CHAN(ata->sc_bank, drive);
		if (ata->sc_attach & (1 << drive))
			continue;

		drvp = &chp->ch_drive[chan];
		drvp->drive = drive;
		drvp->chnl_softc = chp;
		aa->aa_drv_data = drvp;
		
		error = wdc_inquire_idedrv(chp, chan);
		switch (error)
		{
		case ENXIO:
			PRINTF("not found\n");
			continue;

		case EIO:
		default:
			error = wdc_inquire_device(chp, chan);
			switch (error)
			{
			case ENXIO:
				PRINTF("not found\n");
				continue;

			default:
			case EIO:
				PRINTF("unknown\n");
				continue;

			case 0:
				break;
			}
			PRINTF("ATAPI\n");
			aa->aa_type = T_ATAPI;
			break;

		case 0:
			PRINTF("(E)IDE\n");
			aa->aa_type = T_ATA;
			break;
		}

		ata->sc_attach |= (1 << drive);
		if (aa->aa_type == T_ATAPI)
		{
#if	NATAPIDRV > 0
			idec = atabus_alloc_channel_link(chp, ata, drvp, chan);
			ata->sc_idec[drive] = idec;
			drvp->drive_flags |= DRIVE_ATAPI;
			wdc_link_atapi(idec);
#endif	/* NATAPIDRV > 0*/
		}
		else
		{
			drvp->drive_flags |= DRIVE_ATA;
			m = config_search(NULL, (void *) ata, aa);
			if (m == NULL)
				continue;

			idec = atabus_alloc_channel_link(chp, ata, drvp, chan);
			ata->sc_idec[drive] = idec;
			config_attach((struct device *) ata, m, aa, atabusprint);
		}
	}

#if	NATAPIDRV > 0
	if (atapibus_found != NULL)
	{
		memset(aa, 0, sizeof(*aa));
		chan = ata->sc_bank * ATABUS_DRIVE_PER_BANK;
		aa->aa_type = T_ATAPI;
		aa->aa_channel = chp->channel;
		aa->aa_drv_data = &chp->ch_drive[chan];
		(void) atabus_scsipi_establish(ata, chp, aa);
		(void) config_attach((struct device *) ata, atapibus_found,
				     aa, NULL);
	}
#endif	/* NATAPIDRV > 0 */
}

void
atabusattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct atabus_softc *ata = (void *) self;
	struct wdc_attach_args *wa = aux;
	struct channel_softc *chp = wa->wa_chp;

	printf("\n");

	chp->ch_ata = ata;
	ata->sc_maxunit = wa->wa_maxunit;
	ata->sc_bank = wa->wa_bank;

	atabusscan(chp);
}
