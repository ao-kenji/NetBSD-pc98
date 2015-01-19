/*	$NecBSD: if_we.c,v 1.10 1999/07/24 05:56:53 kmatsuda Exp $	*/
/*	$NetBSD: if_we.c,v 1.12 1998/10/27 22:45:13 thorpej Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1997, 1998, 1999
 *	Kouichi Matsuda. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */
#ifndef	ORIGINAL_CODE
/*
 * Probe and initialization routine for SMC EtherEZ98 (SMC8498BTA)
 * is ported by K.Matsuda.
 */
#endif	/* PC-98 */

/*
 * Device driver for the Western Digital/SMC 8003 and 8013 series,
 * and the SMC Elite Ultra (8216).
 */

#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"
#include "rnd.h" 

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h> 
#include <netinet/ip.h>
#include <netinet/if_inarp.h> 
#endif 

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#ifdef	ORIGINAL_CODE
#include <dev/isa/if_wereg.h>
#else	/* PC-98 */
#include <i386/Cbus/dev/if_wereg.h>
#include <i386/Cbus/dev/smc83c790reg.h>
#include <i386/Cbus/dev/if_wehw.h>
#endif	/* PC-98 */

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_read_region_stream_2	bus_space_read_region_2
#define	bus_space_write_stream_2	bus_space_write_2
#define	bus_space_write_region_stream_2	bus_space_write_region_2
#endif

struct we_softc {
	struct dp8390_softc sc_dp8390;

	bus_space_tag_t sc_asict;	/* space tag for ASIC */
	bus_space_handle_t sc_asich;	/* space handle for ASIC */

	u_int8_t sc_laar_proto;
	u_int8_t sc_msr_proto;

	u_int8_t sc_type;		/* our type */

	int sc_16bitp;			/* are we 16 bit? */

	void *sc_ih;			/* interrupt handle */
};

int	we_probe __P((struct device *, struct cfdata *, void *));
void	we_attach __P((struct device *, struct device *, void *));

struct cfattach we_ca = {
	sizeof(struct we_softc), we_probe, we_attach
};

extern struct cfdriver we_cd;

const char *we_params __P((bus_space_tag_t, bus_space_handle_t, u_int8_t *,
	    bus_size_t *, int *, int *));
void	we_set_media __P((struct we_softc *, int));

int	we_mediachange __P((struct dp8390_softc *));
void	we_mediastatus __P((struct dp8390_softc *, struct ifmediareq *));

void	we_recv_int __P((struct dp8390_softc *));
void	we_init_card __P((struct dp8390_softc *));
int	we_write_mbuf __P((struct dp8390_softc *, struct mbuf *, int));
int	we_ring_copy __P((struct dp8390_softc *, int, caddr_t, u_short));
void	we_read_hdr __P((struct dp8390_softc *, int, struct dp8390_ring *));
int	we_test_mem __P((struct dp8390_softc *));
#ifndef	ORIGINAL_CODE
int	we_pio_write_mbuf __P((struct dp8390_softc *, struct mbuf *, int));
int	we_pio_ring_copy __P((struct dp8390_softc *, int, caddr_t, u_short));
void	we_pio_read_hdr __P((struct dp8390_softc *, int, struct dp8390_ring *));
int	we_pio_test_mem __P((struct dp8390_softc *));
#endif	/* PC-98 */

__inline void we_readmem __P((struct we_softc *, int, u_int8_t *, int));
#ifndef	ORIGINAL_CODE
__inline void we_pio_readmem __P((struct we_softc *, int, u_int8_t *, int));
#endif	/* PC-98 */

static const int we_584_irq[] = {
	9, 3, 5, 7, 10, 11, 15, 4,
};
#define	NWE_584_IRQ	(sizeof(we_584_irq) / sizeof(we_584_irq[0]))

static const int we_790_irq[] = {
	IRQUNK, 9, 3, 5, 7, 10, 11, 15,
};
#define	NWE_790_IRQ	(sizeof(we_790_irq) / sizeof(we_790_irq[0]))
#ifndef	ORIGINAL_CODE

static const int we_790_pc98_irq[] = {
	IRQUNK, 3, 5, 6, IRQUNK, 9, 12, 13,
};
#define	NWE_790_PC98_IRQ	(sizeof(we_790_pc98_irq) / sizeof(we_790_pc98_irq[0]))
#endif	/* PC-98 */

int we_media[] = {
	IFM_ETHER|IFM_10_2,
	IFM_ETHER|IFM_10_5,
};
#define	NWE_MEDIA	(sizeof(we_media) / sizeof(we_media[0]))

/*
 * Delay needed when switching 16-bit access to shared memory.
 */
#define	WE_DELAY(wsc) delay(3)

/*
 * Enable card RAM, and 16-bit access.
 */
#define	WE_MEM_ENABLE(wsc) \
do { \
	if ((wsc)->sc_16bitp) \
		bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich, \
		    WE_LAAR, (wsc)->sc_laar_proto | WE_LAAR_M16EN); \
	bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich, \
	    WE_MSR, wsc->sc_msr_proto | WE_MSR_MENB); \
	WE_DELAY((wsc)); \
} while (0)

/*
 * Disable card RAM, and 16-bit access.
 */
#define	WE_MEM_DISABLE(wsc) \
do { \
	bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich, \
	    WE_MSR, (wsc)->sc_msr_proto); \
	if ((wsc)->sc_16bitp) \
		bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich, \
		    WE_LAAR, (wsc)->sc_laar_proto); \
	WE_DELAY((wsc)); \
} while (0)

