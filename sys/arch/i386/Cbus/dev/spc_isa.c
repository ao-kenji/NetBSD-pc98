/*	$NecBSD: spc_isa.c,v 1.11 1998/09/26 11:31:21 kmatsuda Exp $	*/
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
/*
 * derived from MB89352 SCSI Protocol Controller (SPC) port on NetBSD/x68k.
 * ported to PC-98 by Kouichi MATSUDA.
 */

/*
 * MB89352 SCSI Protocol Controller (SPC) routines.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <i386/Cbus/dev/mb89352reg.h>
#include <i386/Cbus/dev/mb89352var.h>

static int	spc_isa_match	__P((struct device *, struct cfdata *, void *));
void	spc_isa_attach	__P((struct device *, struct device *, void *));

struct spc_isa_softc {
	struct	spc_softc sc_spc;	/* real "spc" softc */

	/* ISA-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

struct cfattach spc_isa_ca = {
	sizeof(struct spc_isa_softc), spc_isa_match, spc_isa_attach
};

/*
 * INITIALIZATION ROUTINES (probe, attach ++)
 */
/*
 * spc_isa_match: probe for MB89352 SCSI-controller
 * returns non-zero value if a controller is found.
 */
static int
spc_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rv;

	/* Disallow wildcarded i/o address. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT || ia->ia_irq == ISACF_IRQ_DEFAULT)
		return (0);

	if (bus_space_map(iot, ia->ia_iobase, SPC_ISA_IOSIZE, 0, &ioh))
		return (0);

	SPC_TRACE(("spc_isa_match: port 0x%x\n", ia->ia_iobase));
	rv = spc_find(iot, ioh);

	bus_space_unmap(iot, ioh, SPC_ISA_IOSIZE);

	if (rv) {
		ia->ia_msize = 0;
		ia->ia_iosize = SPC_ISA_IOSIZE;
	}
	return rv;
}

void
spc_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct spc_isa_softc *isc = (void *)self;
	struct spc_softc *sc = &isc->sc_aic;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	isa_chipset_tag_t ic = ia->ia_ic;

	printf("\n");

	if (bus_space_map(iot, ia->ia_iobase, SPC_ISA_IOSIZE, 0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	SPC_TRACE(("spc_isa_attach: port 0x%x\n", ia->ia_iobase));
	if (!spc_find(iot, ioh)) {
		printf("%s: spc_find failed", sc->sc_dev.dv_xname);
		return;
	}

	isc->sc_ih = isa_intr_establish(ic, ia->ia_irq, IST_EDGE, IPL_BIO,
	    spcintr, sc);
	if (isc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	spcattach(sc);
}
