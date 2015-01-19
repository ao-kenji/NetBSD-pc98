/*	$NecBSD: ser_isa.c,v 1.2 1999/07/26 06:32:07 honda Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>
#include <machine/systmbusvar.h>
#include <machine/syspmgr.h>

#include <dev/isa/isavar.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/servar.h>
#include <i386/Cbus/dev/serial/ser_console.h>
#include <i386/Cbus/dev/serial/ser_pc98_Cbus.h>

static void ser_isa_attach __P((struct device *, struct device *, void *));
static int ser_isa_match __P((struct device *, struct cfdata *, void *));
static int ser_systmmsg __P((struct device *, systm_event_t));
static int ser_isa_map __P((struct com_space_handle *, struct com_hw *, bus_addr_t));
static int ser_isa_unmap __P((struct com_space_handle *, struct com_hw *));
static int ser_isa_emulintr __P((void *));

struct cfattach ser_isa_ca = {
	sizeof(struct ser_softc), ser_isa_match, ser_isa_attach
};

int
ser_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct commulti_attach_args ca;
	bus_space_tag_t iot = ia->ia_iot;
	bus_addr_t iobase = ia->ia_iobase;
	struct com_hw *hw;
	struct device dev;
	struct cfdata *cf;


	if (ia->ia_irq == IRQUNK || ia->ia_iobase == IOBASEUNK)
		return 0;

	hw = ser_find_hw(ia->ia_cfgflags);
	if (hw == NULL)
		return 0;

	memset(&ca, 0, sizeof(ca));
	ca.ca_h.cs_iot = iot;
	ca.ca_cfgflags = ia->ia_cfgflags;
	ser_setup_ca(&ca, hw);

	if (is_ser_comconsole_machdep(iot, iobase) != 0)
	{
		ca.ca_h = comconsole_cs;
		ca.ca_console = 1;
	}
	else if (ser_isa_map(&ca.ca_h, hw, iobase) != 0)
		return 0;

	memset(&dev, 0, sizeof(dev));			/* XXX */
	dev.dv_cfdata = match;				/* XXX */
	cf = config_search(NULL, &dev, &ca);

	if (ca.ca_console == 0)
		ser_isa_unmap(&ca.ca_h, hw);

	if (cf != NULL)
		ia->ia_iosize = hw->nports;
	return (cf != NULL) ? 1 : 0;
}

void
ser_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ser_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t sbst = NULL, iot = ia->ia_iot;
	bus_space_handle_t sbsh = NULL;
	struct commulti_attach_args ca;
	struct com_softc *csc = NULL;
	struct com_switch *cswp = NULL;
	struct com_hw *hw;
	bus_addr_t iobase;
	int (*intrfunc) __P((void *)); 
	void *arg;
	int sno;

	printf("\n");

	systmmsg_bind(self, ser_systmmsg);

	hw = ser_find_hw(ia->ia_cfgflags);
	for (sno = 0; sno < hw->maxunit; sno ++)
	{
		memset(&ca, 0, sizeof(ca));
		ser_setup_ca(&ca, hw);
		ca.ca_slave = sno;
		ca.ca_h.cs_iot = iot;
		ca.ca_cfgflags = ia->ia_cfgflags;
		ca.ca_emulintr = ser_isa_emulintr;
		ca.ca_arg = (void *) ia->ia_irq;
		ca.ca_console = 0;

		iobase = ia->ia_iobase + hw->skip * sno;
		if (is_ser_comconsole_machdep(iot, iobase) != 0)
		{
			ca.ca_h = comconsole_cs;
			ca.ca_console = 1;
		}
		else
		{
			if (ser_isa_map(&ca.ca_h, hw, iobase) != 0)
			{
				if (sno != 0)
					continue;
				panic("%s: load map", sc->sc_dev.dv_xname);
			}
		}

		if (sno == 0)
		{
			sbst = ca.ca_h.cs_iot;
			sbsh = ca.ca_h.cs_ioh;
		}
		sc->sc_tbst = sbst;
		sc->sc_tbsh = sbsh;
		sc->sc_ibst = sbst;
		sc->sc_ibsh = sbsh;

		(void) config_found(self, &ca, serprint);
		if (ca.ca_m == NULL)
		{
 			if (ca.ca_console == 0)
				ser_isa_unmap(&ca.ca_h, hw);
			continue;
		}

		if (sno == 0)
		{
			csc = ca.ca_m;
			cswp = csc->sc_cswp;
		}
		sc->sc_slaves[sno] = ca.ca_m;
	}

	if (cswp == NULL)
		return;

	/* attach interrupt handler */
	sc->sc_intr = cswp->cswt_intr;
	if (sc->sc_start != NULL || hw->maxunit > 1)
	{
		intrfunc = serintr;
		arg = sc;
	}
	else
	{
		intrfunc = sc->sc_intr;
		arg = csc;
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic,
		ia->ia_irq, IST_EDGE, cswp->cswt_ipl, intrfunc, arg);

	if (hw->init != NULL)
		(*hw->init) (csc, ia->ia_iobase, ia->ia_irq);
}

