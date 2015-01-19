/*	$NecBSD: if_le_isa.c,v 1.12 1999/07/24 05:56:52 kmatsuda Exp $	*/
/*	$NetBSD: if_le_isa.c,v 1.23 1998/08/15 10:51:19 mycroft Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998, 1999
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
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */
#ifndef	ORIGINAL_CODE
/*
 * Add probe routine of CONTEC C-NET(98)S Ethernet interface for NetBSD/pc98.
 * Ported by Kouichi Matsuda.
 */
#endif	/* PC-98 */

#include "opt_inet.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#ifdef	ORIGINAL_CODE
#include <dev/isa/if_levar.h>
#else	/* PC-98 */
#include <i386/Cbus/dev/if_levar.h>
#endif	/* PC-98 */

#if	!defined(ORIGINAL_CODE) && !defined(LE_SMALL)
int ne2100_isa_probe __P((struct device *, struct cfdata *, void *));
int bicc_isa_probe __P((struct device *, struct cfdata *, void *));
#endif	/* !ORIGINAL_CODE && !LE_SMALL */
#ifndef	ORIGINAL_CODE
int cnet98s_isa_probe __P((struct device *, struct cfdata *, void *));
#endif	/* PC-98 */
void le_dummyattach __P((struct device *, struct device *, void *));
int le_dummyprobe __P((struct device *, struct cfdata *, void *));
#if	!defined(ORIGINAL_CODE) && !defined(LE_SMALL)
void le_ne2100_attach __P((struct device *, struct device *, void *));
void le_bicc_attach __P((struct device *, struct device *, void *));
#endif	/* !ORIGINAL_CODE && !LE_SMALL */
#ifndef	ORIGINAL_CODE
void le_cnet98s_attach __P((struct device *, struct device *, void *));
#endif	/* PC-98 */

#ifndef	LE_SMALL
struct cfattach nele_ca = {
	sizeof(struct device), ne2100_isa_probe, le_dummyattach
}, le_nele_ca = {
	sizeof(struct le_softc), le_dummyprobe, le_ne2100_attach
}, bicc_ca = {
	sizeof(struct device), bicc_isa_probe, le_dummyattach
}, le_bicc_ca = {
	sizeof(struct le_softc), le_dummyprobe, le_bicc_attach
#ifndef	ORIGINAL_CODE
}, cnle_ca = {
	sizeof(struct device), cnet98s_isa_probe, le_dummyattach
}, le_cnle_ca = {
	sizeof(struct le_softc), le_dummyprobe, le_cnet98s_attach
#endif	/* PC-98 */
};
#else	/* LE_SMALL */
struct cfattach cnle_ca = {
	sizeof(struct device), cnet98s_isa_probe, le_dummyattach
}, le_cnle_ca = {
	sizeof(struct le_softc), le_dummyprobe, le_cnet98s_attach
};
#endif	/* LE_SMALL */

#ifndef	ORIGINAL_CODE
bus_addr_t le_cnet98s_iat[] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
	0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
	0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
	0x0408, 0x0409, 0x040a, 0x040b, 0x040c, 0x040d, 0x040e, 0x040f,
};

#endif	/* PC-98 */
struct le_isa_params {
	char *name;
	int iosize, rap, rdp;
	int macstart, macstride;
#ifndef	ORIGINAL_CODE
	bus_space_iat_t iat;
#endif	/* PC-98 */
#ifndef	LE_SMALL
} ne2100_params = {
	"NE2100",
	24, NE2100_RAP, NE2100_RDP,
	0, 1
#ifndef	ORIGINAL_CODE
	, BUS_SPACE_IAT_1
#endif	/* PC-98 */
}, bicc_params = {
	"BICC Isolan",
	16, BICC_RAP, BICC_RDP,
	0, 2
#ifndef	ORIGINAL_CODE
	, BUS_SPACE_IAT_1
#endif	/* PC-98 */
#ifndef	ORIGINAL_CODE
}, cnet98s_params = {
	"C-NET(98)S",
	32, CNET98S_RAP, CNET98S_RDP,
	0, 2
	, le_cnet98s_iat
#endif	/* PC-98 */
};
#else	/* LE_SMALL */
} cnet98s_params = {
	"C-NET(98)S",
	32, CNET98S_RAP, CNET98S_RDP,
	0, 2
	, le_cnet98s_iat
};
#endif	/* LE_SMALL */

