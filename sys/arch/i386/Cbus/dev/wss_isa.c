/*	$NecBSD: wss_isa.c,v 1.7 1999/07/26 06:31:54 honda Exp $	*/
/*	$NetBSD: wss.c,v 1.39.2.1 1998/12/05 07:36:46 cgd Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * MAD support:
 * Copyright (c) 1996 Lennart Augustsson
 * Based on code which is
 * Copyright (c) 1995 Hannu Savolainen
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*
 * Copyright by Hannu Savolainen 1994
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <i386/Cbus/dev/wssreg.h>
#include <dev/isa/madreg.h>

#include <i386/Cbus/dev/ad1848var.h>
#include <dev/isa/cs4231var.h>
#include <i386/Cbus/dev/wssvar.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (wssdebug) printf x
int	wssdebug = 0;
#else
#define DPRINTF(x)
#endif

#ifdef __BROKEN_INDIRECT_CONFIG
static int	wss_isa_probe __P((struct device *, void *, void *));
#else
static int	wss_isa_probe __P((struct device *, struct cfdata *, void *));
#endif
void	wss_isa_attach __P((struct device *, struct device *, void *));
static int	wssfind __P((struct device *, struct wss_softc *, struct isa_attach_args *));

struct cfattach wss_isa_ca = {
	sizeof(struct wss_softc), wss_isa_probe, wss_isa_attach
};

/*
 * Probe for the Microsoft Sound System hardware.
 */
static int
wss_isa_probe(parent, match, aux)
    struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
    void *match;
#else
    struct cfdata *match;
#endif
    void *aux;
{
    struct wss_softc probesc, *sc = &probesc;

    memset(sc, 0, sizeof *sc);
#ifdef __BROKEN_INDIRECT_CONFIG
    sc->sc_dev.dv_cfdata = ((struct device *)match)->dv_cfdata;
#else
    sc->sc_dev.dv_cfdata = match;
#endif
    if (wssfind(parent, sc, aux)) {
        bus_space_unmap(sc->sc_iot, sc->sc_ioh, WSS_CODEC);
        ad1848_unmap(&sc->sc_ad1848);
        madunmap(sc);
        return 1;
    } else
        /* Everything is already unmapped */
        return 0;
}

static int
wssfind(parent, sc, ia)
    struct device *parent;
    struct wss_softc *sc;
    struct isa_attach_args *ia;
{
    static u_char interrupt_bits[12] = {
	-1, -1, -1, -1, -1, -1, -1, 0x08, -1, 0x10, 0x18, 0x20
    };
    static u_char dma_bits[4] = {1, 2, 0, 3};
    
    sc->sc_iot = ia->ia_iot;
    if (sc->sc_dev.dv_cfdata->cf_flags & 1)
	sc->mad_chip_type = madprobe(sc, ia->ia_iobase);
    else
	sc->mad_chip_type = MAD_NONE;

    if (!WSS_BASE_VALID(ia->ia_iobase)) {
	DPRINTF(("wss: configured iobase %x invalid\n", ia->ia_iobase));
	goto bad1;
    }

    /* Map the ports upto the AD1848 port */
    if (bus_space_map(sc->sc_iot, ia->ia_iobase, WSS_CODEC, 0, &sc->sc_ioh))
	goto bad1;

    sc->sc_ad1848.sc_iot = sc->sc_iot;
    sc->sc_ad1848.sc_iobase = ia->ia_iobase + WSS_CODEC;
    sc->sc_ad1848.parent = sc;

    /* Is there an ad1848 chip at (WSS iobase + WSS_CODEC)? */
    if (ad1848_probe(&sc->sc_ad1848) == 0)
	goto bad;
	
    ia->ia_iosize = WSS_NPORT;

    /* Setup WSS interrupt and DMA */
    if (!WSS_DRQ_VALID(ia->ia_drq)) {
	DPRINTF(("wss: configured dma chan %d invalid\n", ia->ia_drq));
	goto bad;
    }
    sc->wss_drq = ia->ia_drq;

    /* XXX reqdrq? */
    if (sc->wss_drq != -1 && isa_drq_isfree(parent, sc->wss_drq) == 0)
	    goto bad;

    if (sc->sc_ad1848.mode > 1 && ia->ia_drq2 != -1 && 
        isa_drq_isfree(parent, ia->ia_drq2) == 0)
    	goto bad;

#ifdef NEWCONFIG
    /*
     * If the IRQ wasn't compiled in, auto-detect it.
     */
    if (ia->ia_irq == IRQUNK) {
	ia->ia_irq = isa_discoverintr(ad1848_forceintr, &sc->sc_ad1848);
	if (!WSS_IRQ_VALID(ia->ia_irq)) {
	    printf("wss: couldn't auto-detect interrupt\n");
	    goto bad;
	}
    }
    else
#endif
    if (!WSS_IRQ_VALID(ia->ia_irq)) {
	DPRINTF(("wss: configured interrupt %d invalid\n", ia->ia_irq));
	goto bad;
    }

    sc->wss_irq = ia->ia_irq;

    bus_space_write_1(sc->sc_iot, sc->sc_ioh, WSS_CONFIG,
		      (interrupt_bits[ia->ia_irq] | dma_bits[ia->ia_drq]));

    if (sc->sc_ad1848.mode <= 1)
	ia->ia_drq2 = -1;
    return 1;

bad:
    bus_space_unmap(sc->sc_iot, sc->sc_ioh, WSS_CODEC);
bad1:
    madunmap(sc);
    return 0;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
wss_isa_attach(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
    struct wss_softc *sc = (struct wss_softc *)self;
    struct isa_attach_args *ia = (struct isa_attach_args *)aux;
    int version;
    
    if (!wssfind(parent, sc, ia)) {
        printf("%s: wssfind failed\n", sc->sc_dev.dv_xname);
        return;
    }

    madattach(sc);

    sc->sc_ad1848.sc_recdrq = sc->sc_ad1848.mode > 1 && ia->ia_drq2 != -1 ? ia->ia_drq2 : ia->ia_drq;
    sc->sc_ad1848.sc_ic = ia->ia_ic;

#ifdef NEWCONFIG
    isa_establish(&sc->sc_id, &sc->sc_dev);
#endif
    sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE, IPL_AUDIO,
        ad1848_intr, &sc->sc_ad1848);

    ad1848_attach(&sc->sc_ad1848);
    
    version = bus_space_read_1(sc->sc_iot, sc->sc_ioh, WSS_STATUS) & WSS_VERSMASK;
    printf(" (vers %d)", version);
    if (sc->mad_chip_type != MAD_NONE)
        printf(", %s",
               sc->mad_chip_type == MAD_82C929 ? "82C929" :
               sc->mad_chip_type == MAD_82C928 ? "82C928" :
               "OTI-601D");
    printf("\n");

    sc->sc_ad1848.parent = sc;

    audio_attach_mi(&wss_hw_if, 0, &sc->sc_ad1848, &sc->sc_dev);
}

