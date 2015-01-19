/*	$NecBSD: mb86950.c,v 1.5 1999/07/29 05:08:44 kmatsuda Exp $	*/
/*	$NetBSD: if_qn.c,v 1.13 1998/01/12 10:39:53 thorpej Exp $	*/

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

/*
 * Copyright (c) 1995 Mika Kortelainen
 * All rights reserved.
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
 *      This product includes software developed by  Mika Kortelainen
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Thanks for Aspecs Oy (Finland) for the data book for the NIC used
 * in this card and also many thanks for the Resource Management Force
 * (QuickNet card manufacturer) and especially Daniel Koch for providing
 * me with the necessary 'inside' information to write the driver.
 *
 * This is partly based on other code:
 * - if_ed.c: basic function structure for Ethernet driver and now also
 *	qn_put() is done similarly, i.e. no extra packet buffers.
 *
 *	Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 *	adapters.
 *
 *	Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 *	Copyright (C) 1993, David Greenman.  This software may be used,
 *	modified, copied, distributed, and sold, in both source and binary
 *	form provided that the above copyright and these terms are retained.
 *	Under no circumstances is the author responsible for the proper
 *	functioning of this software, nor does the author assume any
 *	responsibility for damages incurred with its use.
 *
 * - if_es.c: used as an example of -current driver
 *
 *	Copyright (c) 1995 Michael L. Hitch
 *	All rights reserved.
 *
 * - if_fe.c: some ideas for error handling for qn_rint() which might
 *	have fixed those random lock ups, too.
 *
 *	All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 *
 * TODO:
 * - add multicast support
 */

/*
 * Fujitsu MB86950 Ethernet Controller (as used in the QuickNet QN2000
 * Ethernet card)
 */

#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"
#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

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

/* XXX */
#include <i386/Cbus/dev/mb86950reg.h>
#include <i386/Cbus/dev/mb86950var.h>

#if NetBSD <= 199714
struct cfdriver fes_cd = {
	NULL, "fes", DV_IFNET
};
#endif

void	mb86950_init __P((struct mb86950_softc *));
int	mb86950_ioctl __P((struct ifnet *, u_long, caddr_t));
void	mb86950_start __P((struct ifnet *));
void	mb86950_reset __P((struct mb86950_softc *));
void	mb86950_watchdog __P((struct ifnet *));

int	mb86950_get_packet __P((struct mb86950_softc *, int)); 
void	mb86950_stop __P((struct mb86950_softc *));
void	mb86950_rint __P((struct mb86950_softc *, u_char));
int	mb86950_write_mbufs __P((struct mb86950_softc *, struct mbuf *));
void	mb86950_setmode __P((struct mb86950_softc *));
void	mb86950_flush __P((struct mb86950_softc *));

void	mb86950_shutdown __P((void *));

int	mb86950_enable __P((struct mb86950_softc *));
void	mb86950_disable __P((struct mb86950_softc *));

int	mb86950_mediachange __P((struct ifnet *));
void	mb86950_mediastatus __P((struct ifnet *, struct ifmediareq *));

#if FES_DEBUG >= 1
void	mb86950_dump __P((int, struct mb86950_softc *));
#endif

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
mb86950_attach(sc, type, myea)
	struct mb86950_softc *sc;
	enum mb86950_type type;
	u_int8_t *myea;
{

	sc->type = type;

	sc->txb_pending = 0;

#ifdef DIAGNOSTIC
	if (myea == NULL) {
		printf("%s: ethernet address shouldn't be NULL\n",
		    sc->sc_dev.dv_xname);
		panic("NULL ethernet address");
	}
#endif
	bcopy(myea, sc->sc_enaddr, sizeof(sc->sc_enaddr));
}