#if	!defined(ORIGINAL_CODE) && !defined(LE_SMALL)
int lance_isa_probe __P((struct isa_attach_args *, struct le_isa_params *));
#endif	/* !ORIGINAL_CODE && !LE_SMALL */
void le_isa_attach __P((struct device *, struct le_softc *,
			struct isa_attach_args *, struct le_isa_params *));

int le_isa_intredge __P((void *));

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_ddb.h"
#endif

#ifdef DDB
#define	integrate
#define hide
#else
#define	integrate	static __inline
#define hide		static
#endif

hide void le_isa_wrcsr __P((struct lance_softc *, u_int16_t, u_int16_t));
hide u_int16_t le_isa_rdcsr __P((struct lance_softc *, u_int16_t));  

#define	LE_ISA_MEMSIZE	16384

hide void
le_isa_wrcsr(sc, port, val)
	struct lance_softc *sc;
	u_int16_t port, val;
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	bus_space_write_2(iot, ioh, lesc->sc_rdp, val);
}

hide u_int16_t
le_isa_rdcsr(sc, port)
	struct lance_softc *sc;
	u_int16_t port;
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t iot = lesc->sc_iot;
	bus_space_handle_t ioh = lesc->sc_ioh; 
	u_int16_t val;

	bus_space_write_2(iot, ioh, lesc->sc_rap, port);
	val = bus_space_read_2(iot, ioh, lesc->sc_rdp); 
	return (val);
}

#if	!defined(ORIGINAL_CODE) && !defined(LE_SMALL)
int
ne2100_isa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (lance_isa_probe(aux, &ne2100_params));
}

int
bicc_isa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (lance_isa_probe(aux, &bicc_params));
}
#endif	/* !ORIGINAL_CODE && !LE_SMALL */
#ifndef	ORIGINAL_CODE

/*
 * Probe routine of CONTEC C-NET(98)S Ethernet interface for NetBSD/pc98.
 * Ported by Kouichi Matsuda <kmatsuda@elsip.hokudai.ac.jp>.
 *
 * CONTEC C-NET(98)S uses AMD Am79C960 as Single-Chip Ethernet Controller.
 * This probe routine is almost same as ne2100_isa_probe(), but we prepare
 * probe routine separately. Because ne2100_isa_probe()'s detect method is
 * very weak!
 *
 * TODO:
 * o Complete EEPROM access routine.
 *	It is easy, I wrote already, but it's unsafe and unstable yet.
 */
