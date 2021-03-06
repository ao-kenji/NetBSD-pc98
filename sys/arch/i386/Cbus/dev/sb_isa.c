/*	$NecBSD: sb_isa.c,v 1.6 1999/08/02 19:34:22 honda Exp $	*/
/*	$NetBSD: sb_isa.c,v 1.21 1999/03/22 07:37:35 mycroft Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 * Copyright (c) 1991-1993 Regents of the University of California.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#ifdef	ORIGINAL_CODE
#include <dev/isa/sbreg.h>
#else	/* PC-98 */
#include <i386/Cbus/dev/sbreg.h>
#endif	/* PC-98 */
#include <dev/isa/sbvar.h>

#ifdef	ORIGINAL_CODE
#include <dev/isa/sbdspvar.h>
#else	/* PC-98 */
#include <i386/Cbus/dev/sbdspvar.h>
#endif	/* PC-98 */

static	int sbfind __P((struct device *, struct sbdsp_softc *, struct isa_attach_args *));

int	sb_isa_match __P((struct device *, struct cfdata *, void *));
void	sb_isa_attach __P((struct device *, struct device *, void *));

struct cfattach sb_isa_ca = {
	sizeof(struct sbdsp_softc), sb_isa_match, sb_isa_attach
};

/*
 * Probe / attach routines.
 */
#ifndef	ORIGINAL_CODE
int sb_init_iat __P((bus_space_iat_t, int));

int
sb_init_iat(sbiat, size)
	bus_space_iat_t sbiat;
	int size;
{
	int i;

	for (i = 0; i < size; i++) {
		*sbiat ++ = i * 0x100;	/* port skips 0x100 */
	}

	return 0;
}
#endif	/* PC-98 */

/*
 * Probe for the soundblaster hardware.
 */
int
sb_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct sbdsp_softc probesc, *sc = &probesc;

	bzero(sc, sizeof *sc);
	sc->sc_dev.dv_cfdata = match;
	strcpy(sc->sc_dev.dv_xname, "sb");
	return sbfind(parent, sc, aux);
}

static int
sbfind(parent, sc, ia)
	struct device *parent;
	struct sbdsp_softc *sc;
	struct isa_attach_args *ia;
{
	int rc = 0;
#ifndef	ORIGINAL_CODE
	bus_addr_t sbiat[SBP_NPORT];
#endif	/* PC-98 */

	if (!SB_BASE_VALID(ia->ia_iobase)) {
		printf("sb: configured iobase 0x%x invalid\n", ia->ia_iobase);
		return 0;
	}

	sc->sc_ic = ia->ia_ic;

	sc->sc_iot = ia->ia_iot;
	/* Map i/o space [we map 24 ports which is the max of the sb and pro */
#ifdef	ORIGINAL_CODE
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, SBP_NPORT, 0,
	    &sc->sc_ioh)) {
		printf("sb: can't map i/o space 0x%x/%d in probe\n",
		    ia->ia_iobase, SBP_NPORT);
		return 0;
	}
#else	/* PC-98 */
	if (sb_init_iat(sbiat, SBP_NPORT))
		return 0;
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, 0, 0, &sc->sc_ioh)) {
		printf("sb: can't map i/o space 0x%x/%d in probe\n",
		    ia->ia_iobase, SBP_NPORT);
		return 0;
	}
	if (bus_space_map_load(sc->sc_iot, sc->sc_ioh, SBP_NPORT, sbiat,
	    BUS_SPACE_MAP_FAILFREE)) {
		printf("sb: can't load i/o space 0x%x/%d in probe\n",
		    ia->ia_iobase, SBP_NPORT);
		return 0;
	}
#endif	/* PC-98 */

	/* XXX These are only for setting chip configuration registers. */
	sc->sc_iobase = ia->ia_iobase;
	sc->sc_irq = ia->ia_irq;

	sc->sc_drq8 = ia->ia_drq;
	sc->sc_drq16 = ia->ia_drq2;

	if (!sbmatch(sc))
		goto bad;

	if (ISSBPROCLASS(sc))
		ia->ia_iosize = SBP_NPORT;
	else
		ia->ia_iosize = SB_NPORT;

	if (!ISSB16CLASS(sc) && sc->sc_model != SB_JAZZ)
		ia->ia_drq2 = -1;

	ia->ia_irq = sc->sc_irq;

	rc = 1;

bad:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, SBP_NPORT);
	return rc;
}


/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
sb_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbdsp_softc *sc = (struct sbdsp_softc *)self;
	struct isa_attach_args *ia = aux;
#ifndef	ORIGINAL_CODE
	bus_addr_t sbiat[SBP_NPORT];
#endif	/* PC-98 */

#ifdef	ORIGINAL_CODE
	if (!sbfind(parent, sc, ia) || 
	    bus_space_map(sc->sc_iot, ia->ia_iobase, ia->ia_iosize, 
			  0, &sc->sc_ioh)) {
		printf("%s: sbfind failed\n", sc->sc_dev.dv_xname);
		return;
	}
#else	/* PC-98 */
	if (!sbfind(parent, sc, ia)) {
		printf("%s: sbfind failed\n", sc->sc_dev.dv_xname);
		return;
	}
	if (sb_init_iat(sbiat, ia->ia_iosize))
		return;
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, 0, 0, &sc->sc_ioh)) {
		printf("sb: can't map i/o space 0x%x/%d in probe\n",
		    ia->ia_iobase, ia->ia_iosize);
		return;
	}
	if (bus_space_map_load(sc->sc_iot, sc->sc_ioh, ia->ia_iosize, sbiat,
	    BUS_SPACE_MAP_FAILFREE)) {
		printf("sb: can't load i/o space 0x%x/%d in probe\n",
		    ia->ia_iobase, ia->ia_iosize);
		return;
	}
#endif	/* PC-98 */

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_AUDIO, sbdsp_intr, sc);

	sbattach(sc);
}
