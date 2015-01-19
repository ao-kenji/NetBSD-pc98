/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
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
#include <sys/malloc.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <vm/vm.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

#include <i386/Cbus/pccs16/nepcvar.h>
#include <i386/Cbus/pccs16/nepcreg.h>
#include <i386/Cbus/pccs16/busiosubr_nec.h>

#include <machine/bus.h>
#include <machine/systmbusvar.h>

static bus_addr_t nepciat[] = {
	PCIC98_REG0, PCIC98_REG1, PCIC98_REG2,
	PCIC98_REG3, PCIC98_REG4, PCIC98_REG5,
	PCIC98_REG6, PCIC98_REG_WINSEL,	PCIC98_REG_PAGOFS};

/********************************************
 * Probe Attach
 *******************************************/
#ifndef	PCIC98_IOBASE
#define	PCIC98_IOBASE   0x80d0
#endif	/* PCIC98_IOBASE */
#ifndef	PCIC98_MWINBASE
#define	PCIC98_MWINBASE	0xda000
#endif	/* PCIC98_MWINBASE */

static int nepc_systm_match __P((struct device *, struct cfdata *, void *));
static void nepc_systm_attach __P((struct device *, struct device *, void *));

struct cfattach nepc_systm_ca = {
	sizeof(struct nepc_softc), nepc_systm_match, nepc_systm_attach
};

static int
nepc_systm_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct systm_attach_args *sa = aux;
	bus_space_handle_t ioh;
	bus_space_handle_t memh;
	vaddr_t maddr;
	vsize_t msize;
	int rv = 0;

	maddr = PCIC98_MWINBASE;
	msize = PCCSHW_MAX_WINSIZE;

	if (bus_space_map(sa->sa_iot, 0, 0, 0, &ioh) ||
	    bus_space_map_load(sa->sa_iot, ioh, BUS_SPACE_IAT_SZ(nepciat), 
			       nepciat, BUS_SPACE_MAP_FAILFREE))
		return 0;

	rv = nepcmatchsubr(sa->sa_iot, ioh);
	if (rv == 0)
		goto bad;
	if (bus_space_map(sa->sa_memt, maddr, msize, 0, &memh))
		goto bad;
	bus_space_unmap(sa->sa_memt, memh, msize);
	rv = 1;

bad:
	bus_space_unmap(sa->sa_iot, ioh, 0);
	return rv;
}

static void
nepc_systm_attach(parent, match, aux)
	struct device *parent, *match;
	void *aux;
{
	extern struct bus_space_tag NEPC_io_space_tag;
	extern struct bus_space_tag NEPC_mem_space_tag;
	struct nepc_softc *sc = (struct nepc_softc *) match;
	struct systm_attach_args *sa = aux;
	pccshw_tag_t pp = &sc->sc_pp;
	struct pccs_attach_args pa;
	bus_space_tag_t neciot, necmemt;
	vaddr_t maddr;
	vsize_t msize;

	printf("\n");
	maddr = PCIC98_MWINBASE;
	msize = PCCSHW_MAX_WINSIZE;

	neciot = malloc(sizeof(*neciot), M_DEVBUF, M_NOWAIT);
	necmemt = malloc(sizeof(*necmemt), M_DEVBUF, M_NOWAIT);
	if (neciot == NULL || necmemt == NULL)
		return;

	bcopy(&NEPC_io_space_tag, neciot, sizeof(*neciot));
	bcopy(&NEPC_mem_space_tag, necmemt, sizeof(*necmemt));
	neciot->bs_busc = sc;
	necmemt->bs_busc = sc;

	pp->pp_bc = sa->sa_ic;
	pp->pp_lowmem = sc->sc_pp.pp_startmem = maddr;
	pp->pp_highmem = maddr + 2 * NBPG;
	pp->pp_lowio = 0;
	pp->pp_highio = (1 << 16);
	pp->pp_startio = 0x4000;
	sc->sc_bst = sa->sa_iot;
	sc->sc_csbst = sa->sa_memt;
	sc->sc_busiot = neciot;
	sc->sc_busmemt = necmemt;
	sc->sc_busim_mem.im_type = SLOT_DEVICE_SPMEM;
	sc->sc_busim_mem.im_hwbase = maddr;
	sc->sc_busim_mem.im_base = PCIC98_MAPWIN;
	sc->sc_busim_io.im_type = SLOT_DEVICE_SPIO;
	sc->sc_busim_io.im_hwbase = PCIC98_IOBASE;
	sc->sc_csim.im_type = SLOT_DEVICE_SPMEM;
	sc->sc_csim.im_hwbase = maddr;
	sc->sc_irqmask = systm_intr_routing->sir_allow_pccs;

	if (bus_space_map(sc->sc_csbst, maddr, msize, 0, &sc->sc_csbsh) ||
	    bus_space_map(sc->sc_bst, 0, 0, 0, &sc->sc_bsh) ||
	    bus_space_map_load(sc->sc_bst, sc->sc_bsh,
			       BUS_SPACE_IAT_SZ(nepciat), nepciat, 0))
	{
		printf("%s: iomem map failed", sc->sc_dev.dv_xname);
		return;
	}

	nepc_attachsubr(sc);

	pa.pa_16pp = pp;
	pa.pa_32pp = NULL;
	config_found((void *) match, &pa, NULL);
}
