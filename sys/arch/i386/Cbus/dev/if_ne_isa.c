/*	$NecBSD: if_ne_isa.c,v 1.16.6.4 1999/12/12 00:35:09 kmatsuda Exp $	*/
/*	$NetBSD: if_ne_isa.c,v 1.3 1997/10/19 09:05:04 thorpej Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>
#include <i386/Cbus/dev/dp8390pio.h>

#include <dev/isa/isavar.h>

struct ne_isa_vendor_tag {
	u_long	nh_hwcfg;			/* hdware config flags */

	struct nvt_iat_mkinfo {
#define	NE_ISA_CORE_IAT_TBLSZ	0x10
#define	NE_ISA_ASIC_IAT_TBLSZ	0x10
#define	NE_ISA_IAT_FLEN		0x08
#define	NE_ISA_IAT_SLEN		0x08
		bus_size_t	nh_skip;	/* core port offset skip */
		bus_size_t	nh_fgap;	/* core port offset gap */

		bus_addr_t	nh_asic;	/* asic port offset */
		bus_addr_t	nh_data;	/* data port offset */
		bus_addr_t	nh_reset;	/* reset port offset */
	} nh_nvt;				/* iat data */

	int (*nh_mkcore) __P((struct ne_isa_vendor_tag *, bus_space_iat_t, bus_addr_t, u_int));
	int (*nh_mkasic) __P((struct ne_isa_vendor_tag *, bus_space_iat_t, bus_addr_t, u_int));
};

extern struct dvcfg_hwsel nehwsel;
typedef	struct ne_isa_vendor_tag *ne_isa_vendor_tag_t;

static ne_isa_vendor_tag_t ne_isa_vendor_tag __P((struct isa_attach_args *));
static u_int ne_isa_vendor_settings __P((ne_isa_vendor_tag_t, struct isa_attach_args *));
static int ne_isa_match __P((struct device *, struct cfdata *, void *));
void ne_isa_attach __P((struct device *, struct device *, void *));
int ne_isa_map __P((ne_isa_vendor_tag_t, struct isa_attach_args *, bus_space_handle_t *, bus_space_handle_t *));

struct ne_isa_softc {
	dp8390pio_softc_t sc_dsc;

	/* ISA-specific goo. */
	void	*sc_ih;				/* interrupt cookie */
};

struct cfattach ne_isa_ca = {
	sizeof(struct ne_isa_softc), ne_isa_match, ne_isa_attach
};

static int
ne_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t nich, asich;
	ne_isa_vendor_tag_t vtag;
	int bustype, rv = 0;

	if (ia->ia_irq == ISACF_IRQ_DEFAULT ||
	    ia->ia_iobase == ISACF_PORT_DEFAULT)
		return rv;

	vtag = ne_isa_vendor_tag(ia);
	if (vtag == NULL)
		return rv;

	if (ne_isa_map(vtag, ia, &nich, &asich) != 0)
		return rv;

	bustype = DP8390_BUS_PIO16;
	if ((ne_isa_vendor_settings(vtag, ia) & DP8390_FORCE_8BIT_MODE) != 0)
		bustype = DP8390_BUS_PIO8;

	rv = dp8390pio_detectsubr(bustype, ia->ia_iot, nich,
					ia->ia_iot, asich,
					ia->ia_memt, NULL, 0, 0, 0);
	rv = (rv > 0);
	if (rv != 0)
		ia->ia_iosize = NE_ISA_ASIC_IAT_TBLSZ + NE_ISA_CORE_IAT_TBLSZ;

	bus_space_unmap(ia->ia_iot, asich, NE_ISA_ASIC_IAT_TBLSZ);
	bus_space_unmap(ia->ia_iot, nich, NE_ISA_CORE_IAT_TBLSZ);
	return (rv);
}

