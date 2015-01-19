/*	$NecBSD: if_mbe_pisa.c,v 1.8 1999/07/16 01:31:44 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 *  Copyright (c) 1996, 1997, 1998, 1999
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>
#include <i386/pccs/tuple.h>

#include <i386/Cbus/dev/mb86960reg.h>
#include <i386/Cbus/dev/mb86960var.h>

#include <i386/Cbus/dev/mb86960subr.h>
#include <i386/Cbus/dev/if_mbereg.h>
#include <i386/Cbus/dev/if_mbehw.h>

static int	mbe_pisa_match __P((struct device *, struct cfdata *, void *));
void	mbe_pisa_attach __P((struct device *, struct device *, void *));

struct mbe_pisa_softc {
	struct	mb86960_softc sc_mb86960;

	/* PISA-specific goo. */
	pisa_device_handle_t sc_dh;
	void	*sc_ih;
};

static int mbe_pisa_activate __P((pisa_device_handle_t));
static int mbe_pisa_deactivate __P((pisa_device_handle_t));
int mbe_pisa_enable __P((struct mb86960_softc *));
void mbe_pisa_disable __P((struct mb86960_softc *));
static int mbe_pisa_nodeaddr __P((pisa_device_handle_t, bus_space_tag_t,
    bus_space_handle_t, u_int8_t *));

struct cfattach mbe_pisa_ca = {
	sizeof(struct mbe_pisa_softc), mbe_pisa_match, mbe_pisa_attach
};

struct pisa_functions mbe_pisa_pd = {
	mbe_pisa_activate, mbe_pisa_deactivate,
};

static int
mbe_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;
	int i, iobase, irq, rv = 0;
	u_int8_t myea[ETHER_ADDR_LEN];
	struct mbe_hw *hw;

	hw = DVCFG_HW(&mbe_hwsel, DVCFG_MAJOR(PISA_DR_DVCFG(dr)));
	if (hw == NULL) { 
		printf("mbe_pisa_match: can't find hardware table\n");
		return (0);
	}

	if ((hw->hw_flags & MBE_HW_BUSPISA) == 0) {
		printf("mbe_pisa_match: can't match hardware with pisa\n");
		return (0);
	}

	/*
	 * See if the specified address is valid for hardware.
	 */
	if (hw->hw_iomap != NULL) {
		for (i = 0; i < hw->hw_iomapsz; i++) {
			if (hw->hw_iomap[i] == PISA_DR_IOBASE(dr))
				break;
		}
		if (i == hw->hw_iomapsz) {
#ifdef DIAGNOSTIC
			printf("mbe_pisa_match: unknown iobase 0x%lx\n",
				PISA_DR_IOBASE(dr));
#endif
			return (0);
		}
	}

	/* Map i/o space. */
	if (pisa_space_map_load(dh, PISA_IO0, hw->hw_iatsz, hw->hw_iat, &ioh)) {
#ifdef DIAGNOSTIC
		printf("mbe_pisa_match: couldn't map iospace 0x%lx\n",
		    PISA_DR_IOBASE(dr));
#endif
		return (0);
	}

	if ((*hw->hw_find)(iot, ioh, &iobase, &irq) == 0) {
#ifdef DIAGNOSTIC
		printf("mbe_pisa_match: mbe_hw_find failed\n");
#endif
		goto out;
	}

#ifdef	MBE_PISA_TRUST_EEPROM
	if (hw->hw_flags & MBE_HW_CONFIO) {
		if (iobase != PISA_DR_IOBASE(dr)) {
#ifdef DIAGNOSTIC
			printf("mbe_pisa_match: unexpected iobase in board: "
			    "0x%lx\n", PISA_DR_IOBASE(dr));
#endif
			goto out;
		}
	}
#endif	/* MBE_PISA_TRUST_EEPROM */

	/* XXX: hw_detect does not pass softc and pisa device handle. */
	/* Get our station address. */
	mbe_pisa_nodeaddr(dh, iot, ioh, myea);

	if ((*hw->hw_detect)(iot, ioh, myea) == 0) { /* XXX necessary ? */
#ifdef DIAGNOSTIC
		printf("mbe_pisa_match: mbe_hw_detect failed\n");
#endif
		goto out;
	}

#ifdef	notyet
	if (hw->hw_flags & MBE_HW_CONFIRQ) {
		if (PISA_DR_IRQ(dr) != irq) {
			printf("mbe_pisa_match: irq mismatch; "
			    "kernel configured %ld != board configured %d\n",
			    PISA_DR_IRQ(dr), irq);
			goto out;
		}
	}
#endif

	rv = 1;

out:
	pisa_space_unmap(dh, iot, ioh);
	return (rv);
}

