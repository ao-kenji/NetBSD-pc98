/*	$NecBSD: if_fes_isa.c,v 1.3 1998/09/26 11:30:59 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 *  Copyright (c) 1998
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

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>

#include <i386/Cbus/dev/mb86950reg.h>
#include <i386/Cbus/dev/mb86950var.h>

#include <i386/Cbus/dev/mb86950subr.h>
#include <i386/Cbus/dev/if_fesreg.h>
#include <i386/Cbus/dev/if_feshw.h>

int	fes_isa_match __P((struct device *, struct cfdata *, void *));
void	fes_isa_attach __P((struct device *, struct device *, void *));

struct fes_isa_softc {
	struct	mb86950_softc sc_mb86950;

	/* ISA-specific goo. */
	bus_space_handle_t sc_asich;	/* ASIC bus space */
	void	*sc_ih;
};

struct cfattach fes_isa_ca = {
	sizeof(struct fes_isa_softc), fes_isa_match, fes_isa_attach
};

int
fes_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh, asich;
	int i, iobase, irq, rv = 0;
	u_int8_t myea[ETHER_ADDR_LEN];
	struct fes_hw *hw;

	/* Disallow wildcarded values. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	hw = DVCFG_HW(&fes_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (hw == NULL) { 
		printf("fes_isa_match: can't find hardware table\n");
		return (0);
	}

	if ((hw->hw_flags & FES_HW_BUSISA) == 0) {
		printf("fes_isa_match: can't match hardware with isa\n");
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
			printf("fes_isa_match: unknown iobase 0x%x\n",
				ia->ia_iobase);
#endif
			return (0);
		}
	}

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &ioh)) {
#ifdef DIAGNOSTIC
		printf("fes_isa_match: can't map i/o space\n");
#endif
		return (0);
	}
	if (bus_space_map_load(iot, ioh, hw->hw_iatsz, hw->hw_iat,
	    BUS_SPACE_MAP_FAILFREE)) {
#ifdef DIAGNOSTIC
		printf("fes_isa_match: can't load i/o space\n");
#endif
		return (0);
	}

	/* Map ASIC i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &asich)) {
#ifdef DIAGNOSTIC
		printf("fes_isa_match: can't map ASIC i/o space\n");
#endif
		bus_space_unmap(iot, ioh, hw->hw_iatsz);
		return (0);
	}
	if (bus_space_map_load(iot, asich, hw->hw_asic_iatsz, hw->hw_asic_iat,
	    BUS_SPACE_MAP_FAILFREE)) {
#ifdef DIAGNOSTIC
		printf("fes_isa_match: can't load ASIC i/o space\n");
#endif
		bus_space_unmap(iot, ioh, hw->hw_iatsz);
		return (0);
	}

	if ((*hw->hw_find)(iot, ioh, asich, &iobase, &irq) == 0) {
#ifdef DIAGNOSTIC
		printf("fes_isa_match: fes_hw_find failed\n");
#endif
		goto out;
	}

	if (hw->hw_flags & FES_HW_CONFIO) {
		if (iobase != ia->ia_iobase) {
#ifdef DIAGNOSTIC
			printf("fes_isa_match: unexpected iobase in board: "
			    "0x%x\n", ia->ia_iobase);
#endif
			goto out;
		}
	}

	if ((*hw->hw_detect)(iot, ioh, asich, myea) == 0) { /* XXX necessary ? */
#ifdef DIAGNOSTIC
		printf("fes_isa_match: fes_hw_detect failed\n");
#endif
		goto out;
	}

	if (hw->hw_flags & FES_HW_CONFIRQ) {
		if (ia->ia_irq != ISACF_IRQ_DEFAULT) {
			if (ia->ia_irq != irq) {
				printf("fes_isa_match: irq mismatch; "
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
		printf("fes_isa_match: configured irq %d invalid\n",
		    ia->ia_irq);
		goto out;
	}

	ia->ia_iosize = hw->hw_iatsz + hw->hw_asic_iatsz;
	ia->ia_msize = 0;
	rv = 1;

out:
	bus_space_unmap(iot, asich, hw->hw_asic_iatsz);
	bus_space_unmap(iot, ioh, hw->hw_iatsz);
	return (rv);
}

void
fes_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fes_isa_softc *isc = (struct fes_isa_softc *)self;
	struct mb86950_softc *sc = &isc->sc_mb86950;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh, asich;
	u_int8_t myea[ETHER_ADDR_LEN];
	const char *typestr;
	int type;
	struct fes_hw *hw;

	printf("\n");

	hw = DVCFG_HW(&fes_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
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

	/* Map ASIC i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &asich)) {
		printf("%s: can't map ASIC i/o space\n", sc->sc_dev.dv_xname);
		bus_space_unmap(iot, ioh, hw->hw_iatsz);
		return;
	}
	if (bus_space_map_load(iot, asich, hw->hw_asic_iatsz, hw->hw_asic_iat,
	    BUS_SPACE_MAP_FAILFREE)) {
		printf("%s: can't load ASIC i/o space\n", sc->sc_dev.dv_xname);
		bus_space_unmap(iot, ioh, hw->hw_iatsz);
		return;
	}

	sc->sc_bst = iot;
	sc->sc_bsh = ioh;
	isc->sc_asich = asich;

	/* Determine the card type and get ethernet address. */
	type = (*hw->hw_detect)(iot, ioh, asich, myea);
	switch (type) {
	case FES_TYPE_AL98:
		typestr = "AngeLan AL-98";
		break;
	case FES_TYPE_PC85151:
		typestr = "Ungermann-Bass Access/PC N98C+ (PC85151)";
		break;
	case FES_TYPE_PC86131:
		typestr = "Ungermann-Bass Access/NOTE N98 (PC86131)";
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

#ifdef DIAGNOSTIC
	if (myea == NULL) {
		printf("%s: ethernet address shouldn't be NULL\n",
		    sc->sc_dev.dv_xname);
		panic("NULL ethernet address");
	}
#endif
	bcopy(myea, sc->sc_enaddr, sizeof(sc->sc_enaddr));

	mb86950_config(sc, NULL, 0, 0);

	/* Establish the interrupt handler. */
	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, mb86950_intr, sc);
	if (isc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt handler\n",
		    sc->sc_dev.dv_xname);
}
