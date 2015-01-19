/*	$NecBSD: wdc_cap.c,v 1.7 1999/04/15 01:36:04 kmatsuda Exp $	*/
/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <i386/Cbus/dev/atapi/wdcreg.h>
#include <i386/Cbus/dev/atapi/wdcvar.h>
#include <i386/Cbus/dev/atapi/atapireg.h>
#include <i386/Cbus/dev/scsi-atapi/atapivar.h>
#include <i386/Cbus/dev/atapi/wdchw.h>
#include <dev/ata/atareg.h>

void
wdc_setup_hardware_caps(chp, hwp)
	struct channel_softc *chp;
	struct wdc_hw *hwp;
{
	struct wdc_softc *wdc = chp->wdc;

	wdc->cap = hwp->hw_capability;
	wdc->pio_mode = hwp->hw_pio;
	wdc->dma_mode = hwp->hw_dma;
	wdc->udma_mode = 0;
}

void
wdc_probe_caps(drvp)
	struct ata_drive_datas *drvp;
{
	struct channel_softc *chp = drvp->chnl_softc;
	struct channel_queue *cqp = chp->ch_queue;
	struct channel_link *idec;

	for (idec = cqp->cq_link.tqh_first; idec != NULL;
	     idec = idec->wk_linkchain.tqe_next)
		if (idec->wk_drvp == drvp)
			break;

	if (idec == NULL)
	{
		printf("wdc: missing idec\n");
		return;
	}

	wdc_probe_and_setup_caps(idec, drvp->drv_softc->dv_xname);
	return;
}

	
void
wdc_probe_and_setup_caps(idec, dvname)
	struct channel_link *idec;
	const char *dvname;
{
	struct ataparams params, params2;
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	struct wdc_softc *wdc = chp->wdc;
	int i, printed;
	char *sep = "";

	drvp->drive_flags &= DRIVE;
	drvp->PIO_mode = 0;
	drvp->DMA_mode = 0;
	drvp->UDMA_mode = 0;
	if (wdc_load_params(idec, &params) != 0)
		return;

	if ((wdc->cap & (WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32)) ==
	    (WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32)) {
		/*
		 * Controller claims 16 and 32 bit transferts.
		 * Re-do an UDENTIFY with 32-bit transferts,
		 * and compare results.
		 */
		drvp->drive_flags |= DRIVE_CAP32;
		wdc_load_params(idec, &params2);
		if (bcmp(&params, &params2, sizeof(struct ataparams)) != 0)
		{
			/* Not good. fall back to 16bits */
			drvp->drive_flags &= ~DRIVE_CAP32;
		} else {
			printf("%s: using 32-bits pio transfers\n", dvname);
		}
	}

	/* An ATAPI device is at last PIO mode 3 */
	if (drvp->drive_flags & DRIVE_ATAPI)
		drvp->PIO_mode = 3;

	/*
	 * It's not in the specs, but it seems that some drive 
	 * returns 0xffff in atap_extensions when this field is invalid
	 */
	if (params.atap_extensions != 0xffff &&
	    (params.atap_extensions & WDC_EXT_MODES)) {
		printed = 0;
		/*
		 * XXX some drives report something wrong here (they claim to
		 * support PIO mode 8 !). As mode is coded on 3 bits in
		 * SET FEATURE, limit it to 7 (so limit i to 4).
		 */
		for (i = 4; i >= 0; i--) {
			if ((params.atap_piomode_supp & (1 << i)) == 0)
				continue;
			/*
			 * See if mode is accepted.
			 * If the controller can't set its PIO mode,
			 * assume the defaults are good, so don't try
			 * to set it
			 */
			if ((wdc->cap & WDC_CAPABILITY_MODE) != 0)
			{
				if (wdc_set_mode(idec, WDCC_FEATURES,
						 0x08 | (i + 3), 
						 FEAT_SET_MODE) != 0)
					continue;
			}

			if (!printed) { 
				printf("%s: PIO mode %d", dvname, i + 3);
				sep = ",";
				printed = 1;
			}
			/*
			 * If controller's driver can't set its PIO mode,
			 * get the highter one for the drive.
			 */
			if ((wdc->cap & WDC_CAPABILITY_MODE) == 0 ||
			    wdc->pio_mode >= i + 3) {
				drvp->PIO_mode = i + 3;
				break;
			}
		}
		if (!printed) {
			/* 
			 * We didn't find a valid PIO mode.
			 * Assume the values returned for DMA are buggy too
			 */
			return;
		}

		drvp->drive_flags |= DRIVE_MODE;
		printed = 0;
		for (i = 7; i >= 0; i--) {
			if ((params.atap_dmamode_supp & (1 << i)) == 0)
				continue;
			if ((wdc->cap & WDC_CAPABILITY_DMA) &&
			    (wdc->cap & WDC_CAPABILITY_MODE))
				if (wdc_set_mode(idec, WDCC_FEATURES,
				                 0x20 | i, FEAT_SET_MODE) != 0)
					continue;

			if (!printed) {
				printf("%s DMA mode %d", sep, i);
				sep = ",";
				printed = 1;
			}
			if (wdc->cap & WDC_CAPABILITY_DMA) {
				if ((wdc->cap & WDC_CAPABILITY_MODE) &&
				    wdc->dma_mode < i)
					continue;
				drvp->DMA_mode = i;
				drvp->drive_flags |= DRIVE_DMA;
			}
			break;
		}

		printed = 0;
		if (params.atap_extensions & WDC_EXT_UDMA_MODES) {
			for (i = 7; i >= 0; i--) {
				if ((params.atap_udmamode_supp & (1 << i))
				    == 0)
					continue;
				if ((wdc->cap & WDC_CAPABILITY_MODE) &&
				    (wdc->cap & WDC_CAPABILITY_UDMA))
					if (wdc_set_mode(idec, WDCC_FEATURES,
							 0x40 | i,
				                         FEAT_SET_MODE) != 0)
						continue;
				if (!printed) {
					printf("%s UDMA mode %d", sep, i);
					sep = ",";
					printed = 1;
				}
				/*
				 * ATA-4 specs says if a mode is supported,
				 * all lower modes shall be supported.
				 * No need to look further.
				 */
				if (wdc->cap & WDC_CAPABILITY_UDMA) {
					if ((wdc->cap & WDC_CAPABILITY_MODE) &&
					    wdc->udma_mode < i)
						continue;
					drvp->UDMA_mode = i;
					drvp->drive_flags |= DRIVE_UDMA;
				}
				break;
			}
		}
		printf("\n");
	}
}
