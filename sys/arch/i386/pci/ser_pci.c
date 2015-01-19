/*	$NecBSD: ser_pci.c,v 1.3 1999/04/15 01:36:40 kmatsuda Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/*
 * Remark: the following files are placed in Cbus/dev, however they never
 * depend on the busses (only the matter of places).
 */
#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/servar.h>

struct ser_pci_hw {
	int type;		/* major type */
	int subtype;		/* subtype */
	u_int hwflags;		/* flags of hardware */
	u_long freq;		/* freq */
	u_int maxunit;		/* max units */

	bus_addr_t sp_proffs;	/* pci register set offset */
	bus_size_t sp_prsz;	/* pci register set size */
	bus_space_iat_t sp_iat;	/* generic serial chip iat */
	bus_size_t sp_iatsz;	/* generic serial chip iatsz */
};

#define	SER_PCI_REG_BASE(sp, sno) ((sp)->sp_proffs  + (sno) * (sp)->sp_prsz) 

static struct ser_pci_hw ser_pci_hw_generic = {
	COM_HW_NS16550,	/* type */
	COM_HW_DEFAULT,	/* subtype */
	0,		/* hwflags */
	1843200,	/* freq: 16-bit baud rate divisor */
	1,		/* max unit */

	PCI_MAPREG_START,
	4,
	BUS_SPACE_IAT_1,
	8,
};

static bus_addr_t ser_pci_hw_iat_RSAIII[] = {
	0x8,	/* data register (R/W) */
	0x9,	/* divisor latch high (W) */
	0xa,	/* interrupt identification (R) */
	0xb,	/* line control register (R/W) */
	0xc,	/* modem control register (R/W) */
	0xd,	/* line status register (R/W) */
	0xe,	/* modem status register (R/W) */
	0xf,	/* scratch register (R/W) */
	0x8,	/* data write register (W) */
	0x2,	/* status register (R) */
	0x8,	/* data read register (R) */
	0x2,	/* fifo reset (W) */
	0x0,	/* mode control */
	0x1,	/* interrupt control */
	0x3,	/* timer interval (W) */
	0x4,	/* timer, enable = 1 disenable = 0 (W) */
};

static struct ser_pci_hw ser_pci_hw_RSAIII = {
	COM_HW_SERA,		/* type */
	COM_HW_RSA98III,	/* subtype */
	COM_HW_TIMER,		/* hwflags */
	(1843200 * 8),		/* RSA98III base clock */
	0x02,			/* max unit */

	PCI_MAPREG_START + 4,
	4,
	ser_pci_hw_iat_RSAIII,
	BUS_SPACE_IAT_SZ(ser_pci_hw_iat_RSAIII),
};

struct ser_pci_hw_info {
	u_int16_t sp_vendor;
	u_int16_t sp_product;
	struct ser_pci_hw *sp_hwp;
} ser_pci_hw_table[] = {
	/* IO-DATA RSA-PCI native mode */
	{ PCI_VENDOR_IODATA, 0x0007, &ser_pci_hw_RSAIII },

	/* GENERIC rs232c/modem (add vendor/product entries!) */
	{ 0xffff, 0xffff, &ser_pci_hw_generic },

	/* TERMINATOR */
	{ 0, 0, NULL },
};

static void ser_pci_attach __P((struct device *, struct device *, void *));
static int ser_pci_match __P((struct device *, struct cfdata *, void *));
static struct ser_pci_hw_info *ser_pci_find_hw __P((struct pci_attach_args *));

struct cfattach ser_pci_ca = {
	sizeof(struct ser_softc), ser_pci_match, ser_pci_attach
};

static struct ser_pci_hw_info *
ser_pci_find_hw(pa)
	struct pci_attach_args *pa;
{
	struct ser_pci_hw_info *sp = &ser_pci_hw_table[0];

	for ( ;sp->sp_hwp != NULL; sp ++)
	{
		if (PCI_VENDOR(pa->pa_id) == sp->sp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == sp->sp_product)
			return sp;
	}
	return NULL;
}

int
ser_pci_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct ser_pci_hw_info *sip;

	sip = ser_pci_find_hw(pa);
	if (sip == NULL)
		return 0;
	return 1;
}

void
ser_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct ser_softc *sc = (void *) self;
	struct ser_pci_hw_info *sip;
	struct ser_pci_hw *sp;
	bus_space_tag_t sbst = NULL;
	bus_space_handle_t sbsh = NULL;
	struct commulti_attach_args ca;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
	int sno, rv;

	printf("\n");

	sip = ser_pci_find_hw(pa);
	sp = sip->sp_hwp;
	for (sno = 0; sno < sp->maxunit; sno ++)
	{
		bzero(&ca, sizeof(ca));
		ca.ca_type = sp->type;
		ca.ca_subtype = sp->subtype;
		ca.ca_hwflags = sp->hwflags;
		ca.ca_freq = sp->freq;
		ca.ca_slave = sno;

		rv = pci_mapreg_map(pa, SER_PCI_REG_BASE(sp, sno),
				   PCI_MAPREG_TYPE_IO, 0,
			           &ca.ca_h.cs_iot, &ca.ca_h.cs_ioh,
			           NULL, NULL);
		if (rv != 0)
		{
			if (sno != 0)
				continue;
			printf("%s: can not map io space\n",
				sc->sc_dev.dv_xname);
			return;
		}

		rv = bus_space_map_load(ca.ca_h.cs_iot, ca.ca_h.cs_ioh,
			sp->sp_iatsz, sp->sp_iat, BUS_SPACE_MAP_FAILFREE);
		if (rv != 0)
		{
			printf("%s: can not reload io space\n",
			       sc->sc_dev.dv_xname);
			return;
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
			if (sno != 0)
				continue;
			printf("%s: can not find master hardware\n",
			       sc->sc_dev.dv_xname);
			return;
		}

		sc->sc_slaves[sno] = ca.ca_m;
	}

	if (sc->sc_slaves[0] == NULL)
		return;

	/* attach interrupt handler */
	sc->sc_intr = sc->sc_slaves[0]->sc_cswp->cswt_intr;
	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
	    	pa->pa_intrline, &intrhandle) != 0)
	{
		printf("%s: couldn't map PCI interrupt\n", sc->sc_dev.dv_xname);
		return;
	} 

	intrstr = pci_intr_string(pa->pa_pc, intrhandle);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_COM,
				       serintr, sc);
	if (sc->sc_ih != NULL)
	{
		printf("%s: using %s for interrupt\n",
		        sc->sc_dev.dv_xname,
		        intrstr ? intrstr : "unknown interrupt");
	}
}