int
we_probe(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct isa_attach_args *ia = aux;
#ifdef	ORIGINAL_CODE
	bus_space_tag_t asict, memt;
	bus_space_handle_t asich, memh;
#else	/* PC-98 */
	bus_space_tag_t asict, nict, memt;
	bus_space_handle_t asich, nich, memh;
#endif	/* PC-98 */
	bus_size_t memsize;
#ifdef	ORIGINAL_CODE
	int asich_valid, memh_valid;
#else	/* PC-98 */
	int asich_valid, nich_valid, memh_valid;
#endif	/* PC-98 */
	int i, is790, rv = 0;
	u_int8_t x, type;
#ifndef	ORIGINAL_CODE
	u_int8_t is790pio = 0;
	u_int8_t is_eez98 = IS_EEZ98(ia->ia_cfgflags);
#endif	/* PC-98 */

	asict = ia->ia_iot;
#ifndef	ORIGINAL_CODE
	nict = ia->ia_iot;
#endif	/* PC-98 */
	memt = ia->ia_memt;

	asich_valid = memh_valid = 0;
#ifndef	ORIGINAL_CODE
	nich_valid = 0;
#endif	/* PC-98 */

	/* Disallow wildcarded i/o addresses. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

#ifdef	ORIGINAL_CODE
	/* Disallow wildcarded mem address. */
	if (ia->ia_maddr == ISACF_IOMEM_DEFAULT)
		return (0);
#endif	/* !PC98 */

	/* Attempt to map the device. */
#ifdef	ORIGINAL_CODE
	if (bus_space_map(asict, ia->ia_iobase, WE_NPORTS, 0, &asich))
		goto out;
#else	/* PC-98 */
	if (bus_space_map(asict, ia->ia_iobase, WE_ASIC_NPORTS, 0, &asich))
		goto out;
#endif	/* PC-98 */
	asich_valid = 1;
#ifndef	ORIGINAL_CODE

	/* XXX: only check if mappable, not used. */
	if (bus_space_map(nict,
	    ia->ia_iobase + (is_eez98 ? WE_EZ98_NIC_OFFSET : WE_NIC_OFFSET),
	    WE_NIC_NPORTS, 0, &nich))
		goto out;
	nich_valid = 1;
#endif	/* PC-98 */

#ifdef TOSH_ETHER
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_POW);
#endif

	/*
	 * Attempt to do a checksum over the station address PROM.
	 * If it fails, it's probably not a WD/SMC board.  There is
	 * a problem with this, though.  Some clone WD8003E boards
	 * (e.g. Danpex) won't pass the checksum.  In this case,
	 * the checksum byte always seems to be 0.
	 */
	for (x = 0, i = 0; i < 8; i++)
		x += bus_space_read_1(asict, asich, WE_PROM + i);

	if (x != WE_ROM_CHECKSUM_TOTAL) {
		/* Make sure it's an 8003E clone... */
		if (bus_space_read_1(asict, asich, WE_CARD_ID) !=
		    WE_TYPE_WD8003E)
			goto out;

		/* Check the checksum byte. */
		if (bus_space_read_1(asict, asich, WE_PROM + 7) != 0)
			goto out;
	}

	/*
	 * Reset the card to force it into a known state.
	 */
#ifdef TOSH_ETHER
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_RST | WE_MSR_POW);
#else
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_RST);
#endif
	delay(100);

	bus_space_write_1(asict, asich, WE_MSR,
	    bus_space_read_1(asict, asich, WE_MSR) & ~WE_MSR_RST);

	/* Wait in case the card is reading it's EEPROM. */
	delay(5000);

	/*
	 * Get parameters.
	 */
	if (we_params(asict, asich, &type, &memsize, NULL, &is790) == NULL)
		goto out;

#ifndef	ORIGINAL_CODE
	/* check if PIO */
	if (is790) {
		u_int8_t hwr;

		/* Assemble together the encoded interrupt number. */
		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);

		x = bus_space_read_1(asict, asich, WE790_GCR2);
		is790pio = ((x & WE790_GCR2_PNPIOP) == WE790_GCR2_PNPIOP);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr & ~WE790_HWR_SWH);
	}
#endif	/* PC-98 */

#ifdef	ORIGINAL_CODE
	/* Allow user to override probed value. */
	if (ia->ia_msize)
		memsize = ia->ia_msize;

	/* Attempt to map the memory space. */
	if (bus_space_map(memt, ia->ia_maddr, memsize, 0, &memh))
		goto out;
	memh_valid = 1;
#else	/* PC-98 */
	if (is790pio) {
		/* XXX: override for PIO board. */
		ia->ia_maddr = ISACF_IOMEM_DEFAULT;
		ia->ia_msize = 0;
	} else {
		/* Disallow wildcarded mem address. */
		if (ia->ia_maddr == ISACF_IOMEM_DEFAULT)
			goto out;

		/* Allow user to override probed value. */
		if (ia->ia_msize)
			memsize = ia->ia_msize;

		/* Attempt to map the memory space. */
		if (bus_space_map(memt, ia->ia_maddr, memsize, 0, &memh))
			goto out;
		memh_valid = 1;
	}
#endif	/* PC-98 */

	/*
	 * If possible, get the assigned interrupt number from the card
	 * and use it.
	 */
	if (is790) {
		u_int8_t hwr;

		/* Assemble together the encoded interrupt number. */
		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);

		x = bus_space_read_1(asict, asich, WE790_GCR);
		i = ((x & WE790_GCR_IR2) >> 4) |
		    ((x & (WE790_GCR_IR1|WE790_GCR_IR0)) >> 2);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr & ~WE790_HWR_SWH);

