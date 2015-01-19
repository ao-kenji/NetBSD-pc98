/*	$NecBSD: aic_pisa.c,v 1.11.10.1 1999/12/12 00:35:08 kmatsuda Exp $	*/
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

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <dev/ic/aic6360reg.h>
#include <dev/ic/aic6360var.h>
#include <i386/Cbus/dev/aichw.h>

/*
 * from sys/dev/ic/aic6360.c,
 * which are not declared at sys/dev/ic/aic6360var.h.
 */
void	aic_init	__P((struct aic_softc *));
void	aic_sched	__P((struct aic_softc *));

static int	aic_pisa_probe	__P((struct device *, struct cfdata *, void *));
void	aic_pisa_attach	__P((struct device *, struct device *, void *));

struct aic_pisa_softc {
	struct	aic_softc sc_aic;	/* real "aic" softc */
	/* XXX: for sc_aic->sc_flags */
#define AIC_INACTIVE	0x80	/* The FIFO data path is inactive */

	/* PISA-specific goo. */
	void	*sc_ih;			/* interrupt handler */
	pisa_device_handle_t sc_dh;
};

struct cfattach aic_pisa_ca = {
	sizeof(struct aic_pisa_softc), aic_pisa_probe, aic_pisa_attach
};

static int aic_pisa_scsi_activate __P((pisa_device_handle_t));
static int aic_pisa_scsi_deactivate __P((pisa_device_handle_t));
static int aic_pisa_scsi_notify __P((pisa_device_handle_t, pisa_event_t));

struct pisa_functions aic_pd = {
       aic_pisa_scsi_activate, aic_pisa_scsi_deactivate, NULL, NULL,
       aic_pisa_scsi_notify
};


/*
 * INITIALIZATION ROUTINES (probe, attach ++)
 */

/*
 * aic_pisa_probe: probe for AIC6360 SCSI-controller
 * returns non-zero value if a controller is found.
 */
static int
aic_pisa_probe(parent, match, aux)
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

	if (PISA_DR_IOSIZE(dr) != 0x20 && PISA_DR_IOSIZE(dr) != 0x40)
		return (0);
	PISA_DR_IMF(dr, PISA_IO0) |= SDIM_BUS_FOLLOW;
	if (pisa_space_map_load(dh, PISA_IO0, AIC_ISA_IOSIZE,
	    IS_AHA1030(PISA_DR_DVCFG(dr)) ? BUS_SPACE_IAT_2 : BUS_SPACE_IAT_1,
	    &ioh))
		return (0);

	AIC_TRACE(("aic_pisa_probe: port 0x%x\n", PISA_IO0));
	rv = aic_find(iot, ioh);

	pisa_space_unmap(dh, iot, ioh);

	return rv;
}

void
aic_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	struct aic_pisa_softc *psc = (void *)self;
	struct aic_softc *sc = &psc->sc_aic;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;

	printf("\n");

	psc->sc_dh = dh;
	pisa_register_functions(dh, self, &aic_pd);

	PISA_DR_IMF(dr, PISA_IO0) |= SDIM_BUS_FOLLOW;
	if (pisa_space_map_load(dh, PISA_IO0, AIC_ISA_IOSIZE,
	    IS_AHA1030(PISA_DR_DVCFG(dr)) ? BUS_SPACE_IAT_2 : BUS_SPACE_IAT_1,
	    &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	AIC_TRACE(("aic_pisa_attach: port 0x%x\n", PISA_IO0));
	if (!aic_find(iot, ioh)) {
		printf("%s: aic_find failed", sc->sc_dev.dv_xname);
		return;
	}

	psc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_BIO,
	    aicintr, sc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	aicattach(sc);
}

static int
aic_pisa_scsi_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct aic_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct aic_softc *sc = &psc->sc_aic;

	sc->sc_flags |= AIC_INACTIVE;
	return 0;
}

static int
aic_pisa_scsi_activate(dh)
	pisa_device_handle_t dh;
{
	struct aic_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct aic_softc *sc = &psc->sc_aic;

	sc->sc_flags &= ~AIC_INACTIVE;

	aic_init(sc);

#ifdef	AIC6360_SCSIBUS_RESCAN
	/* rescan the scsi bus if completely free */
	if (PISA_RES_EVENT(dh) == PISA_EVENT_INSERT &&
	    sc->sc_nexus == NULL &&
	    sc->ready_list.tqh_first == NULL &&
	    sc->nexus_list.tqh_first == NULL)
		scsi_probe_busses((int) sc->sc_link.scsibus, -1, -1);
#endif	/* AIC6360_SCSIBUS_RESCAN */

	if (sc->sc_nexus == NULL)
		aic_sched(sc);

	return 0;
}

static int
aic_pisa_scsi_notify(dh, ev)
	pisa_device_handle_t dh;
	pisa_event_t ev;
{
	struct aic_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct aic_softc *sc = &psc->sc_aic;

	switch(ev)
	{
	case PISA_EVENT_QUERY_SUSPEND:
		if (sc->sc_nexus != NULL ||
		    sc->ready_list.tqh_first != NULL ||
		    sc->nexus_list.tqh_first != NULL)
			return SD_EVENT_STATUS_BUSY;
		break;

	default:
		break;
	}
	return 0;
}
