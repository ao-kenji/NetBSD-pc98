/*	$NecBSD: mb86960subr.h,v 1.4 1999/04/06 07:21:27 kmatsuda Exp $	*/
/*	$NetBSD: if_ate.c,v 1.21 1998/03/22 04:25:37 enami Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 *  Copyright (c) 1996, 1997, 1998, 1999
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
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions copyright (C) 1993, David Greenman.  This software may be used,
 * modified, copied, distributed, and sold, in both source and binary form
 * provided that the above copyright and these terms are retained.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

struct fe_simple_probe_struct {
	u_char port;	/* Offset from the base I/O address. */
	u_char mask;	/* Bits to be checked. */
	u_char bits;	/* Values to be compared against. */
};

/* MBE_HW_BUSISA | MBE_HW_BUSPISA */
int mbe_find_cnet98p2 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_cnet98p2 __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_cnet98p2 __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));

/* MBE_HW_BUSISA */
int mbe_find_re1000 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_re1000 __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_re1000 __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_re1000p __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_re1000p __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_re1000p __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_rex9886 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_rex9886 __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_rex9886 __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_rex9883 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_rex9883 __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_rex9883 __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_pc85152 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_pc85152 __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_pc85152 __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_pc86132 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_pc86132 __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_pc86132 __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_lac98 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_lac98 __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_lac98 __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_cnet9ne __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_cnet9ne __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_cnet9ne __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_cnet9nc __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_cnet9nc __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_cnet9nc __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));

/* MBE_HW_BUSPISA */
int mbe_find_mbh1040x __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_mbh1040x __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_mbh1040x __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
int mbe_find_tdk __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_tdk __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_tdk __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));
void mbe_init_mbh __P((struct mb86960_softc *));
int mbe_find_jc89532a __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_detect_jc89532a __P((bus_space_tag_t, bus_space_handle_t,
    u_int8_t enaddr[ETHER_ADDR_LEN]));
void mbe_attach_jc89532a __P((struct mb86960_softc *, enum mb86960_type,
    u_int8_t *, int, int));

int mbe_find_mb86960 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
#ifdef	notyet
int mbe_find_mb86964 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
int mbe_find_mb86965 __P((bus_space_tag_t, bus_space_handle_t, int*, int*));
#endif