#ifdef	ORIGINAL_CODE
		if (ia->ia_irq != IRQUNK && ia->ia_irq != we_790_irq[i])
			printf("%s%d: overriding IRQ %d to %d\n",
			    we_cd.cd_name, cf->cf_unit, ia->ia_irq,
			    we_790_irq[i]);
		ia->ia_irq = we_790_irq[i];
#else	/* PC-98 */
		/* check if board on ISA or PC-98 */
		if (is_eez98) {
			if (ia->ia_irq != IRQUNK &&
			    ia->ia_irq != we_790_pc98_irq[i])
				printf("%s%d: overriding IRQ %d to %d\n",
				    we_cd.cd_name, cf->cf_unit, ia->ia_irq,
				    we_790_pc98_irq[i]);
			ia->ia_irq = we_790_pc98_irq[i];
		} else {
			if (ia->ia_irq != IRQUNK && ia->ia_irq != we_790_irq[i])
				printf("%s%d: overriding IRQ %d to %d\n",
				    we_cd.cd_name, cf->cf_unit, ia->ia_irq,
				    we_790_irq[i]);
			ia->ia_irq = we_790_irq[i];
		}
#endif	/* PC-98 */
	} else if (type & WE_SOFTCONFIG) {
		/* Assemble together the encoded interrupt number. */
		i = (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_IR2) |
		    ((bus_space_read_1(asict, asich, WE_IRR) &
		      (WE_IRR_IR0 | WE_IRR_IR1)) >> 5);

		if (ia->ia_irq != IRQUNK && ia->ia_irq != we_584_irq[i])
			printf("%s%d: overriding IRQ %d to %d\n",
			    we_cd.cd_name, cf->cf_unit, ia->ia_irq,
			    we_584_irq[i]);
		ia->ia_irq = we_584_irq[i];
	}

	/* So, we say we've found it! */
	ia->ia_iosize = WE_NPORTS;
#ifdef	ORIGINAL_CODE
	ia->ia_msize = memsize;
#else	/* PC-98 */
	if (!is790pio)
		ia->ia_msize = memsize;
#endif	/* PC-98 */
	rv = 1;

 out:
#ifdef	ORIGINAL_CODE
	if (asich_valid)
		bus_space_unmap(asict, asich, WE_NPORTS);
#else	/* PC-98 */
	if (asich_valid)
		bus_space_unmap(asict, asich, WE_ASIC_NPORTS);
	if (nich_valid)
		bus_space_unmap(nict, nich, WE_NIC_NPORTS);
#endif	/* PC-98 */
	if (memh_valid)
		bus_space_unmap(memt, memh, memsize);
	return (rv);
}

void
we_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct we_softc *wsc = (struct we_softc *)self;
	struct dp8390_softc *sc = &wsc->sc_dp8390;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t nict, asict, memt;
	bus_space_handle_t nich, asich, memh;
#ifndef	ORIGINAL_CODE
	bus_size_t memsize;
#endif	/* PC-98 */
	const char *typestr;
	u_int8_t x;
#ifndef	ORIGINAL_CODE
	u_int8_t is790pio = 0;
	u_int8_t is_eez98 = IS_EEZ98(ia->ia_cfgflags);
#endif	/* PC-98 */
	int i;

	printf("\n");

	nict = asict = ia->ia_iot;
#ifdef	ORIGINAL_CODE
	memt = ia->ia_memt;
#else	/* PC-98 */
	/* XXX: how ? */
	memt = ia->ia_memt;
#endif	/* PC-98 */

	/* Map the device. */
#ifdef	ORIGINAL_CODE
	if (bus_space_map(asict, ia->ia_iobase, WE_NPORTS, 0, &asich)) {
		printf("%s: can't map nic i/o space\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	if (bus_space_subregion(asict, asich, WE_NIC_OFFSET, WE_NIC_NPORTS,
	    &nich)) {
		printf("%s: can't subregion i/o space\n",
		    sc->sc_dev.dv_xname);
		return;
	}
#else	/* PC-98 */
	if (bus_space_map(asict, ia->ia_iobase, WE_ASIC_NPORTS, 0, &asich)) {
		printf("%s: can't map asic i/o space\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Select NIC_OFFSET value, depending on ISA or PC-98. */
	if (bus_space_map(nict,
	    ia->ia_iobase + (is_eez98 ? WE_EZ98_NIC_OFFSET : WE_NIC_OFFSET),
	    WE_NIC_NPORTS, 0, &nich)) {
		printf("%s: can't map nic i/o space\n",
		    sc->sc_dev.dv_xname);
		return;
	}
#endif	/* PC-98 */

#ifdef	ORIGINAL_CODE
	typestr = we_params(asict, asich, &wsc->sc_type, NULL,
	    &wsc->sc_16bitp, &sc->is790);
#else	/* PC-98 */
	/* check how many memory buffer for PIO mode */
	typestr = we_params(asict, asich, &wsc->sc_type, &memsize,
	    &wsc->sc_16bitp, &sc->is790);
#endif	/* PC-98 */
	if (typestr == NULL) {
		printf("%s: where did the card go?\n", sc->sc_dev.dv_xname);
		return;
	}
#ifndef	ORIGINAL_CODE
	/* check if PIO */
	if (sc->is790) {
		u_int8_t hwr;

		/* Assemble together the encoded interrupt number. */
		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);

		x = bus_space_read_1(asict, asich, WE790_GCR2);
		is790pio = ((x & WE790_GCR2_PNPIOP) == WE790_GCR2_PNPIOP);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr & ~WE790_HWR_SWH);
	}
#endif	/* PC-98 */

