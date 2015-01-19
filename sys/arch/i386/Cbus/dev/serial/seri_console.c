/*	$NecBSD: seri_console.c,v 1.3 1999/07/23 05:39:12 kmatsuda Exp $	*/
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

#include "opt_seri.h"

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

#ifndef	SERI_SPEED
#define	SERI_SPEED TTYDEF_SPEED
#endif	/* !SERI_SPEED */

static int sericnprobe __P((struct commulti_attach_args *));
static void sericninit __P((struct com_softc *, int));
static int sericngetc __P((struct com_softc *, dev_t));
static void sericnputc __P((struct com_softc *, dev_t, int));
static void sericnpollc __P((struct com_softc *, dev_t, int));
static struct com_softc *sericnmalloc __P((void));
extern struct com_switch seri_switch;
static struct i8251_softc seri_console_store;

struct comcons_switch sericons_switch = {
	sericnmalloc,
	sericnprobe,
	sericninit,
	sericngetc,
	sericnputc,
	sericnpollc,
};

static struct com_softc *
sericnmalloc()
{
	struct com_softc *sc = (void *) &seri_console_store;

	sc->sc_cswp = &seri_switch;
	sc->sc_ccswp = &sericons_switch;
	return sc;
}

static int
sericnprobe(ca)
	struct commulti_attach_args *ca;
{
	struct i8251_softc *ic = &seri_console_store;
	u_int hwflags = ca->ca_hwflags;

	i8251_setup_softc(ca, ic);
	if (_seriprobe(ic->ic_iot, ic->ic_ioh, ic->ic_spioh, hwflags) == 0)
		return 0;

	return 1;
}

static void
sericninit(sc, rate)
	struct com_softc *sc;
	int rate;
{
	struct i8251_softc *ic = (void *) sc;
	int s = splhigh();

	CLR(sc->sc_hwflags, COM_HW_ACCEL);
	i8251_init(ic, rate, I8251_DEF_CMD, I8251_DEF_MODE);
	i8251_bset_icr(ic, I8251_RxINTR_ENABLE);
	splx(s);
}

static int
sericngetc(sc, dev)
	struct com_softc *sc;
	dev_t dev;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	u_int8_t stat, c;
	u_int mode = CONSMODE_NOWAIT_INPUT;
	int s;

 	s = splhigh();
	(*ic->ic_control_intr) (sc, 0);

	while(1)
	{
		stat = ISSET(bus_space_read_1(iot, ioh, seri_lsr), \
			     LSR_RXRDY);
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

	c = bus_space_read_1(iot, ioh, seri_data);
out:
	(*ic->ic_control_intr) (sc, ic->ic_ier);
	splx(s);

	return c;
}

static void
sericnputc(sc, dev, c)
	struct com_softc *sc;
	dev_t dev;
	int c;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	u_int8_t stat;
	int s, timo;

 	s = splhigh();
	(*ic->ic_control_intr) (sc, 0);

	timo = 50000;
	while (timo -- > 0)
	{
		stat = ISSET(bus_space_read_1(iot, ioh, seri_lsr), \
			     LSR_TXRDY);
		if (stat != 0)
			break;
	}

	bus_space_write_1(iot, ioh, seri_data, c);

	timo = 1500000;
	while (timo -- > 0)
	{
		stat = ISSET(bus_space_read_1(iot, ioh, seri_lsr),
			     LSR_TXRDY);
		if (stat != 0)
			break;
	}

	(*ic->ic_control_intr) (sc, ic->ic_ier);
	splx(s);
}

static void
sericnpollc(sc, dev, on)
	struct com_softc *sc;
	dev_t dev;
	int on;
{

}
