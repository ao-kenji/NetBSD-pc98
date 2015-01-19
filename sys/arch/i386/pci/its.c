/*	$NecBSD: its.c,v 1.9 1999/07/31 12:30:31 honda Exp $	*/
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

struct its_softc {
	struct device sc_dev;

	pci_chipset_tag_t sc_pc;
};

int its_match __P((struct device *, struct cfdata *, void *));
void its_attach __P((struct device *, struct device *, void *));

struct	cfattach its_ca = {
	sizeof(struct its_softc), its_match, its_attach
};

extern struct cfdriver its_cd;

struct its_chip_fix {
	int icf_addr;

	u_int8_t icf_clrb;
	u_int8_t icf_setb;
};	

struct its_chip_detect {
	int icd_vendor;
	int icd_chip;

	u_int icd_nent;
	struct its_chip_fix *icd_icf;

	u_char *icd_name;
};

struct its_chip_fix pcmc_fix_data[] = {
	{0x57, 0x0, 0xc0},	/* X444 -> X333 */
};

struct its_chip_fix wildcat_fix_data[] = {
	{0x5a, 0xff, 0x11},
	{0x54, 0xff, 0xa0},
	{0x5e, 0xff, 0x00},
	{0x5f, 0xff, 0xd0},
};
	
struct its_chip_fix natoma_fix_data[] = {
};

struct its_chip_detect icdtbl[] = {
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PCMC, 
	 1, pcmc_fix_data, 
	 "Neptune"},

	{PCI_VENDOR_VLSI, PCI_PRODUCT_VLSI_82C594,
	 4, wildcat_fix_data, 
	 "Wildcat"},

#if	notyet
	{PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82439,
	 0, natoma_fix_data,
	 "Natoma"},
#endif

	{-1, -1, 0, NULL},
};

static struct its_chip_detect *its_detect __P((struct pci_attach_args *));

static struct its_chip_detect *
its_detect(pa)
	struct pci_attach_args *pa;
{
	struct its_chip_detect *tp;

	for (tp = icdtbl; tp->icd_vendor != -1; tp ++)
	{
		if (PCI_VENDOR(pa->pa_id) == tp->icd_vendor &&
		    PCI_PRODUCT(pa->pa_id) == tp->icd_chip)
			return tp;
	}
	return NULL;
}

int
its_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (its_detect(pa) == NULL)
		return 0;
	return 1;
}

void
its_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct its_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	struct its_chip_detect *tp;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t pcitag = pa->pa_tag;
	int i;

	sc->sc_pc = pc;
	tp = its_detect(pa);

	if (tp->icd_nent == 0)
	{
		printf(": found the host chipset(%s)\n", tp->icd_name);
		return;
	}

	printf(": change settings of the host chipset(%s)\n", tp->icd_name);

	for (i = 0; i < tp->icd_nent; i ++)
	{
		u_int32_t setopv, clropv, oldregv, regv;
		u_int addr;

		addr = tp->icd_icf[i].icf_addr;
		oldregv = regv = pci_conf_read(pc, pcitag, addr & ~3);

		clropv = (u_int32_t) tp->icd_icf[i].icf_clrb;
		clropv = clropv << (NBBY * (addr & 3));

		setopv = (u_int32_t) tp->icd_icf[i].icf_setb;
		setopv = setopv << (NBBY * (addr & 3));

		regv &= ~clropv;
		regv |= setopv;

		printf("%s: register addr 0x%x: 0x%08x --> 0x%08x\n", 
			sc->sc_dev.dv_xname, addr, oldregv, regv);	

		pci_conf_write(pc, pcitag, addr & ~3, regv);
	}
}
