/*	$NecBSD: ns165subr.c,v 1.1 1998/12/31 02:33:03 honda Exp $	*/
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
#include <i386/Cbus/dev/serial/ns165subr.h>

/****************************************************
 * NS16550 Hardware definition
 ****************************************************/
static u_int8_t tiocm_xxx2mcr __P((int));

/****************************************************
 * comns generic probe
 ****************************************************/
int
_comnsprobe(iot, ioh, spioh, hwflags)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_space_handle_t spioh;
	u_int hwflags;
{
	register u_int8_t tmp;

	bus_space_write_1(iot, ioh, com_lcr, 0);
	delay(10);
	bus_space_write_1(iot, ioh, com_iir, 0);
	delay(10);
	if (bus_space_read_1(iot, ioh, com_iir) & 0x38)
		return 0;

	/* XXX:
	 * com_lcr is located at odd address,
	 * it's better to check an odd address register
	 * in classical machines.
	 */
	bus_space_write_1(iot, ioh, com_lcr, LCR_8BITS | LCR_STOPB);
	tmp = bus_space_read_1(iot, ioh, com_lcr);
	if ((tmp & 0x3f) == (LCR_8BITS) ||
	    (tmp & 0x3f) == (LCR_8BITS | LCR_STOPB))
		return 1;
	return 0;
}

void
comns_setup_softc(ca, ic)
	struct commulti_attach_args *ca;
	struct ns16550_softc *ic;
{

	ic->ic_iot = ca->ca_h.cs_iot;
	ic->ic_ioh = ca->ca_h.cs_ioh;
	ic->ic_spioh = ca->ca_h.cs_spioh;

	com_setup_softc(ca, &ic->ic_com);
}

/****************************************************
 * comns generic ioctl
 ****************************************************/
static u_int8_t
tiocm_xxx2mcr(data)
	int data;
{
	u_int8_t m = 0;

	if (ISSET(data, TIOCM_DTR))
		SET(m, MCR_DTR);
	if (ISSET(data, TIOCM_RTS))
		SET(m, MCR_RTS);
	return m;
}