#ifdef	ORIGINAL_CODE
	/*
	 * Map memory space.  Note we use the size that might have
	 * been overridden by the user.
	 */
	if (bus_space_map(memt, ia->ia_maddr, ia->ia_msize, 0, &memh)) {
		printf("%s: can't map shared memory\n",
		    sc->sc_dev.dv_xname);
		return;
	}
#else	/* PC-98 */
	if (!is790pio) {
		/*
		 * Map memory space.  Note we use the size that might have
		 * been overridden by the user.
		 */
		if (bus_space_map(memt, ia->ia_maddr, ia->ia_msize, 0, &memh)) {
			printf("%s: can't map shared memory\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}
#endif	/* PC-98 */

	/*
	 * Allow user to override 16-bit mode.  8-bit takes precedence.
	 */
	if (self->dv_cfdata->cf_flags & WE_FLAGS_FORCE_16BIT_MODE)
		wsc->sc_16bitp = 1;
	if (self->dv_cfdata->cf_flags & WE_FLAGS_FORCE_8BIT_MODE)
		wsc->sc_16bitp = 0;

	wsc->sc_asict = asict;
	wsc->sc_asich = asich;

	sc->sc_regt = nict;
	sc->sc_regh = nich;

#ifdef	ORIGINAL_CODE
	sc->sc_buft = memt;
	sc->sc_bufh = memh;
#else	/* PC-98 */
	/* XXX: how ? */
	sc->sc_buft = memt;
	sc->sc_bufh = memh;
#endif	/* PC-98 */

	/* Interface is always enabled. */
	sc->sc_enabled = 1;

	/* Registers are linear. */
	for (i = 0; i < 16; i++)
		sc->sc_reg_map[i] = i;

	/* Now we can use the NIC_{GET,PUT}() macros. */

#ifdef	ORIGINAL_CODE
	printf("%s: %s Ethernet (%s-bit)\n", sc->sc_dev.dv_xname,
	    typestr, wsc->sc_16bitp ? "16" : "8");
#else	/* PC-98 */
	printf("%s: %s Ethernet (%s-bit) (%s)\n", sc->sc_dev.dv_xname,
	    typestr, wsc->sc_16bitp ? "16" : "8",
	    is790pio ? "PIO" : "SHM");
#endif	/* PC-98 */

	/* Get station address from EEPROM. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_enaddr[i] = bus_space_read_1(asict, asich, WE_PROM + i);

	/*
	 * Set upper address bits and 8/16 bit access to shared memory.
	 */
	if (sc->is790) {
		wsc->sc_laar_proto =
		    bus_space_read_1(asict, asich, WE_LAAR) &
		    ~WE_LAAR_M16EN;
		bus_space_write_1(asict, asich, WE_LAAR,
		    wsc->sc_laar_proto | (wsc->sc_16bitp ? WE_LAAR_M16EN : 0));
	} else if ((wsc->sc_type & WE_SOFTCONFIG) ||
#ifdef TOSH_ETHER
	    (wsc->sc_type == WE_TYPE_TOSHIBA1) ||
	    (wsc->sc_type == WE_TYPE_TOSHIBA4) ||
#endif
	    (wsc->sc_type == WE_TYPE_WD8013EBT)) {
		wsc->sc_laar_proto = (ia->ia_maddr >> 19) & WE_LAAR_ADDRHI;
		if (wsc->sc_16bitp)
			wsc->sc_laar_proto |= WE_LAAR_L16EN;
		bus_space_write_1(asict, asich, WE_LAAR,
		    wsc->sc_laar_proto | (wsc->sc_16bitp ? WE_LAAR_M16EN : 0));
	}

	/*
	 * Set address and enable interface shared memory.
	 */
	if (sc->is790) {
#ifdef	ORIGINAL_CODE
		/* XXX MAGIC CONSTANTS XXX */
		x = bus_space_read_1(asict, asich, 0x04);
		bus_space_write_1(asict, asich, 0x04, x | 0x80);
		bus_space_write_1(asict, asich, 0x0b,
		    ((ia->ia_maddr >> 13) & 0x0f) |
		    ((ia->ia_maddr >> 11) & 0x40) |
		    (bus_space_read_1(asict, asich, 0x0b) & 0xb0));
		bus_space_write_1(asict, asich, 0x04, x);
		wsc->sc_msr_proto = 0x00;
		sc->cr_proto = 0x00;
#else	/* PC-98 */
		if (!is790pio) {
			x = bus_space_read_1(asict, asich, WE790_HWR);
			bus_space_write_1(asict, asich, WE790_HWR,
			    x | WE790_HWR_SWH);
			bus_space_write_1(asict, asich, WE790_RAR,
			    ((ia->ia_maddr >> 13) & 0x0f) |
			    ((ia->ia_maddr >> 11) & 0x40) |
			    (bus_space_read_1(asict, asich, WE790_RAR) & 0xb0));
			bus_space_write_1(asict, asich, WE790_HWR, x);
		}
		wsc->sc_msr_proto = 0x00;
		sc->cr_proto = 0x00;
#endif	/* PC-98 */
	} else {
#ifdef TOSH_ETHER
		if (wsc->sc_type == WE_TYPE_TOSHIBA1 ||
		    wsc->sc_type == WE_TYPE_TOSHIBA4) {
			bus_space_write_1(asict, asich, WE_MSR + 1,
			    ((ia->ia_maddr >> 8) & 0xe0) | 0x04);
			bus_space_write_1(asict, asich, WE_MSR + 2,
			    ((ia->ia_maddr >> 16) & 0x0f));
			wsc->sc_msr_proto = WE_MSR_POW;
		} else
#endif
			wsc->sc_msr_proto = (ia->ia_maddr >> 13) &
			    WE_MSR_ADDR;

		sc->cr_proto = ED_CR_RD2;
	}

#ifdef	ORIGINAL_CODE
	bus_space_write_1(asict, asich, WE_MSR,
	    wsc->sc_msr_proto | WE_MSR_MENB);
	WE_DELAY(wsc);
#else	/* PC-98 */
	if (!is790pio) {
		bus_space_write_1(asict, asich, WE_MSR,
		    wsc->sc_msr_proto | WE_MSR_MENB);
		WE_DELAY(wsc);
	}
#endif	/* PC-98 */

	/*
	 * DCR gets:
	 *
	 *	FIFO threshold to 8, No auto-init Remote DMA,
	 *	byte order=80x86.
	 *
	 * 16-bit cards also get word-wide DMA transfers.
	 */
	sc->dcr_reg = ED_DCR_FT1 | ED_DCR_LS |
	    (wsc->sc_16bitp ? ED_DCR_WTS : 0);

#ifdef	ORIGINAL_CODE
	sc->test_mem = we_test_mem;
	sc->ring_copy = we_ring_copy;
	sc->write_mbuf = we_write_mbuf;
	sc->read_hdr = we_read_hdr;
#else	/* PC-98 */
	if (is790pio) {
		/* PIO mode */
		sc->test_mem = we_pio_test_mem;
		sc->ring_copy = we_pio_ring_copy;
		sc->write_mbuf = we_pio_write_mbuf;
		sc->read_hdr = we_pio_read_hdr;
#if 0
		/* GENERIC, use dp8390_rint() */
		sc->recv_int = we_pio_recv_int;
#endif
	} else {
		/* shared memory mode */
		sc->test_mem = we_test_mem;
		sc->ring_copy = we_ring_copy;
		sc->write_mbuf = we_write_mbuf;
		sc->read_hdr = we_read_hdr;
		sc->recv_int = we_recv_int;
	}
#endif	/* PC-98 */
	sc->init_card = we_init_card;

	sc->sc_mediachange = we_mediachange;
	sc->sc_mediastatus = we_mediastatus;

#ifdef	ORIGINAL_CODE
	sc->mem_start = 0;
	sc->mem_size = ia->ia_msize;
#else	/* PC-98 */
	/* XXX: how ? */
	sc->mem_start = 0;
	if (is790pio)
		sc->mem_size = memsize;
	else
		sc->mem_size = ia->ia_msize;
#endif	/* PC-98 */

	sc->sc_flags = self->dv_cfdata->cf_flags;

	/* Do generic parts of attach. */
	if (wsc->sc_type & WE_SOFTCONFIG) {
		int defmedia = IFM_ETHER;

		if (sc->is790) {
			x = bus_space_read_1(asict, asich, WE790_HWR);
			bus_space_write_1(asict, asich, WE790_HWR,
			    x | WE790_HWR_SWH);
			if (bus_space_read_1(asict, asich, WE790_GCR) &
			    WE790_GCR_GPOUT)
				defmedia |= IFM_10_2;
			else
				defmedia |= IFM_10_5;
			bus_space_write_1(asict, asich, WE790_HWR,
			    x & ~WE790_HWR_SWH);
		} else {
			x = bus_space_read_1(asict, asich, WE_IRR);
			if (x & WE_IRR_OUT2)
				defmedia |= IFM_10_2;
			else
				defmedia |= IFM_10_5;
		}
		i = dp8390_config(sc, we_media, NWE_MEDIA, defmedia);
	} else
		i = dp8390_config(sc, NULL, 0, 0);
	if (i) {
		printf("%s: configuration failed\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Disable 16-bit access to shared memory - we leave it disabled
	 * so that:
	 *
	 *	(1) machines reboot properly when the board is set to
	 *	    16-bit mode and there are conflicting 8-bit devices
	 *	    within the same 128k address space as this board's
	 *	    shared memory, and
	 *
	 *	(2) so that other 8-bit devices with shared memory
	 *	    in this same 128k address space will work.
	 */
#ifdef	ORIGINAL_CODE
	WE_MEM_DISABLE(wsc);
#else	/* PC-98 */
	if (!is790pio)
		WE_MEM_DISABLE(wsc);
#endif	/* PC-98 */

	/*
	 * Enable the configured interrupt.
	 */
#ifdef	ORIGINAL_CODE
	if (sc->is790)
		bus_space_write_1(asict, asich, WE790_ICR,
		    bus_space_read_1(asict, asich, WE790_ICR) |
		    WE790_ICR_EIL);
#else	/* PC-98 */
	if (sc->is790) {
		if (is790pio) {
			/* PIO mode */
			bus_space_write_1(asict, asich, WE790_ICR,
			    bus_space_read_1(asict, asich, WE790_ICR) |
			    WE790_ICR_EIL | WE790_ICR_IOPEN);
		} else {
			/* shared memory mode */
			bus_space_write_1(asict, asich, WE790_ICR,
			    bus_space_read_1(asict, asich, WE790_ICR) |
			    WE790_ICR_EIL);
		}
	}
#endif	/* PC-98 */
	else if (wsc->sc_type & WE_SOFTCONFIG)
		bus_space_write_1(asict, asich, WE_IRR,
		    bus_space_read_1(asict, asich, WE_IRR) | WE_IRR_IEN);
	else if (ia->ia_irq == IRQUNK) {
		printf("%s: can't wildcard IRQ on a %s\n",
		    sc->sc_dev.dv_xname, typestr);
		return;
	}

	/* Establish interrupt handler. */
	wsc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, dp8390_intr, sc);
	if (wsc->sc_ih == NULL)
		printf("%s: can't establish interrupt\n", sc->sc_dev.dv_xname);
}

int
we_test_mem(sc)
	struct dp8390_softc *sc;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	bus_space_tag_t memt = sc->sc_buft;
	bus_space_handle_t memh = sc->sc_bufh;
	bus_size_t memsize = sc->mem_size;
	int i;

	if (wsc->sc_16bitp)
		bus_space_set_region_2(memt, memh, 0, 0, memsize >> 1);
	else
		bus_space_set_region_1(memt, memh, 0, 0, memsize);

	if (wsc->sc_16bitp) {
		for (i = 0; i < memsize; i += 2) {
			if (bus_space_read_2(memt, memh, i) != 0)
				goto fail;
		}
	} else {
		for (i = 0; i < memsize; i++) {
			if (bus_space_read_1(memt, memh, i) != 0)
				goto fail;
		}
	}

	return (0);

 fail:
	printf("%s: failed to clear shared memory at offset 0x%x\n",
	    sc->sc_dev.dv_xname, i);
	WE_MEM_DISABLE(wsc);
	return (1);
}

/*
 * Given a NIC memory source address and a host memory destination address,
 * copy 'len' from NIC to host using shared memory.  The 'len' is rounded
 * up to a word - ok as long as mbufs are word-sized.
 */
__inline void
we_readmem(wsc, from, to, len)
	struct we_softc *wsc;
	int from;
	u_int8_t *to;
	int len;
{
	bus_space_tag_t memt = wsc->sc_dp8390.sc_buft;
	bus_space_handle_t memh = wsc->sc_dp8390.sc_bufh;

	if (len & 1)
		++len;

	if (wsc->sc_16bitp)
		bus_space_read_region_stream_2(memt, memh, from,
		    (u_int16_t *)to, len >> 1);
	else
		bus_space_read_region_1(memt, memh, from,
		    to, len);
}

int
we_write_mbuf(sc, m, buf)
	struct dp8390_softc *sc;
	struct mbuf *m;
	int buf;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	bus_space_tag_t memt = wsc->sc_dp8390.sc_buft;
	bus_space_handle_t memh = wsc->sc_dp8390.sc_bufh;
	u_int8_t *data, savebyte[2];
	int savelen, len, leftover;
#ifdef DIAGNOSTIC
	u_int8_t *lim;
#endif

	savelen = m->m_pkthdr.len;

	WE_MEM_ENABLE(wsc);

	/*
	 * 8-bit boards are simple; no alignment tricks are necessary.
	 */
	if (wsc->sc_16bitp == 0) {
		for (; m != NULL; buf += m->m_len, m = m->m_next)
			bus_space_write_region_1(memt, memh,
			    buf, mtod(m, u_int8_t *), m->m_len);
		goto out;
	}

	/* Start out with no leftover data. */
	leftover = 0;
	savebyte[0] = savebyte[1] = 0;

	for (; m != NULL; m = m->m_next) {
		len = m->m_len;
		if (len == 0)
			continue;
		data = mtod(m, u_int8_t *);
#ifdef DIAGNOSTIC
		lim = data + len;
#endif
		while (len > 0) {
			if (leftover) {
				/*
				 * Data left over (from mbuf or realignment).
				 * Buffer the next byte, and write it and
				 * the leftover data out.
				 */
				savebyte[1] = *data++;
				len--;
				bus_space_write_stream_2(memt, memh, buf,
				    *(u_int16_t *)savebyte);
				buf += 2;
				leftover = 0;
			} else if (ALIGNED_POINTER(data, u_int16_t) == 0) {
				/*
				 * Unaligned dta; buffer the next byte.
				 */
				savebyte[0] = *data++;
				len--;
				leftover = 1;
			} else {
				/*
				 * Aligned data; output contiguous words as
				 * much as we can, then buffer the remaining
				 * byte, if any.
				 */
				leftover = len & 1;
				len &= ~1;
				bus_space_write_region_stream_2(memt, memh,
				    buf, (u_int16_t *)data, len >> 1);
				data += len;
				buf += len;
				if (leftover)
					savebyte[0] = *data++;
				len = 0;
			}
		}
		if (len < 0)
			panic("we_write_mbuf: negative len");
#ifdef DIAGNOSTIC
		if (data != lim)
			panic("we_write_mbuf: data != lim");
#endif
	}
	if (leftover) {
		savebyte[1] = 0;
		bus_space_write_stream_2(memt, memh, buf,
		    *(u_int16_t *)savebyte);
	}

 out:
	WE_MEM_DISABLE(wsc);

	return (savelen);
}

int
we_ring_copy(sc, src, dst, amount)
	struct dp8390_softc *sc;
	int src;
	caddr_t dst;
	u_short amount;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	u_short tmp_amount;

	/* Does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* Copy amount up to end of NIC memory. */
		we_readmem(wsc, src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	we_readmem(wsc, src, dst, amount);

	return (src + amount);
}

void
we_read_hdr(sc, packet_ptr, packet_hdrp)
	struct dp8390_softc *sc;
	int packet_ptr;
	struct dp8390_ring *packet_hdrp;
{
	struct we_softc *wsc = (struct we_softc *)sc;

	we_readmem(wsc, packet_ptr, (u_int8_t *)packet_hdrp,
	    sizeof(struct dp8390_ring));
#if BYTE_ORDER == BIG_ENDIAN
	packet_hdrp->count = bswap16(packet_hdrp->count);
#endif
}

void
we_recv_int(sc)
	struct dp8390_softc *sc;
{
	struct we_softc *wsc = (struct we_softc *)sc;

	WE_MEM_ENABLE(wsc);
	dp8390_rint(sc);
	WE_MEM_DISABLE(wsc);
}
#ifndef	ORIGINAL_CODE

/*
 * PIO staff
 */
int
we_pio_test_mem(sc)
	struct dp8390_softc *sc;
{

	/* Noop. */
	return (0);
}

/*
 * Given a NIC memory source address and a host memory destination address,
 * copy 'amount' from NIC to host using programmed i/o.  The 'amount' is
 * rounded up to a word - ok as long as mbufs are word sized.
 */
__inline void
we_pio_readmem(wsc, from, to, len)
	struct we_softc *wsc;
	int from;
	u_int8_t *to;
	int len;
{
	bus_space_tag_t asict = wsc->sc_asict;
	bus_space_handle_t asich = wsc->sc_asich;

	if (len & 1)
		++len;

	bus_space_write_1(asict, asich, WE790_IOPA, from);
	bus_space_write_1(asict, asich, WE790_IOPA, from >> 8);

	if (wsc->sc_16bitp)
		/* XXX: stream ? */
		bus_space_read_multi_2(asict, asich, WE790_IOPL,
		    (u_int16_t *)to, len >> 1);
	else
		bus_space_read_multi_1(asict, asich, WE790_IOPL, to, len);
}

/*
 * Write an mbuf chain to the destination NIC memory address using programmed
 * I/O.
 */
int
we_pio_write_mbuf(sc, m, buf)
	struct dp8390_softc *sc;
	struct mbuf *m;
	int buf;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	bus_space_tag_t asict = wsc->sc_asict;
	bus_space_handle_t asich = wsc->sc_asich;
	u_int8_t *data, savebyte[2];
	int savelen, len, leftover;
#ifdef DIAGNOSTIC
	u_int8_t *lim;
#endif

	savelen = m->m_pkthdr.len;

	bus_space_write_1(asict, asich, WE790_IOPA, buf);
	bus_space_write_1(asict, asich, WE790_IOPA, buf >> 8);

	/*
	 * 8-bit boards are simple; no alignment tricks are necessary.
	 */
	if (wsc->sc_16bitp == 0) {
		for (; m != NULL; buf += m->m_len, m = m->m_next)
			bus_space_write_multi_1(asict, asich, WE790_IOPL,
			    mtod(m, u_int8_t *), m->m_len);

		return (savelen);
	}

	/* Start out with no leftover data. */
	leftover = 0;
	savebyte[0] = savebyte[1] = 0;

	for (; m != NULL; m = m->m_next) {
		len = m->m_len;
		if (len == 0)
			continue;
		data = mtod(m, u_int8_t *);
#ifdef DIAGNOSTIC
		lim = data + len;
#endif
		while (len > 0) {
			if (leftover) {
				/*
				 * Data left over (from mbuf or realignment).
				 * Buffer the next byte, and write it and
				 * the leftover data out.
				 */
				savebyte[1] = *data++;
				len--;
				/* XXX: stream ? */
				bus_space_write_2(asict, asich,
				    WE790_IOPL, *(u_int16_t *)savebyte);
				buf += 2;
				leftover = 0;
			} else if (ALIGNED_POINTER(data, u_int16_t) == 0) {
				/*
				 * Unaligned dta; buffer the next byte.
				 */
				savebyte[0] = *data++;
				len--;
				leftover = 1;
			} else {
				/*
				 * Aligned data; output contiguous words as
				 * much as we can, then buffer the remaining
				 * byte, if any.
				 */
				leftover = len & 1;
				len &= ~1;
				/* XXX: stream ? */
				bus_space_write_multi_2(asict, asich,
				    WE790_IOPL, (u_int16_t *)data, len >> 1);
				data += len;
				buf += len;
				if (leftover)
					savebyte[0] = *data++;
				len = 0;
			}
		}
		if (len < 0)
			panic("we_pio_write_mbuf: negative len");
#ifdef DIAGNOSTIC
		if (data != lim)
			panic("we_pio_write_mbuf: data != lim");
#endif
	}
	if (leftover) {
		savebyte[1] = 0;
		/* XXX: stream ? */
		bus_space_write_2(asict, asich, WE790_IOPL,
		    *(u_int16_t *)savebyte);
	}

	return (savelen);
}

