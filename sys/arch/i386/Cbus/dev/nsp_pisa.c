/*	$NecBSD: nsp_pisa.c,v 1.4 1999/04/15 01:35:54 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
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
#include <i386/Cbus/dev/nspreg.h>
#include <i386/Cbus/dev/nspvar.h>

#define	NSP_HOSTID	7

static int nsp_pisa_match __P((struct device *, struct cfdata *, void *));
static void nsp_pisa_attach __P((struct device *, struct device *, void *));
static int nsp_pisa_map __P((struct pisa_attach_args *, bus_space_handle_t *, bus_space_handle_t *));

struct cfattach nsp_pisa_ca = {
	sizeof(struct nsp_softc), (cfmatch_t) nsp_pisa_match, nsp_pisa_attach
};

struct pisa_functions nsp_pd = {
	scsi_low_activate, scsi_low_deactivate, NULL, NULL, scsi_low_notify
};

static int
nsp_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_handle_t ioh, memh;
	int rv;

	if (nsp_pisa_map(pa, &ioh, &memh) != 0)
		return 0;

	rv = nspprobesubr(pa->pa_iot, ioh, PISA_DR_DVCFG(dr));

	pisa_space_unmap(dh, pa->pa_iot, ioh);
	pisa_space_unmap(dh, pa->pa_memt, memh);
	return rv;
}

void
nsp_pisa_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct nsp_softc *sc = (void *) self;
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);

	pisa_register_functions(dh, self, &nsp_pd);
	if (nsp_pisa_map(pa, &sc->sc_ioh, &sc->sc_memh) != 0)
	{
		printf("%s: couldn't map io\n", slp->sl_dev.dv_xname);
		return;
	}

	sc->sc_iot = pa->pa_iot;
	sc->sc_memt = pa->pa_memt;
	sc->sc_iclkdiv = CLKDIVR_20M;
	sc->sc_clkdiv = CLKDIVR_40M;

	slp->sl_hostid = NSP_HOSTID;	/* it seems to be fixed by hardware */
	slp->sl_irq = PISA_DR_IRQ(dr);
	slp->sl_cfgflags = PISA_DR_DVCFG(dr);
	nspattachsubr(sc);

	sc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_BIO, nspintr, sc);

	config_found(self, &slp->sl_link, nspprint);
}


static int
nsp_pisa_map(pa, iohp, memhp)
	struct pisa_attach_args *pa;
	bus_space_handle_t *iohp;
	bus_space_handle_t *memhp;
{
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	int rv;
	
	if (PISA_DR_IMS(dr, PISA_IO0) != NSP_IOSIZE &&
	    PISA_DR_IMS(dr, PISA_MEM0) != NSP_MEMSIZE)
		return EINVAL;

	PISA_DR_IMF(dr, PISA_IO0) |= SDIM_BUS_FOLLOW;
	rv = pisa_space_map(dh, PISA_IO0, iohp);
	if (rv != 0)
		return rv;

	PISA_DR_IMF(dr, PISA_MEM0) |= SDIM_BUS_WIDTH16;
	rv = pisa_space_map(dh, PISA_MEM0, memhp);
	if (rv != 0)
	{
		pisa_space_unmap(dh, pa->pa_memt, *memhp);
		return rv;
	}
	return rv;
}
