/*	$NecBSD: wdc_pisa.c,v 1.43 1999/07/26 06:31:58 honda Exp $	*/
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
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
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

#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <i386/Cbus/dev/atapi/wdcreg.h>
#include <i386/Cbus/dev/atapi/wdcvar.h>
#include <i386/Cbus/dev/atapi/atapireg.h>
#include <i386/Cbus/dev/scsi-atapi/atapivar.h>
#include <i386/Cbus/dev/atapi/wdchw.h>
#include <i386/Cbus/dev/atapi/wdc_pc98_Cbus.h>

#define	WDC_PISA_MAXMSIZE	0x2000

static int wdc_pisa_activate __P((pisa_device_handle_t));
static int wdc_pisa_deactivate __P((pisa_device_handle_t));
static int wdc_pisa_notify __P((pisa_device_handle_t, pisa_event_t));
static int wdc_pisa_match __P((struct device *, struct cfdata *, void *));
static void wdc_pisa_attach __P((struct device *, struct device *, void *));
static void wdc_init_pisa_channel_softc __P((struct channel_softc *,
			      struct pisa_attach_args *, struct wdc_pc98_hw *,
			      bus_space_handle_t, bus_space_handle_t,
			      bus_space_handle_t));
static struct wdc_pc98_hw *wdc_find_hw __P((pisa_device_handle_t));
int wdc_space_map __P((struct pisa_attach_args *, struct wdc_pc98_hw *, bus_space_handle_t *, bus_space_handle_t *, bus_space_handle_t *));

extern struct dvcfg_hwsel wdc_pc98_hwsel;

struct wdc_pisa_softc {
	struct wdc_softc sc_wdc;
	struct channel_softc sc_channel;
};

struct cfattach wdc_pisa_ca = {
	sizeof(struct wdc_pisa_softc), wdc_pisa_match, wdc_pisa_attach
};

struct pisa_functions wdc_pd = {
	wdc_pisa_activate, wdc_pisa_deactivate, NULL, NULL, wdc_pisa_notify
};

static int
wdc_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct wdc_pisa_softc wdc_pisa_probe_sc;
	struct wdc_pisa_softc *wdc = &wdc_pisa_probe_sc;
	struct channel_softc *chp;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t ioh0, ioh1, memh;
	struct wdc_pc98_hw *hw;
	int rv;

	hw = wdc_find_hw(dh);
	if (hw == NULL)
		return 0;

	if (wdc_space_map(pa, hw, &ioh0, &ioh1, &memh) != 0)
		return 0;

	memset(wdc, 0, sizeof(*wdc));
 	chp = &wdc->sc_channel;
	chp->wdc = &wdc->sc_wdc;

	wdc_init_pisa_channel_softc(chp, pa, hw, ioh0, ioh1, memh);
	rv = wdcprobesubr(chp);

	if (memh != NULL)
		pisa_space_unmap(dh, pa->pa_memt, memh);
	if (hw->hw_nmaps == 1)
		bus_space_unmap(pa->pa_iot, ioh1, hw->hw_iat1sz);
	else if (hw->hw_nmaps == 2)
		pisa_space_unmap(dh, pa->pa_iot, ioh1);
	pisa_space_unmap(dh, pa->pa_iot, ioh0);
	return rv;
}

void
wdc_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wdc_pisa_softc *wdc = (void *) self;
	struct channel_softc *chp;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t ioh0, ioh1, memh;
	struct wdc_pc98_hw *hw;
	int rv;

	chp = &wdc->sc_channel;
	chp->wdc = &wdc->sc_wdc;

	pisa_register_functions(dh, self, &wdc_pd);

	hw = wdc_find_hw(dh);
	rv = wdc_space_map(pa, hw, &ioh0, &ioh1, &memh);
	if (rv != 0)
	{
		printf("%s: can not map\n", chp->wdc->sc_dev.dv_xname);
		return;
	}

	wdc_init_pisa_channel_softc(chp, pa, hw, ioh0, ioh1, memh);
	wdcattachsubr(chp);

	chp->wdc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_BIO,
					      wdcintr, chp);
}

/********************************************************
 * interface funcs
 ********************************************************/
static int
wdc_pisa_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct wdc_pisa_softc *wdc = (void *) PISA_DEV_SOFTC(dh);
	struct channel_softc *chp = &wdc->sc_channel;

	return wdc_deactivate(chp);
}

static int
wdc_pisa_activate(dh)
	pisa_device_handle_t dh;
{
	struct wdc_pisa_softc *wdc = (void *) PISA_DEV_SOFTC(dh);
	struct channel_softc *chp = &wdc->sc_channel;
	int error;

	error = wdc_activate(chp);
	if (error != NULL)
		return error;

#ifdef	notyet
	if (PISA_RES_EVENT(dh) == PISA_EVENT_INSERT && 
	    (chp->ch_flags & WDCF_NEXUS) == 0)
		atabusscan(chp);
#endif	/* netyet */

	return error;
}

static int
wdc_pisa_notify(dh, ev)
	pisa_device_handle_t dh;
	pisa_event_t ev;
{
	struct wdc_pisa_softc *wdc = (void *) PISA_DEV_SOFTC(dh);
	struct channel_softc *chp = &wdc->sc_channel;

	switch(ev)
	{
	case PISA_EVENT_QUERY_SUSPEND:
		if (wdc_is_busy(chp) != 0)
			return SD_EVENT_STATUS_BUSY;
		break;

	default:
		break;	
	}
	return 0;
}

/********************************************************
 * resource map
 ********************************************************/
