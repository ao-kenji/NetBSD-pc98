/*	$NecBSD: wss_systm.c,v 1.10 1999/07/23 11:04:41 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
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
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/pio.h>
#include <machine/systmbusvar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <i386/systm/necssreg.h>
#include <i386/systm/necssvar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/madreg.h>
#include <i386/Cbus/dev/wssreg.h>

#include <i386/Cbus/dev/ad1848var.h>
#include <dev/isa/cs4231var.h>
#include <i386/Cbus/dev/wssvar.h>

static int wss_systmmsg __P((struct device *, systm_event_t));
static void wss_shutdown __P((void *));
int ad1848_deactivate __P((struct ad1848_softc *));
static int wss_necss_match __P((struct device *, struct cfdata *, void *));
void wss_necss_attach __P((struct device *, struct device *, void *));

struct wss_necss_softc {
	struct wss_softc sc_wss;

	u_int8_t sc_conf;			/* configuration register */
	int sc_spin;

	void *sc_ats;
};

struct cfattach wss_necss_ca = {
	sizeof(struct wss_necss_softc), wss_necss_match, wss_necss_attach
};

static int
wss_necss_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct necss_attach_args *na = aux;
	struct systm_attach_args *sa = na->na_sa;
	bus_space_tag_t iot = sa->sa_iot;
	bus_space_handle_t cioh;

	if ((na->na_ident & NECSS_IDENT) != NECSS_WSS)
		return 0;

	if (bus_space_map(iot, na->na_pcmbase, WSS_CODEC, 0, &cioh))
		return 0;
	bus_space_unmap(iot, cioh, WSS_CODEC);
	return 1;
}

void
wss_necss_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wss_necss_softc *sc = (void *) self;
	struct wss_softc *wsc = &sc->sc_wss;
	struct necss_attach_args *na = aux;
	struct systm_attach_args *sa = na->na_sa;
	bus_space_tag_t iot = sa->sa_iot;
	bus_space_handle_t ioh;
	u_int8_t val, version;
	isa_chipset_tag_t ic;
	static u_char dma_bits[4] = {1, 2, 0, 3};
    
    	if (bus_space_map(iot, na->na_pcmbase, WSS_CODEC, 0, &ioh))
	{
		printf("%s: can not map code registers\n", 
			wsc->sc_dev.dv_xname);
		return;
	}

	ic = sa->sa_ic;
	wsc->sc_iot = sa->sa_iot;
	wsc->sc_ioh = ioh;
	wsc->mad_chip_type = MAD_NONE;

	wsc->sc_ad1848.sc_iot = iot;
	wsc->sc_ad1848.sc_iobase = na->na_pcmbase + WSS_CODEC;
	wsc->sc_ad1848.sc_ic = ic;
    	wsc->sc_ad1848.parent = sc;

	if (ad1848_probe(&wsc->sc_ad1848) == 0)
	{
		printf("%s: ad1848 hardware missing\n", 
			wsc->sc_dev.dv_xname);
		goto bad;
	}
	
	val = bus_space_read_1(iot, ioh, 0);
	sc->sc_conf = val;

	/* drq */
	wsc->wss_drq = 1;
	if (isa_drq_isfree(ic, wsc->wss_drq) == 0)
	{
		wsc->wss_drq = 3;
		if (isa_drq_isfree(ic, wsc->wss_drq) == 0)
			goto bad;
	}
	wsc->sc_ad1848.sc_recdrq = wsc->wss_drq;
	sc->sc_conf &= ~7;
	sc->sc_conf |= dma_bits[wsc->wss_drq];
	printf(" drq %d", wsc->wss_drq);

	/* irq */
	sc->sc_spin = na->na_spin;

	/* attach */
	wsc->sc_ad1848.sc_reload = 1;
	ad1848_attach(&wsc->sc_ad1848);
    
	version = bus_space_read_1(iot, ioh, WSS_STATUS) & WSS_VERSMASK;
	printf(" (vers %d)\n", version);

	audio_attach_mi(&wss_hw_if, &wsc->sc_ad1848, &wsc->sc_dev);

	wsc->sc_ih = isa_intr_establish(sa->sa_ic, sc->sc_spin, IST_EDGE,
				IPL_AUDIO, ad1848_intr, &wsc->sc_ad1848);

	systmmsg_bind(self, wss_systmmsg);
    	sc->sc_ats = shutdownhook_establish(wss_shutdown, sc);

	bus_space_write_1(iot, ioh, 0, sc->sc_conf);
	return;

bad:
	bus_space_unmap(iot, ioh, WSS_CODEC);
}

void
wss_shutdown(arg)
	void * arg;
{
	struct wss_necss_softc *sc = arg;
	struct wss_softc *wsc = &sc->sc_wss;
	
	(void) ad1848_deactivate(&wsc->sc_ad1848);
}

static int
wss_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct wss_necss_softc *sc = (void *) dv;
	struct wss_softc *wsc = &sc->sc_wss;

	switch (ev)
	{
	case SYSTM_EVENT_SUSPEND:
		ad1848_deactivate(&wsc->sc_ad1848);
		break;

	case SYSTM_EVENT_RESUME:
		bus_space_write_1(wsc->sc_iot, wsc->sc_ioh, WSS_CONFIG, 
				  sc->sc_conf);
		ad1848_activate(&wsc->sc_ad1848);
		break;

	default:
		break;
	}

	return 0;
}
