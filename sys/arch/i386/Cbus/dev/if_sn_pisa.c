/*	$NecBSD: if_sn_pisa.c,v 1.4 1999/02/06 07:07:22 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 *  Copyright (c) 1997, 1998, 1999
 *	Kouichi Matsuda. All rights reserved.
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
 *      This product includes software developed by Kouichi Matsuda.
 * 4. The names of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Routines of NEC PC-9801-103, 104 and PC-9801N-J02, J02R  Ethernet interface
 * for NetBSD/pc98, ported by Kouichi Matsuda.
 *
 * These cards use National Semiconductor DP83934AVQB as Ethernet Controller
 * and National Semiconductor NS46C46 as (64 * 16 bits) Microwire Serial EEPROM.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#if 0 /* XXX this shouldn't be necessary; else reinsert */
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif
#endif

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <i386/Cbus/dev/dp83932reg.h>
#include <i386/Cbus/dev/dp83932var.h>

#include <i386/Cbus/dev/if_snreg.h>
#include <i386/Cbus/dev/dp83932subr.h>

int	sn_pisa_match __P((struct device *, struct cfdata *, void *));
void	sn_pisa_attach __P((struct device *, struct device *, void *));

struct sn_pisa_softc {
	struct	sn_softc sc_sn;

	/* PISA-specific goo. */
	pisa_device_handle_t sc_dh;
	void	*sc_ih;
};

static int sn_pisa_activate __P((pisa_device_handle_t));
static int sn_pisa_deactivate __P((pisa_device_handle_t));
int sn_pisa_enable __P((struct sn_softc *));
void sn_pisa_disable __P((struct sn_softc *));
#if 0
static int sn_pisa_nodeaddr __P((pisa_device_handle_t, bus_space_tag_t,
    bus_space_handle_t, u_int8_t *));
#endif

struct cfattach sn_pisa_ca = {
	sizeof(struct sn_pisa_softc), sn_pisa_match, sn_pisa_attach
};

struct pisa_functions sn_pisa_pd = {
	sn_pisa_activate, sn_pisa_deactivate,
};

int
sn_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;
	bus_space_tag_t memt = pa->pa_memt;
	bus_space_handle_t memh;
	int ioh_valid, memh_valid;
	int rv = 0;

	ioh_valid = memh_valid = 0;

	/* Map i/o space. */
	if (pisa_space_map(dh, PISA_IO0, &ioh)) {
#ifdef DIAGNOSTIC
		printf("sn_pisa_match: couldn't map i/o space 0x%lx\n",
		    PISA_DR_IOBASE(dr));
#endif
		return (0);
	}
	ioh_valid = 1;

	/* validate memory base */
	if (sn_nec16_validate_mem(PISA_DR_MEMBASE(dr)) == 0)
		goto out;

	PISA_DR_IMF(dr, PISA_MEM0) |= SDIM_BUS_WIDTH16;
	if (pisa_space_map(dh, PISA_MEM0, &memh)) {
#ifdef DIAGNOSTIC
		printf("sn_pisa_match: can't map mem space 0x%lx\n",
		    PISA_DR_MEMBASE(dr));
#endif
		goto out;
	}
	memh_valid = 1;

#ifdef	SNDEBUG
	sn_nec16_dump_reg(iot, ioh);
#endif	/* SNDEBUG */

	if (sn_nec16_detectsubr(iot, ioh, memt, memh, PISA_DR_IRQ(dr),
	    PISA_DR_MEMBASE(dr), SNEC_TYPE_PNP) == 0) {
		goto out;
	}

	rv = 1;

out:
	if (memh_valid)
		pisa_space_unmap(dh, memt, memh);
	if (ioh_valid)
		pisa_space_unmap(dh, iot, ioh);
	return (rv);
}

