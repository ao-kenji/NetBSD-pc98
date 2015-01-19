/*	$NecBSD: systmbus.c,v 1.20.2.4 1999/08/22 19:46:35 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
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

/*
 * Copyright (c) 1996, 1997, 1998, 1999
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/kthread.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/syspmgr.h>
#include <machine/systmbusvar.h>
#include <machine/vm86bios.h>

static int systmmatch __P((struct device *, struct cfdata *, void *));
static void systmattach __P((struct device *, struct device *, void *));
static int systmsearch __P((struct device *, struct cfdata *, void *));
static int systmprint __P((void *, const char *));

u_int32_t global_intrmap;

struct cfattach systm_ca = {
	sizeof(struct systm_softc), systmmatch, systmattach
};

extern struct cfdriver systm_cd;

static int
systmmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct systmbus_attach_args *sba = aux;

	if (strcmp(sba->sba_busname, cf->cf_driver->cd_name))
		return (0);

	/* XXX check other indicators */

	return (1);
}

static void
systmattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct systm_softc *sc = (struct systm_softc *)self;
	struct systmbus_attach_args *sba = aux;
	bus_space_tag_t delayt;

	printf("\n");
	printf("%s: intr routing: global 0x%lx(0x%lx) pccs 0x%lx ippi 0x%lx\n",
		sc->sc_dev.dv_xname,
		systm_intr_routing->sir_allow_global,
		systm_intr_routing->sir_global,
		systm_intr_routing->sir_allow_pccs,
		systm_intr_routing->sir_allow_ippi);
	
	/* start systm bus */
	sc->sc_ic = sba->sba_ic;
	sc->sc_iot = sba->sba_iot;
	sc->sc_memt = sba->sba_memt;
	sc->sc_dmat = sba->sba_dmat;
	syspmgr_alloc_delayh(&delayt, &sc->sc_delaybah);

	config_search(systmsearch, self, NULL);

	kthread_create_deferred(systm_kthread_dispatch,
				&systm_kthread_dispatch_table[0]);
}

static int
systmprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct systm_attach_args *sa = aux;

	printf(" dvcfg 0x%x", sa->sa_cfgflags);
	return (UNCONF);
}

static int
systmsearch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct systm_softc *sc = (struct systm_softc *) parent;
	struct systm_attach_args sa;
	int tryagain;

	do {
		sa.sa_iot = sc->sc_iot;
		sa.sa_memt = sc->sc_memt;
		sa.sa_dmat = sc->sc_dmat;
		sa.sa_ic = sc->sc_ic;

		sa.sa_delaybah = sc->sc_delaybah;
		sa.sa_cfgflags = cf->cf_flags;

		tryagain = 0;
		if ((*cf->cf_attach->ca_match) (parent, cf, &sa) > 0) {
			config_attach(parent, cf, &sa, systmprint);
			tryagain = 0;
		}
	} while (tryagain);

	return (0);
}

/*******************************************************
 * Systm intr routing
 *******************************************************/
#define	IRQBIT(NO)	(1 << (NO))
#define	GLOBAL_BIT_MAP	(IRQBIT(3) | IRQBIT(5) | IRQBIT(6) | IRQBIT(9) | \
			 IRQBIT(10) | IRQBIT(11) | IRQBIT(12) | IRQBIT(13))

#include "opt_pccshw.h"
#ifndef	PCCSHW_IRQMASK
#define	PCCSHW_IRQMASK (IRQBIT(3)|IRQBIT(5)|IRQBIT(10)|IRQBIT(12))
#endif

#include "opt_ipp.h"
#ifndef	IPP_IRQMASK
#define	IPP_IRQMASK (IRQBIT(3)|IRQBIT(5)|IRQBIT(9)|IRQBIT(10)|IRQBIT(12))
#endif

struct systm_intr_routing systm_intr_routing_table = {
	GLOBAL_BIT_MAP,			/* global allow */
	GLOBAL_BIT_MAP,			/* global allow */

	0,				/* isa mask */
	0,				/* pci mask */
	(PCCSHW_IRQMASK) & GLOBAL_BIT_MAP, /* pccs mask */
	(IPP_IRQMASK) & GLOBAL_BIT_MAP,	   /* ippi mask */
};

struct systm_intr_routing *systm_intr_routing = &systm_intr_routing_table;
	
/*******************************************************
 * Kthread daemon dispatch tables 
 *******************************************************/
#include "opt_vm86biosd.h"
#include "opt_systm_daemon.h"

struct systm_kthread_dispatch_table systm_kthread_dispatch_table[] = {
#ifdef	KTHREAD_VM86BIOSD
	{"/sbin/vm86biosd", "vm86task", NULL, SYSTM_KTHREAD_DISPATCH_OK},
#endif	/* KTHREAD_VM86BIOSD */
#ifdef	PCSD_AUTO_START
	{"/sbin/pcsd", "pcsd", "-F", SYSTM_KTHREAD_DISPATCH_OK},
#endif	/* PCSD_AUTO_START */
#ifdef	IPPCTRL_AUTO_START
	{"/sbin/ippctrl", "ippctrl_onetime", "-a", SYSTM_KTHREAD_DISPATCH_OK},
#endif	/* IPPCTRL_AUTO_START */
	{NULL, NULL, NULL},
};