/*
 * Given a source and destination address, copy 'amout' of a packet from
 * the ring buffer into a linear destination buffer.  Takes into account
 * ring-wrap.
 */
int
we_pio_ring_copy(sc, src, dst, amount)
	struct dp8390_softc *sc;
	int src;
	caddr_t dst;
	u_short amount;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	u_short tmp_amount;

	/* Does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* Copy amount up to end of NIC memory. */
		we_pio_readmem(wsc, src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	we_pio_readmem(wsc, src, dst, amount);

	return (src + amount);
}

void
we_pio_read_hdr(sc, packet_ptr, packet_hdrp)
	struct dp8390_softc *sc;
	int packet_ptr;
	struct dp8390_ring *packet_hdrp;
{
	struct we_softc *wsc = (struct we_softc *)sc;

	we_pio_readmem(wsc, packet_ptr, (u_int8_t *)packet_hdrp,
	    sizeof(struct dp8390_ring));
#if BYTE_ORDER == BIG_ENDIAN
	packet_hdrp->count = bswap16(packet_hdrp->count);
#endif
}
#endif	/* PC-98 */

int
we_mediachange(sc)
	struct dp8390_softc *sc;
{

	/*
	 * Current media is already set up.  Just reset the interface
	 * to let the new value take hold.  The new media will be
	 * set up in we_init_card() called via dp8390_init().
	 */
	dp8390_reset(sc);
	return (0);
}

