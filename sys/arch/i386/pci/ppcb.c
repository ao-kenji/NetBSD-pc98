/*	$NecBSD: ppcb.c,v 1.10 1999/07/23 05:40:19 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 * Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting stuff. All rights reserved.
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
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <vm/vm.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <i386/pci/ppcbvar.h>

#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>

#include <i386/Cbus/pccs16/gipcvar.h>

struct ppcb_softc {
	struct device sc_dev;
};

int ppcb_probe __P((struct device *, void *, void *));
void ppcb_attach __P((struct device *, struct device *, void *));

struct	cfattach ppcb_ca = {
	sizeof(struct ppcb_softc), ppcb_probe, ppcb_attach
};

extern struct	cfdriver ppcb_cd;

#define	PCMCIA_SHOULD_BE_PROBED_AFTER_PCI
int
ppcb_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
#ifndef	PCMCIA_SHOULD_BE_PROBED_AFTER_PCI
	struct cfdata *cf = match;
	struct ppcb_attach_args *pp = aux;
	struct pci_attach_args *pa = pp->pp_pa;
#endif


#ifndef	PCMCIA_SHOULD_BE_PROBED_AFTER_PCI
	if (cf->cf_loc[PCI_PCCS_BRIDGECF_FUNCTION] != pa->pa_function)
		return 0;
	return 1;
#else	
	return 0;
#endif	
}

void
ppcb_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ppcb_softc *sc = (void *) self;
	struct ppcb_attach_args *pp = aux;
	struct pci_attach_args *pa = pp->pp_pa;
	struct gipc_attach_args ga;
	bus_space_tag_t memt = pa->pa_memt;
	bus_addr_t maddr;
	bus_size_t msize;

	printf("\n");

	maddr = PCCS16_MADDR;
	msize = PCCS16_MSIZE;
	if (bus_space_map_prefer(memt, &maddr, 0x100000, NBPG, msize))
	{
		printf("%s:no attribute memory window\n", sc->sc_dev.dv_xname);
		return;
	}

	ga.ga_iot = pa->pa_iot;
	ga.ga_memt = memt;
	ga.ga_bc = pa->pa_pc;
	ga.ga_iobase = PCCS16_IOBASE + pa->pa_function * 2;
	ga.ga_maddr = maddr;
	ga.ga_msize = msize;
	ga.ga_irq = SLOT_DEVICE_UNKVAL;

	config_found((void *) sc, &ga, NULL);
}
