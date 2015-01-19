/*	$NecBSD: necssvar.h,v 1.2 1998/03/14 07:10:30 kmatsuda Exp $	*/
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

struct necss_attach_args {
	struct systm_attach_args *na_sa;
	bus_space_handle_t na_n86ioh;	/* common necss control hand */
	u_long na_spin;			/* common interrupt pin */

#define	NECSS_IDENT	0xf0000
#define	NECSS_PCM86	0x00000
#define	NECSS_PCM86_0	0x00000
#define	NECSS_PCM86_1	0x00001
#define	NECSS_WSS	0x10000
#define	NECSS_WSS_0	0x10000
#define	NECSS_WSS_1	0x10001
#define	NECSS_WSS_2	0x10002
#define	NECSS_YMS	0x20000
	int na_ident;			/* identificaion */

	/* each device specific resource */
	u_long na_pcmbase;		/* pcm or wss iobase */
	u_long na_ymbase;		/* ym iobase */
};
