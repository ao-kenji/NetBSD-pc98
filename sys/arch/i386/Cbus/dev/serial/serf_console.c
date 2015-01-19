/*	$NecBSD: serf_console.c,v 1.3 1999/04/15 01:36:19 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define	SERF_DEBUG(msg)

#ifndef	SERF_FIFO_HIWATER
#define	SERF_FIFO_HIWATER 16
#endif	/* SERF_FIFO_HIWAT */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/ser_console.h>
#include <i386/Cbus/dev/serial/i8251subr.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif	/* KGDB */

#ifndef	SERF_SPEED
#define	SERF_SPEED TTYDEF_SPEED
#endif	/* !SERF_SPEED */

static int serfcnprobe __P((struct commulti_attach_args *));
static void serfcninit __P((struct com_softc *, int));
static int serfcngetc __P((struct com_softc *, dev_t));
static void serfcnputc __P((struct com_softc *, dev_t, int));
static void serfcnpollc __P((struct com_softc *, dev_t, int));
static struct com_softc *serfcnmalloc __P((void));
extern struct com_switch serf_switch;
static struct i8251_softc serf_console_store;

struct comcons_switch serfcons_switch = {
	serfcnmalloc,
	serfcnprobe,
	serfcninit,
	serfcngetc,
	serfcnputc,
	serfcnpollc,
};

static struct com_softc *
serfcnmalloc()
{
	struct com_softc *sc = (void *) &serf_console_store;

	sc->sc_cswp = &serf_switch;
	sc->sc_ccswp = &serfcons_switch;
	return sc;
}

static int
serfcnprobe(ca)
	struct commulti_attach_args *ca;
{
	struct i8251_softc *ic = &serf_console_store;
	u_int hwflags = ca->ca_hwflags;
	int rv;

	i8251_setup_softc(ca, ic);
	if (_seriprobe(ic->ic_iot, ic->ic_ioh, ic->ic_spioh, hwflags) == 0)
		return 0;
	rv = _serfprobe(ic->ic_iot, ic->ic_ioh, ic->ic_spioh, hwflags);
	if (rv == 0)
		return 0;
	ic->ic_maxspeed = (rv == COM_HW_I8251F_EDIV) ? 115200 : 76800;
	return 1;
}
	
static void
serfcninit(sc, rate)
	struct com_softc *sc;
	int rate;
{
	struct i8251_softc *ic = (void *) sc;
	int s;

	SET(sc->sc_hwflags, COM_HW_ACCEL);
	SET(ic->ic_triger, FIFO_ENABLE | I8251F_FIFO_TRIG);

 	s = splhigh();
	i8251_init(ic, rate, I8251_DEF_CMD, I8251_DEF_MODE);
	i8251_bset_icr(ic, I8251_RxINTR_ENABLE);
	splx(s);
}

static int
serfcngetc(sc, dev)
	struct com_softc *sc;
	dev_t dev;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t spioh = ic->ic_spioh;
	u_int8_t stat, c;
	u_int mode = CONSMODE_NOWAIT_INPUT;
	u_int hwflags = sc->sc_hwflags;
	int s;

 	s = splhigh();
	(*ic->ic_control_intr) (sc, 0);

	while(1)
	{
		stat = i8251f_cr_read_1(iot, spioh, hwflags, serf_lsr);
		stat = ISSET(stat, FLSR_RXRDY);
		if (stat != 0)
		{
			break;
		}
		else if (mode != 0)
		{
			c = 0;
			goto out;
		}
	};

	c = i8251f_cr_read_1(iot, spioh, hwflags, serf_data);

out:
	(*ic->ic_control_intr) (sc, ic->ic_ier);
	splx(s);

	return c;
}

static void
serfcnputc(sc, dev, c)
	struct com_softc *sc;
	dev_t dev;
	int c;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t spioh = ic->ic_spioh;
	u_int8_t stat;
	register int timo;
	u_int hwflags = sc->sc_hwflags;
	int s;

 	s = splhigh();
	(*ic->ic_control_intr) (sc, 0);

	timo = 50000;
	while (timo -- > 0)
	{
		stat = i8251f_cr_read_1(iot, spioh, hwflags, serf_lsr);
		stat = ISSET(stat, FLSR_TXRDY);
		if (stat != 0)
			break;
	}

	i8251f_cr_write_1(iot, spioh, hwflags, serf_data, c);

	timo = 1500000;
	while (timo -- > 0)
	{
		stat = i8251f_cr_read_1(iot, spioh, hwflags, serf_lsr);
		stat = ISSET(stat, FLSR_TXRDY);
		if (stat != 0)
			break;
	}

	(*ic->ic_control_intr) (sc, ic->ic_ier);
	splx(s);
}

static void
serfcnpollc(sc, dev, on)
	struct com_softc *sc;
	dev_t dev;
	int on;
{

}
