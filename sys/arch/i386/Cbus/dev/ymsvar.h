/*	$NecBSD: ymsvar.h,v 1.3 1998/03/14 07:05:33 kmatsuda Exp $	*/
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

struct yms_softc {
	struct	device sc_dev;		/* base device */

	bus_space_tag_t sc_iot;		/* bus space tag */
	bus_space_handle_t sc_ioh;		/* bus space handle_t */

	int sc_enabled;
};

int yms_probesubr __P((bus_space_tag_t, bus_space_handle_t));
void yms_attachsubr __P((struct yms_softc *));
int yms_activate __P((struct yms_softc *));
int yms_deactivate __P((struct yms_softc *));

int opna_wait __P((bus_space_tag_t, bus_space_handle_t));
void opna_disable __P((bus_space_tag_t, bus_space_handle_t));
u_int8_t opna_read __P((bus_space_tag_t, bus_space_handle_t, bus_addr_t));
void opna_write __P((bus_space_tag_t, bus_space_handle_t, bus_addr_t, u_int8_t));