void
sn_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sn_pisa_softc *psc = (struct sn_pisa_softc *)self;
	struct sn_softc *sc = &psc->sc_sn;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;
	bus_space_tag_t memt = pa->pa_memt;
	bus_space_handle_t memh;
	u_int8_t myea[ETHER_ADDR_LEN];

	printf("\n");

	/* Map i/o space. */
	if (pisa_space_map(dh, PISA_IO0, &ioh)) {
		printf("%s: can't map i/o space 0x%lx\n", sc->sc_dev.dv_xname,
		    PISA_DR_IOBASE(dr));
		return;
	}

	PISA_DR_IMF(dr, PISA_MEM0) |= SDIM_BUS_WIDTH16;
	if (pisa_space_map(dh, PISA_MEM0, &memh)) {
		printf("%s: can't map mem space 0x%lx\n", sc->sc_dev.dv_xname,
		    PISA_DR_MEMBASE(dr));
		pisa_space_unmap(dh, iot, ioh);
		return;
	}

	psc->sc_dh = dh;
	pisa_register_functions(dh, self, &sn_pisa_pd);

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_memt = memt;
	sc->sc_memh = memh;

	if (sn_nec16_register_irq(sc, PISA_DR_IRQ(dr)) == 0) {
		pisa_space_unmap(dh, memt, memh);
		pisa_space_unmap(dh, iot, ioh);
		return;
	}

	if (sn_nec16_register_mem(sc, PISA_DR_MEMBASE(dr)) == 0) {
		/* XXX: un-register irq */
		bus_space_unmap(memt, memh, SNEC_NMEMS);
		bus_space_unmap(iot, ioh, SNEC_NREGS);
		return;
	}

	sn_nec16_get_enaddr(iot, ioh, myea);

#ifdef DIAGNOSTIC
	if (myea == NULL) {
		printf("%s: ethernet address shouldn't be NULL\n",
		    sc->sc_dev.dv_xname);
		panic("NULL ethernet address");
	}
#endif

	printf("%s: %s Ethernet\n", sc->sc_dev.dv_xname,
	    sn_nec16_detect_type(myea));

	sc->snr_dcr = DCR_SYNC | DCR_WAIT0 |
	    DCR_DMABLOCK | DCR_RFT16 | DCR_TFT28;
	sc->snr_dcr2 = 0;	/* XXX */
	sc->bitmode = 0;	/* 16 bit card */

	sc->sc_nic_put = sn_nec16_nic_put;
	sc->sc_nic_get = sn_nec16_nic_get;
	sc->sc_writetodesc = sn_nec16_writetodesc;
	sc->sc_readfromdesc = sn_nec16_readfromdesc;
	sc->sc_copytobuf = sn_nec16_copytobuf;
	sc->sc_copyfrombuf = sn_nec16_copyfrombuf;
	sc->sc_zerobuf = sn_nec16_zerobuf;

	/* snsetup returns 1 if something fails */
	if (snsetup(sc, myea)) {
		/* XXX: un-register irq */
		pisa_space_unmap(dh, memt, memh);
		pisa_space_unmap(dh, iot, ioh);
		return;
	}

	/* This interface is always enabled. */
	sc->sc_enabled = 1;
	sc->sc_enable = sn_pisa_enable;
	sc->sc_disable = sn_pisa_disable;

	snconfig(sc, NULL, 0, 0, myea);

	/* Establish the interrupt handler. */
	psc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_NET, snintr, sc);
	if (psc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt handler\n",
		    sc->sc_dev.dv_xname);
}

static int
sn_pisa_activate(dh)
	pisa_device_handle_t dh;
{
	struct sn_pisa_softc *sc = PISA_DEV_SOFTC(dh);
	struct sn_softc *snc = &sc->sc_sn;
	slot_device_res_t dr = PISA_RES_DR(dh);
	u_int8_t enaddr[ETHER_ADDR_LEN];

	/*
	 * reassign irq and membase
	 * XXX: check if it is properly registered
	 */
	(void) sn_nec16_register_irq(snc, PISA_DR_IRQ(dr));
	(void) sn_nec16_register_mem(snc, PISA_DR_MEMBASE(dr));

	sn_nec16_get_enaddr(snc->sc_iot, snc->sc_ioh, enaddr);

	snc->sc_enabled = 1;
	return 0;
}

static int
sn_pisa_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct sn_pisa_softc *sc = PISA_DEV_SOFTC(dh);
	struct sn_softc *snc = &sc->sc_sn;
	struct ifnet *ifp = &snc->sc_if;
	int s = splimp();

	if_down(ifp);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_UP);
	snc->sc_enabled = 0;
	splx(s);
	return 0;
}

int
sn_pisa_enable(snc)
	struct sn_softc *snc;
{
	struct sn_pisa_softc *sc = (void *) snc;

	return pisa_slot_device_enable(sc->sc_dh);
}

void
sn_pisa_disable(snc)
	struct sn_softc *snc;
{
	struct sn_pisa_softc *sc = (void *) snc;

	pisa_slot_device_disable(sc->sc_dh);
}
