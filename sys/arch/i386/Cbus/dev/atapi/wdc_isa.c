/*	$NecBSD: wdc_isa.c,v 1.30 1999/07/26 06:31:58 honda Exp $	*/
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
#include <machine/systmbusvar.h>

#include <i386/Cbus/dev/atapi/wdcreg.h>
#include <i386/Cbus/dev/atapi/wdcvar.h>
#include <i386/Cbus/dev/atapi/atapireg.h>
#include <i386/Cbus/dev/scsi-atapi/atapivar.h>
#include <i386/Cbus/dev/atapi/wdchw.h>
#include <i386/Cbus/dev/atapi/wdc_pc98_Cbus.h>

#define	WDC_ISA_MAXMSIZE (NBPG)

static int wdc_isa_match __P((struct device *, struct cfdata *, void *));
static void wdc_isa_attach __P((struct device *, struct device *, void *));
static int wdc_systmmsg __P((struct device *, systm_event_t));
static void wdc_init_isa_channel_softc __P((struct channel_softc *,
			       struct isa_attach_args *, struct wdc_pc98_hw *,
			       bus_space_handle_t, bus_space_handle_t,
			       bus_space_handle_t));
int wdc_isa_map __P((bus_space_handle_t *, bus_space_handle_t *, bus_space_handle_t *, bus_size_t *, struct wdc_pc98_hw *, struct isa_attach_args *));
static int wdc_isa_probe_NECbank __P((struct channel_softc *wdc));
static void wdc_select_bank_NEC __P((struct channel_softc *, u_int));

struct wdc_isa_softc {
	struct wdc_softc sc_wdc;
	struct channel_softc sc_channel;
};

struct cfattach wdc_isa_ca = {
	sizeof(struct wdc_isa_softc), wdc_isa_match, wdc_isa_attach
};

static int
wdc_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct wdc_isa_softc wdc_isa_probe_sc;
	struct wdc_isa_softc *wdc = &wdc_isa_probe_sc;
	struct channel_softc *chp = &wdc->sc_channel;
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh0, ioh1, memh;
	struct wdc_pc98_hw *hw;
	bus_size_t msize;
	int rv;

	if (ia->ia_iobase == IOBASEUNK || ia->ia_irq == IRQUNK)
		return 0;

	hw = DVCFG_HW(&wdc_pc98_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (hw == NULL)
		return 0;

	if (wdc_isa_map(&ioh0, &ioh1, &memh, &msize, hw, ia))
		return 0;

	memset(chp, 0, sizeof(*chp));
	chp->wdc = &wdc->sc_wdc;
	wdc_init_isa_channel_softc(chp, ia, hw, ioh0, ioh1, memh);

	if (ia->ia_iobase == NEC_IDE_PADDR)
		(void) wdc_isa_probe_NECbank(chp);

	if ((rv = wdcprobesubr(chp)) != 0)
	{
		ia->ia_iosize = hw->hw_iosz;
		ia->ia_msize = msize;
	}

	if (memh != NULL)
		bus_space_unmap(ia->ia_memt, memh, msize);
	if (chp->ch_bkbsh != NULL)
		bus_space_unmap(chp->ch_bkbst, chp->ch_bkbsh, 1);
	bus_space_unmap(ia->ia_iot, ioh0, hw->hw_iat0sz);
	bus_space_unmap(ia->ia_iot, ioh1, hw->hw_iat1sz);
	return rv;
}

void
wdc_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wdc_isa_softc *wdc = (void *) self;
	struct channel_softc *chp;
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh0, ioh1, memh;
	struct wdc_pc98_hw *hw;
	bus_size_t msize;

	systmmsg_bind(self, wdc_systmmsg);

	chp = &wdc->sc_channel;
	chp->wdc = &wdc->sc_wdc;

	hw = DVCFG_HW(&wdc_pc98_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (wdc_isa_map(&ioh0, &ioh1, &memh, &msize, hw, ia) != 0)
	{
		printf("%s: can not map\n", chp->wdc->sc_dev.dv_xname);
		return;
	}

	wdc_init_isa_channel_softc(chp, ia, hw, ioh0, ioh1, memh);

	if (ia->ia_iobase == NEC_IDE_PADDR)
		(void) wdc_isa_probe_NECbank(chp);

	wdcattachsubr(chp);

	chp->wdc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
				             IPL_BIO, wdcintr, chp);
}

static int
wdc_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct wdc_isa_softc *wdc = (struct wdc_isa_softc *) dv;
	struct channel_softc *chp = &wdc->sc_channel;
	int error = 0;

	switch (ev)
	{
	case SYSTM_EVENT_QUERY_SUSPEND:
		if (wdc_is_busy(chp) != 0)
			error = SYSTM_EVENT_STATUS_BUSY;
		break;

	case SYSTM_EVENT_SUSPEND:
		if (wdc_deactivate(chp) != 0)
			error = SYSTM_EVENT_STATUS_BUSY;
		break;

	case SYSTM_EVENT_RESUME:
		if (wdc_activate(chp) != 0)
			error = SYSTM_EVENT_STATUS_FERR;
		break;

	default:
		break;
	}

	return error;
}

