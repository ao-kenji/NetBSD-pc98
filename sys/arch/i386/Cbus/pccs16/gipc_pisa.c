/*	$NecBSD: gipc_pisa.c,v 1.16.4.1 1999/08/15 17:18:09 honda Exp $	*/
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

#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

#include <i386/Cbus/pccs16/gipcvar.h>

static int gipc_pisa_match __P((struct device *, struct cfdata *, void *));
static void gipc_pisa_attach __P((struct device *, struct device *, void *));
static int gipc_pisa_deactivate __P((pisa_device_handle_t));
static int gipc_pisa_activate __P((pisa_device_handle_t));

struct gipc_pisa_softc {
	struct gipc_softc sc_gipc;

	pisa_device_handle_t sc_dh;
};

struct cfattach gipc_pisa_ca = {
	sizeof(struct gipc_pisa_softc), gipc_pisa_match, gipc_pisa_attach
};

struct pisa_functions gipc_pd = {
	gipc_pisa_activate, gipc_pisa_deactivate,
};

#define	GIPC_SYSTM_MEMBASE	(0xc0000)

static int
gipc_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t ioh;
	int rv = 0;	

	if (pisa_space_map(dh, PISA_IO0, &ioh))
		return rv;

	if (gipcprobesubr(pa->pa_iot, ioh) > 0)
		rv = 1;

	pisa_space_unmap(dh, pa->pa_iot, ioh);
	return rv;
}

static void
gipc_pisa_attach(parent, match, aux)
	struct device *parent, *match;
	void *aux;
{
	struct gipc_pisa_softc *psc = (void *) match;
	struct pisa_attach_args *pa = aux;
	struct gipc_softc *sc = &psc->sc_gipc;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_handle_t ioh, memh;
	bus_space_tag_t iot, memt;
	bus_size_t msize;
	bus_addr_t maddr;
	u_int dvcfg;
	struct pccs_attach_args pca;

	iot = pa->pa_iot;
	memt = pa->pa_memt;
	psc->sc_dh = dh;
	dvcfg = PISA_DR_DVCFG(dr);
#ifdef	GIPC_POLLING_ONLY
	dvcfg |= GIPC_DVCFG_POLLING;
#endif	/* GIPC_POLLING_ONLY */

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
	if (pisa_space_map(dh, PISA_IO0, &ioh)) 
	{
		printf("%s: iomem map failed", sc->sc_dev.dv_xname);
		return;
	}

	if (bus_space_map(memt, maddr, msize, 0, &memh))
	{
		pisa_space_unmap(dh, iot, ioh);
		printf("%s: missing memory window", sc->sc_dev.dv_xname);
		return;
	}
		
	/*
	 * register functions
	 */
	pisa_register_functions(dh, match, &gipc_pd);

	/*
	 * attach now!
	 */
	sc->sc_irqmask = systm_intr_routing->sir_allow_pccs;
	sc->sc_irq = (dvcfg & GIPC_DVCFG_POLLING) ? IRQUNK : PISA_DR_IRQ0(dr);
	sc->sc_pp.pp_bc = pa->pa_ic;
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

	if (sc->sc_irq != IRQUNK)
		sc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_PCCS, gipcintr, sc);

	pca.pa_16pp = &sc->sc_pp;
	pca.pa_32pp = NULL;
	(void) config_found((void *) sc, &pca, NULL);
}

static int
gipc_pisa_activate(dh)
	pisa_device_handle_t dh;
{
	struct gipc_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct gipc_softc *sc = &psc->sc_gipc;

	if (sc->sc_pp.pp_pcsc == NULL)
		return 0;

	return pccs_activate(sc->sc_pp.pp_pcsc);
}

static int
gipc_pisa_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct gipc_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct gipc_softc *sc = &psc->sc_gipc;

	if (sc->sc_pp.pp_pcsc == NULL)
		return 0;

	return pccs_deactivate(sc->sc_pp.pp_pcsc);
}
