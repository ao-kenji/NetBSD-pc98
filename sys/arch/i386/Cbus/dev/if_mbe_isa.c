/*	$NecBSD: if_mbe_isa.c,v 1.5 1999/04/06 07:21:23 kmatsuda Exp $	*/
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

#include <i386/Cbus/dev/mb86960reg.h>
#include <i386/Cbus/dev/mb86960var.h>

#include <i386/Cbus/dev/mb86960subr.h>
#include <i386/Cbus/dev/if_mbereg.h>
#include <i386/Cbus/dev/if_mbehw.h>

static int	mbe_isa_match __P((struct device *, struct cfdata *, void *));
void	mbe_isa_attach __P((struct device *, struct device *, void *));

struct mbe_isa_softc {
	struct	mb86960_softc sc_mb86960;

	/* ISA-specific goo. */
	void	*sc_ih;
};

struct cfattach mbe_isa_ca = {
	sizeof(struct mbe_isa_softc), mbe_isa_match, mbe_isa_attach
};

static int
mbe_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int i, iobase, irq, rv = 0;
	u_int8_t myea[ETHER_ADDR_LEN];
	struct mbe_hw *hw;

	/* Disallow wildcarded values. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	hw = DVCFG_HW(&mbe_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (hw == NULL) { 
		printf("mbe_isa_match: can't find hardware table\n");
		return (0);
	}

	if ((hw->hw_flags & MBE_HW_BUSISA) == 0) {
		printf("mbe_isa_match: can't match hardware with isa\n");
		return (0);
	}

	/*
	 * See if the specified address is valid for hardware.
	 */
	if (hw->hw_iomap != NULL) {
		for (i = 0; i < hw->hw_iomapsz; i++) {
			if (hw->hw_iomap[i] == ia->ia_iobase)
				break;
		}
		if (i == hw->hw_iomapsz) {
#ifdef DIAGNOSTIC
			printf("mbe_isa_match: unknown iobase 0x%x\n",
				ia->ia_iobase);
#endif
			return (0);
		}
	}

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &ioh)) {
#ifdef DIAGNOSTIC
		printf("mbe_isa_match: can't map i/o space\n");
#endif
		return (0);
	}
	if (bus_space_map_load(iot, ioh, hw->hw_iatsz, hw->hw_iat,
	    BUS_SPACE_MAP_FAILFREE)) {
#ifdef DIAGNOSTIC
		printf("mbe_isa_match: can't load i/o space\n");
#endif
		return (0);
	}

	if ((*hw->hw_find)(iot, ioh, &iobase, &irq) == 0) {
#ifdef DIAGNOSTIC
		printf("mbe_isa_match: mbe_hw_find failed\n");
#endif
		goto out;
	}

	if (hw->hw_flags & MBE_HW_CONFIO) {
		if (iobase != ia->ia_iobase) {
#ifdef DIAGNOSTIC
			printf("mbe_isa_match: unexpected iobase in board: "
			    "0x%x\n", ia->ia_iobase);
#endif
			goto out;
		}
	}

	if ((*hw->hw_detect)(iot, ioh, myea) == 0) { /* XXX necessary ? */
#ifdef DIAGNOSTIC
		printf("mbe_isa_match: mbe_hw_detect failed\n");
#endif
		goto out;
	}

	if (hw->hw_flags & MBE_HW_CONFIRQ) {
		if (ia->ia_irq != ISACF_IRQ_DEFAULT) {
			if (ia->ia_irq != irq) {
				printf("mbe_isa_match: irq mismatch; "
				    "kernel configured %d != "
				    "board configured %d\n",
				    ia->ia_irq, irq);
				goto out;
			}
		} else {
			ia->ia_irq = irq;
		}
	}

	/*
	 * We need explicit IRQ and supported address.
	 */
	if ((ia->ia_irq == ISACF_IRQ_DEFAULT)
	    || ((1 << ia->ia_irq) & hw->hw_irqmask) == 0) {
		printf("mbe_isa_match: configured irq %d invalid\n",
		    ia->ia_irq);
		goto out;
	}

	ia->ia_iosize = hw->hw_iatsz;
	ia->ia_msize = 0;
	rv = 1;

out:
	bus_space_unmap(iot, ioh, hw->hw_iatsz);
	return (rv);
}

void
mbe_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mbe_isa_softc *isc = (struct mbe_isa_softc *)self;
	struct mb86960_softc *sc = &isc->sc_mb86960;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	u_int8_t myea[ETHER_ADDR_LEN];
	const char *typestr;
	int type;
	struct mbe_hw *hw;

	printf("\n");

	hw = DVCFG_HW(&mbe_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (hw == NULL) {
		printf("%s: can't find hardware table\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
	if (bus_space_map_load(iot, ioh, hw->hw_iatsz, hw->hw_iat,
	    BUS_SPACE_MAP_FAILFREE)) {
		printf("%s: can't load i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
	
	sc->sc_bst = iot;
	sc->sc_bsh = ioh;

	/* Determine the card type and get ethernet address. */
	type = (*hw->hw_detect)(iot, ioh, myea);
	switch (type) {
	case FE_TYPE_RE1000:
		typestr = "RE1000";
		break;
	case FE_TYPE_RE1000P:
		typestr = "RE1000Plus/ME1500";
		break;
	case FE_TYPE_CNET98P2:
		typestr = "C-NET(98)P2";
		break;
	case FE_TYPE_TDKLAC:
		typestr = "LAC-98012/13/25";
		break;
	case FE_TYPE_PC85152:
		typestr = "Access/PC N98C+ (PC85152)";
		break;
	case FE_TYPE_PC86132:
		typestr = "Access/NOTE N98 (PC86132)";
		break;
	case FE_TYPE_REX9880:
		typestr = "REX-9880/81/82/83";
		break;
	case FE_TYPE_REX9886:
		typestr = "REX-9886/87";
		break;
	case FE_TYPE_CNET9NE:
		typestr = "C-NET(9N)E";
		break;
	case FE_TYPE_CNET9NC:
		typestr = "C-NET(9N)C";
		break;
	default:
		/* Unknown card type: maybe a new model, but... */
		printf("%s: where did the card go?!\n", sc->sc_dev.dv_xname);
		panic("unknown card");
	}

	printf("%s: %s Ethernet\n", sc->sc_dev.dv_xname, typestr);

	/* This interface is always enabled. */
	sc->sc_enabled = 1;

	/*
	 * Do hardware dependent attach.
	 */
	(*hw->hw_attachsubr)(sc, type, myea, ia->ia_iobase, ia->ia_irq);

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
	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, mb86960_intr, sc);
	if (isc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt handler\n",
		    sc->sc_dev.dv_xname);
}