void
mb86950_config(sc, media, nmedia, defmedia)
	struct mb86950_softc *sc;
	int *media, nmedia, defmedia;
{
#ifdef	notyet
	struct cfdata *cf = sc->sc_dev.dv_cfdata;
#endif	/* notyet */
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int i;

	/* set interface to stopped condition (reset) */
	mb86950_stop(sc);

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = mb86950_start;
	ifp->if_ioctl = mb86950_ioctl;
	ifp->if_watchdog = mb86950_watchdog;
	/* XXX IFF_MULTICAST */
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;

#if FES_DEBUG >= 3
	log(LOG_INFO, "%s: mb86950_config()\n", sc->sc_dev.dv_xname);
	mb86950_dump(LOG_INFO, sc);
#endif

	/* Initialize media goo. */
	ifmedia_init(&sc->sc_media, 0, mb86950_mediachange,
	    mb86950_mediastatus);
	if (media != NULL) {
		for (i = 0; i < nmedia; i++)
			ifmedia_add(&sc->sc_media, media[i], 0, NULL);
		ifmedia_set(&sc->sc_media, defmedia);
	} else {
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	}

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

#if NBPFILTER > 0
	/* If BPF is in the kernel, call the attach for it. */
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	sc->sc_sh = shutdownhook_establish(mb86950_shutdown, sc);
	if (sc->sc_sh == NULL)
		panic("mb86950_config: can't establish shutdownhook");

#if NRND > 0
	rnd_attach_source(&sc->rnd_source, sc->sc_dev.dv_xname,
			  RND_TYPE_NET, 0);
#endif
	/* Print additional info when attached. */
	printf("%s: Ethernet address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_enaddr));
}

void
mb86950_shutdown(arg)
	void *arg;
{

	mb86950_stop((struct mb86950_softc *)arg);
}

/*
 * Initialize device
 *
 */
void
mb86950_init(sc)
	struct mb86950_softc *sc;
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int i;

#if FES_DEBUG >= 3
	log(LOG_INFO, "%s: top of mb86950_init()\n", sc->sc_dev.dv_xname);
	mb86950_dump(LOG_INFO, sc);
#endif

	NIC_PUT(sc, FES_DLCR0, FES_D0_BUSERR | FES_D0_COLL16 | FES_D0_COLLID |
	    FES_D0_UDRFLO | FES_D0_TXDONE);	/* Clear all bits. */
	NIC_PUT(sc, FES_DLCR1, 0x00);

	/* flush buffer. */
	mb86950_flush(sc);

	NIC_PUT(sc, FES_DLCR2, FES_D2_OVRFLO | FES_D2_CRCERR | FES_D2_ALGERR |
	    FES_D2_SRTPKT | FES_D2_BUSERR | FES_D2_PKTRDY);	/* Clear all bits. */
	NIC_PUT(sc, FES_DLCR3, 0x00);

	NIC_PUT(sc, FES_DLCR4, FES_D4_LBC_DISABLE);
	NIC_PUT(sc, FES_DLCR5, 0x00);

	/* Set physical ethernet address. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		NIC_PUT(sc, FES_DLCR8 + i, sc->sc_enaddr[i]);

	/* Enable interrupts. */
	NIC_PUT(sc, FES_DLCR1, FES_TMASK);
	NIC_PUT(sc, FES_DLCR3, FES_RMASK);

	/* Do we need this here? */
	NIC_PUT(sc, FES_DLCR0, FES_D0_BUSERR | FES_D0_COLL16 | FES_D0_COLLID |
	    FES_D0_UDRFLO | FES_D0_TXDONE);	/* Clear all bits. */
	NIC_PUT(sc, FES_DLCR2, FES_D2_OVRFLO | FES_D2_CRCERR | FES_D2_ALGERR |
	    FES_D2_SRTPKT | FES_D2_BUSERR | FES_D2_PKTRDY);	/* ditto. */

	/* Enable transmitter and receiver. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->txb_pending = 0;

	mb86950_flush(sc);

	/* Enable data link controller. */
	delay(400);
	NIC_PUT(sc, FES_DLCR6, FES_D6_DLC_ENABLE);
	delay(400);

	/*
	 * At this point, the interface is runnung properly,
	 * except that it receives *no* packets.  we then call
	 * mb86950_setmode() to tell the chip what packets to be
	 * received, based on the if_flags and multicast group
	 * list.  It completes the initialization process.
	 */
	mb86950_setmode(sc);

	/* Attempt to start output, if any. */
	mb86950_start(ifp);
}

