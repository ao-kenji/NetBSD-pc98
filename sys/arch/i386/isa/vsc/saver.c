/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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

#include "opt_kbd.h"
#include "opt_vsc.h"

#ifdef	SCREEN_SAVER
#include "apm.h"
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <i386/isa/vsc/config.h>
#include <i386/isa/vsc/vsc.h>
#include <i386/isa/vsc/savervar.h>
#include <i386/isa/vsc/video.h>

void saver_soft_intr __P((void *));
int vsc_default_saver __P((struct saver_softc *, int));

int
saver_init(sc, vsp)
	struct saver_softc *sc;
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;
	int s = spltty();

	sc->count = 0;
	sc->func = vsc_default_saver;
	sc->saver_time = DEFAULT_SAVER_TIME;
	sc->vsp = vsp;
	sc->timeout_active = 0;
	sc->count = 1;
	sc->state = SAVEROFF;	/* OK */
	vscp->cursaver = sc;
	splx(s);
	return 0;
}

void
saver_soft_intr(arg)
	void *arg;
{
	struct saver_softc *sc = arg;

	sc->timeout_active = 0;

	if (sc->count)
	{
		sc->count = 0;
		if (sc->saver_time)
		{
			sc->timeout_active = 1;
			timeout(saver_soft_intr,
				(void *) sc, hz * sc->saver_time);
		}
	}
	else
		(*(sc->func)) (sc, SAVERON);
}

/* update current saver satatus. */
int
saver_update(sc, flags)
	struct saver_softc *sc;
	int flags;
{

	sc->count = 1;
	if (ISSET(sc->state, SAVERSTAT) == SAVERON)
		(*(sc->func)) (sc, SAVEROFF);

	if (sc->timeout_active)
	{
		sc->timeout_active = 0;
		untimeout(saver_soft_intr, (void *) sc);
	}

	if (flags == SAVER_START && sc->saver_time > 0)
		saver_soft_intr(sc);
	return 0;
}

int
saver_switch(sc, vsp)
	struct saver_softc *sc;
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (vsp != vscp->cvsp)
		return EBUSY;

	saver_update(vscp->cursaver, SAVER_STOP);
	vscp->cursaver = sc;
	sc->vsp = vsp;

	if (ISSET(vsp->flags, VSC_SAVER))
		saver_update(sc, SAVER_START);
	return 0;
}

#if	NAPM > 0
void vsc_periodic_gdc_check __P((void *));

void
vsc_periodic_gdc_check(arg)
	void *arg;
{
	struct saver_softc *sc = arg;
	struct vsc_softc *vscp = sc->vsp->vs_vscp;
	int s = spltty();

	vsc_text_off(vscp);
	vsc_graphic_off(vscp);
	timeout(vsc_periodic_gdc_check, sc, hz);
	splx(s);
}
#endif	/* NAPM > 0 */

int
vsc_default_saver(sc, flag)
	struct saver_softc *sc;
	int flag;
{
	struct vsc_softc *vscp = sc->vsp->vs_vscp;
	int s = spltty();

	vsc_vsync_wait(vscp);
	if (flag == SAVEROFF)
	{
		vsc_text_on(vscp);
		vsc_graphic_on(vscp);
#if	NAPM > 0
		if (ISSET(sc->state, SAVERCHKGDC))
		{
			untimeout(vsc_periodic_gdc_check, (void *) sc);
			CLR(sc->state, SAVERCHKGDC);
		}
#endif	/* NAPM > 0 */
		SET(sc->state, SAVEROFF);
	}
	else if (flag == SAVERON)
	{
		vsc_text_off(vscp);
		vsc_graphic_off(vscp);
#if	NAPM > 0
		if (ISSET(sc->state, SAVERCHKGDC) == 0)
		{
			SET(sc->state, SAVERCHKGDC);
			timeout(vsc_periodic_gdc_check, (void *) sc, hz);
		}
#endif	/* NAPM > 0 */

		CLR(sc->state, SAVEROFF);
	}
	splx(s);
	return 0;
}
#endif	/* SCREEN_SAVER */
