/*	$NecBSD: mb86950subr.h,v 1.2 1998/09/08 03:23:51 kmatsuda Exp $	*/
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

struct fes_simple_probe_struct {
	u_char port;	/* Offset from the base I/O address. */
	u_char mask;	/* Bits to be checked. */
	u_char bits;	/* Values to be compared against. */
};

int fes_find_al98 __P((bus_space_tag_t, bus_space_handle_t, bus_space_handle_t,
    int *, int *));
int fes_detect_al98 __P((bus_space_tag_t, bus_space_handle_t,
    bus_space_handle_t, u_int8_t []));
void fes_attach_al98 __P((struct mb86950_softc *, enum mb86950_type,
    u_int8_t *, int, int));
int fes_find_pc85151 __P((bus_space_tag_t, bus_space_handle_t, bus_space_handle_t,
    int *, int *));
int fes_detect_pc85151 __P((bus_space_tag_t, bus_space_handle_t,
    bus_space_handle_t, u_int8_t []));
int fes_find_pc86131 __P((bus_space_tag_t, bus_space_handle_t, bus_space_handle_t,
    int *, int *));
int fes_detect_pc86131 __P((bus_space_tag_t, bus_space_handle_t,
    bus_space_handle_t, u_int8_t []));
void fes_attach_pc85151 __P((struct mb86950_softc *, enum mb86950_type,
    u_int8_t *, int, int));
