/*	$NecBSD: ser_console_pc98_Cbus.c,v 1.3 1999/04/15 01:36:17 kmatsuda Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>
#include <machine/syspmgr.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/ser_console.h>
#include <i386/Cbus/dev/serial/ser_pc98_Cbus.h>

#include <sys/conf.h>

#include "serf.h"
#include "seri.h"
#include "sern.h"

#define	comconshwdecl(c, name)	extern struct com_hw name
#define	comconsdecl(c, name)	extern struct comcons_switch name
#define	comconsent(c, name)	(c > 0 ? &##name : NULL)

comconshwdecl(NSERF,com_hw_i8251);
comconshwdecl(NSERI,com_hw_i8251);
comconshwdecl(NSERN,com_hw_NEC_2nd);
comconsdecl(NSERF,serfcons_switch);
comconsdecl(NSERI,sericons_switch);
comconsdecl(NSERN,serncons_switch);

struct comcons_switch_machdep {
	struct com_hw *csm_dfhw;
	bus_space_tag_t csm_dftag;
	bus_addr_t csm_dfbase;
};

struct comcons_switch *comcons_switch[] = {
	comconsent(NSERF,serfcons_switch),
	comconsent(NSERI,sericons_switch),
	comconsent(NSERN,serncons_switch),
	NULL,
};

struct comcons_switch_machdep comcons_switch_machdep[] = {
	{comconsent(NSERF,com_hw_i8251), I386_BUS_SPACE_IO, 0x30},
	{comconsent(NSERI,com_hw_i8251), I386_BUS_SPACE_IO, 0x30},
	{comconsent(NSERN,com_hw_NEC_2nd), I386_BUS_SPACE_IO, 0x238},
	{NULL, 0},
};

#define	COMCONS_TBLSIZE \
	(sizeof(comcons_switch) / sizeof(struct comcons_switch  *))

static bus_addr_t comcons_iobase;

struct com_softc *
ser_console_probe_machdep(cp, ca)
	struct consdev *cp;
	struct commulti_attach_args *ca;
{
	bus_space_tag_t iot, icrt, msrt;
	bus_space_handle_t ioh, spioh, icrh, msrh;
	struct com_space_handle *csp = &ca->ca_h;
	struct com_softc *sc = NULL;
	struct comcons_switch_machdep *ccsmp;
	struct comcons_switch *ccsp;
	struct com_switch *cswp;
	struct com_hw *hw;
	bus_addr_t iobase;
	int target, rv;

	for (target = 0; target < COMCONS_TBLSIZE; target ++)
	{
		ccsp = comcons_switch[target];
		if (ccsp == NULL)
			continue;

 		sc = (*ccsp->comcnmalloc) ();
		if (sc == NULL)
			continue;

		cswp = sc->sc_cswp;
		if (sc->sc_ccswp != ccsp)
			continue;
		ccsmp = &comcons_switch_machdep[target];

		spioh = NULL;
		iot = ccsmp->csm_dftag;
		iobase = ccsmp->csm_dfbase;

		hw = ccsmp->csm_dfhw;
		ser_setup_ca(ca, hw);

		if (bus_space_map(iot, iobase, 0, 0, &ioh) != 0 ||
		    bus_space_map_load(iot, ioh, hw->iatsz,
				       hw->iat, BUS_SPACE_MAP_FAILFREE) != 0)
			continue;

		if (hw->spiat != NULL &&
		    (bus_space_map(iot, iobase, 0, 0, &spioh) ||
		     bus_space_map_load(iot, spioh, hw->spiatsz,
				   hw->spiat, BUS_SPACE_MAP_FAILFREE)))
		{
			bus_space_unmap(iot, ioh, hw->iatsz);
			continue;
		}

		if (hw->type == COM_HW_I8251 || hw->type == COM_HW_I8251_F ||
		    hw->type == COM_HW_I8251_C)
		{
			if ((hw->hwflags & COM_HW_INTERNAL) != 0)
			{
				syspmgr_alloc_systmph(&msrt, &msrh,
						      SYSPMGR_PORTB);
				syspmgr_alloc_systmph(&icrt, &icrh,
						      SYSPMGR_PORTC);
			}
			else
			{
				msrt = icrt = iot;
				if (bus_space_map(msrt, iobase, 0, 1, &msrh))
				{
					bus_space_unmap(iot, ioh, hw->iatsz);
					if (spioh != NULL)
						bus_space_unmap(iot, spioh, hw->spiatsz);
					continue;
				}
				icrh = msrh;
			}
		}

		csp->cs_iot = iot;
		csp->cs_ioh = ioh;
		csp->cs_spioh = spioh;
		csp->cs_icrt = iot;
		csp->cs_icrh = icrh;
		csp->cs_msrt = msrt;
		csp->cs_msrh = msrh;

 		rv = (*ccsp->comcnprobe) (ca);
 		if (rv != 0)
		{
			comcons_iobase = iobase;
			return sc;
		}

		bus_space_unmap(iot, ioh, hw->iatsz);
		if (spioh != NULL)
			bus_space_unmap(iot, spioh, hw->spiatsz);

		if (hw->type == COM_HW_I8251 || hw->type == COM_HW_I8251_F ||
		    hw->type == COM_HW_I8251_C)
		{
			if ((hw->hwflags & COM_HW_INTERNAL) == 0)
				bus_space_unmap(icrt, icrh, 1);
		}
	}
	return NULL;
}

int
is_ser_comconsole_machdep(iot, base)
	bus_space_tag_t iot;
	bus_addr_t base;
{
	struct com_softc *sc = sercnsgsc;

	if (sc != NULL && base == comcons_iobase)
		return 1;
	return 0;
}