int
cnet98s_isa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct le_isa_params *p = &cnet98s_params;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rap, rdp;
	int rv = 0;
	static int const iomap[8] =
		{ 0x03d0, 0x13d0, 0x23d0, 0x33d0, 0x43d0,
		  0x53d0, 0x63d0, 0xffff /* terminate */ };
	u_long devid;
	u_short rom_sum, sum = 0;
	int i;

	/* Disallow wildcarded i/o address. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	/* Disallow wildcarded irq. */
	if (ia->ia_irq == ISACF_IRQ_DEFAULT)
		return (0);

	/*
	 * See if the sepcified address is possible for C-NET(98)S.
	 */
	for (i = 0; i < 7; i++) {
		if (iomap[i] == ia->ia_iobase)
			break;
	}
	if (i == 7) {
		printf("cnle: configured port 0x%x invalid\n", ia->ia_iobase);
		return (0);
	}

	/*
	 * See if the sepcified irq is possible for C-NET(98)S.
	 */
	if (((1 << ia->ia_irq) & CNET98S_IRQMASK) == 0) {
		printf("cnle: configured irq %d invalid\n", ia->ia_irq);
		return (0);
	}

	/*
	 * See if the sepcified drq is possible for C-NET(98)S.
	 */
	if (((1 << ia->ia_drq) & CNET98S_DRQMASK) == 0) {
		printf("cnle: configured drq %d invalid\n", ia->ia_drq);
		return (0);
	}

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &ioh))
		return (0);
	if (bus_space_map_load(iot, ioh, p->iosize, p->iat,
	    BUS_SPACE_MAP_FAILFREE))
		return (0);

	/*
	 * See if the vendor code of the physical MAC address is possible
	 * for CONTEC and the board assign code for C-NET(98)S.
	 */
	if ((bus_space_read_1(iot, ioh, 0) != 0x00) ||
	    (bus_space_read_1(iot, ioh, 2) != 0x80) ||
	    (bus_space_read_1(iot, ioh, 4) != 0x4c) ||
	    (bus_space_read_1(iot, ioh, 6) != 0x21)) {
		printf("cnle: MAC address mismatch\n");
		goto bad;
	}

	/* It seems a CONTEC ether board. Check checksum. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sum += bus_space_read_1(iot, ioh, i * 2);
	sum = (sum & 0x00ff) + ((sum & 0xff00) >> 8);

	rom_sum = bus_space_read_1(iot, ioh, ETHER_ADDR_LEN * 2);

	if (sum != rom_sum) {
		printf("cnle: checksum mismatch; calculated %02x != read %02x\n",
		    sum, rom_sum);
		goto bad;
	}

	rap = p->rap;
	rdp = p->rdp;

	/* Stop the LANCE chip and put it in a known state. */
	bus_space_write_2(iot, ioh, rap, LE_CSR0);
	bus_space_write_2(iot, ioh, rdp, LE_C0_STOP);
	delay(100);

	bus_space_write_2(iot, ioh, rap, LE_CSR0);
	if (bus_space_read_2(iot, ioh, rdp) != LE_C0_STOP)
		goto bad;

	bus_space_write_2(iot, ioh, rap, LE_CSR3);
	bus_space_write_2(iot, ioh, rdp, 0);

	/*
	 * OK, she seems lance.
	 * Now check 32 bit device id (chip id) from the registers csr88 and 89.
	 */
	bus_space_write_2(iot, ioh, rap, LE_CSR88);
	devid = bus_space_read_2(iot, ioh, rdp);
	bus_space_write_2(iot, ioh, rap, LE_CSR89);
	devid |= (bus_space_read_2(iot, ioh, rdp) << 16);
#ifdef	LEDEBUG
	printf("cnle: id %lx rev %lx part %lx manf %lx 1 %lx\n", devid,
	    (devid & LE_C88_REV_MASK) >> 28, (devid & LE_C88_PART_MASK) >> 12,
	    (devid & LE_C88_MANF_MASK) >> 1, (devid & LE_C88_ONE_MASK));
#endif	/* LEDEBUG */
	if ((((devid & LE_C88_PART_MASK) >> 12) != LE_PART_AM79C960)
	    || (((devid & LE_C88_MANF_MASK) >> 1) != LE_MANF_AMD)
	    || !(devid & LE_C88_ONE_MASK)) {
		printf("cnle: Device ID(%lx) mismatch\n", devid);
		goto bad;
	}

	ia->ia_iosize = p->iosize;
	rv = 1;

bad:
	bus_space_unmap(iot, ioh, p->iosize);
	return (rv);
}
#endif	/* PC-98 */

#if	!defined(ORIGINAL_CODE) && !defined(LE_SMALL)
/*
 * Determine which chip is present on the card.
 */
int
lance_isa_probe(ia, p)
	struct isa_attach_args *ia;
	struct le_isa_params *p;
{
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rap, rdp;
	int rv = 0;

	/* Disallow wildcarded i/o address. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, p->iosize, 0, &ioh))
		return (0);

	rap = p->rap;
	rdp = p->rdp;

	/* Stop the LANCE chip and put it in a known state. */
	bus_space_write_2(iot, ioh, rap, LE_CSR0);
	bus_space_write_2(iot, ioh, rdp, LE_C0_STOP);
	delay(100);

	bus_space_write_2(iot, ioh, rap, LE_CSR0);
	if (bus_space_read_2(iot, ioh, rdp) != LE_C0_STOP)
		goto bad;

	bus_space_write_2(iot, ioh, rap, LE_CSR3);
	bus_space_write_2(iot, ioh, rdp, 0);

	ia->ia_iosize = p->iosize;
	rv = 1;

bad:
	bus_space_unmap(iot, ioh, p->iosize);
	return (rv);
}
#endif	/* !ORIGINAL_CODE && !LE_SMALL */

