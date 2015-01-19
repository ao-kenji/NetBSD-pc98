/*	$NecBSD: dp8390pio.c,v 1.10.6.5 1999/08/23 13:12:17 honda Exp $	*/
/*	$NetBSD: ne2000.c,v 1.3.2.2 1997/11/05 18:43:52 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
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

/*
 * Common code shared by all NE2000-compatible Ethernet interfaces.
 */

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

#include <machine/bus.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <i386/Cbus/dev/dp8390pio.h>
#include <i386/Cbus/dev/endsubr.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_stream_2	bus_space_write_2
#define	bus_space_write_multi_stream_2	bus_space_write_multi_2
#define	bus_space_read_multi_stream_2	bus_space_read_multi_2
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

int	dp8390pio_write_mbuf __P((struct dp8390_softc *, struct mbuf *, int));
int	dp8390pio_ring_copy __P((struct dp8390_softc *, int, caddr_t, u_short));
void	dp8390pio_read_hdr __P((struct dp8390_softc *, int, struct dp8390_ring *));
int	dp8390pio_test_mem __P((struct dp8390_softc *));

void	dp8390pio_writemem __P((bus_space_tag_t, bus_space_handle_t,
	    bus_space_tag_t, bus_space_handle_t, u_int8_t *, int, size_t, int));
void	dp8390pio_readmem __P((bus_space_tag_t, bus_space_handle_t,
	    bus_space_tag_t, bus_space_handle_t, int, u_int8_t *, size_t, int));

/*
 * Alternative attach detect routines for ne2000.
 *
 * Almost all functions are the same as the original ne2000.c.
 *
 * The main purposes are:
 *
 * Since some boards never support single byte remote DMA transfer
 * which causes remote DMA stall, we here probe NE2000 only,
 *
 * Ne2000_detect() in dp8390pio_attach() might fail, 
 * becuase some NE2000 compatible boards would not back to the initial 
 * power up (or hardware bus reset) state by reading a reset port.
 */