/*
 * Device timeout/watchdog routine.  Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
void
mb86950_watchdog(ifp)
	struct ifnet *ifp;
{
	struct mb86950_softc *sc = ifp->if_softc;

	log(LOG_INFO, "%s: device timeout\n", sc->sc_dev.dv_xname);
#if FES_DEBUG >= 3
	mb86950_dump(LOG_INFO, sc);
#endif
	++sc->sc_ec.ec_if.if_oerrors;

	mb86950_reset(sc);
}

/*
 * Flush card's buffer RAM.
 */
void
mb86950_flush(sc)
	struct mb86950_softc *sc;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

#if 1
	/* Read data until bus read error (i.e. buffer empty). */
	while (!(NIC_GET(sc, FES_DLCR2) & FES_D2_BUSERR))
		(void) bus_space_read_2(bst, bsh, FES_BMPR0);
#else
	/* Read data twice to clear some internal pipelines. */
	for (i = 0; i < 4; i++)
		(void) bus_space_read_2(bst, bsh, FES_BMPR0);
#endif

	/* Clear bus read error. */
	NIC_PUT(sc, FES_DLCR2, FES_D2_BUSERR);
}

/*
 * Media change callback.
 */
int
mb86950_mediachange(ifp)
	struct ifnet *ifp;
{
	struct mb86950_softc *sc = ifp->if_softc;

	if (sc->sc_mediachange)
		return ((*sc->sc_mediachange)(sc));
	return (EINVAL);
}

/*
 * Media status callback.
 */
void
mb86950_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct mb86950_softc *sc = ifp->if_softc;

	if (sc->sc_enabled == 0) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	if (sc->sc_mediastatus)
		(*sc->sc_mediastatus)(sc, ifmr);
}

/*
 * Reset the interface.
 *
 */
void
mb86950_reset(sc)
	struct mb86950_softc *sc;
{
	int s;

	s = splnet();
	mb86950_stop(sc);
	mb86950_init(sc);
	splx(s);
}

/*
 * Take interface offline.
 */
void
mb86950_stop(sc)
	struct mb86950_softc *sc;
{
#if 1
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
#endif

#if FES_DEBUG >= 3
	log(LOG_INFO, "%s: top of mb86950_stop()\n", sc->sc_dev.dv_xname);
	mb86950_dump(LOG_INFO, sc);
#endif
	/* Stop the interface. */
	delay(400);
	NIC_PUT(sc, FES_DLCR6, FES_D6_DLC_DISABLE);
	delay(400);

	NIC_PUT(sc, FES_DLCR0, FES_D0_BUSERR | FES_D0_COLL16 |
	    FES_D0_COLLID | FES_D0_UDRFLO);	/* Clear all bits. */
	NIC_PUT(sc, FES_DLCR1, 0x00);

	/* flush buffer. */
	mb86950_flush(sc);

	NIC_PUT(sc, FES_DLCR2, FES_D2_OVRFLO | FES_D2_CRCERR |
	    FES_D2_ALGERR | FES_D2_SRTPKT | FES_D2_BUSERR |
	    FES_D2_PKTRDY);	/* Clear all bits. */
	NIC_PUT(sc, FES_DLCR3, 0x00);

	/* Turn DMA off */
	bus_space_write_2(bst, bsh, FES_BMPR4, 0x00);

	/* Accept no packets. */
	NIC_PUT(sc, FES_DLCR5, 0);
	NIC_PUT(sc, FES_DLCR4, 0);

	mb86950_flush(sc);

#if FES_DEBUG >= 3
	log(LOG_INFO, "%s: end of mb86950_stop()\n", sc->sc_dev.dv_xname);
	mb86950_dump(LOG_INFO, sc);
#endif
}

