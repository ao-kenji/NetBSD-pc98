/*	$NecBSD: joy_isa.c,v 1.4 1998/09/26 11:31:05 kmatsuda Exp $	*/
/*	$NetBSD: joy_isa.c,v 1.1.14.1 1997/10/31 20:57:35 mellon Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#define WANT_JOYSTICK_CONNECTED		/* for debug & safe probe */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1995 Jean-Marc Zucconi
 * All rights reserved.
 *
 * Ported to NetBSD by Matthieu Herrb <matthieu@laas.fr>.  Additional
 * modification by Jason R. Thorpe <thorpej@NetBSD.ORG>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>

#ifdef	ORIGINAL_CODE
#include <i386/isa/joyvar.h>
#else	/* PC-98 */
#include <i386/Cbus/dev/joyvar.h>
#endif	/* PC-98 */

#define JOY_NPORTS    1

#ifdef  ORIGINAL_CODE
int	joy_isa_probe __P((struct device *, void *, void *));
#else   /* PC-98 */
int	joy_isa_probe __P((struct device *, struct cfdata *, void *));
#endif	/* PC-98 */
void	joy_isa_attach __P((struct device *, struct device *, void *));

struct cfattach joy_isa_ca = {
	sizeof(struct joy_softc), joy_isa_probe, joy_isa_attach
};

int
joy_isa_probe(parent, match, aux)
	struct device *parent;
#ifdef  ORIGINAL_CODE
	void *match, *aux;
#else   /* PC-98 */
	struct cfdata *match;
	void *aux;
#endif	/* PC-98 */
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rval = 0;

	if (ia->ia_iobase == IOBASEUNK)
		return (0);

	if (bus_space_map(iot, ia->ia_iobase, JOY_NPORTS, 0, &ioh))
		return (0);

#ifdef WANT_JOYSTICK_CONNECTED
	bus_space_write_1(iot, ioh, 0, 0xff);
	DELAY(10000);		/* 10 ms delay */
	if ((bus_space_read_1(iot, ioh, 0) & 0x0f) != 0x0f)
		rval = 1;
#else
	rval = 1;
#endif

	bus_space_unmap(iot, ioh, JOY_NPORTS);

	ia->ia_iosize = JOY_NPORTS;
	ia->ia_msize = 0;
	return (rval);
}

void
joy_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct joy_softc *sc = (struct joy_softc *) self;
	struct isa_attach_args *ia = aux;

	printf("\n");

	sc->sc_iot = ia->ia_iot;

	if (bus_space_map(sc->sc_iot, ia->ia_iobase, JOY_NPORTS, 0,
	    &sc->sc_ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	joyattach(sc);
}
