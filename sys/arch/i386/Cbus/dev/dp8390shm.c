/*	$NecBSD: dp8390shm.c,v 1.8.6.4 1999/08/23 11:16:31 kmatsuda Exp $	*/
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

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <i386/Cbus/dev/dp8390pio.h>	/* XXX */
#include <i386/Cbus/dev/dp8390shm.h>

__inline void dp8390shm_writemem __P((dp8390shm_softc_t *, u_int8_t *, int, int));
__inline void dp8390shm_readmem __P((dp8390shm_softc_t *, int, u_int8_t *, int));
void dp8390shm_recv_int __P((struct dp8390_softc *));
int dp8390shm_write_mbuf __P((struct dp8390_softc *, struct mbuf *, int));
int dp8390shm_ring_copy __P((struct dp8390_softc *, int, caddr_t, u_short));
void dp8390shm_read_hdr __P((struct dp8390_softc *, int, struct dp8390_ring *));
int dp8390shm_test_mem __P((struct dp8390_softc *));

int
dp8390shm_attachsubr(dsc, bustype, mstart, msize, nodep, media, nmedia, defmedia)
	dp8390shm_softc_t *dsc;
	int bustype;
	bus_addr_t mstart;
	bus_size_t msize;
	u_int8_t *nodep;
	int *media, nmedia, defmedia;
{
	struct dp8390_softc *sc = &dsc->sc_dp8390;
	int n;

	dsc->sc_type = bustype;
	bcopy(nodep, sc->sc_enaddr, ETHER_ADDR_LEN);

	for (n = 0; n < 16; n ++)
		sc->sc_reg_map[n] = n;

	if (sc->test_mem == NULL)
		sc->test_mem = dp8390shm_test_mem;
	if (sc->ring_copy == NULL)
		sc->ring_copy = dp8390shm_ring_copy;
	if (sc->write_mbuf == NULL)
		sc->write_mbuf = dp8390shm_write_mbuf;
	if (sc->read_hdr == NULL)
		sc->read_hdr = dp8390shm_read_hdr;
	if (sc->recv_int == NULL)
		sc->recv_int = dp8390shm_recv_int;

	sc->mem_start = mstart;
	sc->mem_size = msize;
	sc->dcr_reg |= ED_DCR_FT1 | ED_DCR_LS | 
		((dsc->sc_type & DP8390_BUS_SHM16) ? ED_DCR_WTS : 0);

	n = dp8390_config(sc, media, nmedia, defmedia);
	if (n != 0)
	{
		printf("%s: configuration failed\n", sc->sc_dev.dv_xname);
		return n;
	}
	return n;
}

int
dp8390shm_test_mem(sc)
	struct dp8390_softc *sc;
{
	dp8390shm_softc_t *dsc = (dp8390shm_softc_t *)sc;
	bus_space_tag_t memt = sc->sc_buft;
	bus_space_handle_t memh = sc->sc_bufh;
	bus_size_t memsize = sc->mem_size;
	bus_addr_t memstart = sc->mem_start;
	int i;

	if (dsc->sc_type & DP8390_BUS_SHM16)
		bus_space_set_region_2(memt, memh, memstart, 0, memsize >> 1);
	else
		bus_space_set_region_1(memt, memh, memstart, 0, memsize);

	if (dsc->sc_type & DP8390_BUS_SHM16) {
		for (i = 0; i < memsize; i += 2) {
			if (bus_space_read_2(memt, memh, memstart + i) != 0)
				goto fail;
		}
	} else {
		for (i = 0; i < memsize; i++) {
			if (bus_space_read_1(memt, memh, memstart + i) != 0)
				goto fail;
		}
	}

	return (0);

 fail:
	printf("%s: failed to clear shared memory at offset 0x%x\n",
	    sc->sc_dev.dv_xname, i);
	return (1);
}

__inline void
dp8390shm_writemem(dsc, from, to, len)
	dp8390shm_softc_t *dsc;
	u_int8_t *from;
	int to, len;
{
	bus_space_tag_t memt = dsc->sc_dp8390.sc_buft;
	bus_space_handle_t memh = dsc->sc_dp8390.sc_bufh;

	if (dsc->sc_type & DP8390_BUS_SHM16) {
		bus_space_write_region_2(memt, memh, to, (u_int16_t *)from,
		    len >> 1);
		if (len & 1)
			bus_space_write_2(memt, memh, to + (len & ~1),
			    (u_int16_t)(*(from + (len & ~1))));
	} else
		bus_space_write_region_1(memt, memh, to, from, len);
}

__inline void
dp8390shm_readmem(dsc, from, to, len)
	dp8390shm_softc_t *dsc;
	int from;
	u_int8_t *to;
	int len;
{
	bus_space_tag_t memt = dsc->sc_dp8390.sc_buft;
	bus_space_handle_t memh = dsc->sc_dp8390.sc_bufh;

	if (dsc->sc_type & DP8390_BUS_SHM16) {
		bus_space_read_region_2(memt, memh, from, (u_int16_t *)to,
		    len >> 1);
		if (len & 1)
			*(to + (len & ~1)) = bus_space_read_2(memt,
			    memh, from + (len & ~1)) & 0xff;
	} else
		bus_space_read_region_1(memt, memh, from, to, len);
}

int
dp8390shm_write_mbuf(sc, m, buf)
	struct dp8390_softc *sc;
	struct mbuf *m;
	int buf;
{
	dp8390shm_softc_t *dsc = (dp8390shm_softc_t *)sc;
	int savelen;

	savelen = m->m_pkthdr.len;

	for (; m != NULL; buf += m->m_len, m = m->m_next)
	{
		if ((u_long) buf + (u_long) m->m_len > (u_long) sc->mem_end)
		{
			printf("%s: overflow memory buffer\n", 
				sc->sc_dev.dv_xname);
			break;
		}
		dp8390shm_writemem(dsc, mtod(m, u_int8_t *), buf, m->m_len);
	}

	return (savelen);
}

int
dp8390shm_ring_copy(sc, src, dst, amount)
	struct dp8390_softc *sc;
	int src;
	caddr_t dst;
	u_short amount;
{
	dp8390shm_softc_t *dsc = (dp8390shm_softc_t *)sc;
	u_short tmp_amount;

	/* Does copy wrap to losicr addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* Copy amount up to end of NIC memory. */
		dp8390shm_readmem(dsc, src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	dp8390shm_readmem(dsc, src, dst, amount);

	return (src + amount);
}

void
dp8390shm_read_hdr(sc, packet_ptr, packet_hdrp)
	struct dp8390_softc *sc;
	int packet_ptr;
	struct dp8390_ring *packet_hdrp;
{
	dp8390shm_softc_t *dsc = (dp8390shm_softc_t *)sc;

	dp8390shm_readmem(dsc, packet_ptr, (u_int8_t *)packet_hdrp,
	    sizeof(struct dp8390_ring));
}

void
dp8390shm_recv_int(sc)
	struct dp8390_softc *sc;
{

	dp8390_rint(sc);
}