/*
 * Start output on interface. Get another datagram to send
 * off the interface queue, and copy it to the
 * interface before starting the output.
 *
 * This assumes that it is called inside a critical section...
 */
void
mb86950_start(ifp)
	struct ifnet *ifp;
{
	struct mb86950_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct mbuf *m;
	int len = 0;	/* XXX */
	int timout = 60000;


	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == 0)
		return;

#if NBPFILTER > 0
		/* Tap off here if there is a BPF listener. */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

	len = mb86950_write_mbufs(sc, m);
#if FES_DEBUG >= 4
	printf("len (%x) ", len);
#endif
	m_freem(m);

	/*
	 * Really transmit the packet.
	 */

	/* Set packet length. */
	len = (len & 0x07FF) | (FES_B2_START << 8);
	bus_space_write_2(bst, bsh, FES_BMPR2, len);

	/* Wait for the packet to really leave. */
	while (!(NIC_GET(sc, FES_DLCR0) & FES_D0_TXDONE) && --timout) {
		if ((timout % 10000) == 0)
			log(LOG_INFO, "%s: timout...\n", sc->sc_dev.dv_xname);
	}

	if (timout == 0)
		/* Maybe we should try to recover from this one? */
		/* But now, let's just fall thru and hope the best... */
		log(LOG_INFO, "%s: transmit timout (fatal?)\n",
		    sc->sc_dev.dv_xname);

	sc->txb_pending = 1;
	NIC_PUT(sc, FES_DLCR1, FES_D1_TXDONE | FES_D1_COLL16);

	ifp->if_flags |= IFF_OACTIVE;
	ifp->if_timer = 2;
}

/*
 * Copy packet from mbuf to the board memory
 */
int
mb86950_write_mbufs(sc, m)
	struct mb86950_softc *sc;
	struct mbuf *m;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	u_char *data;
	u_short savebyte;	/* WARNING: Architecture dependent! */
	int totlen, len, wantbyte;
#if FES_DEBUG >= 2
	struct mbuf *mp;
#endif

	/* XXX thorpej 960116 - quiet bogus compiler warning. */
	savebyte = 0;

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m->m_flags & M_PKTHDR) == 0)
	  	panic("mb86950_write_mbufs: no header mbuf");

#if FES_DEBUG >= 2
	/* First, count up the total number of bytes to copy. */
	for (totlen = 0, mp = m; mp != 0; mp = mp->m_next)
		totlen += mp->m_len;
	/* Check if this matches the one in the packet header. */
	if (totlen != m->m_pkthdr.len)
		log(LOG_WARNING, "%s: packet length mismatch? (%d/%d)\n",
		    sc->sc_dev.dv_xname, totlen, m->m_pkthdr.len);
#else
	/* Just use the length value in the packet header. */
	totlen = m->m_pkthdr.len;
#endif

#if FES_DEBUG >= 1
	/*
	 * Should never send big packets.  If such a packet is passed,
	 * it should be a bug of upper layer.  We just ignore it.
	 * ... Partial (too short) packets, neither.
	 */
	if (totlen > (ETHER_MAX_LEN - ETHER_CRC_LEN) ||
	    totlen < ETHER_HDR_LEN) {
		log(LOG_ERR, "%s: got a %s packet (%u bytes) to send\n",
		    sc->sc_dev.dv_xname,
		    totlen < ETHER_HDR_LEN ? "partial" : "big", totlen);
		sc->sc_ec.ec_if.if_oerrors++;
		/* XXX: return (0); */
	}
