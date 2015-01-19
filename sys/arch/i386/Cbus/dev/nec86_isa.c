/*	$NecBSD: nec86_isa.c,v 1.9 1998/09/26 11:31:11 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <machine/systmbusvar.h>

#include <i386/Cbus/dev/nec86reg.h>
#include <i386/Cbus/dev/nec86hwvar.h>
#include <i386/Cbus/dev/nec86var.h>

static int	nec86_isa_match __P((struct device *, struct cfdata *, void *));
void	nec86_isa_attach __P((struct device *, struct device *, void *));
static  int nec86_systmmsg __P((struct device *, systm_event_t));

struct cfattach pcm_isa_ca = {
	sizeof(struct nec86_softc), (cfmatch_t) nec86_isa_match, nec86_isa_attach
};

static int
nec86_isa_match(parent, match, aux)
    struct device *parent;
	struct cfdata *match;
	void *aux;
{
    struct isa_attach_args *ia = aux;
    bus_space_tag_t iot = ia->ia_iot;
    bus_space_handle_t n86h, coreh;
    int rv = 0;

    if (NEC86_IRQ_VALID(ia->ia_irq) == 0 || 
	NEC86_BASE_VALID(ia->ia_iobase) == 0)
	return 0;

    if (bus_space_map(iot, ia->ia_iobase, 1, 0, &n86h))
        return rv;
    if (bus_space_map(iot, ia->ia_iobase + NEC86_COREOFFSET, 
	NEC86_CORESIZE, 0, &coreh))
	goto bad;
   
    if (nec86_probesubr(iot, coreh, n86h) >= 0)
    {
	ia->ia_iosize = NEC86_PORT;
	rv = 1;
    }

    bus_space_unmap(iot, coreh, NEC86_CORESIZE);
bad:
    bus_space_unmap(iot, n86h, 1);
    return rv;
}

void
nec86_isa_attach(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
    struct nec86_softc *nsc = (struct nec86_softc *) self;
    struct nec86hw_softc *ysc = &nsc->sc_nec86hw;
    struct isa_attach_args *ia = aux;
    bus_space_tag_t iot = ia->ia_iot;
    bus_space_handle_t coreh, n86h;

    if (bus_space_map(iot, ia->ia_iobase, 1, 0, &n86h) ||
        bus_space_map(iot, ia->ia_iobase + NEC86_COREOFFSET, 
		      NEC86_CORESIZE, 0, &coreh))
    {
	printf("%s: can not map\n", ysc->sc_dev.dv_xname);
        return;
    }

    nsc->sc_n86iot = iot;
    nsc->sc_n86ioh = n86h;
    ysc->sc_iot = iot;
    ysc->sc_ioh = coreh;
    ysc->sc_cfgflags = ia->ia_cfgflags;

    systmmsg_bind(self, nec86_systmmsg);

    nec86_attachsubr(nsc);

    isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE, IPL_AUDIO,
		       nec86hw_intr, ysc);
}

static int
nec86_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
    struct nec86_softc *nsc = (struct nec86_softc *) dv;

    if (ev == SYSTM_EVENT_RESUME)
        nec86_attachsubr(nsc);

    return 0;
}
