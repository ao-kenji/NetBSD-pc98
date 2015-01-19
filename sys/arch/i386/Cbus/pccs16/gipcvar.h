/*	$NecBSD: gipcvar.h,v 1.6 1999/07/12 16:49:01 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
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

#ifndef	_GIPCVAR_H_
#define	_GIPCVAR_H_

#define	GIPC_MAX_SLOTS	4

struct gipc_softc {
	struct device sc_dev;			/* device head */

	bus_space_tag_t sc_bst;			/* controller space tag */
	bus_space_handle_t sc_bsh;		/* controller space handle */

	bus_space_tag_t sc_busiot;		/* bus space io tag */
	bus_space_tag_t sc_busmemt;		/* bus space mem tag */

	bus_space_tag_t sc_csbst;		/* configuration space tag */
	bus_space_handle_t sc_csbsh;		/* configuration space handle */
	struct slot_device_iomem sc_csim;

	struct pccshw_tag sc_pp;		/* hw tag */
	void *sc_ih;

	u_long sc_immask[GIPC_MAX_SLOTS];	/* slot mask */
	u_int  sc_timing[GIPC_MAX_SLOTS];	/* hw timing */
	u_long sc_irqmask;			/* irq mask */
	u_long sc_irq;				/* status intr */
};

struct gipc_attach_args {
	bus_space_tag_t ga_iot;			/* io space tag */
	bus_space_tag_t ga_memt;		/* mem space tag */
	void *ga_bc;				/* bus chipset tag */

	bus_addr_t ga_iobase;			/* iobase */
	bus_addr_t ga_maddr;			/* attribute mem base */
	bus_addr_t ga_msize;
	u_long	ga_irq;				/* intr pin */
};

int gipcprobesubr __P((bus_space_tag_t, bus_space_handle_t));
void gipcattachsubr __P((struct gipc_softc *));
int gipcintr __P((void *));

#define	GIPC_DVCFG_POLLING	0x0001
#define	GIPC_DVCFG_SELBASE(cfg)	(((cfg) & 0x100) >> 8)
#endif	/* !_GIPCVAR_H_ */