#endif

	/*
	 * Update buffer status now.
	 * Truncate the length up to an even number, since we use outw().
	 */
	totlen = (totlen + 1) & ~1;

	/*
	 * Transfer the data from mbuf chain to the transmission buffer. 
	 * MB86950 seems to require that data be transferred as words, and
	 * only words.  So that we require some extra code to patch
	 * over odd-length mbufs.
	 */
	wantbyte = 0;
	for (; m != 0; m = m->m_next) {
		/* Ignore empty mbuf. */
		len = m->m_len;
		if (len == 0)
			continue;

		/* Find the actual data to send. */
		data = mtod(m, caddr_t);

		/* Finish the last byte. */
		if (wantbyte) {
			bus_space_write_2(bst, bsh, FES_BMPR0,
			    savebyte | (*data << 8));
			data++;
			len--;
			wantbyte = 0;
		}

		/* Output contiguous words. */
		if (len > 1)
			bus_space_write_multi_2(bst, bsh, FES_BMPR0,
			    (u_int16_t *) data, len >> 1);

		/* Save remaining byte, if there is one. */
		if (len & 1) {
			data += len & ~1;
			savebyte = *data;
			wantbyte = 1;
		}
	}

	/* Spit the last byte, if the length is odd. */
	if (wantbyte)
		bus_space_write_2(bst, bsh, FES_BMPR0, savebyte);

	/*
	 * Pad the packet to the minimum length if necessary.
	 */
	if (totlen < (ETHER_MIN_LEN - ETHER_CRC_LEN)) {
		for (len = totlen + 1; len < (ETHER_MIN_LEN - ETHER_CRC_LEN); len += 2) {
#if FES_DEBUG >= 4
			printf("padding len(%x) ", len);
#endif
			bus_space_write_2(bst, bsh, FES_BMPR0, 0);
		}
		totlen = (ETHER_MIN_LEN - ETHER_CRC_LEN);
	}

	return (totlen);
}

/*
 * Copy packet from board RAM.
 *
 * Trailers not supported.
 */
int
mb86950_get_packet(sc, len)
	struct mb86950_softc *sc;
	int len;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct ether_header *eh;
	struct mbuf *m;

	/* Allocate a header mbuf. */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = len;

	/* The following silliness is to make NFS happy. */
#define	EROUND	((sizeof(struct ether_header) + 3) & ~3)
#define	EOFF	(EROUND - sizeof(struct ether_header))

	/*
	 * Our strategy has one more problem.  There is a policy on
	 * mbuf cluster allocation.  It says that we must have at
	 * least MINCLSIZE (208 bytes) to allocate a cluster.  For a
	 * packet of a size between (MHLEN - 2) to (MINCLSIZE - 2),
	 * our code violates the rule...
	 * On the other hand, the current code is short, simle,
	 * and fast, however.  It does no harmful thing, just waists
	 * some memory.  Any comments?  FIXME.
	 */

	/* Attach a cluster if this packet doesn't fit in a normal mbuf. */
	if (len > MHLEN - EOFF) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return (0);
		}
	}

	/*
	 * The following assumes there is room for the ether header in the
	 * header mbuf.
	 */
	m->m_data += EOFF;
	eh = mtod(m, struct ether_header *);

	/* Set the length of this packet. */
	m->m_len = len;

	/* Get a packet. */
	bus_space_read_multi_2(bst, bsh, FES_BMPR0, (u_int16_t *) m->m_data,
	    (len + 1) >> 1);

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.  If so, hand off
	 * the raw packet to bpf.
	 */
	if (ifp->if_bpf) {
		bpf_mtap(ifp->if_bpf, m);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((ifp->if_flags & IFF_PROMISC) != 0 &&
		    (eh->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
	  	    bcmp(eh->ether_dhost, sc->sc_enaddr,
			sizeof(eh->ether_dhost)) != 0) {
			m_freem(m);
			return (1);
		}
	}
#endif

	/* Fix up data start offset in mbuf to point past ether header. */
	m_adj(m, sizeof(struct ether_header));
	ether_input(ifp, eh, m);
	return (1);
}

/*
 * Ethernet interface receiver interrupt.
 */
