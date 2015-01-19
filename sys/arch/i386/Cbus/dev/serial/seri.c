/*	$NecBSD: seri.c,v 1.3 1999/07/23 05:39:11 kmatsuda Exp $	*/
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

#define	SERI_DEBUG(msg)

#include "opt_seri.h"

#ifndef	SERI_FIFO_HIWATER
#define	SERI_FIFO_HIWATER 1
#endif	/* SERI_FIFO_HIWAT */

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
#include <i386/Cbus/dev/serial/sersubr.h>
#include <i386/Cbus/dev/serial/i8251subr.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif	/* KGDB */

#include "locators.h"
/**************************************************
 * I8251 Hardware definition
 **************************************************/
static inline void seri_delay __P((void));
static void serimodem __P((struct com_softc *, struct tty *));

void seri_hw_attach __P((struct com_softc *));
int seri_hw_activate __P((struct com_softc *));
int seri_hw_deactivate __P((struct com_softc *, int));
int seri_hw_drain __P((struct com_softc *, int));
void seri_hw_fifo __P((struct com_softc *, int));
void seristart __P((struct com_softc *, struct tty *));
int seriintr __P((void *));

#ifndef	SERI_SPEED
#define	SERI_SPEED TTYDEF_SPEED
#endif	/* !SERI_SPEED */

struct com_switch seri_switch = {
	  seri_hw_attach,
	  seri_hw_activate,
	  seri_hw_deactivate,
	  seri_hw_drain,
	  seri_hw_fifo,

	  seristart,
	  seriintr,
	  IPL_COM,

	  i8251ioctl,
	  i8251param,
	  serimodem,
	  i8251errmap,

	  SERI_SPEED,
};

static int serimatch __P((struct device *, struct cfdata *, void *));
void seriattach __P((struct device *, struct device *, void *));

struct cfattach seri_ca = {
	sizeof(struct i8251_softc), serimatch, seriattach
};

extern struct cfdriver seri_cd;

/****************************************************
 * seri probe attach
 ****************************************************/
static int
serimatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct commulti_attach_args *ca = aux;
	bus_space_tag_t iot = ca->ca_h.cs_iot;
	bus_space_handle_t ioh = ca->ca_h.cs_ioh;
	bus_space_handle_t spioh = ca->ca_h.cs_spioh;

	if (ca->ca_type != COM_HW_I8251 && ca->ca_type != COM_HW_I8251_C)
		return 0;

	if (cf->cf_loc[SERCF_SLAVE] != SERCF_SLAVE_DEFAULT &&
	    cf->cf_loc[SERCF_SLAVE] != ca->ca_slave)
		return 0;

	if (_seriprobe(iot, ioh, spioh, ca->ca_hwflags) == 0)
		return 0;

	return SERI_PROBE_PRI;
}

void
seriattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct commulti_attach_args *ca = aux;
	struct i8251_softc *ic = (void *) self;

	printf("\n");
	ca->ca_m = self;
	ic->ic_com.sc_ser = (void *) parent;
	ic->ic_com.sc_cswp = &seri_switch;
	i8251_setup_softc(ca, ic);
	config_found(self, ca, NULL);

#ifdef	KGDB
	comkgdb_attach(&ic->ic_com);
#endif	/* KGDB */
}

/****************************************************
 * seri hardware attach
 ****************************************************/
void
seri_hw_attach(sc)
	struct com_softc *sc;
{
	struct i8251_softc *ic = (void *) sc;

	CLR(sc->sc_hwflags, COM_HW_ACCEL);
	sc->sc_maxputs = SERI_FIFO_HIWATER;
	ic->ic_maxspeed = 38400;

	ic->ic_ier = 0;
	(*ic->ic_control_intr) (sc, ic->ic_ier);
	i8251_init(ic, sc->sc_termios.c_ospeed, I8251_DEF_CMD, I8251_DEF_MODE);

	printf(": I8251 fifo emulation - size(%d).\n", COM_IBUFSIZE);
}

/****************************************************
 * seri (de) activate
 ****************************************************/
void
seri_hw_fifo(sc, flags)
	struct com_softc *sc;
	int flags;
{

	/* nothing */
}

int
seri_hw_activate(sc)
	struct com_softc *sc;
{
	struct i8251_softc *ic = (void *) sc;

	ic->ic_ier = 0;
	(*ic->ic_control_intr) (sc, ic->ic_ier);
	i8251_bset_cmdr(ic, I8251_DEF_CMD);

	sc->sc_nmsr = sc->sc_msr = i8251_read_msr(ic);
	i8251_bset_icr(ic, I8251_RxINTR_ENABLE);

	return ISSET(sc->sc_msr, MSR_DCD);
}