static int
ser_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct ser_softc *sc = (struct ser_softc *) dv;
	struct com_softc *csc;
	register int i;

	for (i = 0; i < NSLAVES; i++)
	{
		csc = sc->sc_slaves[i];
		if (csc == NULL)
			continue;

		switch (ev)
		{
		case SYSTM_EVENT_SUSPEND:
			if (csc->sc_sertty_deactivate != NULL)
				csc->sc_sertty_deactivate(csc);
			break;

		case SYSTM_EVENT_RESUME:
			if (csc->sc_sertty_activate != NULL)
				csc->sc_sertty_activate(csc);
			break;
		}
	}

	return 0;
}

static int
ser_isa_map(cs, hw, iobase)
	struct com_space_handle *cs;
	struct com_hw *hw;
	bus_addr_t iobase;
{
	bus_space_tag_t iot = cs->cs_iot, icrt, msrt;
	bus_space_handle_t ioh, spioh = NULL, icrh = NULL, msrh = NULL;
	int error = ENOSPC;

	if (bus_space_map(iot, iobase, 0, 0, &ioh) != 0 ||
	    bus_space_map_load(iot, ioh, hw->iatsz, hw->iat,
		BUS_SPACE_MAP_FAILFREE) != 0)
		return error;

	if (hw->spiat != NULL &&
	    (bus_space_map(iot, iobase, 0, 0, &spioh) != 0 ||
	     bus_space_map_load(iot, spioh, hw->spiatsz, hw->spiat, 
			        BUS_SPACE_MAP_FAILFREE) != 0))
	{
		bus_space_unmap(iot, ioh, hw->iatsz);
		return error;
	}

	if (hw->type == COM_HW_I8251 || hw->type == COM_HW_I8251_F ||
	    hw->type == COM_HW_I8251_C)
	{
		if ((hw->hwflags & COM_HW_INTERNAL) != 0)
		{
			syspmgr_alloc_systmph(&msrt, &msrh, SYSPMGR_PORTB);
			syspmgr_alloc_systmph(&icrt, &icrh, SYSPMGR_PORTC);
		}
		else
		{
			msrt = icrt = iot;
			if (bus_space_map(msrt, iobase, 0, 1, &msrh) != 0)
			{
				bus_space_unmap(iot, ioh, hw->iatsz);
				if (spioh != NULL)
					bus_space_unmap(iot, spioh, hw->spiatsz);
				return error;
			}
			icrh = msrh;
		}
	}

	cs->cs_ioh = ioh;
	cs->cs_spioh = spioh;
	cs->cs_icrt = icrt;
	cs->cs_icrh = icrh;
	cs->cs_msrt = msrt;
	cs->cs_msrh = msrh;
	return 0;
}


static int
ser_isa_unmap(cs, hw)
	struct com_space_handle *cs;
	struct com_hw *hw;
{
	bus_space_tag_t iot = cs->cs_iot;
	bus_space_handle_t ioh = cs->cs_ioh;
	bus_space_handle_t spioh = cs->cs_spioh;

	bus_space_unmap(iot, ioh, hw->iatsz);
	if (spioh != NULL)
		bus_space_unmap(iot, spioh, hw->spiatsz);
	if (hw->type == COM_HW_I8251 || hw->type == COM_HW_I8251_F ||
	    hw->type == COM_HW_I8251_C)
	{
		if ((hw->hwflags & COM_HW_INTERNAL) == 0)
			bus_space_unmap(cs->cs_icrt, cs->cs_icrh, 1);
	}
	return 0;
}

static int
ser_isa_emulintr(arg)
	void *arg;
{
	u_long irq = (u_long) arg;

	softintr(irq);
	return 1;
}