void
mb86950_rint(sc, rstat)
	struct mb86950_softc *sc;
	u_char rstat;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int len;
	u_char status;
	int i;

	/* Clear the status register. */
	NIC_PUT(sc, FES_DLCR2, FES_D2_OVRFLO | FES_D2_CRCERR |
	    FES_D2_ALGERR | FES_D2_SRTPKT | FES_D2_BUSERR |
	    FES_D2_PKTRDY);	/* Clear all bits. */

	/*
	 * Was there some error?
	 * Some of them are senseless because they are masked off.
	 * XXX
	 */
	if (rstat & FES_D3_OVRFLO) {
#if FES_DEBUG >= 1
		log(LOG_INFO, "%s: Overflow\n", sc->sc_dev.dv_xname);
#endif
		ifp->if_ierrors++;
	}
	if (rstat & FES_D3_CRCERR) {
#if FES_DEBUG >= 1
		log(LOG_INFO, "%s: CRC Error\n", sc->sc_dev.dv_xname);
#endif
		ifp->if_ierrors++;
	}
	if (rstat & FES_D3_ALGERR) {
#if FES_DEBUG >= 1
		log(LOG_INFO, "%s: Alignment error\n", sc->sc_dev.dv_xname);
#endif
		ifp->if_ierrors++;
	}
	if (rstat & FES_D3_SRTPKT) {
		/* Short packet (these may occur and are
		 * no reason to worry about - or maybe
		 * they are?).
		 */
#if FES_DEBUG >= 1
		log(LOG_INFO, "%s: Short packet\n", sc->sc_dev.dv_xname);
#endif
		ifp->if_ierrors++;
	}
	if (rstat & FES_D3_BUSERR) {
#if FES_DEBUG >= 1
		log(LOG_INFO, "%s: Bus read error\n", sc->sc_dev.dv_xname);
#endif
		ifp->if_ierrors++;
		mb86950_reset(sc);
	}

	/*
	 * Read at most FES_MAX_RECV_COUNT packets per interrupt
	 */
	for (i = 0; i < FES_MAX_RECV_COUNT; i++) {
		if (NIC_GET(sc, FES_DLCR5) & FES_D5_BUFEMP)
			/* Buffer empty. */
			break;

		/*
		 * Read the first word: upper byte contains useful
		 * information.
		 */
		status = (u_char) bus_space_read_2(bst, bsh, FES_BMPR0);
		if ((status & 0x70) != 0x20) {
			log(LOG_INFO, "%s: ERROR: status=%04x\n",
			    sc->sc_dev.dv_xname, status);
			continue;
		}

		/*
		 * Read packet length).
		 * CRC is stripped off by the NIC.
		 */
		len = bus_space_read_2(sc->sc_bst, sc->sc_bsh, FES_BMPR0);

#if FES_DEBUG >= 2
		if (len > (ETHER_MAX_LEN - ETHER_CRC_LEN) ||
		    len < ETHER_HDR_LEN) {
			log(LOG_WARNING,
			    "%s: received a %s packet? (%u bytes)\n",
			    sc->sc_dev.dv_xname,
			    len < ETHER_HDR_LEN ? "partial" : "big", len);
			ifp->if_ierrors++;
			continue;
		}
#endif
#if FES_DEBUG >= 2
		if (len < (ETHER_MIN_LEN - ETHER_CRC_LEN))
			log(LOG_WARNING,
			    "%s: received a short packet? (%u bytes)\n",
			    sc->sc_dev.dv_xname, len);
#endif 

		/* Read the packet. */
		mb86950_get_packet(sc, len);

		ifp->if_ipackets++;
	}

#if FES_DEBUG >= 1
	/* This print just to see whether FES_MAX_RECV_COUNT is large enough. */
	if (i == FES_MAX_RECV_COUNT)
		log(LOG_INFO, "%s: used all the %d loops\n",
		    sc->sc_dev.dv_xname, FES_MAX_RECV_COUNT);
#endif
}

/*
 * Our interrupt routine
 */