void
we_mediastatus(sc, ifmr)
	struct dp8390_softc *sc;
	struct ifmediareq *ifmr;
{
	struct ifmedia *ifm = &sc->sc_media;

	/*
	 * The currently selected media is always the active media.
	 */
	ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

void
we_init_card(sc)
	struct dp8390_softc *sc;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	struct ifmedia *ifm = &sc->sc_media;

	we_set_media(wsc, ifm->ifm_cur->ifm_media);
}

void
we_set_media(wsc, media)
	struct we_softc *wsc;
	int media;
{
	struct dp8390_softc *sc = &wsc->sc_dp8390;
	bus_space_tag_t asict = wsc->sc_asict;
	bus_space_handle_t asich = wsc->sc_asich;
	u_int8_t hwr, gcr, irr;

	if (sc->is790) {
		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);
		gcr = bus_space_read_1(asict, asich, WE790_GCR);
		if (IFM_SUBTYPE(media) == IFM_10_2)
			gcr |= WE790_GCR_GPOUT;
		else
			gcr &= ~WE790_GCR_GPOUT;
		bus_space_write_1(asict, asich, WE790_GCR,
		    gcr | WE790_GCR_LIT);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr & ~WE790_HWR_SWH);
		return;
	}

	irr = bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE_IRR);
	if (IFM_SUBTYPE(media) == IFM_10_2)
		irr |= WE_IRR_OUT2;
	else
		irr &= ~WE_IRR_OUT2;
	bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE_IRR, irr);
}