int
dp8390pio_attachsubr(nsc, bustype, start, size, myea, media, nmedia, defmedia)
	dp8390pio_softc_t *nsc;
	int bustype;
	bus_addr_t start;
	bus_size_t size;
	u_int8_t *myea;
	int *media, nmedia, defmedia;
{
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	bus_space_tag_t nict = dsc->sc_regt;
	bus_space_handle_t nich = dsc->sc_regh;
	bus_space_tag_t asict = nsc->sc_asict;
	bus_space_handle_t asich = nsc->sc_asich;
	struct dp8390pio_probe_args daa;
	u_int8_t romdata[16];
	int rv, i, useword, romstart;

	if (nsc->sc_readmem == NULL)
		nsc->sc_readmem = dp8390pio_readmem;
	if (nsc->sc_writemem == NULL)
		nsc->sc_writemem = dp8390pio_writemem;

	/*
	 * Calcute the ramstart and the ramsize if not specified.
	 */
	nsc->sc_type = bustype;
	dsc->cr_proto = ED_CR_RD2;
	useword = (nsc->sc_type & DP8390_BUS_PIO16);

	if (size == 0)
	{
		bus_size_t ramsize;

		ramsize = (nsc->sc_type & DP8390_BUS_PIO16) ? 16384 : 8192;
		dsc->mem_start = ramsize;
		dsc->mem_size = ramsize;
		romstart = 0;
	}
	else
	{
		dsc->mem_start = start;
		dsc->mem_size = size;
		if (start == 0)
			romstart = start + size;
		else
			romstart = 0;
	}

	dp8390pio_setup_probe_args(&daa, bustype,
				   nict, nich, asict, asich,
				   dsc->sc_buft, dsc->sc_bufh,
				   dsc->mem_start, dsc->mem_size);
	daa.da_pio_writemem = nsc->sc_writemem;
	daa.da_pio_readmem = nsc->sc_readmem;
	(void) __dp8390pio_detectsubr(&daa);

	/*
	 * DCR gets:
	 *
	 *	FIFO threshold to 8, No auto-init Remote DMA,
	 *	byte order=80x86.
	 *
	 * NE1000 gets byte-wide DMA, NE2000 gets word-wide DMA.
	 */
	dsc->dcr_reg = ED_DCR_FT1 | ED_DCR_LS |
	    ((nsc->sc_type & DP8390_BUS_PIO16) ? ED_DCR_WTS : 0);

	if (dsc->test_mem == NULL)
		dsc->test_mem = dp8390pio_test_mem;
	if (dsc->ring_copy == NULL)
		dsc->ring_copy = dp8390pio_ring_copy;
	if (dsc->write_mbuf == NULL)
		dsc->write_mbuf = dp8390pio_write_mbuf;
	if (dsc->read_hdr == NULL)
		dsc->read_hdr = dp8390pio_read_hdr;

	/* Registers are linear. */
	for (i = 0; i < 16; i++)
		dsc->sc_reg_map[i] = i;

	/*
	 * 8k of memory plus an additional 8k if an NE2000.
	 */

	/*
	 * NIC memory doens't start at zero on an NE board.
	 * The start address is tied to the bus width.
	 * (It happens to be computed the same way as mem size.)
	 */
	if (myea == NULL) {
		/* Read the station address. */
		nsc->sc_readmem(nict, nich, asict, asich, romstart, romdata,
		    sizeof(romdata), useword);
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			dsc->sc_enaddr[i] = romdata[i * (useword ? 2 : 1)];
	} else
		bcopy(myea, dsc->sc_enaddr, sizeof(dsc->sc_enaddr));

	rv = end_lookup(dsc->sc_enaddr);
	switch (rv)
	{
	case END_LOOKUP_INVALID:
		printf("%s: FATAL: invalid ether addr [%x][%x][%x]\n", 
			dsc->sc_dev.dv_xname,
			(u_int) dsc->sc_enaddr[0],
			(u_int) dsc->sc_enaddr[1],
			(u_int) dsc->sc_enaddr[2]);
		return EINVAL;

	case END_LOOKUP_NOTFOUND:
		printf("%s: WARNING: ether vendor [%x][%x][%x] unknown\n",
			dsc->sc_dev.dv_xname,
			(u_int) dsc->sc_enaddr[0],
			(u_int) dsc->sc_enaddr[1],
			(u_int) dsc->sc_enaddr[2]);
		break;
	default:
		break;
	}

	/* Clear any pending interrupts that might have occurred above. */
	bus_space_write_1(nict, nich, ED_P0_ISR, 0xff);

	if (dp8390_config(dsc, media, nmedia, defmedia)) {
		printf("%s: setup failed\n", dsc->sc_dev.dv_xname);
		return EINVAL;
	}

	/*
	 * We need to compute mem_ring a bit differently; override the
	 * value set up in dp8390_config().
	 */
	dsc->mem_ring =
	    dsc->mem_start + ((dsc->txb_cnt * ED_TXBUF_SIZE) << ED_PAGE_SHIFT);
	return 0;
}

/*
 * Detect an NE-2000 or compatible.  Returns a model code.
 */
int
dp8390pio_detectsubr(bustype, nict, nich, asict, asich, memt, memh, ramstart, ramsize, cksum)
	int bustype;
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_addr_t ramstart;
	bus_size_t ramsize;
	u_int8_t cksum;
{
	struct dp8390pio_probe_args daa;

	dp8390pio_setup_probe_args(&daa, bustype,
				   nict, nich, asict, asich, memt, memh,
				   ramstart, ramsize);
	daa.da_pio_writemem = dp8390pio_writemem;
	daa.da_pio_readmem = dp8390pio_readmem;

	return __dp8390pio_detectsubr(&daa);
}

int
__dp8390pio_detectsubr(da)
	struct dp8390pio_probe_args *da;
{
	int bustype = da->da_type;
	bus_space_tag_t nict = da->da_nict;
	bus_space_handle_t nich=da->da_nich;
	bus_space_tag_t asict = da->da_asict;
	bus_space_handle_t asich = da->da_asich;
	bus_addr_t ramstart = da->da_ramstart;
	bus_size_t ramsize = da->da_ramsize;
	static u_int8_t test_pattern[32] = "THIS is A memory TEST pattern";
	u_int8_t test_buffer[32], tmp;
	int i, useword, rv = 0;

	if (da->da_pio_writemem == NULL)
		da->da_pio_writemem = dp8390pio_writemem;
	if (da->da_pio_readmem == NULL)
		da->da_pio_readmem = dp8390pio_readmem;

	/*
	 * Calcute the ramstart and the ramsize if not specified.
	 */
	if (ramsize == 0)
	{
		ramsize = (bustype & DP8390_BUS_PIO16) ? (8192 * 2) : 8192;
		ramstart = ramsize;
	}
	useword = (bustype & DP8390_BUS_PIO16) ? 1 : 0;

