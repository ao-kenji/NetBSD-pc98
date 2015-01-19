/*	$NecBSD: nulldv_isa.c,v 1.3 1998/09/26 06:43:20 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
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

/*
 * Place holder for IO and MEMORY spaces
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>
#include <dev/isa/pisavar.h>

#include <vm/vm.h>

#define	NULLDV_ISA_IOSZ	0x10

struct nulldv_softc {
	struct device sc_dev;
};

static int nulldv_isa_match __P((struct device *, struct cfdata *, void *));
static void nulldv_isa_attach __P((struct device *, struct device *, void *));
int nulldvintr __P((void *));

struct cfattach nulldv_isa_ca = {
	sizeof(struct nulldv_softc), nulldv_isa_match, nulldv_isa_attach
};

static int
nulldv_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	int rv;

	if (ia->ia_iobase != IOBASEUNK)
	{
		rv = bus_space_map(ia->ia_iot, ia->ia_iobase, NULLDV_ISA_IOSZ,
				   0, &ioh);
		if (rv != 0)
			return 0;
		bus_space_unmap(ia->ia_iot, ioh, NULLDV_ISA_IOSZ);
		ia->ia_iosize = NULLDV_ISA_IOSZ;
	}

	if (ia->ia_maddr != MADDRUNK)
	{
		rv = bus_space_map(ia->ia_memt, ia->ia_maddr, ia->ia_msize,
				   0, &ioh);
		if (rv != 0)
			return 0;
		bus_space_unmap(ia->ia_memt, ioh, ia->ia_msize);
	}

	return 1;
}

static void
nulldv_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct nulldv_softc *sc = (void *) self;
	bus_space_handle_t ioh;
	int rv;

	printf("\n");

	if (ia->ia_iobase != IOBASEUNK)
		rv = bus_space_map(ia->ia_iot, ia->ia_iobase, NULLDV_ISA_IOSZ,
			 	   0, &ioh);

	if (ia->ia_maddr != MADDRUNK)
		bus_space_map(ia->ia_memt, ia->ia_maddr, ia->ia_msize,
			      0, &ioh);

	if (ia->ia_irq != IRQUNK)
		isa_intr_establish(ia->ia_ic, ia->ia_irq,
			 	   IST_EDGE, IPL_NONE, nulldvintr, sc);

}

int
nulldvintr(arg)
	void *arg;
{

	return 1;
}
