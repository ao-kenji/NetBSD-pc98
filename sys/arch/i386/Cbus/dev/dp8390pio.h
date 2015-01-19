/*	$NecBSD: dp8390pio.h,v 1.6.6.5 1999/08/23 13:12:18 honda Exp $	*/
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
#ifndef	_DP8390PIO_H_
#define	_DP8390PIO_H_

#include <dev/ic/ne2000reg.h>

struct ne2001_softc {
	struct dp8390_softc sc_dp8390;

	bus_space_tag_t sc_asict;	/* space tag for ASIC */
	bus_space_handle_t sc_asich;	/* space handle for ASIC */

	void (*sc_writemem) __P((bus_space_tag_t, bus_space_handle_t,\
	    bus_space_tag_t, bus_space_handle_t, u_int8_t *, int, size_t, int));
	void (*sc_readmem) __P((bus_space_tag_t, bus_space_handle_t,\
	    bus_space_tag_t, bus_space_handle_t, int, u_int8_t *, size_t, int));

	int sc_type;
};

/* sc_type */
#define	NE2000_TYPE_NULL	0
#define	NE2000_TYPE_NE1000	1
#define	NE2000_TYPE_NE2000	2

typedef struct ne2001_softc dp8390pio_softc_t;
#define	DP8390_BUS_PIO8		0x0001
#define	DP8390_BUS_PIO16	0x0002
#define	DP8390_BUS_SHM8		0x0004
#define	DP8390_BUS_SHM16	0x0008

struct dp8390pio_probe_args {
	int da_type;

	bus_space_tag_t da_nict;
	bus_space_handle_t da_nich;

	bus_space_tag_t da_asict;
	bus_space_handle_t da_asich;

	bus_space_tag_t da_memt;
	bus_space_handle_t da_memh;

	bus_addr_t da_ramstart;
	bus_size_t da_ramsize;
	
	void (*da_pio_writemem) __P((bus_space_tag_t, bus_space_handle_t,\
	    bus_space_tag_t, bus_space_handle_t, u_int8_t *, int, size_t, int));
	void (*da_pio_readmem) __P((bus_space_tag_t, bus_space_handle_t,\
	    bus_space_tag_t, bus_space_handle_t, int, u_int8_t *, size_t, int));

	u_int8_t da_cksum;
};

static __inline void dp8390pio_setup_probe_args __P((struct dp8390pio_probe_args *,
	int, bus_space_tag_t, bus_space_handle_t, bus_space_tag_t, bus_space_handle_t,
	bus_space_tag_t, bus_space_handle_t, bus_addr_t, bus_size_t));

static __inline void
dp8390pio_setup_probe_args(da, type, nict, nich, asict, asich, memt, memh, ramstart, ramsize)
	struct dp8390pio_probe_args *da;
	int type;
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_addr_t ramstart;
	bus_size_t ramsize;
{

	memset(da, 0, sizeof(*da));
	da->da_type = type;
	da->da_nict = nict;
	da->da_nich = nich;
	da->da_asict = asict;
	da->da_asich = asich;
	da->da_memt = memt;
	da->da_memh = memh;
	da->da_ramstart = ramstart;
	da->da_ramsize = ramsize;
}

int dp8390pio_detectsubr __P((int, bus_space_tag_t,
	bus_space_handle_t, bus_space_tag_t, bus_space_handle_t,
	bus_space_tag_t, bus_space_handle_t,
	bus_addr_t, bus_size_t, u_int8_t));
int __dp8390pio_detectsubr __P((struct dp8390pio_probe_args *));
int dp8390pio_attachsubr __P((dp8390pio_softc_t *, int, bus_addr_t, bus_size_t, u_int8_t *, int *, int, int));
int dp8390pio_validate_nd __P((u_int8_t *));
#endif	/* !_DP8390PIO_H_ */
