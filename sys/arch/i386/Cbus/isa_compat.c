/*	$NecBSD: isa_compat.c,v 1.19 1999/07/25 16:53:00 honda Exp $	*/
/*	$NetBSD: isa.c,v 1.106 1999/02/19 16:15:06 mycroft Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmareg.h>
#ifndef	ORIGINAL_CODE
#include <machine/syspmgr.h>
#endif	/* PC-98 */

#include "isadma.h"

#ifdef	ORIGINAL_CODE
#include "isapnp.h"
#ifdef NISAPNP
#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#endif
#endif	/* !PC-98 */

int isamatch __P((struct device *, struct cfdata *, void *));
void isaattach __P((struct device *, struct device *, void *));
int isaprint __P((void *, const char *));

struct cfattach isa_ca = {
	sizeof(struct isa_softc), isamatch, isaattach
};

int	isasearch __P((struct device *, struct cfdata *, void *));

int
isamatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct isabus_attach_args *iba = aux;

	if (strcmp(iba->iba_busname, cf->cf_driver->cd_name))
		return (0);

	/* XXX check other indicators */

        return (1);
}

void
isaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_softc *sc = (struct isa_softc *)self;
	struct isabus_attach_args *iba = aux;
#ifndef	ORIGINAL_CODE
	bus_space_tag_t delayt;
#endif	/* PC-98 */

	isa_attach_hook(parent, self, iba);
	printf("\n");

	sc->sc_iot = iba->iba_iot;
	sc->sc_memt = iba->iba_memt;
	sc->sc_dmat = iba->iba_dmat;
	sc->sc_ic = iba->iba_ic;
#ifndef	ORIGINAL_CODE
	syspmgr_alloc_delayh(&delayt, &sc->sc_delaybah); /* XXX */
#endif	/* PC-98 */

#ifdef	ORIGINAL_CODE
#if NISAPNP > 0
	/*
	 * Reset isapnp cards that the bios configured for us
	 */
	isapnp_isa_attach_hook(sc);
#endif
#endif	/* !PC-98 */

#if NISADMA > 0
	/*
	 * Initialize our DMA state.
	 */
	isa_dmainit(sc->sc_ic, sc->sc_iot, sc->sc_dmat, self);
#endif

	config_search(isasearch, self, NULL);
}

int
isaprint(aux, isa)
	void *aux;
	const char *isa;
{
	struct isa_attach_args *ia = aux;

	if (ia->ia_iosize)
		printf(" port 0x%x", ia->ia_iobase);
	if (ia->ia_iosize > 1)
		printf("-0x%x", ia->ia_iobase + ia->ia_iosize - 1);
	if (ia->ia_msize)
		printf(" iomem 0x%x", ia->ia_maddr);
	if (ia->ia_msize > 1)
		printf("-0x%x", ia->ia_maddr + ia->ia_msize - 1);
	if (ia->ia_irq != IRQUNK)
		printf(" irq %d", ia->ia_irq);
	if (ia->ia_drq != DRQUNK)
		printf(" drq %d", ia->ia_drq);
	if (ia->ia_drq2 != DRQUNK)
		printf(" drq2 %d", ia->ia_drq2);
#ifndef	ORIGINAL_CODE
	printf(" dvcfg 0x%x", ia->ia_cfgflags);
#endif	/* PC-98 */
	return (UNCONF);
}

int
isasearch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct isa_softc *sc = (struct isa_softc *)parent;
	struct isa_attach_args ia;
	int tryagain;

	do {
		ia.ia_iot = sc->sc_iot;
		ia.ia_memt = sc->sc_memt;
		ia.ia_dmat = sc->sc_dmat;
		ia.ia_ic = sc->sc_ic;
		ia.ia_iobase = cf->cf_iobase;
		ia.ia_iosize = 0x666; /* cf->cf_iosize; */
		ia.ia_maddr = cf->cf_maddr;
		ia.ia_msize = cf->cf_msize;
#ifdef	ORIGINAL_CODE
		ia.ia_irq = cf->cf_irq == 2 ? 9 : cf->cf_irq;
#else	/* PC-98 */
		ia.ia_irq = cf->cf_irq;
#endif	/* PC-98 */
		ia.ia_drq = cf->cf_drq;
		ia.ia_drq2 = cf->cf_drq2;
#ifndef	ORIGINAL_CODE
		ia.ia_cfgflags = cf->cf_flags;
		ia.ia_delaybah = sc->sc_delaybah; /* XXX */
#endif	/* PC-98 */

		tryagain = 0;
		if ((*cf->cf_attach->ca_match)(parent, cf, &ia) > 0) {
			config_attach(parent, cf, &ia, isaprint);
#ifdef	ORIGINAL_CODE
			tryagain = (cf->cf_fstate == FSTATE_STAR);
#else	/* PC-98 */
			tryagain = 0;
#endif	/* PC-98 */
		}
	} while (tryagain);

	return (0);
}

char *
isa_intr_typename(type)
	int type;
{

	switch (type) {
        case IST_NONE :
		return ("none");
        case IST_PULSE:
		return ("pulsed");
        case IST_EDGE:
		return ("edge-triggered");
        case IST_LEVEL:
		return ("level-triggered");
	default:
		panic("isa_intr_typename: invalid type %d", type);
	}
}
