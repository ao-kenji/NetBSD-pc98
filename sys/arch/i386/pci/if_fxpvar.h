/*	$NetBSD: if_fxpvar.h,v 1.3 1997/10/20 01:15:56 thorpej Exp $	*/

/*                  
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *              
 * Modifications to support NetBSD:
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 *                  
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:             
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.  
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Id: if_fxpvar.h,v 1.3 1997/09/29 11:27:43 davidg Exp
 */

/*
 * Misc. defintions for the Intel EtherExpress Pro/100B PCI Fast
 * Ethernet driver
 */

struct ofxp_softc {
#if defined(__NetBSD__)
	struct device sc_dev;		/* generic device structures */
	void *sc_ih;			/* interrupt handler cookie */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	struct ethercom sc_ethercom;	/* ethernet common part */
#else
	struct arpcom arpcom;		/* per-interface network data */
	caddr_t csr;			/* control/status registers */
	struct callout_handle stat_ch;	/* Handle for canceling our stat timeout */
#endif /* __NetBSD__ */
	struct ifmedia sc_media;	/* media information */
	struct ofxp_cb_tx *cbl_base;	/* base of TxCB list */
	struct ofxp_cb_tx *cbl_first;	/* first active TxCB in list */
	struct ofxp_cb_tx *cbl_last;	/* last active TxCB in list */
	struct mbuf *rfa_headm;		/* first mbuf in receive frame area */
	struct mbuf *rfa_tailm;		/* last mbuf in receive frame area */
	struct ofxp_stats *ofxp_stats;	/* Pointer to interface stats */
	int tx_queued;			/* # of active TxCB's */
	int promisc_mode;		/* promiscuous mode enabled */
	int phy_primary_addr;		/* address of primary PHY */
	int phy_primary_device;		/* device type of primary PHY */
	int phy_10Mbps_only;		/* PHY is 10Mbps-only device */
	int rx_idle_secs;		/* # of seconds RX has been idle */
	int need_mcsetup;		/* multicast filter needs programming */
	int all_mcasts;			/* receive all multicasts */
	struct ofxp_cb_mcs *mcsp;	/* Pointer to mcast setup descriptor */
};

/* Macros to ease CSR access. */
#if defined(__NetBSD__)
#define	CSR_READ_1(sc, reg)						\
	bus_space_read_1((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_READ_2(sc, reg)						\
	bus_space_read_2((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_READ_4(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_WRITE_1(sc, reg, val)					\
	bus_space_write_1((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_2(sc, reg, val)					\
	bus_space_write_2((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_4(sc, reg, val)					\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#else
#define	CSR_READ_1(sc, reg)						\
	(*((u_int8_t *)((sc)->csr + (reg))))
#define	CSR_READ_2(sc, reg)						\
	(*((u_int16_t *)((sc)->csr + (reg))))
#define	CSR_READ_4(sc, reg)						\
	(*((u_int32_t *)((sc)->csr + (reg))))
#define	CSR_WRITE_1(sc, reg, val)					\
	(*((u_int8_t *)((sc)->csr + (reg)))) = (val)
#define	CSR_WRITE_2(sc, reg, val)					\
	(*((u_int16_t *)((sc)->csr + (reg)))) = (val)
#define	CSR_WRITE_4(sc, reg, val)					\
	(*((u_int32_t *)((sc)->csr + (reg)))) = (val)
#endif /* __NetBSD__ */

/* Deal with slight differences in software interfaces. */
#if defined(__NetBSD__)
#define	sc_if			sc_ethercom.ec_if
#define	FXP_FORMAT		"%s"
#define	FXP_ARGS(sc)		(sc)->sc_dev.dv_xname
#define	FXP_INTR_TYPE		int
#define	FXP_IOCTLCMD_TYPE	u_long
#define	FXP_BPFTAP_ARG(ifp)	(ifp)->if_bpf
#define	FXP_TIMEOUT(sc, func, hz)					\
				timeout((func), (sc), (hz))
#define	FXP_UNTIMEOUT(sc, func)	untimeout((func), (sc))
#else /* __FreeBSD__ */
#define	sc_if			arpcom.ac_if
#define	FXP_FORMAT		"ofxp%d"
#define	FXP_ARGS(sc)		(sc)->arpcom.ac_if.if_unit
#define	FXP_INTR_TYPE		void
#define	FXP_IOCTLCMD_TYPE	int
#define	FXP_BPFTAP_ARG(ifp)	ifp
#define	FXP_TIMEOUT(sc, func, hz)					\
				(sc)->stat_ch = timeout((func), (sc), (hz))
#define	FXP_UNTIMEOUT(sc, func)	untimeout((func), (sc), (sc)->stat_ch)
#endif /* __NetBSD__ */