void
ne_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ne_isa_softc *isc = (struct ne_isa_softc *) self;
	dp8390pio_softc_t *dsc = &isc->sc_dsc;
	struct dp8390_softc *sc = &dsc->sc_dp8390;
	struct isa_attach_args *ia = aux;
	bus_space_handle_t nich, asich;
	ne_isa_vendor_tag_t vtag;
	int bustype;

	printf("\n");

	vtag = ne_isa_vendor_tag(ia);
	if (vtag == NULL)
		return;

	if (ne_isa_map(vtag, ia, &nich, &asich) != 0)
	{
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_regt = ia->ia_iot;
	sc->sc_regh = nich;
	dsc->sc_asict = ia->ia_iot;
	dsc->sc_asich = asich;

	sc->sc_enabled = 1;

	bustype = DP8390_BUS_PIO16;
	if ((ne_isa_vendor_settings(vtag, ia) & DP8390_FORCE_8BIT_MODE) != 0)
		bustype = DP8390_BUS_PIO8;
	if (dp8390pio_attachsubr(dsc, bustype, 0, 0, NULL, NULL, 0, 0) != 0)
	{
		sc->sc_enabled = 0;
		return;
	}

	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
					IPL_NET, dp8390_intr, sc);
	if (isc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt handler\n",
		        sc->sc_dev.dv_xname);
}

/*****************************************************************
 * vendor tags
 *****************************************************************/
static int nvt_iat_core_default __P((ne_isa_vendor_tag_t, bus_space_iat_t, bus_addr_t, u_int));
static int nvt_iat_asic_default __P((ne_isa_vendor_tag_t, bus_space_iat_t, bus_addr_t, u_int));
static int nvt_iat_asic_NEC108 __P((ne_isa_vendor_tag_t, bus_space_iat_t, bus_addr_t, u_int));

static int
nvt_iat_core_default(vtag, iat, iobase, dvcfg)
	ne_isa_vendor_tag_t vtag;
	bus_space_iat_t iat;
	bus_addr_t iobase;
	u_int dvcfg;
{
	struct nvt_iat_mkinfo *nvtp = &vtag->nh_nvt;
	int i;

	for (i = 0; i < NE_ISA_IAT_FLEN; i++)
		*iat ++ = nvtp->nh_skip * i;

	for (i = 0; i < NE_ISA_IAT_SLEN; i++)
		*iat ++ = nvtp->nh_fgap + nvtp->nh_skip * i;

	return 0;
}

static int
nvt_iat_asic_default(vtag, iat, iobase, dvcfg)
	ne_isa_vendor_tag_t vtag;
	bus_space_iat_t iat;
	bus_addr_t iobase;
	u_int dvcfg;
{
	struct nvt_iat_mkinfo *nvtp = &vtag->nh_nvt;
	int i;

	for (i = 0; i < NE_ISA_ASIC_IAT_TBLSZ; i ++)
		iat[i] = nvtp->nh_asic;

	iat[NE2000_ASIC_DATA] = nvtp->nh_data;
	iat[NE2000_ASIC_RESET] = nvtp->nh_reset;
	return 0;
}

static int
nvt_iat_asic_NEC108(vtag, iat, iobase, dvcfg)
	ne_isa_vendor_tag_t vtag;
	bus_space_iat_t iat;
	bus_addr_t iobase;
	u_int dvcfg;
{
	struct nvt_iat_mkinfo *nvtp = &vtag->nh_nvt;
	bus_addr_t adj = (iobase & 0xf000) / 2;
	int i;

	for (i = 0; i < NE_ISA_ASIC_IAT_TBLSZ; i ++)
		iat[i] = (nvtp->nh_asic | adj) - iobase;

	iat[NE2000_ASIC_DATA] = (nvtp->nh_data | adj) - iobase;
	iat[NE2000_ASIC_RESET] = (nvtp->nh_reset | adj) - iobase;
	return 0;
}

