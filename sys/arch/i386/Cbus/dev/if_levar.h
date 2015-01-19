/*	$NecBSD: if_levar.h,v 1.8 1998/09/29 11:58:23 kmatsuda Exp $	*/
/*	$NetBSD: if_levar.h,v 1.12 1998/08/15 10:51:19 mycroft Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting staff.  All rights reserved.
 *  Copyright (c) 1997, 1998
 *	Kouichi Matsuda.  All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
#ifndef	ORIGINAL_CODE
/*
 * Add register informations of CONTEC C-NET(98)S Ethernet interface
 * for NetBSD/pc98 by Kouichi Matsuda.
 */
#endif	/* PC-98 */

#define	BICC_RDP	0xc
#define	BICC_RAP	0xe

#define	NE2100_RDP	0x10
#define	NE2100_RAP	0x12
#ifndef	ORIGINAL_CODE

/*
 * Register informations of CONTEC C-NET(98)S Ethernet interface
 * for NetBSD/pc98 by Kouichi Matsuda.
 */
#define	CNET98S_IOROFFS	0x400	/* I/O Register base, offset from iobase */

#define	CNET98S_RDP	0x10	/* Register Data Port */
#define	CNET98S_RAP	0x12	/* Register Address Port */
#define	CNET98S_RESET	0x14	/* Reset */
#define	CNET98S_IDP	0x16	/* Configuration Data Port */
#define	CNET98S_BDP	0x16	/* Configuration Data Port (alias) */
#define	CNET98S_VSW	0x18	/* Vendor-Specific Word */
#define	CNET98S_RSVD0	0x1A	/* Reserved */
#define	CNET98S_RSVD1	0x1C	/* Reserved */
/* XXX:
 * CONTEC C-NET(98)S-12 seems to use a register at offset 0x1E,
 * which is reserved by AMD!
 */
#define	CNET98S_RSVD2	0x1E	/* Reserved */

#define	CNET98S_IRQMASK	0x0268	/* valid irq are 3, 5, 6 or 9 */
#define	CNET98S_DRQMASK	0x0006	/* valid drq are 1 or 2 */

/*
 * XXX: Should be in dev/ic/am7990reg.h
 */
/* Control and status register 88-89 (csr88-89) (32 bits) */
#define	LE_CSR88	88	/* device id (chip id) lo */
#define	LE_CSR89	89	/* device id (chip id) hi */
#define LE_C88_REV_MASK		0xf0000000	/* 31-28: silicon revision */
#define	LE_C88_PART_MASK	0x0ffff000	/* 27-12: part number */
#define	LE_C88_MANF_MASK	0x00000ffe	/* 11-1: manufacturer id */
#define	LE_C88_ONE_MASK		0x00000001	/* 0: always a logic 1 */

#define	LE_PART_AM79C960	0x0003	/* part number of Am79C960 */
#define	LE_MANF_AMD		0x001	/* manufacturer ID code for AMD */
#endif	/* PC-98 */

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * ethercom.ec_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct le_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */

	void	*sc_ih;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t	sc_dmat;	/* DMA glue */
	bus_dmamap_t	sc_dmam;
	int	sc_rap, sc_rdp;		/* offsets to LANCE registers */
};
