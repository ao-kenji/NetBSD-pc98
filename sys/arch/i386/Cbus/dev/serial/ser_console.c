/*	$NecBSD: ser_console.c,v 1.5 1999/07/26 06:32:06 honda Exp $	*/
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

/*
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/ser_console.h>

#include <sys/conf.h>

#include "serf.h"
#include "seri.h"
#include "sern.h"

void comcnprobe __P((struct consdev *));
void comcninit __P((struct consdev *));
int comcngetc __P((dev_t));
void comcnputc __P((dev_t, int));
void comcnpollc __P((dev_t, int));
int comopen __P((dev_t, int, int, struct proc *));

static struct com_softc comcnsoftc_store;
static int comconsinit = -1;

struct com_softc *sercnsgsc = &comcnsoftc_store;
struct com_space_handle comconsole_cs;

#ifdef KGDB
#include <sys/kgdb.h>

extern int kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
static int comkgdbinit;

int comkgdb_getc __P((void *));
void comkgdb_putc __P((void *, int));
#endif /* KGDB */

int
ser_connect_comconsole(sc)
	struct com_softc *sc;
{

	if (sercnsgsc == NULL || comconsinit < 0)
		return ENXIO;
	if (sercnsgsc->sc_cswp != sc->sc_cswp)
		return EINVAL;

	SET(sc->sc_hwflags, COM_HW_CONSOLE);
	SET(sc->sc_swflags, COM_SW_SOFTCAR);
	comconsinit = 0;
	sc->sc_ccswp = sercnsgsc->sc_ccswp;
	sercnsgsc = sc;
	printf(":<cons> ");
	return 0;
}

void
comcnprobe(cp)
	struct consdev *cp;
{
	extern int sercnspri, sercnspeed;
	struct commulti_attach_args ca;
	struct com_softc *sc;
	int commajor;

	memset(&ca, 0, sizeof(ca));
	sc = ser_console_probe_machdep(cp, &ca);
	if (sc == NULL)
	{
		cp->cn_pri = CN_DEAD;
		return;
	}

	for (commajor = 0; commajor < nchrdev; commajor ++)
		if (cdevsw[commajor].d_open == comopen)
			break;

	sercnsgsc = sc;
	comconsole_cs = ca.ca_h;

	cp->cn_dev = makedev(commajor, 0);
	cp->cn_pri = sercnspri;
	if (sercnspeed != 0)
	{
		struct com_switch *cswp;

		cswp = sc->sc_cswp;
		cswp->cswt_speed = sercnspeed;
	}
}

void
comcninit(cp)
	struct consdev *cp;
{
	struct com_softc *sc = sercnsgsc;
	struct com_switch *cswp = sc->sc_cswp;
	struct comcons_switch *ccsp = sc->sc_ccswp;

	(*ccsp->comcnhwinit) (sc, cswp->cswt_speed);
	comconsinit = 0;
}

int
comcngetc(dev)
	dev_t dev;
{
	struct com_softc *sc = sercnsgsc;
	struct comcons_switch *ccsp = sc->sc_ccswp;

	return (*ccsp->comcngetc) (sc, dev);
}

void
comcnputc(dev, c)
	dev_t dev;
	int c;
{
	struct com_softc *sc = sercnsgsc;
	struct com_switch *cswp = sc->sc_cswp;
	struct comcons_switch *ccsp = sc->sc_ccswp;

	if (comconsinit <= 0)
	{
		(*ccsp->comcnhwinit) (sc, cswp->cswt_speed);
		comconsinit = 1;
	}

	(*ccsp->comcnputc) (sc, dev, c);
}

void
comcnpollc(dev, on)
	dev_t dev;
	int on;
{
	struct com_softc *sc = sercnsgsc;
	struct comcons_switch *ccsp = sc->sc_ccswp;

	(*ccsp->comcnpollc) (sc, dev, on);
}

#ifdef	KGDB
/**************************************************
 * generic com KGDB
 *************************************************/
int
comkgdb_getc(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	struct comcons_switch *ccsp = sc->sc_ccswp;

	if (comkgdbinit == 0)
	{
		struct com_switch *cswp = sc->sc_cswp;

		(*ccsp->comcnhwinit) (sc, cswp->cswt_speed);
		comkgdbinit = 1;
	}

	return (*ccsp->comcngetc) (sc, 0);
}

void
comkgdb_putc(arg, c)
	void *arg;
	int c;
{
	struct com_softc *sc = arg;
	struct comcons_switch *ccsp = sc->sc_ccswp;

	if (comkgdbinit == 0)
	{
		struct com_switch *cswp = sc->sc_cswp;

		(*ccsp->comcnhwinit) (comkgdbgsc, cswp->cswt_speed);
		comkgdbinit = 1;
	}

	(*ccsp->comcnputc) (comkgdbgsc, 0, c);
}

void
comkgdb_attach(sc)
	struct com_softc *sc;
{
	struct comcons_switch *ccsp;
	struct com_switch *cswp;
	struct commulti_attach_args ca;
	int commajor, i;

	if (comkgdbinit >= 0)
		return;

	sc = ser_console_probe_machdep(cp, &ca);
	if (sc == NULL)
	{
		kgdb_dev = NODEV;
		return;
	}

	for (commajor = 0; commajor < nchrdev; commajor ++)
		if (cdevsw[commajor].d_open == comopen)
			break;

	kgdb_dev = makedev(commajor, 0);
	SET(sc->sc_hwflags, COM_HW_KGDB);

	comconsole_cs = ca.ca_h;
	comkgdbgsc = sc;
 	cswp = sc->sc_cswp;
	ccsp = sc->sc_ccswp;

	kgdb_attach(comkgdb_getc, comkgdb_putc, sc);
	comkgdbinit = 0;

	if (kgdb_debug_init != 0)
	{
		printf("%s: ", sc->sc_dev.dv_xname);
		kgdb_connect(1);
	}
	else
		printf("%s: kgdb enabled\n", sc->sc_dev.dv_xname);

	(*ccsp->comcnhwinit) (sc, cswp->cswt_speed);
	comkgdbinit = 0;
}
#endif	/* KGDB */
