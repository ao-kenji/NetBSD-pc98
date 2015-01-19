/*	$NecBSD: tmc18c30_pisa.c,v 1.22 1998/11/26 01:59:21 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Kouichi Matsuda. All rights reserved.
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
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isa/pisaif.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_disk.h>

#include <machine/dvcfg.h>

#include <i386/Cbus/dev/scsi_low.h>
#include <i386/Cbus/dev/scsi_low_pisa.h>
#include <i386/Cbus/dev/tmc18c30reg.h>
#include <i386/Cbus/dev/tmc18c30var.h>

#define	STG_HOSTID	7

static int stg_pisa_match __P((struct device *, struct cfdata *, void *));
static void stg_pisa_attach __P((struct device *, struct device *, void *));
static int stg_pisa_map __P((pisa_device_handle_t, bus_space_handle_t *));

struct cfattach stg_pisa_ca = {
	sizeof(struct stg_softc), (cfmatch_t) stg_pisa_match, stg_pisa_attach
};

struct pisa_functions stg_pd = {
	scsi_low_activate, scsi_low_deactivate, NULL, NULL, scsi_low_notify
};

static int
stg_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_handle_t ioh;
	int rv;

	if (stg_pisa_map(dh, &ioh) != 0)
		return 0;

	rv = stgprobesubr(pa->pa_iot, ioh, PISA_DR_DVCFG(dr));
	pisa_space_unmap(dh, pa->pa_iot, ioh);
	return rv;
}

void
stg_pisa_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct stg_softc *sc = (void *) self;
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);

	pisa_register_functions(dh, self, &stg_pd);
	if (stg_pisa_map(dh, &sc->sc_ioh) != 0)
	{
		printf("%s: couldn't map io\n", slp->sl_dev.dv_xname);
		return;
	}

	sc->sc_iot = pa->pa_iot;
	sc->sc_memt = pa->pa_memt;
	slp->sl_irq = PISA_DR_IRQ(dr);
	slp->sl_hostid = STG_HOSTID;
	slp->sl_cfgflags = PISA_DR_DVCFG(dr);
	stgattachsubr(sc);

	sc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_BIO, stgintr, sc);

	config_found(self, &slp->sl_link, stgprint);
}


static int
stg_pisa_map(dh, iohp)
	pisa_device_handle_t dh;
	bus_space_handle_t *iohp;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	
	if (PISA_DR_IMS(dr, PISA_IO0) != STGIOSZ)
		return EINVAL;

	PISA_DR_IMF(dr, PISA_IO0) |= SDIM_BUS_FOLLOW;
	return pisa_space_map(dh, PISA_IO0, iohp);
}
