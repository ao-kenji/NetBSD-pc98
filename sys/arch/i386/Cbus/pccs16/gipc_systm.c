/*	$NecBSD: gipc_systm.c,v 1.15.4.2 1999/08/31 09:10:55 honda Exp $	*/
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

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/dvcfg.h>

#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

#include <i386/Cbus/pccs16/gipcvar.h>

static int gipc_systm_match __P((struct device *, struct cfdata *, void *));
static void gipc_systm_attach __P((struct device *, struct device *, void *));

struct cfattach gipc_systm_ca = {
	sizeof(struct gipc_softc), gipc_systm_match, gipc_systm_attach
};

#define	GIPC_SYSTM_IOBASE	(0x3e0)
#define	GIPC_SYSTM_MEMBASE	(0xc0000)

static int
gipc_systm_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct systm_attach_args *sa = aux;
	u_int dvcfg = sa->sa_cfgflags;
	bus_space_handle_t ioh;
	bus_addr_t base;
	int rv = 0;	
	
	base = GIPC_DVCFG_SELBASE(dvcfg) * 2 + GIPC_SYSTM_IOBASE;
	if (bus_space_map(sa->sa_iot, base, 2, 0, &ioh))
		return rv;

	if (gipcprobesubr(sa->sa_iot, ioh) > 0)
		rv = 1;

	bus_space_unmap(sa->sa_iot, ioh, 2);
	return rv;
}

static void
gipc_systm_attach(parent, match, aux)
	struct device *parent, *match;
	void *aux;
{
	struct systm_attach_args *sa = aux;
	struct gipc_softc *sc = (struct gipc_softc *) match;
	u_int dvcfg = sa->sa_cfgflags;
	bus_space_handle_t ioh, memh;
	bus_space_tag_t iot, memt;
	bus_size_t msize;
	bus_addr_t maddr, base;
	struct pccs_attach_args pa;

	iot = sa->sa_iot;
	memt = sa->sa_memt;

	/*
	 * find a msize memory window in range GIPC_SYSTM_MEMBASE - ISA_HOLE_END
	 */
	msize = PCCSHW_MAX_WINSIZE;
	maddr = GIPC_SYSTM_MEMBASE;
	if (bus_space_map_prefer(memt, &maddr, 0x100000, NBPG, msize))
	{
		printf("%s:no attribute memory window\n", sc->sc_dev.dv_xname);
		return;
	}
	printf(": mem0 0x%lx-0x%lx", maddr, maddr + msize - 1); 
	
	/*
	 * map io and mem 
	 */
	base = GIPC_DVCFG_SELBASE(dvcfg) * 2 + GIPC_SYSTM_IOBASE;
	if (bus_space_map(iot, base, 2, 0, &ioh) ||
	    bus_space_map(memt, maddr, msize, 0, &memh))
	{
		printf("%s: iomem map failed", sc->sc_dev.dv_xname);
		return;
	}
		
	/*
	 * setup dvcfg
	 */
	if (gipcprobesubr(iot, ioh) == 0x84)
		dvcfg |= GIPC_DVCFG_POLLING;
#ifdef	GIPC_POLLING_ONLY
	dvcfg |= GIPC_DVCFG_POLLING;
#endif	/* GIPC_POLLING_ONLY */

	/*
	 * search a free irq
	 */
	sc->sc_irqmask = systm_intr_routing->sir_allow_pccs;
	sc->sc_irq = IRQUNK;
	if ((dvcfg & GIPC_DVCFG_POLLING) == 0)
	{
		sc->sc_irq = isa_get_empty_irq(sa->sa_ic, sc->sc_irqmask);
		if (sc->sc_irq != IRQUNK)
		{
			/*
			 * Check free interrupt pins for pccards.
			 * If no irqs left, force the chip polling mode.
			 */
			if (isa_get_empty_irq(sa->sa_ic,
			    (sc->sc_irqmask & (~(1 << sc->sc_irq)))) == IRQUNK)	
			{
				sc->sc_irq = IRQUNK;
				dvcfg |= GIPC_DVCFG_POLLING;
			}
		}
	}

	if (sc->sc_irq == IRQUNK)
		printf(" polling");
	else
		printf(" pin0 %ld", sc->sc_irq);

	/*
	 * attach now!
	 */
	sc->sc_pp.pp_bc = sa->sa_ic;

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
	sc->sc_csim.im_hwbase = maddr;

	gipcattachsubr(sc);
	if (sc->sc_pp.pp_chip == CHIP_BRIDGE_PCIISA)
		sc->sc_irqmask &= ~IRQBIT(3);		/* XXX */
	if (sc->sc_irq != IRQUNK)
		sc->sc_ih = isa_intr_establish(sa->sa_ic, sc->sc_irq,
				IST_EDGE, IPL_PCCS, gipcintr, sc);

	pa.pa_16pp = &sc->sc_pp;
	pa.pa_32pp = NULL;
	config_found((void *) sc, &pa, NULL);
}