	/* Reset the board. */
	tmp = bus_space_read_1(asict, asich, NE2000_ASIC_RESET);
	delay(10000);

	/*
	 * I don't know if this is necessary; probably cruft leftover from
	 * Clarkson packet driver code. Doesn't do a thing on the boards I've
	 * tested. -DG [note that a outb(0x84, 0) seems to work here, and is
	 * non-invasive...but some boards don't seem to reset and I don't have
	 * complete documentation on what the 'right' thing to do is...so we do
	 * the invasive thing for now.  Yuck.]
	 */
	bus_space_write_1(asict, asich, NE2000_ASIC_RESET, tmp);
	delay(5000);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up).
	 * XXX - this makes the probe invasive!  Done against my better
	 * judgement.  -DLG
	 */
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STP);

	delay(5000);

	/*
	 * Generic probe routine for testing for the existance of a DS8390.
	 * Must be performed  after the NIC has just been reset.  This
	 * works by looking at certain register values that are guaranteed
	 * to be initialized a certain way after power-up or reset.
	 *
	 * Specifically:
	 *
	 *	Register		reset bits	set bits
	 *	--------		----------	--------
	 *	CR			TXP, STA	RD2, STP
	 *	ISR					RST
	 *	IMR			<all>
	 *	DCR					LAS
	 *	TCR			LB1, LB0
	 *
	 * We only look at CR and ISR, however, since looking at the others
	 * would require changing register pages, which would be intrusive
	 * if this isn't an 8390.
	 */

	tmp = bus_space_read_1(nict, nich, ED_P0_CR);
	if ((tmp & (ED_CR_RD2 | ED_CR_TXP | ED_CR_STA | ED_CR_STP)) !=
	    (ED_CR_RD2 | ED_CR_STP))
		goto out;

	(void) bus_space_read_1(nict, nich, ED_P0_ISR);

	bus_space_write_1(nict, nich,
	    ED_P0_CR, ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);

	for (i = 0; i < 100; i++) {
		if ((bus_space_read_1(nict, nich, ED_P0_ISR) & ED_ISR_RST) ==
		    ED_ISR_RST) {
			/* Ack the reset bit. */
			bus_space_write_1(nict, nich, ED_P0_ISR, ED_ISR_RST);
			break;
		}
		delay(100);
	}

	/*
	 * This prevents packets from being stored in the NIC memory when
	 * the readmem routine turns on the start bit in the CR.
	 */
	bus_space_write_1(nict, nich, ED_P0_RCR, ED_RCR_MON);

	tmp = (bustype & DP8390_BUS_PIO16) ? ED_DCR_WTS : 0;
	bus_space_write_1(nict, nich, ED_P0_DCR, tmp | ED_DCR_FT1 | ED_DCR_LS);
	bus_space_write_1(nict, nich, ED_P0_PSTART, ramstart >> ED_PAGE_SHIFT);
	bus_space_write_1(nict, nich, ED_P0_PSTOP,
		          (ramstart + ramsize) >> ED_PAGE_SHIFT);

	/*
	 * Write the test pattern in word mode.  If this also fails,
	 * then we don't know what this board is.
	 */
	da->da_pio_writemem(nict, nich, asict, asich, test_pattern, ramstart,
			   sizeof(test_pattern), useword);
	da->da_pio_readmem(nict, nich, asict, asich, ramstart, test_buffer,
		    	  sizeof(test_buffer), useword);

	if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)) == 0)
	{
		/* again check the bottom */
		bus_addr_t bottom = ramstart + ramsize - ED_PAGE_SIZE;

		da->da_pio_writemem(nict, nich, asict, asich, test_pattern,
				   bottom, sizeof(test_pattern), useword);
		da->da_pio_readmem(nict, nich, asict, asich, bottom, test_buffer,
				  sizeof(test_buffer), useword);
		if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)) == 0)
			rv = NE2000_TYPE_NE2000;
	}

	bus_space_write_1(nict, nich, ED_P0_ISR, 0xff);

out:
	return rv;
}

/*
 * Write an mbuf chain to the destination NIC memory address using programmed
 * I/O.
 */
