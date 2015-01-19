/*	$NecBSD: necss.c,v 1.20 1999/07/23 05:40:24 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
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
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/systmbusvar.h>
#include <machine/dvcfg.h>

#include <i386/systm/necssreg.h>
#include <i386/systm/necssvar.h>

#include <i386/Cbus/dev/nec86reg.h>

#include <dev/ic/ym2203reg.h>
#include <i386/Cbus/dev/ymsvar.h>

struct necss_iomap;
struct necss_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_n86ioh;

	int sc_remapped;
	struct necss_iomap *sc_nsp;

	u_int8_t sc_pregv;
};

static int necssmatch __P((struct device *, struct cfdata *, void *));
static void necssattach __P((struct device *, struct device *, void *));
static int necss_systmmsg __P((struct device *, systm_event_t));
static void necss_select __P((struct necss_softc *, int));
static void necss_disable __P((struct necss_softc *));
static int necss_identify __P((bus_space_tag_t, bus_space_handle_t));
int necss_systm_resource __P((bus_space_tag_t, int, struct necss_iomap *, bus_addr_t *, bus_addr_t *, u_long *));
int necss_remap __P((u_int, struct necss_iomap **));
int necss_rmap __P((bus_addr_t, bus_addr_t, bus_addr_t));
static void necss_shutdown __P((void *));

extern struct cfdriver necss_cd;

struct cfattach necss_ca = {
	sizeof(struct necss_softc), necssmatch, necssattach
};

#define	NECSS_IOMAP_RELOCATE 0x08
#define	NECSS_IOMAP_MASK  0x07
#define	NECSS_IOMAP_TBLSZ 0x08
struct necss_iomap {
	bus_addr_t nsp_ctrl;
	bus_addr_t nsp_yms;
	bus_addr_t nsp_wss;
} necss_iomap[] = {
	{0xa460, 0x188, 0xf40},
	{0xa460, 0x288, 0xf40},
	{0xa470, 0x188, 0xf40},
	{0xa470, 0x288, 0xf40},
	{0xa480, 0x188, 0xf40},
	{0xa480, 0x288, 0xf40},
	{0xa490, 0x188,	0xf40},
	{0xa490, 0x288,	0xf40},
};

int
necss_remap(sel, nspp)
	u_int sel;
	struct necss_iomap **nspp;
{
	u_int8_t regv;

	*nspp = &necss_iomap[(sel & NECSS_IOMAP_MASK)];
	if ((sel & NECSS_IOMAP_RELOCATE) == 0)
		return 0;

	regv = inb(NECSS_PnP_CONFIG);
	if (regv == (u_int8_t) -1 || regv == 0 || (regv & NEC_PnPR_ENABLE) == 0)
	{
		*nspp = &necss_iomap[0];
		return 0;
	}

	if (!necss_rmap((*nspp)->nsp_ctrl, (*nspp)->nsp_wss, (*nspp)->nsp_yms))
		return 1;

	*nspp = &necss_iomap[0];
	return 0;
}

int
necss_identify(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	u_int8_t val;
	int model = -1;

	val = bus_space_read_1(iot, ioh, 0);
	switch (val & NEC_SCR_SIDMASK)
	{
	case 0x40:
		model = NECSS_PCM86_0;
		break;
	case 0x50:
		model = NECSS_PCM86_1;
		break;
	case 0x60:
		model = NECSS_WSS_0;
		goto check;
	case 0x70:
		model = NECSS_WSS_1;
		goto check;
	case 0x80:
		model = NECSS_WSS_2;
	check:
		if ((inb(NECSS_PnP_CONFIG + necss_pnpb) & NEC_PnPR_ENABLE) == 0)
			return -1;
		break;
	}
	return model;
}

static int
necssmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct systm_attach_args *sa = aux;
	bus_space_tag_t iot = sa->sa_iot;
	bus_space_handle_t n86ioh;
	struct necss_iomap *nsp;

	necss_remap(DVCFG_MAJOR(sa->sa_cfgflags), &nsp);
	if (bus_space_map(iot, nsp->nsp_ctrl, 1, 0, &n86ioh))
		return 0;

	if (necss_identify(iot, n86ioh) < 0)
		return 0;

	bus_space_unmap(iot, n86ioh, 1);
	return 1;
}

static void
necssattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct necss_softc *sc = (struct necss_softc *) self;
	struct systm_attach_args *sa = aux;
	bus_space_tag_t iot = sa->sa_iot;
	bus_space_handle_t n86ioh;
	struct necss_iomap *nsp;
	struct necss_attach_args na;
	int ident;

	if (necss_remap(DVCFG_MAJOR(sa->sa_cfgflags), &nsp))
		sc->sc_remapped = 1;

	if (bus_space_map(iot, nsp->nsp_ctrl, 1, 0, &n86ioh))
	{
		printf("%s: can not map\n", sc->sc_dev.dv_xname);
		return;
	}

	/* load pure register value */
	sc->sc_pregv = bus_space_read_1(iot, n86ioh, 0);

	sc->sc_nsp = nsp;
	sc->sc_iot = iot;
	sc->sc_n86ioh = n86ioh;
	ident = necss_identify(iot, n86ioh);
	na.na_sa = sa;
	na.na_n86ioh = n86ioh;
	necss_systm_resource(iot, ident, nsp, 
			     &na.na_pcmbase, &na.na_ymbase, &na.na_spin);

	printf(" io0 0x%lx io1 0x%lx pin0 %ld(shared)\n", na.na_pcmbase,
		na.na_ymbase, na.na_spin);

	/* 
	 * First establish shutdownhook, because shutdownhook functions
	 * are inserted into the head of the list.
         */
	shutdownhook_establish(necss_shutdown, sc);

	/* enable sound control reg */
	necss_select(sc, NEC_SCR_EXT_ENABLE);

	/* attach pcm part */
	na.na_ident = ident;
	config_found(self, &na, NULL);

	/* attach ym230x part */
	na.na_ident = NECSS_YMS;
	config_found(self, &na, NULL);

	/* bind systmmsg */
	systmmsg_bind(self, necss_systmmsg);
}