int
mb86950_intr(arg)
	void *arg;
{
	struct mb86950_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	u_char tstat, rstat, tstatmask;
	char return_tstatmask = 0;

	if (sc->sc_enabled == 0)
		return (0);

#if FES_DEBUG >= 4
	log(LOG_INFO, "%s: mb86950_intr()\n", sc->sc_dev.dv_xname);
	mb86950_dump(LOG_INFO, sc);
#endif

	/* Get interrupt statuses and masks. */
	rstat = NIC_GET(sc, FES_DLCR2) & FES_RMASK;
	tstatmask = NIC_GET(sc, FES_DLCR1);
	tstat = NIC_GET(sc, FES_DLCR0) & tstatmask;
	if (tstat == 0 && rstat == 0)
		return (0);

	/* Disable interrupts so that we won't miss anything. */
	NIC_PUT(sc, FES_DLCR3, 0x00);
	NIC_PUT(sc, FES_DLCR1, 0x00);

	/*
	 * Handle transmitter interrupts. Some of them are not asked for
	 * but do happen, anyway.
	 */

	if (tstat != 0) {
		/* Clear transmit interrupt status. */
		NIC_PUT(sc, FES_DLCR0, FES_D0_BUSERR | FES_D0_COLL16 |
		    FES_D0_COLLID | FES_D0_UDRFLO);

		if (sc->txb_pending && (tstat & FES_D0_TXDONE)) {
			sc->txb_pending = 0;
			/*
			 * Update total number of successfully
			 * transmitted packets.
			 */
			ifp->if_opackets++;
		}

		if (tstat & FES_D0_COLL16) {
			/*
			 * 16 collision (i.e., packet lost).
			 */
			log(LOG_INFO, "%s: 16 collision - packet lost\n",
			    sc->sc_dev.dv_xname);
#if FES_DEBUG >= 1
			mb86950_dump(LOG_INFO, sc);
#endif
			ifp->if_oerrors++;
			ifp->if_collisions += 16;
			sc->txb_pending = 0;
		}

		if (sc->txb_pending) {
			log(LOG_INFO, "%s: still pending...\n",
			   sc->sc_dev.dv_xname);

			/* Must return transmission interrupt mask. */
			return_tstatmask = 1;
		} else {
			ifp->if_flags &= ~IFF_OACTIVE;

			/* Clear watchdog timer. */
			ifp->if_timer = 0;
		}
	} else
		return_tstatmask = 1;

	/*
	 * Handle receiver interrupts.
	 */
	if (rstat != 0)
		mb86950_rint(sc, rstat);

	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		mb86950_start(ifp);
	else if (return_tstatmask == 1)
		NIC_PUT(sc, FES_DLCR1, tstatmask);

	/* Set receive interrupt mask back. */
	NIC_PUT(sc, FES_DLCR3, FES_RMASK);

	return (1);
}

/*
 * Process an ioctl request. This code needs some work - it looks pretty ugly.
 * I somehow think that this is quite a common excuse... ;-)
 */
int
mb86950_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct mb86950_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

#if FES_DEBUG >= 3
	log(LOG_INFO, "%s: ioctl(%lx)\n", sc->sc_dev.dv_xname, cmd);
#endif

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		if ((error = mb86950_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			mb86950_stop(sc);
			mb86950_init(sc);
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)LLADDR(ifp->if_sadl);
			else {
				bcopy(ina->x_host.c_host, LLADDR(ifp->if_sadl),
				    ETHER_ADDR_LEN);
			}
			mb86950_stop(sc);
			/* Set new address. */
			mb86950_init(sc);
			break;
		    }
#endif
		default:
			mb86950_stop(sc);
			mb86950_init(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			mb86950_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			mb86950_disable(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			if ((error = mb86950_enable(sc)) != 0)
				break;
			mb86950_init(sc);
		} else if (sc->sc_enabled) {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			mb86950_setmode(sc);
		}
#if FES_DEBUG >= 1
		/* "ifconfig fes0 debug" to print register dump. */
		if (ifp->if_flags & IFF_DEBUG) {
			log(LOG_INFO, "%s: SIOCSIFFLAGS(DEBUG)\n",
			    sc->sc_dev.dv_xname);
			mb86950_dump(LOG_DEBUG, sc);
		}
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (sc->sc_enabled == 0) {
			error = EIO;
			break;
		}

#ifdef	notyet
		/* Update our multicast list. */
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ec) :
		    ether_delmulti(ifr, &sc->sc_ec);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			mb86950_setmode(sc);
			error = 0;
		}
		break;
