/*	$NecBSD: ncr53c500_pisa.c,v 1.28 1998/11/26 01:59:11 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
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

#include <i386/Cbus/dev/ncr53c500reg.h>
#include <i386/Cbus/dev/ncr53c500hw.h>
#include <i386/Cbus/dev/ncr53c500var.h>

#include <i386/pccs/tuple.h>

struct ncv_pccs16_card {
	u_char *cardname;
	u_int cfgflags;
	bus_addr_t offset;
};

/* major = (C5IMG << 8) | (SPECIAL ACTION << 4) | CLKFACTOR */
struct ncv_pccs16_card ncv_pccs16_card[] = {
	{"KME KXLC002 00",
		DVCFG_MKCFG(0xb4d0, 0), 0},
	{"KME  KXLC004 01",
		DVCFG_MKCFG(0x84d0, 0), 0x10},
	{"RATOC System Inc. SOUND/SCSI2 CARD 72",
		DVCFG_MKCFG(0x84d0, 0), 0},
	{"RATOC System Inc. SCSI2 CARD 37",
		DVCFG_MKCFG(0x84d0, 0), 0},
	{"MACNICA MIRACLE SCSI mPS100 D.0",	
		DVCFG_MKCFG(0xb625, 0), 0},
	{"MIDORI ELECTRONICS  CN-SC43    A.0.0.0",
		DVCFG_MKCFG(0x8010, 0), 0},
	{ NULL,				
		DVCFG_MKCFG(0, 0), 0},
};

static int ncv_pisa_match __P((struct device *, struct cfdata *, void *));
static void ncv_pisa_attach __P((struct device *, struct device *, void *));
static int ncv_scan_dvcfg __P((pisa_device_handle_t, u_int *, bus_addr_t *));
static int ncv_pisa_map __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t *, bus_addr_t));

struct cfattach ncv_pisa_ca = {
	sizeof(struct ncv_softc), (cfmatch_t) ncv_pisa_match, ncv_pisa_attach
};

struct pisa_functions ncv_pd = {
	scsi_low_activate, scsi_low_deactivate,	NULL, NULL, scsi_low_notify
};

static int
ncv_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	slot_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t ioh;
	bus_addr_t offset;
	u_int dvcfg;
	int rv;

	ncv_scan_dvcfg(dh, &dvcfg, &offset);
	if (ncv_pisa_map(dh, pa->pa_iot, &ioh, offset))
		return 0;

	rv = ncvprobesubr(pa->pa_iot, ioh, dvcfg, NCV_HOSTID);

	pisa_space_unmap(dh, pa->pa_iot, ioh);
	return rv;
}

void
ncv_pisa_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ncv_softc *sc = (void *) self;
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_addr_t offset;
	u_int dvcfg;

	pisa_register_functions(dh, self, &ncv_pd);

	ncv_scan_dvcfg(dh, &dvcfg, &offset);
	if (ncv_pisa_map(dh, pa->pa_iot, &sc->sc_ioh, offset))
	{
		printf("%s: couldn't map io\n", slp->sl_dev.dv_xname);
		return;
	}

	sc->sc_iot = pa->pa_iot;
	sc->sc_memt = pa->pa_memt;
	slp->sl_irq = PISA_DR_IRQ(dr);
	slp->sl_hostid = NCV_HOSTID;
	slp->sl_cfgflags = dvcfg;
	ncvattachsubr(sc);

	sc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_BIO, ncvintr, sc);

	config_found(self, &slp->sl_link, ncvprint);
}

int
ncv_scan_dvcfg(dh, cfgp, offp)
	pisa_device_handle_t dh;
	u_int *cfgp;
	bus_addr_t *offp;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	struct ncv_pccs16_card *npc;
	u_int8_t *cp;

	*cfgp = PISA_DR_DVCFG(dr);
	*offp = 0;

	cp = SLOT_DEVICE_IDENT_IDS(dh);
	if (cp == NULL)
		return ENOENT;

	for (npc = &ncv_pccs16_card[0]; npc->cardname; npc ++)
	{
		if (strncmp(npc->cardname, cp, strlen(npc->cardname)))
			continue;

		*cfgp = DVCFG_MKCFG(DVCFG_MAJOR(npc->cfgflags), \
				   DVCFG_MINOR(*cfgp));
		*offp = npc->offset;
		break;
	}
	return 0;
}

int
ncv_pisa_map(dh, iot, iohp, offset)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	bus_space_handle_t *iohp;
	bus_addr_t offset;
{
	slot_device_res_t dr = PISA_RES_DR(dh);

	if (PISA_DR_IMS(dr, PISA_IO0) != NCVIOSZ &&
	    PISA_DR_IMS(dr, PISA_IO0) != NCVIOSZ * 2)
		return EINVAL;

	if (offset != 0 && PISA_DR_IMS(dr, PISA_IO0) == NCVIOSZ * 2)
	{
		PISA_DR_IMB(dr, PISA_IO0) += offset;
		PISA_DR_IMS(dr, PISA_IO0) = NCVIOSZ;
	}

	PISA_DR_IMF(dr, PISA_IO0) |= SDIM_BUS_FOLLOW;
	if (pisa_space_map(dh, PISA_IO0, iohp) != 0)
		return ENOSPC;

	return 0;
}