static void
wdc_init_isa_channel_softc(chp, ia, hwp, ioh0, ioh1, memh)
	struct channel_softc *chp;
	struct isa_attach_args *ia;
	struct wdc_pc98_hw *hwp;
	bus_space_handle_t ioh0, ioh1, memh;
{
	u_int f;

	/* Generic initialization */
	wdc_init_channel_softc(chp, &hwp->wdc_hw);
	wdc_setup_hardware_caps(chp, &hwp->wdc_hw);

	/* Setup bus io and mem space handles */
	chp->cmd_iot = chp->ctl_iot = ia->ia_iot;
	chp->data16iot = chp->data32iot = chp->cmd_iot;
	chp->ch_bkbst = ia->ia_iot;
	chp->ch_memt = ia->ia_memt;

	chp->cmd_ioh = ioh0;
	chp->ctl_ioh = ioh1;
	chp->data16ioh = chp->data32ioh = chp->cmd_ioh;
	chp->ch_memh = memh;

	chp->ch_vsize = ia->ia_msize;

	/* Flags overwrite */
	f = (DVCFG_MINOR(ia->ia_cfgflags) & WDC_ACCESS_USER) |
	    hwp->wdc_hw.hw_access;
	if ((f & WDC_ACCESS_BRESET) != 0)
		chp->ch_flags |= WDCF_BRESET;
}

int
wdc_isa_map(ioh0p, ioh1p, memhp, msizep, hw, ia)
	bus_space_handle_t *ioh0p, *ioh1p, *memhp;
	bus_size_t *msizep;
	struct wdc_pc98_hw *hw;
	struct isa_attach_args *ia;
{
	bus_space_tag_t iot = ia->ia_iot, memt = ia->ia_memt;
	bus_addr_t maddr = ia->ia_maddr;
	bus_addr_t iobase = ia->ia_iobase;
	bus_size_t msize = ia->ia_msize;

	*memhp = NULL;
	*msizep = 0;
	if ((hw->wdc_hw.hw_access & WDC_ACCESS_MEM) != 0 && maddr == MADDRUNK)
		return EINVAL;
	     
	if (bus_space_map(iot, iobase + hw->hw_ofs0, 0, 0, ioh0p) ||
	    bus_space_map_load(iot, *ioh0p, hw->hw_iat0sz, hw->hw_iat0,
			       BUS_SPACE_MAP_FAILFREE))
		return ENOSPC;

	if (bus_space_map(iot, iobase + hw->hw_ofs1, 0, 0, ioh1p) ||
	    bus_space_map_load(iot, *ioh1p, hw->hw_iat1sz, hw->hw_iat1,
			       BUS_SPACE_MAP_FAILFREE))
	{
		bus_space_unmap(iot, *ioh0p, hw->hw_iat0sz);
		return ENOSPC;
	}

	if (maddr == MADDRUNK)
		return 0;

	if (msize >= WDC_ISA_MAXMSIZE)
		msize = WDC_ISA_MAXMSIZE;
	if(bus_space_map(memt, maddr, msize, 0, memhp))
	{
		bus_space_unmap(iot, *ioh0p, hw->hw_iat0sz);
		bus_space_unmap(iot, *ioh1p, hw->hw_iat1sz);
		return ENOSPC;
	}

	*msizep = msize;
	return 0;
}

/*************************************************************
 * NEC bank select
 *************************************************************/
static void
wdc_select_bank_NEC(chp, bank)
	struct channel_softc *chp;
	u_int bank;
{

	if (bank == WDC_BANK_UNKNOWN)
		return;

	bus_space_write_1(chp->ch_bkbst, chp->ch_bkbsh, 0, WDC_BANK_NO(chp, bank));
}

static int
wdc_isa_probe_NECbank(chp)
	struct channel_softc *chp;
{
	u_int8_t st1, st2;
	bus_space_tag_t bkbst = chp->ch_bkbst;
	bus_space_handle_t bkbsh;
	int cnt, error;

	if (bus_space_map(bkbst, NEC_BANK_PADDR, 1, 0, &bkbsh))
		return EINVAL;

	chp->ch_bank = 1;
	chp->ch_bkbsh = bkbsh;
	chp->ch_hwfuncs.banksel = wdc_select_bank_NEC;
	wdc_select_bank(chp, 0);
	st1 = wdc_cr0_read(chp, wd_status);
	wdc_select_bank(chp, WDC_SECOND_BANK);
	st2 = wdc_cr0_read(chp, wd_status);
	if (st1 != st2)
		return 0;

	if (wdc_inquire_idedrv(chp, 0) == ENXIO)
		goto bad;

	wdc_select_bank(chp, 0);
	wdc_cr0_write(chp, wd_sdh, WDSD_IBM | 0);
	wdc_cr0_write(chp, wd_command, WDCC_IDENTIFY);
	if ((error = wait_for_drq(chp)) < 0)
		goto bad;

	st1 = wdc_cr0_read(chp, wd_status);
	wdc_select_bank(chp, WDC_SECOND_BANK);
	st2 = wdc_cr0_read(chp, wd_status);
	wdc_select_bank(chp, 0);
	if (error == 0)
	{
		for (cnt = 0; cnt < DEV_BSIZE / 2; cnt ++)
			(void) bus_space_read_2(chp->data16iot,
						chp->data16ioh, wd_data);
	}
	wait_for_ready(chp);

	if (st1 != st2)
		return 0;

bad:
	bus_space_unmap(bkbst, chp->ch_bkbsh, 1);
	chp->ch_hwfuncs.banksel = NULL;
	chp->ch_bkbsh = NULL;
	chp->ch_bank = 0;
	return EINVAL;
}