#else	/* notyet */
		error = EINVAL;
		break;
#endif	/* notyet */

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

/*
 * Put the 86950 receiver in appropriate mode.
 */
void
mb86950_setmode(sc)
	struct mb86950_softc *sc;
{
	int flags = sc->sc_ec.ec_if.if_flags;

	/*
	 * If the interface is not running, we postpone the update
	 * process for receive modes until the interface is restarted. 
	 * It reduces some complicated job on maintaining chip states. 
	 *
	 * To complete the trick, mb86950_init() calls mb86950_setmode() after
	 * restarting the interface.
	 */
	if ((flags & IFF_RUNNING) == 0)
		return;

	/*
	 * Promiscuous mode is handled separately.
	 */
	if ((flags & IFF_PROMISC) != 0) {
		/*
		 * Program 86950 to receive all packets on the segment
		 * including those directed to other stations.
		 *
		 * Promiscuous mode is used solely by BPF, and BPF only
		 * listens to valid (no error) packets.  So, we ignore
		 * errornous ones even in this mode.
		 */
		NIC_PUT(sc, FES_DLCR5, FES_D5_AFM0 | FES_D5_AFM1);

#if FES_DEBUG >= 3
		log(LOG_INFO, "%s: promiscuous mode\n", sc->sc_dev.dv_xname);
#endif
		return;
	}

	/*
	 * Turn the chip to the normal (non-promiscuous) mode.
	 */
	NIC_PUT(sc, FES_DLCR5, FES_D5_AFM1);
}

/*
 * Enable power on the interface.
 */
int
mb86950_enable(sc)
	struct mb86950_softc *sc;
{

#if FES_DEBUG >= 3
	log(LOG_INFO, "%s: mb86950_enable()\n", sc->sc_dev.dv_xname);
#endif

	if (sc->sc_enabled == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			printf("%s: device enable failed\n",
			    sc->sc_dev.dv_xname);
			return (EIO);
		}
	}

	sc->sc_enabled = 1;
	return (0);
}

/*
 * Disable power on the interface.
 */
void
mb86950_disable(sc)
	struct mb86950_softc *sc;
{

#if FES_DEBUG >= 3
	log(LOG_INFO, "%s: mb86950_disable()\n", sc->sc_dev.dv_xname);
#endif

	if (sc->sc_enabled != 0 && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_enabled = 0;
	}
}

/*
 * Dump some register information.
 */
#if FES_DEBUG >= 1
void
mb86950_dump(level, sc)
	int level;
	struct mb86950_softc *sc;
{

	log(level, "\tDLCR = %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    NIC_GET(sc, FES_DLCR0),
	    NIC_GET(sc, FES_DLCR1),
	    NIC_GET(sc, FES_DLCR2),
	    NIC_GET(sc, FES_DLCR3),
	    NIC_GET(sc, FES_DLCR4),
	    NIC_GET(sc, FES_DLCR5),
	    NIC_GET(sc, FES_DLCR6),
	    NIC_GET(sc, FES_DLCR7));

	log(level, "\t       %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    NIC_GET(sc, FES_DLCR8),
	    NIC_GET(sc, FES_DLCR9),
	    NIC_GET(sc, FES_DLCR10),
	    NIC_GET(sc, FES_DLCR11),
	    NIC_GET(sc, FES_DLCR12),
	    NIC_GET(sc, FES_DLCR13),
	    NIC_GET(sc, FES_DLCR14),
	    NIC_GET(sc, FES_DLCR15));

	log(level,
	    "\tBMPR = xx xx %02x %02x %02x xx xx xx\n",
	    NIC_GET(sc, FES_BMPR2),
	    NIC_GET(sc, FES_BMPR3),
	    NIC_GET(sc, FES_BMPR4));
}
#endif
