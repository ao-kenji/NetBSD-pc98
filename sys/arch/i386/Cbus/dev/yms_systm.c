/*	$NecBSD: yms_systm.c,v 1.5 1998/09/26 11:31:25 kmatsuda Exp $	*/
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

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/bus.h>
#include <machine/systmbusvar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/ic/ym2203reg.h>
#include <i386/Cbus/dev/ymsvar.h>
#include <i386/systm/necssreg.h>
#include <i386/systm/necssvar.h>
#include <i386/Cbus/dev/nec86reg.h>

static int	yms_necss_match __P((struct device *, struct cfdata *, void *));
void	yms_necss_attach __P((struct device *, struct device *, void *));
static  int yms_systmmsg __P((struct device *, systm_event_t));

struct cfattach yms_necss_ca = {
	sizeof(struct yms_softc), yms_necss_match, yms_necss_attach
};

static int
yms_necss_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct necss_attach_args *na = aux;
	struct systm_attach_args *sa = na->na_sa;
	bus_space_tag_t iot = sa->sa_iot;
	bus_space_handle_t ioh;

	if ((na->na_ident & NECSS_IDENT) != NECSS_YMS)
		return 0;

	if (bus_space_map(iot, na->na_ymbase, 8, 0, &ioh))
		return 0;

	if (yms_probesubr(sa->sa_iot, ioh) < 0)
		return 0;

	bus_space_unmap(iot, ioh, 8);
	return 1;
}

void
yms_necss_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct yms_softc *sc = (struct yms_softc *) self;
	struct necss_attach_args *na = aux;
	struct systm_attach_args *sa = na->na_sa;
	bus_space_tag_t iot = sa->sa_iot;
	bus_space_handle_t ioh;

	printf("\n");

	if (bus_space_map(iot, na->na_ymbase, 8, 0, &ioh))
	{
		printf("%s: can not map\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	systmmsg_bind(self, yms_systmmsg);
	yms_attachsubr(sc);

#ifdef	notyet
	isa_intr_establish(sa->sa_ic, na->na_spin, IST_EDGE, IPL_AUDIO,
			   yms_intr, sc);
#endif	/* notyet */
}

static int
yms_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct yms_softc *sc = (struct yms_softc *) dv;

    	if (ev == SYSTM_EVENT_RESUME)
        	yms_activate(sc);

    	return 0;
}