int
dp8390pio_write_mbuf(sc, m, buf)
	struct dp8390_softc *sc;
	struct mbuf *m;
	int buf;
{
	dp8390pio_softc_t *nsc = (dp8390pio_softc_t *)sc;
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;
	bus_space_tag_t asict = nsc->sc_asict;
	bus_space_handle_t asich = nsc->sc_asich;
	int savelen;
	int maxwait = 100;	/* about 120us */

	savelen = m->m_pkthdr.len;

	/* Select page 0 registers. */
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);

	/* Reset remote DMA complete flag. */
	bus_space_write_1(nict, nich, ED_P0_ISR, ED_ISR_RDC);

	/* Set up DMA byte count. */
	bus_space_write_1(nict, nich, ED_P0_RBCR0, savelen);
	bus_space_write_1(nict, nich, ED_P0_RBCR1, savelen >> 8);

	/* Set up destination address in NIC mem. */
	bus_space_write_1(nict, nich, ED_P0_RSAR0, buf);
	bus_space_write_1(nict, nich, ED_P0_RSAR1, buf >> 8);

	/* Set remote DMA write. */
	bus_space_write_1(nict, nich,
	    ED_P0_CR, ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);

	/*
	 * Transfer the mbuf chain to the NIC memory.  NE2000 cards
	 * require that data be transferred as words, and only words,
	 * so that case requires some extra code to patch over odd-length
	 * mbufs.
	 */
	if (nsc->sc_type & DP8390_BUS_PIO8) {
		/* NE1000s are easy. */
		for (; m != 0; m = m->m_next) {
			if (m->m_len) {
				bus_space_write_multi_1(asict, asich,
				    NE2000_ASIC_DATA, mtod(m, u_int8_t *),
				    m->m_len);
			}
		}
	} else {
		/* NE2000s are a bit trickier. */
		u_int8_t *data, savebyte[2];
		int l, leftover;
#ifdef DIAGNOSTIC
		u_int8_t *lim;
#endif
		/* Start out with no leftover data. */
		leftover = 0;
		savebyte[0] = savebyte[1] = 0;

		for (; m != 0; m = m->m_next) {
			l = m->m_len;
			if (l == 0)
				continue;
			data = mtod(m, u_int8_t *);
#ifdef DIAGNOSTIC
			lim = data + l;
#endif
			while (l > 0) {
				if (leftover) {
					/*
					 * Data left over (from mbuf or
					 * realignment).  Buffer the next
					 * byte, and write it and the
					 * leftover data out.
					 */
					savebyte[1] = *data++;
					l--;
					bus_space_write_stream_2(asict, asich,
					    NE2000_ASIC_DATA,
					    *(u_int16_t *)savebyte);
					leftover = 0;
				} else if (ALIGNED_POINTER(data,
					   u_int16_t) == 0) {
					/*
					 * Unaligned data; buffer the next
					 * byte.
					 */
					savebyte[0] = *data++;
					l--;
					leftover = 1;
				} else {
					/*
					 * Aligned data; output contiguous
					 * words as much as we can, then
					 * buffer the remaining byte, if any.
					 */
					leftover = l & 1;
					l &= ~1;
					bus_space_write_multi_stream_2(asict,
					    asich, NE2000_ASIC_DATA,
					    (u_int16_t *)data, l >> 1);
					data += l;
					if (leftover)
						savebyte[0] = *data++;
					l = 0;
				}
			}
			if (l < 0)
				panic("ne2000_write_mbuf: negative len");
#ifdef DIAGNOSTIC
			if (data != lim)
				panic("ne2000_write_mbuf: data != lim");
#endif
		}
		if (leftover) {
			savebyte[1] = 0;
			bus_space_write_stream_2(asict, asich, NE2000_ASIC_DATA,
			    *(u_int16_t *)savebyte);
		}
	}

	/*
	 * Wait for remote DMA to complete.  This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts, and
	 * we can't start another remote DMA until this one completes.  Not
	 * waiting causes really bad things to happen - like the NIC wedging
	 * the bus.
	 */
	while (((bus_space_read_1(nict, nich, ED_P0_ISR) & ED_ISR_RDC) !=
	    ED_ISR_RDC) && --maxwait);

	if (maxwait == 0) {
		log(LOG_WARNING,
		    "%s: remote transmit DMA failed to complete\n",
		    sc->sc_dev.dv_xname);
		dp8390_reset(sc);
	}

	return (savelen);
}

/*
 * Given a source and destination address, copy 'amout' of a packet from
 * the ring buffer into a linear destination buffer.  Takes into account
 * ring-wrap.
 */