int
ne_isa_map(vtag, ia, corehp, asichp)
	ne_isa_vendor_tag_t vtag;
	struct isa_attach_args *ia;
	bus_space_handle_t *corehp, *asichp;
{
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t coreh, asich;
	bus_addr_t iobase = ia->ia_iobase;
	u_int dvcfg = ia->ia_cfgflags;
	bus_addr_t coreiat[NE_ISA_CORE_IAT_TBLSZ];
	bus_addr_t asiciat[NE_ISA_ASIC_IAT_TBLSZ];

	(*vtag->nh_mkcore) (vtag, coreiat, iobase, dvcfg);
	(*vtag->nh_mkasic) (vtag, asiciat, iobase, dvcfg);

	if (bus_space_map(iot, iobase, 0, 0, &coreh) ||
	    bus_space_map_load(iot, coreh, NE_ISA_CORE_IAT_TBLSZ, coreiat,
	    		       BUS_SPACE_MAP_FAILFREE))
		return EINVAL;

	if (bus_space_map(iot, iobase, 0, 0, &asich) ||
	    bus_space_map_load(iot, asich, NE_ISA_ASIC_IAT_TBLSZ, asiciat,
	    		       BUS_SPACE_MAP_FAILFREE))
	{
		bus_space_unmap(iot, coreh, NE_ISA_CORE_IAT_TBLSZ);
		return EINVAL;
	}

	*corehp = coreh;
	*asichp = asich;
	return 0;
}

#include <machine/dvcfg.h>
static struct ne_isa_vendor_tag nehw_pure = {
	0,
	{ 0x01, 0x08, 0x10, 0x10, 0x1f },
	nvt_iat_core_default,
	nvt_iat_asic_default
};

static struct ne_isa_vendor_tag nehw_SMARTCOM = {
	0,
	{ 0x1000, 0x8000, 0x100, 0x100, 0x7f00 },
	nvt_iat_core_default,
	nvt_iat_asic_default
};

static struct ne_isa_vendor_tag nehw_EGY98 = {
	0,
	{ 0x02, 0x100, 0x200, 0x200, 0x300 },
	nvt_iat_core_default,
	nvt_iat_asic_default
};

static struct ne_isa_vendor_tag nehw_LGY98 = {
	0,
	{ 0x01, 0x08, 0x200, 0x200, 0x300 },
	nvt_iat_core_default,
	nvt_iat_asic_default
};

static struct ne_isa_vendor_tag nehw_IF27 = {
	0,
	{ 0x01, 0x08, 0x100, 0x100, 0x10f },
	nvt_iat_core_default,
	nvt_iat_asic_default
};

static struct ne_isa_vendor_tag nehw_NEC108 = {
	0,
	{ 0x02, 0x1000, 0x888, 0x888, 0x88a },
	nvt_iat_core_default,
	nvt_iat_asic_NEC108
};

static struct ne_isa_vendor_tag nehw_IOLA98 = {
	0,
	{ 0x1000, 0x8000, 0x100, 0x100, 0xf100 },
	nvt_iat_core_default,
	nvt_iat_asic_default
};

static struct ne_isa_vendor_tag nehw_NW98X3 = {
	0,
	{ 0x0100, 0x0800, 0x1000, 0x1000, 0x1f00 },
	nvt_iat_core_default,
	nvt_iat_asic_default
};

static dvcfg_hw_t nehwsel_array[] = {
/* 0x00 */	&nehw_pure,
/* 0x01 */	&nehw_pure,
/* 0x02 */	&nehw_SMARTCOM,
/* 0x03 */	&nehw_EGY98,
/* 0x04 */	&nehw_LGY98,
/* 0x05 */	&nehw_IF27,
/* 0x06 */	NULL,	/* &nehw_sic98, */
/* 0x07 */	NULL,	/* &nehw_apex, */
/* 0x08 */	&nehw_NEC108,
/* 0x09 */	&nehw_IOLA98,
/* 0x0a */	NULL,	/* &nehw_cnet98, */
/* 0x0b */	NULL,	/* &nehw_cnet98el, */
/* 0x0c */	&nehw_NW98X3,	/* XXX: should 0x6, 0x7, 0xa, 0xb */
};

struct dvcfg_hwsel nehwsel = {
	DVCFG_HWSEL_SZ(nehwsel_array),
	nehwsel_array
};

static ne_isa_vendor_tag_t
ne_isa_vendor_tag(ia)
	struct isa_attach_args *ia;
{

	return DVCFG_HW(&nehwsel, (DVCFG_MAJOR(ia->ia_cfgflags)) >> 4);
}

static u_int
ne_isa_vendor_settings(vtag, ia)
	ne_isa_vendor_tag_t vtag;
	struct isa_attach_args *ia;
{

	return vtag->nh_hwcfg | DVCFG_MINOR(ia->ia_cfgflags);
}