int
comnsioctl(sc, cmd, data, flag, p)
	struct com_softc *sc;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	int error = 0, s, bits;
	u_int8_t m;

	/* XXX:  Do not change ic->ic_mcr without spltty ! */
	s = splcom();
	switch (cmd)
	{
	case TIOCSDTR:
		SET(ic->ic_mcr, ic->ic_dtr);
		bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
		break;

	case TIOCCDTR:
		CLR(ic->ic_mcr, ic->ic_dtr);
		bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
		break;

	case TIOCMSET:
		CLR(ic->ic_mcr, MCR_DTR | MCR_RTS);
	case TIOCMBIS:
		SET(ic->ic_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
		break;

	case TIOCMBIC:
		CLR(ic->ic_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
		break;

	default:
		error = -1;
		break;
	}
	splx(s);

	if (error >= 0)
		return error;

	switch (cmd)
	{
	case TIOCSBRK:
		SET(ic->ic_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, com_lcr, ic->ic_lcr);
		break;

	case TIOCCBRK:
		CLR(ic->ic_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, com_lcr, ic->ic_lcr);
		break;

	case TIOCMGET:
		bits = 0;
		m = ic->ic_mcr;
		if (ISSET(m, MCR_DTR))
			SET(bits, TIOCM_DTR);
		if (ISSET(m, MCR_RTS))
			SET(bits, TIOCM_RTS);
		m = sc->sc_msr;
		if (ISSET(m, MSR_DCD))
			SET(bits, TIOCM_CD);
		if (ISSET(m, MSR_CTS))
			SET(bits, TIOCM_CTS);
		if (ISSET(m, MSR_DSR))
			SET(bits, TIOCM_DSR);
		if (ISSET(m, MSR_RI | MSR_TERI))
			SET(bits, TIOCM_RI);
		if (sc->sc_vicr != 0)
			SET(bits, TIOCM_LE);
		*(int *)data = bits;
		break;

	default:
		return ENOTTY;
	}

	return 0;
}

/****************************************************
 * comns generic params
 ****************************************************/
int
comnsparam(sc, tp, t)
	struct com_softc *sc;
	struct tty *tp;
	struct termios *t;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	int s, ospeed, carrier;
	u_int8_t mcrr, lcr;

	if (ISSET(sc->sc_cflags, COM_MODE_FIXED))
		comsetfake(sc, t, &ospeed);
	else
		ospeed = t->c_ospeed;

	if (comns_speed(ospeed, sc->sc_freq) < 0 ||
	    (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return EINVAL;

	lcr = ISSET(ic->ic_lcr, LCR_SBREAK);
	switch (ISSET(t->c_cflag, CSIZE))
	{
	case CS5:
		SET(lcr, LCR_5BITS);
		break;
	case CS6:
		SET(lcr, LCR_6BITS);
		break;
	case CS7:
		SET(lcr, LCR_7BITS);
		break;
	case CS8:
		SET(lcr, LCR_8BITS);
		break;
	}

	if (ISSET(t->c_cflag, PARENB))
	{
		SET(lcr, LCR_PENAB);
		if (!ISSET(t->c_cflag, PARODD))
			SET(lcr, LCR_PEVEN);
	}

	if (ISSET(t->c_cflag, CSTOPB))
		SET(lcr, LCR_STOPB);

	s = splcom();
	mcrr = ic->ic_mcr;
	if (ISSET(t->c_cflag, CRTSCTS))
	{
		ic->ic_mcts = MSR_CTS;
		ic->ic_mrts = MCR_RTS;
		ic->ic_dtr = MCR_DTR;
	}
	else if (ISSET(t->c_cflag, MDMBUF))
	{
		ic->ic_mcts = MSR_DCD;
		ic->ic_mrts = MCR_DTR;
		ic->ic_dtr = 0;
	}
	else
	{
		ic->ic_mcts = ic->ic_mrts = 0;
		ic->ic_dtr = MCR_DTR | MCR_RTS;
	}

	if (ospeed == 0)
	{
		CLR(mcrr, ic->ic_dtr);
		sc->sc_ospeed = ospeed;
	}
	else 
	{
		if (sc->sc_ospeed != ospeed)
		{
			ic->ic_lcr = lcr;
			if (sc->sc_speedfunc != NULL)
				(*sc->sc_speedfunc) (sc, ospeed);
			else
				comns_set_speed(sc, ospeed);
			sc->sc_ospeed = ospeed;
		}
		else if (ic->ic_lcr != lcr)
		{
			ic->ic_lcr = lcr;
			bus_space_write_1(iot, ioh, com_lcr, ic->ic_lcr);
		}

		SET(mcrr, ic->ic_dtr);
	}

	if (ISSET(sc->sc_swflags, COM_SW_SOFTCAR) ||
	    ISSET(t->c_cflag, CLOCAL | MDMBUF))
	{
		sc->sc_mdcd = 0;
		carrier = 1;
	}
	else
	{
		sc->sc_mdcd = MSR_DCD;
		carrier = ISSET(sc->sc_msr, MSR_DCD);
	}

	if (sc->sc_rtscts != ISSET(t->c_cflag, CHWFLOW))
	{
		sc->sc_rtscts = ISSET(t->c_cflag, CHWFLOW);
		(*sc->sc_cswp->cswt_hwfifo) (sc, 0);
	}

	if (mcrr != ic->ic_mcr)
	{
		ic->ic_mcr = mcrr;
		bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
	}

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	(void) (*linesw[tp->t_line].l_modem) (tp, carrier);
	splx(s);

	return 0;
}

/****************************************************
 * comns error code mapping
 ****************************************************/
u_int
comnserrmap(sc, c, stat)
	struct com_softc *sc;
	u_int c;
	u_int8_t stat;
{
	static u_int lsrmap[8] = {
		0,      TTY_PE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE
	};

#ifdef DDB
	if (ISSET(stat, LSR_BI) && ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
	{
		Debugger();
		/* remaining break characters in the fifo should be cleared */
		return RBFLUSH;
	}
#endif	/* DDB */

	if (ISSET(stat, LSR_OE))
		comoerr(sc, COM_HOVF);
	return c | lsrmap[(stat & (LSR_BI|LSR_FE|LSR_PE)) >> 2];
}

/****************************************************
 * comns generic modem status change
 ****************************************************/
void
comnsmodem(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct ns16550_softc *ic = (void *) sc;

	if (sc->sc_txintr != 0 && ISSET(ic->ic_ier, IER_ETXRDY))
	{
		CLR(ic->ic_ier, IER_ETXRDY);
		bus_space_write_1(ic->ic_iot, ic->ic_ioh, com_ier, ic->ic_ier);
	}

	if (sc->sc_nmsr != sc->sc_msr)
		commsc(sc, tp);

	if (ISSET(ic->ic_mcr, ic->ic_mrts) != ic->ic_mrts &&
	    rb_lowater(&sc->sc_invrb))
	{
		COMNS_DEBUG("on");
		comns_rtson(ic);
	}
}

/****************************************************
 * comns speed func (internal)
 ****************************************************/
int
comns_speed(speed, freq)
	long speed;
	u_long freq;
{
#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */

	int x, err;

	if (speed == 0)
		return 0;
	if (speed < 0)
		return -1;
	x = divrnd((freq / 16), speed);
	if (x <= 0)
		return -1;
	err = divrnd((freq / 16) * 1000, speed * x) - 1000;
	if (err < 0)
		err = -err;
	if (err > COM_TOLERANCE)
		return -1;
	return x;

#undef	divrnd(n, q)
}

void
comns_set_speed(sc, speed)
	struct com_softc *sc;
	int speed;
{
	struct ns16550_softc *ic = (void *) sc;
	register bus_space_tag_t iot = ic->ic_iot;
	register bus_space_handle_t ioh = ic->ic_ioh;

	speed = comns_speed(speed, sc->sc_freq);
	if (speed <= 0)
		return;

	bus_space_write_1(iot, ioh, com_lcr, ic->ic_lcr | LCR_DLAB);
	bus_space_write_1(iot, ioh, com_dlbl, speed);
	bus_space_write_1(iot, ioh, com_dlbh, speed >> 8);
	bus_space_write_1(iot, ioh, com_lcr, ic->ic_lcr);
}