const char *
we_params(asict, asich, typep, memsizep, is16bitp, is790p)
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *typep;
	bus_size_t *memsizep;
	int *is16bitp, *is790p;
{
	const char *typestr;
	bus_size_t memsize;
	int is16bit, is790;
	u_int8_t type;

	memsize = 8192;
	is16bit = is790 = 0;

	type = bus_space_read_1(asict, asich, WE_CARD_ID);
	switch (type) {
	case WE_TYPE_WD8003S: 
		typestr = "WD8003S"; 
		break;
	case WE_TYPE_WD8003E:
		typestr = "WD8003E";
		break;
	case WE_TYPE_WD8003EB: 
		typestr = "WD8003EB";
		break;
	case WE_TYPE_WD8003W:
		typestr = "WD8003W";
		break;
	case WE_TYPE_WD8013EBT: 
		typestr = "WD8013EBT";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013W:
		typestr = "WD8013W";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EP:		/* also WD8003EP */
		if (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_16BIT) {
			is16bit = 1;
			memsize = 16384;
			typestr = "WD8013EP";
		} else
			typestr = "WD8003EP";
		break;
	case WE_TYPE_WD8013WC:
		typestr = "WD8013WC";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EBP:
		typestr = "WD8013EBP";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EPC:
		typestr = "WD8013EPC";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_SMC8216C:
	case WE_TYPE_SMC8216T:
	    {
		u_int8_t hwr;

		typestr = (type == WE_TYPE_SMC8216C) ?
		    "SMC8216/SMC8216C" : "SMC8216T";

		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);
		switch (bus_space_read_1(asict, asich, WE790_RAR) &
		    WE790_RAR_SZ64) {
		case WE790_RAR_SZ64:
			memsize = 65536;
			break;
		case WE790_RAR_SZ32:
			memsize = 32768;
			break;
		case WE790_RAR_SZ16:
			memsize = 16384;
			break;
		case WE790_RAR_SZ8:
			/* 8216 has 16K shared mem -- 8416 has 8K */
			typestr = (type == WE_TYPE_SMC8216C) ?
			    "SMC8416C/SMC8416BT" : "SMC8416T";
			memsize = 8192;
			break;
		}
		bus_space_write_1(asict, asich, WE790_HWR, hwr);

		is16bit = 1;
		is790 = 1;
		break;
	    }
#ifdef TOSH_ETHER
	case WE_TYPE_TOSHIBA1:
		typestr = "Toshiba1";
		memsize = 32768;
		is16bit = 1;
		break;
	case WE_TYPE_TOSHIBA4:
		typestr = "Toshiba4";
		memsize = 32768;
		is16bit = 1;
		break;
#endif
	default:
		/* Not one we recognize. */
		return (NULL);
	}

	/*
	 * Make some adjustments to initial values depending on what is
	 * found in the ICR.
	 */
	if (is16bit && (type != WE_TYPE_WD8013EBT) &&
#ifdef TOSH_ETHER
	    (type != WE_TYPE_TOSHIBA1 && type != WE_TYPE_TOSHIBA4) &&
#endif
	    (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_16BIT) == 0) {
		is16bit = 0;
		memsize = 8192;
	}

#ifdef WE_DEBUG
	{
		int i;

		printf("we_params: type = 0x%x, typestr = %s, is16bit = %d, "
#ifdef  ORIGINAL_CODE
		    "memsize = %d\n", type, typestr, is16bit, memsize);
#else   /* PC-98 */
		    /* XXX: should send-pr */
		    "memsize = %ld\n", type, typestr, is16bit, memsize);
#endif	/* PC-98 */
		for (i = 0; i < 8; i++)
			printf("     %d -> 0x%x\n", i,
			    bus_space_read_1(asict, asich, i));
	}
#endif

	if (typep != NULL)
		*typep = type;
	if (memsizep != NULL)
		*memsizep = memsize;
	if (is16bitp != NULL)
		*is16bitp = is16bit;
	if (is790p != NULL)
		*is790p = is790;
	return (typestr);
}
