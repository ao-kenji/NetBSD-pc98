/*	$NecBSD: wss_scp.c,v 1.8 1999/07/23 11:04:41 honda Exp $	*/
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

#include <dev/ic/ad1848reg.h>
#include <dev/isa/madreg.h>
#include <i386/Cbus/dev/wssreg.h>

#include <i386/Cbus/dev/ad1848var.h>
#include <dev/isa/cs4231var.h>
#include <i386/Cbus/dev/wssvar.h>
#include <i386/Cbus/dev/scpvar.h>

static int wss_scp_match __P((struct device *, struct cfdata *, void *));
void wss_scp_attach __P((struct device *, struct device *, void *));
int wss_scp_activate __P((void *));

struct wss_scp_softc {
	struct wss_softc sc_wss;
};

struct cfattach wss_scp_ca = {
	sizeof(struct wss_scp_softc), wss_scp_match, wss_scp_attach
};

static speed_struct speed_table_scp[] =  {
	{11025, (1 << 1) | 0},
	{22050, (3 << 1) | 0},
	{44100, (5 << 1) | 0},
};

static int
wss_scp_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct scp_attach_args *sa = aux;

	if (strcmp(sa->sa_name, "wss") != 0)
		return 0;
	return 1;
}

void
wss_scp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wss_scp_softc *sc = (void *) self;
	struct wss_softc *wsc = &sc->sc_wss;
	struct scp_attach_args *sa = aux;
	bus_space_tag_t iot = sa->sa_iot;
	bus_space_handle_t ioh = sa->sa_ioh;

	wsc->sc_iot = iot;
	wsc->sc_ioh = ioh;
	wsc->mad_chip_type = MAD_NONE;

	wsc->sc_ad1848.sc_iot = iot;
	wsc->sc_ad1848.sc_ioh = ioh;
	wsc->sc_ad1848.sc_iobase = 0;
	wsc->sc_ad1848.sc_ic = sa->sa_ic;
    	wsc->sc_ad1848.parent = sc;

	wsc->sc_ad1848.sc_st = speed_table_scp;
        wsc->sc_ad1848.sc_stsz =
		sizeof(speed_table_scp) / sizeof(speed_struct);
	wsc->sc_ad1848.sc_std = 0;

	if (ad1848_probe(&wsc->sc_ad1848) == 0)
	{
		printf("%s: ad1848 hardware missing\n", wsc->sc_dev.dv_xname);
		return;
	}
	
	wsc->wss_drq = -1;
	wsc->sc_ad1848.sc_recdrq = wsc->wss_drq;
	ad1848_attach(&wsc->sc_ad1848);
	printf("\n");

	audio_attach_mi(&wss_hw_if, &wsc->sc_ad1848, &wsc->sc_dev);

	sa->sa_arg = &wsc->sc_ad1848;
	sa->sa_activate = wss_scp_activate;
	sa->sa_intr = ad1848_intr;
}

int
wss_scp_activate(arg)
	void *arg;
{

	ad1848_activate(arg);
	return 0;
}
