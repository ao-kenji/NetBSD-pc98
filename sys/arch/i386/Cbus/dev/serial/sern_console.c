/*	$NecBSD: sern_console.c,v 1.3 1999/07/23 05:39:12 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */
/*
 * Copyright (c) 1995 Charles Hannum.  All rights reserved.
 *
 * This code is derived from public-domain software written by
 * Roland McGrath.
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

#include "opt_sern.h"

#ifndef	SERN_FIFO_HIWATER
#define	SERN_FIFO_HIWATER	4
#endif	/* SERN_FIFO_HIWAT */
#define	NS165_FTL FIFO_TRIGGER_8

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
#include <i386/Cbus/dev/serial/ns165subr.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif	/* KGDB */

#ifndef	SERN_SPEED
#define	SERN_SPEED TTYDEF_SPEED
#endif	/* !SERN_SPEED */

static int serncnprobe __P((struct commulti_attach_args *));
static void serncninit __P((struct com_softc *, int));
static int serncngetc __P((struct com_softc *, dev_t));
static void serncnputc __P((struct com_softc *, dev_t, int));
static void serncnpollc __P((struct com_softc *, dev_t, int));
static struct com_softc *serncnmalloc __P((void));
static struct ns16550_softc sern_console_store;
extern struct com_switch sern_switch;

struct comcons_switch serncons_switch = {
	serncnmalloc,
	serncnprobe,
	serncninit,
	serncngetc,
	serncnputc,
	serncnpollc,
};

static struct com_softc *
serncnmalloc()
{
	struct com_softc *sc = (void *) &sern_console_store;

	sc->sc_cswp = &sern_switch;
	sc->sc_ccswp = &serncons_switch;
	return sc;
}

static int
serncnprobe(ca)
	struct commulti_attach_args *ca;
{
	struct ns16550_softc *ic = &sern_console_store;
	u_int hwflags = ca->ca_hwflags;

	comns_setup_softc(ca, ic);
	if (_comnsprobe(ic->ic_iot, ic->ic_ioh, ic->ic_spioh, hwflags) == 0)
		return 0;
	return 1;
}

static void
serncninit(sc, rate)
	struct com_softc *sc;
	int rate;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	int s = splhigh();

	rate = comns_speed(rate, sc->sc_freq);
	bus_space_write_1(iot, ioh, com_lcr, LCR_DLAB);
	bus_space_write_1(iot, ioh, com_dlbl, rate);
	bus_space_write_1(iot, ioh, com_dlbh, rate >> 8);
	bus_space_write_1(iot, ioh, com_lcr, LCR_8BITS);
	bus_space_write_1(iot, ioh, com_fifo,
			  FIFO_ENABLE | FIFO_RCV_RST |
			  FIFO_XMT_RST | FIFO_TRIGGER_4);
	bus_space_write_1(iot, ioh, com_mcr, MCR_DTR | MCR_RTS);
	bus_space_write_1(iot, ioh, com_ier, IER_ERXRDY);

	(void) bus_space_read_1(iot, ioh, com_iir);
	splx(s);
}

static int
serncngetc(sc, dev)
	struct com_softc *sc;
	dev_t dev;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	u_int mode = CONSMODE_NOWAIT_INPUT;
	u_int8_t stat, c;
	int s = splhigh();

	while(1)
	{
		stat = ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY);
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

	c = bus_space_read_1(iot, ioh, com_data);

out:
	(void) bus_space_read_1(iot, ioh, com_iir);
	splx(s);
	return c;
}

/*
 * Console kernel output character routine.
 */
static void
serncnputc(sc, dev, c)
	struct com_softc *sc;
	dev_t dev;
	int c;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register int timo;
	int s = splhigh();

	timo = 50000;
	while (!ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_TXRDY) && --timo)
		;

	bus_space_write_1(iot, ioh, com_data, c);

	timo = 1500000;
	while (!ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_TXRDY) && --timo)
		;

	(void) bus_space_read_1(iot, ioh, com_iir);
	splx(s);
}

static void
serncnpollc(sc, dev, on)
	struct com_softc *sc;
	dev_t dev;
	int on;
{

}