void
mbe_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mbe_pisa_softc *psc = (struct mbe_pisa_softc *)self;
	struct mb86960_softc *sc = &psc->sc_mb86960;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;
	u_int8_t myea[ETHER_ADDR_LEN];
	const char *typestr;
	int type;
	struct mbe_hw *hw;

	printf("\n");

	hw = DVCFG_HW(&mbe_hwsel, DVCFG_MAJOR(PISA_DR_DVCFG(dr)));
	if (hw == NULL) { 
		printf("%s: can't find hardware table\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Map i/o space. */
	if (pisa_space_map_load(dh, PISA_IO0, hw->hw_iatsz, hw->hw_iat, &ioh)) {
		printf("%s: can't load i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
	
	psc->sc_dh = dh;
	pisa_register_functions(dh, self, &mbe_pisa_pd);

	sc->sc_bst = iot;
	sc->sc_bsh = ioh;

	/* XXX: hw_detect does not pass softc and pisa device handle. */
	/* Get our station address. */
	mbe_pisa_nodeaddr(dh, iot, ioh, myea);

	/* Determine the card type and get ethernet address. */
	type = (*hw->hw_detect)(iot, ioh, myea);
	switch (type) {
	case FE_TYPE_MBH10302:
		typestr = "MBH10302 (PCMCIA)";
		break;
	case FE_TYPE_MBH10304:
		typestr = "MBH10304 (PCMCIA)";
		break;
	case FE_TYPE_TDK:
		typestr = "TDK/CONTEC (PCMCIA)";
		break;
	case FE_TYPE_CNET98P2:
		typestr = "C-NET(98)P2";
		break;
	case FE_TYPE_JC89532A:
		typestr = "Access/CARD (JC89532A)";
		break;
	default:
		/* Unknown card type: maybe a new model, but... */
		printf("%s: where did the card go?!\n", sc->sc_dev.dv_xname);
		panic("unknown card");
	}

	printf("%s: %s Ethernet\n", sc->sc_dev.dv_xname, typestr);

	/* This interface is always enabled. */
	sc->sc_enabled = 1;
	sc->sc_enable = mbe_pisa_enable;
	sc->sc_disable = mbe_pisa_disable;

	/*
	 * Do hardware dependent attach.
	 */
	(*hw->hw_attachsubr)(sc, type, myea, PISA_DR_IOBASE(dr), PISA_DR_IRQ(dr));

	printf("%s: %s, %s duplex\n", sc->sc_dev.dv_xname,
	    (sc->proto_dlcr6 & FE_D6_SBW_BYTE) ?  "8-bit" : "16-bit",
	    (sc->proto_dlcr4 & FE_D4_DSC) ? "full" : "half");

#ifdef DIAGNOSTIC
	if (myea == NULL) {
		printf("%s: ethernet address shouldn't be NULL\n",
		    sc->sc_dev.dv_xname);
		panic("NULL ethernet address");
	}
#endif
	bcopy(myea, sc->sc_enaddr, sizeof(sc->sc_enaddr));

	mb86960_config(sc, NULL, 0, 0);

	/* Establish the interrupt handler. */
	psc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_NET,
	    mb86960_intr, sc);
	if (psc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt handler\n",
		    sc->sc_dev.dv_xname);
}

static int
mbe_pisa_activate(dh)
	pisa_device_handle_t dh;
{
	struct mbe_pisa_softc *sc = PISA_DEV_SOFTC(dh);
	struct mb86960_softc *msc = &sc->sc_mb86960;
	u_int8_t enaddr[ETHER_ADDR_LEN];

	mbe_pisa_nodeaddr(dh, msc->sc_bst, msc->sc_bsh, enaddr);
	msc->sc_enabled = 1;
	return 0;
}

static int
mbe_pisa_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct mbe_pisa_softc *sc = PISA_DEV_SOFTC(dh);
	struct mb86960_softc *msc = &sc->sc_mb86960;
	struct ifnet *ifp = &msc->sc_ec.ec_if;
	int s = splimp();

	if_down(ifp);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_UP);
	msc->sc_enabled = 0;
	splx(s);
	return 0;
}

int
mbe_pisa_enable(msc)
	struct mb86960_softc *msc;
{
	struct mbe_pisa_softc *sc = (void *) msc;

	return pisa_slot_device_enable(sc->sc_dh);
}

void
mbe_pisa_disable(msc)
	struct mb86960_softc *msc;
{
	struct mbe_pisa_softc *sc = (void *) msc;

	pisa_slot_device_disable(sc->sc_dh);
}

static int
mbe_pisa_nodeaddr(dh, iot, ioh, enaddr)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t *enaddr;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	u_int8_t *ids, ea[ETHER_ADDR_LEN];
	int n;

	if (st->st_type != PISA_SLOT_PCMCIA)
		return 0;

	/* I */
	pisa_device_info(dh, CIS_REQINIT);
	ids = pisa_device_info(dh, CIS_REQCODE(CIS_FUNCE, 0x04));
	if (ids != NULL)
	{
		ids = (u_int8_t *) CIS_DATAP(ids);
		goto load;
	}

	/* II */
	pisa_device_info(dh, CIS_REQINIT);
	ids = pisa_device_info(dh, CIS_REQCODE(CIS_FUNCE, 0x00));
	if (ids != NULL)
	{
		ids = ((u_int8_t *) CIS_DATAP(ids)) + 3;	/* XXX */
		goto load;
	}

	/* III */
	/* XXX: assume register size > 16 (MBH10402, JC89532A) */
	bus_space_read_region_1(iot, ioh, FE_MBH10, ea, ETHER_ADDR_LEN);
	ids = ea;
#if FE_DEBUG >= 3
	printf("asic dump: ");
	for (n = 0; n < 16; n++)
		printf("%02x ", bus_space_read_1(iot, ioh, 16 + n));
	printf("\n");
#endif	/* FE_DEBUG >= 3 */

	/* XXX: for UB Networks Access/CARD (JC89532A) */
	if ((ids[0] & 0x03) != 0x00
	    || (ids[0] == 0x00 && ids[1] == 0x00 && ids[2] == 0x00)) {
		/* XXX: read again from different offsets */
		bus_space_read_region_1(iot, ioh, 16 + 8, ea, ETHER_ADDR_LEN);
		ids = ea;
	}

load:
	if (ids[0] == 0xff || (ids[0] | ids[1] | ids[2]) == 0 || ids[1] & 1)
		return ENOENT;

	for (n = 0; n < ETHER_ADDR_LEN; n++)
		enaddr[n] = ids[n];

	/* XXX: mac address will out to a card twice. */
	if (enaddr != NULL) {
		/* Feed the station address. */
		bus_space_write_region_1(iot, ioh, FE_DLCR8, enaddr,
		    ETHER_ADDR_LEN);
	}

	return 0;
}
