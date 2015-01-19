/*	$NecBSD: if_feshw.h,v 1.1 1998/06/08 02:09:31 kmatsuda Exp $	*/
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

#include <machine/dvcfg.h>

struct fes_hw {
	/* allowable parent bus type. */
#define	FES_HW_BUSISA	0x01	/* PC-98 C-bus (possibly ISA bus) */
#define	FES_HW_BUSPISA	0x02	/* PnP */
	/* set if we can read (iobase|irq) data from board. */
#define	FES_HW_CONFIO	0x10	/* read iobase setting from EEPROM */
#define	FES_HW_CONFIRQ	0x20	/* read irq setting from EEPROM */
	u_int hw_flags;

	bus_space_iat_t hw_iat;	/* NIC */
	bus_size_t hw_iatsz;	/* sizeof hw_iat */
	bus_space_iat_t hw_asic_iat;	/* ASIC */
	bus_size_t hw_asic_iatsz;	/* sizeof hw_asic_iat */
	int *hw_iomap;		/* allowable iobase list table. */
	int hw_iomapsz;		/* sizeof hw_iomap */
	int hw_irqmask;		/* allowable irq (mask) list. */
	/* hardware depend find routine. */
	int (*hw_find) \
	    __P((bus_space_tag_t, bus_space_handle_t, bus_space_handle_t, int *, int *));
	/* hardware depend detect routine. */
	int (*hw_detect) \
	    __P((bus_space_tag_t, bus_space_handle_t, bus_space_handle_t, u_int8_t []));
	/* hardware depend attach routine. */
	void (*hw_attachsubr) \
	    __P((struct mb86950_softc *, enum mb86950_type, u_int8_t *, int, int));
};

extern struct dvcfg_hwsel fes_hwsel;
