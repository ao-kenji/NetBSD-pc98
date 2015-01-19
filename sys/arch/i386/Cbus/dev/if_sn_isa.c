/*	$NecBSD: if_sn_isa.c,v 1.4 1999/02/06 07:07:21 kmatsuda Exp $	*/
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
 * Routines of NEC PC-9801-83, 84, 103, 104 and PC-9801N-25 Ethernet interface
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

#include <i386/Cbus/dev/dp83932var.h>
#include <i386/Cbus/dev/dp83932reg.h>

#include <i386/Cbus/dev/if_snreg.h>
#include <i386/Cbus/dev/dp83932subr.h>

int	sn_isa_match __P((struct device *, struct cfdata *, void *));
void	sn_isa_attach __P((struct device *, struct device *, void *));

struct sn_isa_softc {
	struct	sn_softc sc_sn;

	/* ISA-specific goo. */
	void	*sc_ih;
};

struct cfattach sn_isa_ca = {
	sizeof(struct sn_isa_softc), sn_isa_match, sn_isa_attach
};

int
sn_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t memh;
	int ioh_valid, memh_valid;
	int rv = 0;

	ioh_valid = memh_valid = 0;

	/* Disallow wildcarded values. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);
	if (ia->ia_maddr == ISACF_IOMEM_DEFAULT)
		return (0);
	if (ia->ia_irq == ISACF_IRQ_DEFAULT)
		return (0);

	/* validate iobase */
	if ((ia->ia_iobase & ~0x3000) != 0x0888)
		return (0);

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, SNEC_NREGS, 0, &ioh)) {
#ifdef DIAGNOSTIC
		printf("sn_isa_match: can't map i/o space\n");
#endif
		return (0);
	}
	ioh_valid = 1;

	/* validate memory base */
	if (sn_nec16_validate_mem(ia->ia_maddr) == 0)
		goto out;

	if (bus_space_map(memt, ia->ia_maddr, SNEC_NMEMS, 0, &memh)) {
#ifdef DIAGNOSTIC
		printf("sn_isa_match: can't map mem space\n");
#endif
		goto out;
	}
	memh_valid = 1;

#ifdef	SNDEBUG
	sn_nec16_dump_reg(iot, ioh);
#endif	/* SNDEBUG */

	if (sn_nec16_detectsubr(iot, ioh, memt, memh, ia->ia_irq, ia->ia_maddr,
	    SNEC_TYPE_LEGACY) == 0) {
		goto out;
	}

	ia->ia_iosize = SNEC_NREGS;
	ia->ia_msize = SNEC_NMEMS;
	rv = 1;

out:
	if (memh_valid)
		bus_space_unmap(memt, memh, SNEC_NMEMS);
	if (ioh_valid)
		bus_space_unmap(iot, ioh, SNEC_NREGS);
	return (rv);
}

void
sn_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sn_isa_softc *isc = (struct sn_isa_softc *)self;
	struct sn_softc *sc = &isc->sc_sn;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t memh;
	u_int8_t myea[ETHER_ADDR_LEN];

	printf("\n");

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, SNEC_NREGS, 0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
	if (bus_space_map(memt, ia->ia_maddr, SNEC_NMEMS, 0, &memh)) {
		printf("%s: can't map mem space\n", sc->sc_dev.dv_xname);
		bus_space_unmap(iot, ioh, SNEC_NREGS);
		return;
	}
	
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_memt = memt;
	sc->sc_memh = memh;

	if (sn_nec16_register_irq(sc, ia->ia_irq) == 0) {
		bus_space_unmap(memt, memh, SNEC_NMEMS);
		bus_space_unmap(iot, ioh, SNEC_NREGS);
		return;
	}

	if (sn_nec16_register_mem(sc, ia->ia_maddr) == 0) {
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
		bus_space_unmap(memt, memh, SNEC_NMEMS);
		bus_space_unmap(iot, ioh, SNEC_NREGS);
		return;
	}

	/* This interface is always enabled. */
	sc->sc_enabled = 1;

	snconfig(sc, NULL, 0, 0, myea);

	/* Establish the interrupt handler. */
	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, snintr, sc);
	if (isc->sc_ih == NULL)
		printf("%s: couldn't establish interrupt handler\n",
		    sc->sc_dev.dv_xname);
}