/************************************************************
 * subfunctions
 ************************************************************/
void
necss_select(sc, sel)
	struct necss_softc *sc;
	int sel;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_n86ioh;
	u_int8_t val;

	val = bus_space_read_1(iot, ioh, 0);
	val = ~NEC_SCR_MASK;
	val |= (sel & NEC_SCR_MASK);
	bus_space_write_1(iot, ioh, 0, val);
}

void
necss_disable(sc)
	struct necss_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_n86ioh;
#if	0
	u_int8_t val;

	val = bus_space_read_1(iot, ioh, 0);
	val = ~NEC_SCR_MASK;
	val |= NEC_SCR_OPNA_DISABLE;
	bus_space_write_1(iot, ioh, 0, val);
#else
	bus_space_write_1(iot, ioh, 0, sc->sc_pregv);
#endif
}

static void
necss_shutdown(arg)
	void *arg;
{
	struct necss_softc *sc = (struct necss_softc *) arg;
	struct necss_iomap *nsp;

	necss_disable(sc);
	if (sc->sc_remapped != 0)
	{
		nsp = &necss_iomap[0];
		necss_rmap(nsp->nsp_ctrl, nsp->nsp_wss, nsp->nsp_yms);
	}
}

static int
necss_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct necss_softc *sc = (struct necss_softc *) dv;
	struct necss_iomap *nsp;

	switch (ev)
	{
	case SYSTM_EVENT_SUSPEND:
		necss_disable(sc);
		if (sc->sc_remapped != 0)
		{
			nsp = &necss_iomap[0];
			necss_rmap(nsp->nsp_ctrl, nsp->nsp_wss, nsp->nsp_yms);
		}
		break;

	case SYSTM_EVENT_RESUME:
		if (sc->sc_remapped != 0)
		{
			nsp = sc->sc_nsp;
			necss_rmap(nsp->nsp_ctrl, nsp->nsp_wss, nsp->nsp_yms);
		}
		necss_select(sc, NEC_SCR_EXT_ENABLE);
		break;
	}

	return 0;
}

