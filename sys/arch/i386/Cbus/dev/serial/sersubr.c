/*	$NecBSD: sersubr.c,v 1.2 1999/04/15 01:36:20 kmatsuda Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/vnode.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/sersubr.h>

/****************************************************
 * error and diag utils
 ****************************************************/
static void comdiag __P((void *));

static void
comdiag(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	int overflows, floods;
	int s;

	s = spltty();
	sc->sc_errors = 0;
	overflows = sc->sc_overflows;
	sc->sc_overflows = 0;
	floods = sc->sc_floods;
	sc->sc_floods = 0;
	splx(s);

	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf overflow%s\n",
	    sc->sc_dev.dv_xname,
	    overflows, overflows == 1 ? "" : "s",
	    floods, floods == 1 ? "" : "s");
}

void
comoerr(sc, type)
	struct com_softc *sc;
	int type;
{

	if (type == COM_SOVF)
		sc->sc_floods++;
	else
		sc->sc_overflows++;

	if (sc->sc_errors++ == 0)
		timeout(comdiag, sc, 60 * hz);
}

/**************************************************
 * modem control
 *************************************************/
void
commsc(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	register u_int8_t delta, msr;

	msr = sc->sc_nmsr;		/* should load the data */
	delta = sc->sc_msr ^ msr;
	sc->sc_msr = msr;

	if (ISSET(delta, sc->sc_mdcd) != 0)
		(void) (*linesw[tp->t_line].l_modem)(tp, ISSET(msr, sc->sc_mdcd));
}

/**************************************************
 * fake settings
 *************************************************/
void
comsetfake(sc, t, spp)
	struct com_softc *sc;
	struct termios *t;
	int *spp;
{
	struct sertty_softc *ts = sc->sc_cts;

	*spp = ts->sc_termios.c_ospeed;
	t->c_cflag = ts->sc_termios.c_cflag;
}

struct comspeed *
comfindspeed(spt, speed)
	struct comspeed *spt;
	u_long speed;
{
	struct comspeed *tsp = spt;

	for (tsp = spt; spt->cs_div != -1; spt ++)	
		if (spt->cs_speed == speed)
			break;
	return spt;
}

void
com_setup_softc(ca, sc)
	struct commulti_attach_args *ca;
	struct com_softc *sc;
{

	sc->sc_emulintr = ca->ca_emulintr;
	sc->sc_arg = ca->ca_arg;

	sc->sc_type = ca->ca_type;
	sc->sc_subtype = ca->ca_subtype;
	sc->sc_freq = ca->ca_freq;
	sc->sc_speedtab = ca->ca_speedtab;
	sc->sc_speedfunc = ca->ca_speedfunc;

	SET(sc->sc_hwflags, ISSET(DVCFG_MINOR(ca->ca_cfgflags), COM_HW_USER));
	SET(sc->sc_hwflags, ISSET(ca->ca_hwflags, COM_HW_EMM));
}