int
seri_hw_deactivate(sc, hupctl)
	struct com_softc *sc;
	int hupctl;
{
	struct i8251_softc *ic = (void *) sc;

	i8251_bclr_cmdr(ic, CMD8251_SBRK);
	ic->ic_ier = 0;
	(*ic->ic_control_intr) (sc, ic->ic_ier);

	if (hupctl != 0)
		i8251_bclr_cmdr(ic, CMD8251_DTR | CMD8251_RTS);
	return 0;
}

/****************************************************
 * seri drain
 ****************************************************/
int
seri_hw_drain(sc, flags)
	struct com_softc *sc;
	int flags;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	int sp, cnt;

	if (flags != 0)
		return 0;

	if (sc->sc_ospeed <= 0 || !ISSET(sc->sc_msr, MSR_DSR))
		return -1;

	sp = splcom();
	if ((cnt = sc->ob_cc) < 0)
		cnt = 0;
	if (!ISSET(bus_space_read_1(iot, ioh, seri_lsr), LSR_TXE))
		cnt += 2;
	splx(sp);

	return cnt;
}

/*****************************************************
 * modem status register access (emulation)
 ****************************************************/
static void
serimodem(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct i8251_softc *ic = (void *) sc;

	sc->sc_nmsr = i8251_read_msr(ic);
	if (sc->sc_nmsr != sc->sc_msr)
		commsc(sc, tp);

	if (ISSET(ic->ic_cmdr, ic->ic_mrts) != ic->ic_mrts &&
	    rb_lowater(&sc->sc_invrb))
		i8251_rtson(ic);
}

/****************************************************
 * start
 ****************************************************/
void
seristart(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct i8251_softc *ic = (void *) sc;
	register bus_space_tag_t iot = ic->ic_iot;
	register bus_space_handle_t ioh = ic->ic_ioh;
	register u_int8_t lsr;
	register int n;
	int s;

	sc->ob_cp = tp->t_outq.c_cf;
	n = sc->sc_cc = ndqb(&tp->t_outq, 0);

	s = splcom();
	bus_space_write_1(iot, ioh, seri_data, *sc->ob_cp++);
	-- n;
	lsr = bus_space_read_1(iot, ioh, seri_lsr);
	if (ISSET(lsr, LSR_TXRDY) && n > 0)
	{
		bus_space_write_1(iot, ioh, seri_data, *sc->ob_cp++);
		-- n;
	}
	i8251_bset_icr(ic, I8251_TxINTR_ENABLE);
	sc->ob_cc = n;
	splx(s);
}

/****************************************************
 * intr
 ****************************************************/
static inline void
seri_delay()
{
#if	SERI_DELAY_COUNT > 0
	register int n = SERI_DEALY_COUNT;

	while (n -- > 0)
		inb(0x5f);
#endif	/* SERI_DELAY_COUNT > 0 */
}

int
seriintr(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	struct i8251_softc *ic = arg;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	struct v_ring_buf *rb = &sc->sc_invrb;
	register u_int8_t lsr;
	u_int loop_count;


	if (sc->sc_vicr == 0)
	{
#ifdef	KGDB
		lsr = bus_space_read_1(iot, ioh, seri_lsr);
		if (ISSET(sc->sc_hwflags, COM_HW_KGDB) &&
		    ISSET(lsr, LSR_RCV_ERR | LSR_RXRDY))
			kgdb_connect(1);
#endif	/* KGDB */
		(void) bus_space_read_1(iot, ioh, seri_data);
		return 0;
	}
		
	/* XXX:
	 * Avoid infinite loops due to the chip hang up.
	 */
	for (loop_count = 0; loop_count < COM_IBUFSIZE; loop_count++)
	{
		lsr = bus_space_read_1(iot, ioh, seri_lsr);
		if (ISSET(lsr, LSR_RCV_ERR | LSR_RXRDY))
		{
			register u_int8_t c;

			if (ISSET(lsr, LSR_RCV_ERR))
				i8251_write_cmdr(ic, CMD8251_ER | ic->ic_cmdr);
			c = bus_space_read_1(iot, ioh, seri_data);
			rb_input(rb, sc->sc_rdata, sc->sc_rstat,
				 c, ISSET(lsr, LSR_RCV_ERR));
			if (rb_highwater(rb))
			{
				i8251_rtsoff(ic);
				SERI_DEBUG("off");
			}

			seri_delay();
			continue;
		}

		if (sc->ob_cc >= 0 && ISSET(lsr, LSR_TXRDY))
		{
			if (sc->ob_cc == 0)
			{
				i8251_xmit_terminate(sc);
				i8251_bclr_icr(ic, I8251_TxINTR_ENABLE);
				continue;
			}

			bus_space_write_1(iot, ioh, seri_data, *sc->ob_cp++);
			-- sc->ob_cc;
			seri_delay();
			continue;
		}
		break;
	}

	if (sc->ob_cc >= 0 && ISSET(ic->ic_ier, I8251_TxINTR_ENABLE) == 0)
	{
		i8251_bset_icr(ic, I8251_TxINTR_ENABLE);
	}
	return 1;
}