/************************************************************
 * NEC resource allocation rules 
 ************************************************************/
#include "yms_necss.h"

int
necss_systm_resource(iot, type, nsp, basep, baseap, irqp)
	bus_space_tag_t iot;
	int type;
	struct necss_iomap *nsp;
	bus_addr_t *basep, *baseap;
	u_long *irqp;
{
	bus_addr_t iobase;
	u_int8_t regv;
	bus_space_handle_t ioh;
	static char wssirq[8] = {-1, 3, 5, 10, 12, -1, -1, -1};
	bus_addr_t opna_iobase[2] = {OPNA_IOBASE1, OPNA_IOBASE2};

	*irqp = 12;
	switch (type & NECSS_IDENT)
	{
	case NECSS_PCM86:
		if (nsp == &necss_iomap[0])
			iobase = opna_iobase[type & 2];
		else
			iobase = nsp->nsp_yms;
		*baseap = iobase;
		*basep = nsp->nsp_ctrl;

#if	NYMS_NECSS > 0
		{
		static u_int pcm86irq[4] = { 3, 13, 10, 12 };

		if (bus_space_map(iot, iobase, 4, 0, &ioh))
			return 0;
		if (opna_wait(iot, ioh) < 0)
			goto out1;

		regv = opna_read(iot, ioh, SSG_ENABLE);
		regv &= ~SSG_MASK_EN_IO;
		regv |= SSG_EN_IOB_OUT;
		opna_write(iot, ioh, SSG_ENABLE, regv);

		*irqp = pcm86irq[(opna_read(iot, ioh, SSG_IOA) >> 6) & 3];
out1:
		bus_space_unmap(iot, ioh, 4);
		}
#endif	/* NYMS_NECSS */
		return 0;		

	case NECSS_WSS:
		*baseap = nsp->nsp_yms;
		*basep = nsp->nsp_wss;
		if (bus_space_map(iot, *basep, 4, 0, &ioh))
			return 0;

		regv = bus_space_read_1(iot, ioh, 0);
		*irqp = wssirq[(regv >> 3) & 0x07];

		bus_space_unmap(iot, ioh, 4);
		return 0;
	}

	return 0;
}

/************************************************************
 * NEC PnP port allocation
 ************************************************************/
int
necss_rmap(ctrlp, wssp, ymsp)
	bus_addr_t ctrlp, wssp, ymsp;
{

	outb(NECSS_PnP_CONFIG, 
	     NEC_PnPR_ENABLE | 0x40 | NEC_PnPR_MAPENABLE | NEC_PnPR_CTRL);
	if ((inb(NECSS_PnP_CONFIG) & NEC_PnPR_MAPENABLE) == 0)
		return EINVAL;

	outb(NECSS_PnP_dataLow, ctrlp);
	outb(NECSS_PnP_dataHigh, ctrlp >> NBBY);

#ifdef	notyet
	outb(NECSS_PnP_CONFIG, 
	     NEC_PnPR_ENABLE | 0x40 | NEC_PnPR_MAPENABLE | NEC_PnPR_YMS);
	outb(NECSS_PnP_dataLow, ymsp);
	outb(NECSS_PnP_dataHigh, ymsp >> NBBY);

	outb(NECSS_PnP_CONFIG, 
	     NEC_PnPR_ENABLE | 0x40 | NEC_PnPR_MAPENABLE | NEC_PnPR_WSS);
	outb(NECSS_PnP_dataLow, wssp);
	outb(NECSS_PnP_dataHigh, wssp >> NBBY);
#endif	/* notyet */
	return 0;
}
