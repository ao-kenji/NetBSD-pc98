/*	$NecBSD: spc_pisa.c,v 1.16 1998/09/26 11:31:22 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff.  All rights reserved.
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
#include <dev/isa/pisaif.h>
#include <i386/Cbus/dev/mb89352reg.h>
#include <i386/Cbus/dev/mb89352var.h>

static int	spc_pisa_match	__P((struct device *, struct cfdata *, void *));
void	spc_pisa_attach	__P((struct device *, struct device *, void *));

struct spc_pisa_softc {
	struct	spc_softc sc_spc;	/* real "spc" softc */

	/* PISA-specific goo. */
	void	*sc_ih;			/* interrupt handler */
	pisa_device_handle_t sc_dh;
};

static int spc_scsi_activate __P((pisa_device_handle_t));
static int spc_scsi_deactivate __P((pisa_device_handle_t));

struct cfattach spc_pisa_ca = {
	sizeof(struct spc_pisa_softc), spc_pisa_match, spc_pisa_attach
};

struct pisa_functions spc_pd = {
	spc_scsi_activate, spc_scsi_deactivate
};

/*
 * INITIALIZATION ROUTINES (probe, attach ++)
 */

/*
 * spc_pisa_match: probe for MB89352 SCSI-controller
 * returns non-zero value if a controller is found.
 */
static int
spc_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;
	int rv;

	if (PISA_DR_IMS(dr, PISA_IO0) != SPC_ISA_IOSIZE)
		return (0);
	if (pisa_space_map(dh, PISA_IO0, &ioh))
		return (0);

	SPC_TRACE(("spc_pisa_match: port 0x%x\n", PISA_IO0));
	rv = spc_find(iot, ioh);

	pisa_space_unmap(dh, iot, ioh);

	return rv;
}

void
spc_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	struct spc_pisa_softc *psc = (void *)self;
	struct spc_softc *sc = &psc->sc_spc;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;

	printf("\n");

	psc->sc_dh = dh;
	pisa_register_functions(dh, self, &spc_pd);

	if (PISA_DR_IMS(dr, PISA_IO0) != SPC_ISA_IOSIZE) {
		printf("%s: illegal i/o size, can't map i/o space\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	if (pisa_space_map(dh, PISA_IO0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	SPC_TRACE(("spc_pisa_attach: port 0x%x\n", PISA_IO0));
	if (!spc_find(iot, ioh)) {
		printf("%s: spc_find failed", sc->sc_dev.dv_xname);
		return;
	}

	psc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_BIO, spcintr, sc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	spcattach(sc);
}

static int
spc_scsi_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct spc_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct spc_softc *sc = &psc->sc_spc;

	sc->sc_flags |= SPC_INACTIVE;
	return 0;
}

static int
spc_scsi_activate(dh)
	pisa_device_handle_t dh;
{
	struct spc_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct spc_softc *sc = &psc->sc_spc;

	sc->sc_flags &= ~SPC_INACTIVE;

	spc_init(sc);

#ifdef	SPC_SCSIBUS_RESCAN
	/* rescan the scsi bus if completely free */
	if (PISA_RES_EVENT(dh) == PISA_EVENT_INSERT &&
	    sc->sc_nexus == NULL &&
	    sc->ready_list.tqh_first == NULL &&
	    sc->nexus_list.tqh_first == NULL)
		scsi_probe_busses((int) sc->sc_link.scsibus, -1, -1);
#endif	/* SPC_SCSIBUS_RESCAN */

	if (sc->sc_nexus == NULL)
		spc_sched(sc);

	return 0;
}
