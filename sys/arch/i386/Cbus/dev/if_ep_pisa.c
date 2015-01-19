/*	$NecBSD: if_ep_pisa.c,v 1.22 1999/07/23 07:08:26 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@novatel.ca>
 * Copyright (c) 1996 John Kohl
 * All rights reserved.
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include "opt_inet.h"
#include "opt_ns.h"
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

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mii/miivar.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/isa/isavar.h>
#include <dev/isa/elink.h>
#include <dev/isa/pisaif.h>

static void ep_pisa_attach __P((struct device *, struct device *, void *));
static int ep_pisa_probe __P((struct device *, struct cfdata *, void *));
static int ep_pisa_activate __P((pisa_device_handle_t));
static int ep_pisa_deactivate __P((pisa_device_handle_t));
int ep_pisa_enable __P((struct ep_softc *));
void ep_pisa_disable __P((struct ep_softc *));
static int ep_pisa_probesubr __P((bus_space_tag_t iot, bus_space_handle_t));

struct ep_pisa_softc {
	struct ep_softc sc_ep;

	pisa_device_handle_t sc_dh;
};

struct cfattach ep_pisa_ca = {
	sizeof(struct ep_pisa_softc), ep_pisa_probe, ep_pisa_attach
};

struct pisa_functions ep_pd = {
	ep_pisa_activate, ep_pisa_deactivate,
};

static int
ep_pisa_probe(parent, self, aux)
	struct device *parent;
	struct cfdata *self;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t ioh;
	int rv = 0;

	if (pisa_space_map(dh, PISA_IO0, &ioh))
		return rv;

	rv = ep_pisa_probesubr(pa->pa_iot, ioh);

	pisa_space_unmap(dh, pa->pa_iot, ioh);
	return rv;
}

void
ep_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ep_pisa_softc *sc = (void *) self;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_slot_tag_t st = dh->dh_stag;
	bus_space_handle_t ioh;

	if (pisa_space_map(dh, PISA_IO0, &ioh))
	{
		printf("%s: can't map I/O space", sc->sc_ep.sc_dev.dv_xname);
		return;
	}

	sc->sc_dh = dh;
	pisa_register_functions(dh, self, &ep_pd);

	sc->sc_ep.sc_iot = pa->pa_iot;
	sc->sc_ep.sc_ioh = ioh;
	if (st->st_type == PISA_SLOT_PCMCIA)
		sc->sc_ep.bustype = ELINK_BUS_PCMCIA;
	else
		sc->sc_ep.bustype = ELINK_BUS_ISA;

	sc->sc_ep.enabled = 1;
	sc->sc_ep.enable = ep_pisa_enable;
	sc->sc_ep.disable = ep_pisa_disable;

	printf(": 3Com 3C589 Ethernet\n");
	epconfig(&sc->sc_ep, ELINK_CHIPSET_3C509, NULL);

	sc->sc_ep.sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_NET,
		 		epintr, &sc->sc_ep);
}

static int
ep_pisa_activate(dh)
	pisa_device_handle_t dh;
{
	struct ep_pisa_softc *sc = PISA_DEV_SOFTC(dh);

	if (ep_pisa_probesubr(sc->sc_ep.sc_iot, sc->sc_ep.sc_ioh) == 0)
		return EINVAL;

	sc->sc_ep.enabled = 1;
	return 0;
}

static int
ep_pisa_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct ep_pisa_softc *sc = PISA_DEV_SOFTC(dh);
	struct ifnet *ifp = &sc->sc_ep.sc_ethercom.ec_if;
	int s = splimp();

	if_down(ifp);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_UP);
	sc->sc_ep.enabled = 0;
	splx(s);

	return 0;
}

int
ep_pisa_enable(esc)
	struct ep_softc *esc;
{
	struct ep_pisa_softc *sc = (void *) esc;

	return pisa_slot_device_enable(sc->sc_dh);
}

void
ep_pisa_disable(esc)
	struct ep_softc *esc;
{
	struct ep_pisa_softc *sc = (void *) esc;

	pisa_slot_device_disable(sc->sc_dh);
}

static int
ep_pisa_probesubr(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	u_int16_t vendor;

	bus_space_write_2(iot, ioh, ELINK_COMMAND, WINDOW_SELECT | 0);
	vendor = htons(bus_space_read_2(iot, ioh, ELINK_W0_MFG_ID));

	return ((vendor == MFG_ID) ? 1 : 0);
}