int
dp8390pio_ring_copy(sc, src, dst, amount)
	struct dp8390_softc *sc;
	int src;
	caddr_t dst;
	u_short amount;
{
	dp8390pio_softc_t *nsc = (dp8390pio_softc_t *)sc;
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;
	bus_space_tag_t asict = nsc->sc_asict;
	bus_space_handle_t asich = nsc->sc_asich;
	u_short tmp_amount;
	int useword = (nsc->sc_type & DP8390_BUS_PIO16);

	/* Does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* Copy amount up to end of NIC memory. */
		nsc->sc_readmem(nict, nich, asict, asich, src,
		    (u_int8_t *)dst, tmp_amount, useword);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	nsc->sc_readmem(nict, nich, asict, asich, src, (u_int8_t *)dst,
	    amount, useword);

	return (src + amount);
}

void
dp8390pio_read_hdr(sc, buf, hdr)
	struct dp8390_softc *sc;
	int buf;
	struct dp8390_ring *hdr;
{
	dp8390pio_softc_t *nsc = (dp8390pio_softc_t *)sc;

	nsc->sc_readmem(sc->sc_regt, sc->sc_regh, nsc->sc_asict,
	    nsc->sc_asich, buf, (u_int8_t *)hdr, sizeof(struct dp8390_ring),
	    (nsc->sc_type & DP8390_BUS_PIO16));
#if BYTE_ORDER == BIG_ENDIAN
	hdr->count = bswap16(hdr->count);
#endif
}

int
dp8390pio_test_mem(sc)
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
void
dp8390pio_readmem(nict, nich, asict, asich, src, dst, amount, useword)
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int src;
	u_int8_t *dst;
	size_t amount;
	int useword;
{

	/* Select page 0 registers. */
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);

	/* Round up to a word. */
	if (amount & 1)
		++amount;

	/* Set up DMA byte count. */
	bus_space_write_1(nict, nich, ED_P0_RBCR0, amount);
	bus_space_write_1(nict, nich, ED_P0_RBCR1, amount >> 8);

	/* Set up source address in NIC mem. */
	bus_space_write_1(nict, nich, ED_P0_RSAR0, src);
	bus_space_write_1(nict, nich, ED_P0_RSAR1, src >> 8);

	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD0 | ED_CR_PAGE_0 | ED_CR_STA);

	if (useword)
		bus_space_read_multi_stream_2(asict, asich, NE2000_ASIC_DATA,
		    (u_int16_t *)dst, amount >> 1);
	else
		bus_space_read_multi_1(asict, asich, NE2000_ASIC_DATA,
		    dst, amount);
}

/*
 * Stripped down routine for writing a linear buffer to NIC memory.  Only
 * used in the probe routine to test the memory.  'len' must be even.
 */
void
dp8390pio_writemem(nict, nich, asict, asich, src, dst, len, useword)
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *src;
	int dst;
	size_t len;
	int useword;
{
	int maxwait = 100;	/* about 120us */

	/* Select page 0 registers. */
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD2 | ED_CR_PAGE_0 | ED_CR_STA);

	/* Reset remote DMA complete flag. */
	bus_space_write_1(nict, nich, ED_P0_ISR, ED_ISR_RDC);

	/* Set up DMA byte count. */
	bus_space_write_1(nict, nich, ED_P0_RBCR0, len);
	bus_space_write_1(nict, nich, ED_P0_RBCR1, len >> 8);

	/* Set up destination address in NIC mem. */
	bus_space_write_1(nict, nich, ED_P0_RSAR0, dst);
	bus_space_write_1(nict, nich, ED_P0_RSAR1, dst >> 8);

	/* Set remote DMA write. */
	bus_space_write_1(nict, nich, ED_P0_CR,
	    ED_CR_RD1 | ED_CR_PAGE_0 | ED_CR_STA);

	if (useword)
		bus_space_write_multi_stream_2(asict, asich, NE2000_ASIC_DATA,
		    (u_int16_t *)src, len >> 1);
	else
		bus_space_write_multi_1(asict, asich, NE2000_ASIC_DATA,
		    src, len);

	/*
	 * Wait for remote DMA to complete.  This is necessary because on the
	 * transmit side, data is handled internally by the NIC in bursts, and
	 * we can't start another remote DMA until this one completes.  Not
	 * waiting causes really bad things to happen - like the NIC wedging
	 * the bus.
	 */
	while (((bus_space_read_1(nict, nich, ED_P0_ISR) & ED_ISR_RDC) !=
	    ED_ISR_RDC) && --maxwait);

	if (maxwait == 0)
		printf("dp8390pio_writemem: failed to complete\n");
}
