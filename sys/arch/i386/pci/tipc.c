/*	$NecBSD: tipc.c,v 1.11 1999/07/23 05:40:19 kmatsuda Exp $	*/
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

#include <machine/bus.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <i386/pci/tipcreg.h>
#include <i386/pci/ppcbvar.h>

/* #define	TIPC_DEBUG */	/* XXX */

struct tipc_softc {
	struct device sc_dev;

	pci_chipset_tag_t sc_pc;
};

static int tipc_match __P((struct device *, struct cfdata *, void *));
void tipc_attach __P((struct device *, struct device *, void *));
static void tipc_pci_pcmcia_bridge_open __P((struct tipc_softc *, struct pci_attach_args *));

struct	cfattach tipc_ca = {
	sizeof(struct tipc_softc), tipc_match, tipc_attach
};

extern struct	cfdriver tipc_cd;

static int
tipc_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) !=  PCI_VENDOR_TI)
		return 0;
	if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_TI_PCI1130
	    && PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_TI_PCI1131)
		return 0;
	if (pa->pa_function != 0)
		return 0;

	return 1;
}

void
tipc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct tipc_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	struct ppcb_attach_args pp;
#ifdef	TIPC_DEBUG
	pcitag_t pcitag = pa->pa_tag;
	int i;
#endif	/* TIPC_DEBUG */

	printf(": pci-pcmcia(pisa) bridge enable\n");
	sc->sc_pc = pc;
#ifdef	TIPC_DEBUG
	for (i = 0; i < 4 * 40; i = i + 4) {
		printf("%02x %08x ", i, pci_conf_read(pc, pcitag, i));
		if ((i + 4) % 16 == 0)
			printf("\n");
	}
	printf("\n");
#endif	/* TIPC_DEBUG */
	tipc_pci_pcmcia_bridge_open(sc, pa);
#ifdef	TIPC_DEBUG
	for (i = 0; i < 4 * 40; i = i + 4) {
		printf("%02x %08x ", i, pci_conf_read(pc, pcitag, i));
		if ((i + 4) % 16 == 0)
			printf("\n");
	}
	printf("\n");
#endif	/* TIPC_DEBUG */

	pp.pp_pa = pa;		
	config_found((void *) sc, &pp, NULL);
}

static void
tipc_pci_pcmcia_bridge_open(sc, pa)
	struct tipc_softc *sc;
	struct pci_attach_args *pa;
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t pcitag = pa->pa_tag;
#ifdef	TIPC_NOT_INITIALIZED_BY_BIOS
	u_int regv;
#endif	/* TIPC_NOT_INITIALIZED_BY_BIOS */

	pci_conf_write(pc, pcitag, PCI_COMMAND_STATUS_REG, 
		       PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE | 
		       PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_STEPPING_ENABLE);

#ifdef	TIPC_NOT_INITIALIZED_BY_BIOS
	regv = pci_conf_read(pc, pcitag, TIPC_DCR_BASE);
	regv &= ~TIPC_DCR_IMODE_MASK;
	/*
	 * TIPC_DCR_IMODE register initialized by bios as follows:
	 * Nr15   TIPC_DCR_IMODE_ISA 
	 * Nw13	  TIPC_DCR_IMODE_SERIRQ
	 */
#ifdef 	TIPC_NR15
	regv |= TIPC_DCR_IMODE_ISA;
#else
	regv |= TIPC_DCR_IMODE_SERIRQ;
#endif
	pci_conf_write(pc, pcitag, TIPC_DCR_BASE, regv);

#ifdef	TIPC_NR15	
	regv = pci_conf_read(pc, pcitag, CARDBUS_BRIDGE_CONTROL);
	regv |= CARDBUS_BCR_ISA_IRQ;
	pci_conf_write(pc, pcitag, CARDBUS_BRIDGE_CONTROL, regv);
#endif
#endif	/* TIPC_NOT_INITIALIZED_BY_BIOS */

	pci_conf_write(pc, pcitag, CARDBUS_LEGACY_MODE_BASE,
		       PCCS16_IOBASE | 0x01);
}
