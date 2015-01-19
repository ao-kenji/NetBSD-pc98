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

#ifndef _I386_VSC_SAVERVAR_H_
#define _I386_VSC_SAVERVAR_H_
#define	SAVER_START	0
#define	SAVER_STOP	1

struct video_state;
struct saver_softc {
	u_int timeout_active;

	int saver_time;

#define	SAVERSTAT	0x01
#define	SAVERON		0x00
#define	SAVEROFF	0x01
#define	SAVERCHKGDC	0x02
	u_int state;
	u_int count;

	struct video_state *vsp;
	int (*func) (struct saver_softc *, int);
};

#ifdef	SCREEN_SAVER
static __inline void vsc_invoke_saver __P((struct saver_softc *));
int saver_init __P((struct saver_softc *, struct video_state *));
int saver_switch __P((struct saver_softc *, struct video_state *));
int saver_update __P((struct saver_softc *, int));

static __inline void
vsc_invoke_saver(svp)
	struct saver_softc *svp;
{

	svp->count = 1;
	if ((svp->state & SAVERSTAT) == SAVERON)
		saver_update(svp, SAVER_START);
}
#else	/* !SCREEN_SAVER */
#define	vsc_invoke_saver(vscp)
#endif	/* !SCREEN_SAVER */
#endif /* !_I386_VSC_SAVERVAR_H_ */