void
le_dummyattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	printf("\n");

	config_found(self, aux, 0);
}

int
le_dummyprobe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	return (1);
}

#if	!defined(ORIGINAL_CODE) && !defined(LE_SMALL)
void
le_ne2100_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	le_isa_attach(parent, (void *)self, aux, &ne2100_params);
}

void
le_bicc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	le_isa_attach(parent, (void *)self, aux, &bicc_params);
}
#endif	/* !ORIGINAL_CODE && !LE_SMALL */
#ifndef	ORIGINAL_CODE

void
le_cnet98s_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	le_isa_attach(parent, (void *)self, aux, &cnet98s_params);
}
#endif	/* PC-98 */

void
le_isa_attach(parent, lesc, ia, p)
	struct device *parent;
	struct le_softc *lesc;
	struct isa_attach_args *ia;
	struct le_isa_params *p;
{
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	bus_dma_tag_t dmat = ia->ia_dmat;
	bus_dma_segment_t seg;
	int i, rseg, error;

	printf(": %s Ethernet\n", p->name);

#ifdef	ORIGINAL_CODE
	if (bus_space_map(iot, ia->ia_iobase, p->iosize, 0, &ioh))
		panic("%s: can't map io", sc->sc_dev.dv_xname);
#else	/* PC-98 */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &ioh))
		panic("%s: can't map io", sc->sc_dev.dv_xname);
	if (bus_space_map_load(iot, ioh, p->iosize, p->iat,
	    BUS_SPACE_MAP_FAILFREE))
		panic("%s: can't load io", sc->sc_dev.dv_xname);
#endif	/* PC-98 */

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_enaddr); i++)
		sc->sc_enaddr[i] =
		    bus_space_read_1(iot, ioh, p->macstart + i * p->macstride);

	lesc->sc_iot = iot;
	lesc->sc_ioh = ioh;
	lesc->sc_dmat = dmat;
	lesc->sc_rap = p->rap;
	lesc->sc_rdp = p->rdp;

	/*
	 * Allocate a DMA area for the card.
	 */
	if (bus_dmamem_alloc(dmat, LE_ISA_MEMSIZE, NBPG, 0, &seg, 1,
			     &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: couldn't allocate memory for card\n",
		       sc->sc_dev.dv_xname);
		return;
	}
	if (bus_dmamem_map(dmat, &seg, rseg, LE_ISA_MEMSIZE,
			   (caddr_t *)&sc->sc_mem,
			   BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		printf("%s: couldn't map memory for card\n",
		       sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Create and load the DMA map for the DMA area.
	 */
	if (bus_dmamap_create(dmat, LE_ISA_MEMSIZE, 1,
			LE_ISA_MEMSIZE, 0, BUS_DMA_NOWAIT, &lesc->sc_dmam)) {
		printf("%s: couldn't create DMA map\n",
		       sc->sc_dev.dv_xname);
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}
	if (bus_dmamap_load(dmat, lesc->sc_dmam,
			sc->sc_mem, LE_ISA_MEMSIZE, NULL, BUS_DMA_NOWAIT)) {
		printf("%s: coundn't load DMA map\n",
		       sc->sc_dev.dv_xname);
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}

	sc->sc_conf3 = 0;
	sc->sc_addr = lesc->sc_dmam->dm_segs[0].ds_addr;
	sc->sc_memsize = LE_ISA_MEMSIZE;

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = le_isa_rdcsr;
	sc->sc_wrcsr = le_isa_wrcsr;
	sc->sc_hwinit = NULL;

	if (ia->ia_drq != DRQUNK) {
		if ((error = isa_dmacascade(ia->ia_ic, ia->ia_drq)) != 0) {
			printf("%s: unable to cascade DRQ, error = %d\n",
			    sc->sc_dev.dv_xname, error);
			return;
		}
	}

	lesc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, le_isa_intredge, sc);

	printf("%s", sc->sc_dev.dv_xname);
	am7990_config(&lesc->sc_am7990);
}

/*
 * Controller interrupt.
 */
int
le_isa_intredge(arg)
	void *arg;
{

	if (am7990_intr(arg) == 0)
		return (0);
	for (;;)
		if (am7990_intr(arg) == 0)
			return (1);
}
