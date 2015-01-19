/*	$NecBSD: joy_pisa.c,v 1.6 1998/09/26 11:31:06 kmatsuda Exp $	*/
/*	$NetBSD: joy_isa.c,v 1.1 1997/01/16 23:17:48 christos Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
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

#include <i386/Cbus/dev/joyvar.h>

#include <dev/isa/pisaif.h>

static int	joy_pisa_match __P((struct device *, struct cfdata *, void *));
void	joy_pisa_attach __P((struct device *, struct device *, void *));
int	joy_pisa_map __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t *));

struct joy_pisa_softc {
	struct joy_softc sc_joy;	/* real "joy" softc */

	/* PISA-specific goo. */
	pisa_device_handle_t sc_dh;
};

struct cfattach joy_pisa_ca = {
	sizeof(struct joy_pisa_softc), joy_pisa_match, joy_pisa_attach
};

struct pisa_functions joy_pd = {
};

int
joy_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;	/* pisa attach arg */
	pisa_device_handle_t dh = pa->pa_dh;	/* slot device handle */
	bus_space_tag_t iot = pa->pa_iot;	/* io space tag */
	bus_space_handle_t ioh;			/* io space handle */
	int rval = 0;

	/*
	 * allocate a bus space handle and
	 * open the physical device window 0.
	 */
	if (joy_pisa_map(dh, iot, &ioh))
		return (0);

	/*
	 * check hardware responses to ensure the core chip really works.
	 */
	bus_space_write_1(iot, ioh, 0, 0xff);
	delay(10000);
	if ((bus_space_read_1(iot, ioh, 0) & 0x0f) != 0x0f)
		rval = 1;

	/*
	 * deallocate the bus space handle
	 * and close physical device window 0.
	 */
	pisa_space_unmap(dh, iot, ioh);
	return rval;
}

void
joy_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	struct joy_pisa_softc *jsc = (void *) self;
	struct joy_softc *sc = &jsc->sc_joy;
	pisa_device_handle_t dh = pa->pa_dh;

	/*
	 * register activate and deactivate functions.
	 * (no functions in case of joy)
	 */
	jsc->sc_dh = dh;
	pisa_register_functions(dh, self, &joy_pd);

	/*
	 * call the attach function of joy.c.
	 */
	printf("\n");

	sc->sc_iot = pa->pa_iot;
	if (joy_pisa_map(dh, sc->sc_iot, &sc->sc_ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	joyattach(sc);
}

int
joy_pisa_map(dh, iot, iohp)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	bus_space_handle_t *iohp;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	int idx;

	for (idx = 0; idx < SLOT_DEVICE_NIO; idx ++)
		if (PISA_DR_IMS(dr, idx) == 1)
			break;
	if (idx == SLOT_DEVICE_NIO)
		return EINVAL;

	return pisa_space_map(dh, idx, iohp);
}
