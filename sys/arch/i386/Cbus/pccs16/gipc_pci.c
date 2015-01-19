/*	$NecBSD: gipc_pci.c,v 1.12.4.1 1999/08/15 17:18:07 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
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

#include "opt_pccshw.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <machine/bus.h>

#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

#include <i386/Cbus/pccs16/gipcvar.h>

static int gipc_pci_match __P((struct device *, struct cfdata *, void *));
static void gipc_pci_attach __P((struct device *, struct device *, void *));

struct cfattach gipc_pci_ca = {
	sizeof(struct gipc_softc), gipc_pci_match, gipc_pci_attach
};

static int
gipc_pci_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct gipc_attach_args *ga = aux;
	bus_space_handle_t ioh, memh;
	bus_space_tag_t iot, memt;
	int rv = 0;

	iot = ga->ga_iot;
	memt = ga->ga_memt;

	if (bus_space_map(iot, ga->ga_iobase, 2, 0, &ioh))
		return rv;
	if (bus_space_map(memt, ga->ga_maddr, ga->ga_msize, 0, &memh))
	{
		bus_space_unmap(iot, ioh, 2);
		return rv;
	}

	if (gipcprobesubr(iot, ioh) > 0)
		rv = 1;

	bus_space_unmap(memt, memh, ga->ga_msize);
	bus_space_unmap(iot, ioh, 2);
	return rv;
}

static void
gipc_pci_attach(parent, match, aux)
	struct device *parent, *match;
	void *aux;
{
	struct gipc_softc *sc = (struct gipc_softc *) match;
	struct gipc_attach_args *ga = aux;
	bus_space_handle_t ioh, memh;
	bus_space_tag_t iot, memt;
	struct pccs_attach_args sa;

	iot = ga->ga_iot;
	memt = ga->ga_memt;

	if (bus_space_map(iot, ga->ga_iobase, 2, 0, &ioh) ||
	    bus_space_map(memt, ga->ga_maddr, ga->ga_msize, 0, &memh))
		panic("%s: iomem map failed", sc->sc_dev.dv_xname);
		
	printf(": polling mode io0 0x%lx-0x%lx mem0 0x%lx-0x%lx",
		ga->ga_iobase, ga->ga_iobase + 1, 
		ga->ga_maddr, ga->ga_maddr + ga->ga_msize - 1);

	sc->sc_pp.pp_bc = ga->ga_bc;
	sc->sc_pp.pp_lowmem = sc->sc_pp.pp_startmem = NBPG;
	sc->sc_pp.pp_highmem = (1 << 24);
	sc->sc_pp.pp_lowio = 0;
	sc->sc_pp.pp_highio = (1 << 16);
	sc->sc_pp.pp_startio = 0x4000;
	sc->sc_bst = iot;
	sc->sc_bsh = ioh;
	sc->sc_busiot = iot;
	sc->sc_busmemt = memt;
	sc->sc_csbst = memt;
	sc->sc_csbsh = memh;
	sc->sc_csim.im_type = SLOT_DEVICE_SPMEM;
	sc->sc_csim.im_hwbase = ga->ga_maddr;

	sc->sc_irqmask = systm_intr_routing->sir_allow_pccs;
	sc->sc_irq = ga->ga_irq;

	gipcattachsubr(sc);

	sa.pa_16pp = &sc->sc_pp;
	sa.pa_32pp = NULL;
	config_found((void *) sc, &sa, NULL);
}
