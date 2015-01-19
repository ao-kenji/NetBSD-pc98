/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
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

#ifndef	_NEPCVAR_H_
#define	_NEPCVAR_H_

struct nepc_softc {
	struct device sc_dev;

	bus_space_tag_t sc_bst;			/* controller space tag */
	bus_space_handle_t sc_bsh;		/* controller space handle */

	bus_space_tag_t sc_busiot;		/* bus space io tag */
	struct slot_device_iomem sc_busim_io;	/* io description */
	bus_space_tag_t sc_busmemt;		/* bus space mem tag */
	struct slot_device_iomem sc_busim_mem;	/* mem description */

	bus_space_tag_t sc_csbst;		/* configuration space tag */
	bus_space_handle_t sc_csbsh;		/* configuration space handle */
	struct slot_device_iomem sc_csim;

	u_long sc_immask;			/* slot mask */
	u_long sc_irqmask;			/* interrupt routing */
	struct pccshw_tag sc_pp;

	u_int8_t sc_btype;
	u_int16_t sc_bpage;
};

int nepcmatchsubr __P((bus_space_tag_t, bus_space_handle_t));
void nepc_attachsubr __P((struct nepc_softc *));
#endif	/* !_NEPCVAR_H_ */