int
wdc_space_map(pa, hw, ioh0p, ioh1p, memhp)
	struct pisa_attach_args *pa;
	struct wdc_pc98_hw *hw;
	bus_space_handle_t *ioh0p, *ioh1p;
	bus_space_handle_t *memhp;
{
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	int error;

	*memhp = NULL;
	error = pisa_space_map_load(dh, PISA_IO0, hw->hw_iat0sz, hw->hw_iat0,
				    ioh0p);
	if (error != 0)
		return error;

	switch (hw->hw_nmaps)
	{
	case 2:
		error = pisa_space_map_load(dh, PISA_IO1, hw->hw_iat1sz,
					    hw->hw_iat1, ioh1p);
		if (error != 0)
			goto out;
		break;

	case 1:
		error = bus_space_subregion(pa->pa_iot, *ioh0p,
				hw->hw_ofs1, hw->hw_iat1sz, ioh1p);
		if (error != 0)
			goto out;
		break;

	default:
		panic("wdc_pisa_map: map corrupt");
	}

	if ((hw->wdc_hw.hw_access & WDC_ACCESS_MEM) == 0)
		return error;

	PISA_DR_IMF(dr, PISA_MEM0) |= SDIM_BUS_WIDTH16;
	error = pisa_space_map(dh, PISA_MEM0, memhp);
	if (error != 0)
	{
		if (hw->hw_nmaps == 2)
			pisa_space_unmap(dh, pa->pa_iot, *ioh1p);
		else if (hw->hw_nmaps == 1)
			bus_space_unmap(pa->pa_iot, *ioh1p, hw->hw_iat1sz);
		pisa_space_unmap(dh, pa->pa_iot, *ioh0p);
	}

	return error;

out:
	pisa_space_unmap(dh, pa->pa_iot, *ioh0p);
	return error;
}

static struct wdc_pc98_hw *
wdc_find_hw(dh)
	pisa_device_handle_t dh;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	struct wdc_pc98_hw *hw = NULL;
	extern struct wdc_pc98_hw wdc_pc98_hw_ide98;
	extern struct wdc_pc98_hw wdc_pc98_hw_integral;
	extern struct wdc_pc98_hw wdc_pc98_hw_ibm;
	extern struct wdc_pc98_hw wdc_pc98_hw_ibmuni;

	if (PISA_DR_IMS(dr, PISA_IO0) != WDC_IBM_IO0SZ &&
	    PISA_DR_IMS(dr, PISA_IO0) != WDC_IBM_IO0SZ * 2)
		return hw;

	if (DVCFG_MAJOR(PISA_DR_DVCFG(dr)) != 0)
	{
		hw = DVCFG_HW(&wdc_pc98_hwsel, DVCFG_MAJOR(PISA_DR_DVCFG(dr)));
		goto out;
	}

	switch (dh->dh_stag->st_type)
	{
	case PISA_SLOT_PCMCIA:
		if (PISA_DR_IMB(dr, PISA_IO1) != PISA_UNKVAL && 
		    PISA_DR_IMS(dr, PISA_IO1) <= WDC_IBM_IO1SZ)
		{	
			if (PISA_DR_IMS(dr, PISA_IO1) == WDC_IBM_IO1SZ)
				hw = &wdc_pc98_hw_ibm;
			else
				hw = &wdc_pc98_hw_integral;
		}

		if (PISA_DR_IMS(dr, PISA_IO0) == WDC_IBM_IO0SZ * 2 &&
		    PISA_DR_IMB(dr, PISA_IO1) == PISA_UNKVAL)
		{
			hw = &wdc_pc98_hw_ibmuni;
		}
		break;

	case PISA_SLOT_PnPISA:
		hw = &wdc_pc98_hw_ide98;
		break;
	}

out:
	if (hw != NULL && (hw->wdc_hw.hw_access & WDC_ACCESS_MEM) != 0 &&
	     PISA_DR_IMB(dr, PISA_MEM0) == PISA_UNKVAL)
		hw = NULL;

	return hw;
}

static void
wdc_init_pisa_channel_softc(chp, pa, hwp, ioh0, ioh1, memh)
	struct channel_softc *chp;
	struct pisa_attach_args *pa;
	struct wdc_pc98_hw *hwp;
	bus_space_handle_t ioh0, ioh1, memh;
{
	slot_device_res_t dr = PISA_RES_DR(pa->pa_dh);
	u_int f;

	/* Generic initialization */
	wdc_init_channel_softc(chp, &hwp->wdc_hw);
	wdc_setup_hardware_caps(chp, &hwp->wdc_hw);

	/* Setup bus io and mem space handles */
	chp->cmd_iot = chp->ctl_iot = pa->pa_iot;
	chp->data16iot = chp->data32iot = chp->cmd_iot;
	chp->ch_bkbst = pa->pa_iot;
	chp->ch_memt = pa->pa_memt;

	chp->cmd_ioh = ioh0;
	chp->ctl_ioh = ioh1;
	chp->data16ioh = chp->data32ioh = chp->cmd_ioh;
	chp->ch_memh = memh;

	chp->ch_vsize = PISA_DR_IMS(dr, PISA_MEM0);

	/* Flags overwrite */
	f = (DVCFG_MINOR(PISA_DR_DVCFG(dr)) & WDC_ACCESS_USER) | 
	    hwp->wdc_hw.hw_access;
	chp->ch_flags |= WDCF_REMOVAL;
	if ((f & WDC_ACCESS_BRESET) != 0)
		chp->ch_flags |= WDCF_BRESET;
}
