/*	$NecBSD: i8251subr.c,v 1.1 1998/12/31 02:33:01 honda Exp $	*/
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

static u_char tiocm_xxx2cmd __P((int));
/****************************************************
 * generic probe
 ****************************************************/
int
_serfprobe(iot, ioh, spioh, hwflags)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_space_handle_t spioh;
	u_int hwflags;
{
	u_int8_t stat, stat1, statd;
	int i;

	if (spioh == NULL)
		return 0;

	hwflags = COM_HW_ACCEL | ISSET(hwflags, COM_HW_EMM);
	i8251f_cr_write_1(iot, spioh, hwflags, serf_fcr, 0);
	delay(10);

	for (i = 0; i < 100; i ++)
	{
		stat = i8251f_cr_read_1(iot, spioh, hwflags, serf_fcr);
		if ((stat & 1) == 0)
		{
			stat = i8251f_cr_read_1(iot, spioh, hwflags, serf_lsr);
			i8251f_cr_write_1(iot, spioh,
					  hwflags, serf_fcr, FIFO_ENABLE);
			delay(100);
			stat1 = i8251f_cr_read_1(iot, spioh, hwflags, serf_lsr);
			statd = i8251f_cr_read_1(iot, spioh, hwflags, serf_div);
			i8251f_cr_write_1(iot, spioh, hwflags, serf_fcr, 0);
			delay(100);

			if (stat1 == stat)
				return 0;
			if (statd == (u_int8_t) -1)
				return COM_HW_I8251F_NDIV;
			return COM_HW_I8251F_EDIV;
		}
		delay(1);
	}

	return 0;
}

int
_seriprobe(iot, ioh, spioh, hwflags)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_space_handle_t spioh;
	u_int hwflags;
{
	register u_int8_t tmp;

	/*
	 * clear fifo advanced mode, because line status register has
	 * no response under the i8251F mode. 
	 * (consider the case after hot reboot from WIN95, etc ...)
	 */
	if (spioh != NULL)
	{
		hwflags = ISSET(hwflags, COM_HW_EMM);
		i8251f_cr_write_1(iot, spioh, hwflags, serf_fcr, 0);
	}
		
	i8251_reset(iot, ioh, I8251_DEF_MODE, 1);
	delay(100);

#ifdef	I8251_OLD_PROBE
	(void) bus_space_read_1(iot, ioh, seri_lsr);
	delay(100);
	tmp = bus_space_read_1(iot, ioh, seri_lsr);
	tmp = ISSET(tmp, STS8251_PE | STS8251_OE | STS8251_FE | STS8251_TxEMP);
	if (tmp != STS8251_TxEMP)
		return 0;
#else	/* I8251_OLD_PROBE */
	/* Disable transmit */
	bus_space_write_1(iot, ioh, seri_cmdr, CMD8251_DTR | CMD8251_RTS);
	delay(50);

	/* Check tx buffer empty */
	bus_space_write_1(iot, ioh, seri_cmdr, CMD8251_DTR | CMD8251_RTS);
	tmp = bus_space_read_1(iot, ioh, seri_lsr);
	if (ISSET(tmp, STS8251_TxRDY) == 0)
		return 0;

	/* Write 2 bytes */
	bus_space_write_1(iot, ioh, seri_data, ' ');
	delay(50);
	bus_space_write_1(iot, ioh, seri_data, ' ');
	delay(50);

	/* Check tx buffer non empty */
	tmp = bus_space_read_1(iot, ioh, seri_lsr);
	if (ISSET(tmp, STS8251_TxRDY) != 0)
		return 0;

	/* Clear tx buffer */
	i8251_reset(iot, ioh, I8251_DEF_MODE, 0);
	delay(50);
#endif	/* !I8251_OLD_PROBE */

	bus_space_write_1(iot, ioh, seri_cmdr, I8251_DEF_CMD);
	return 1;
}

void
i8251_setup_softc(ca, ic)
	struct commulti_attach_args *ca;
	struct i8251_softc *ic;
{

	ic->ic_iot = ca->ca_h.cs_iot;
	ic->ic_ioh = ca->ca_h.cs_ioh;
	ic->ic_spioh = ca->ca_h.cs_spioh;

	ic->ic_icrt = ca->ca_h.cs_icrt;
	ic->ic_icrh = ca->ca_h.cs_icrh;
	ic->ic_control_intr = ca->ca_h.cs_control_intr;

	ic->ic_msrt = ca->ca_h.cs_msrt;
	ic->ic_msrh = ca->ca_h.cs_msrh;
	ic->ic_read_msr = ca->ca_h.cs_read_msr;

	com_setup_softc(ca, &ic->ic_com);
}

/****************************************************
 * seri ioctl
 ****************************************************/
static u_char
tiocm_xxx2cmd(data)
	int data;
{
	u_char m = 0;

	if (ISSET(data, TIOCM_DTR))
		SET(m, CMD8251_DTR);
	if (ISSET(data, TIOCM_RTS))
		SET(m, CMD8251_RTS);
	return m;
}

int
i8251ioctl(sc, cmd, data, flag, p)
	struct com_softc *sc;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct i8251_softc *ic = (void *) sc;
	u_int8_t m;
	int s, bits;

	switch (cmd)
	{
	case TIOCSBRK:
		s = splcom();
		i8251_bset_cmdr(ic, CMD8251_SBRK);
		splx(s);
		break;

	case TIOCCBRK:
		s = splcom();
		i8251_bclr_cmdr(ic, CMD8251_SBRK);
		splx(s);
		break;

	case TIOCSDTR:
		s = splcom();
		i8251_bset_cmdr(ic, ic->ic_dtr);
		splx(s);
		break;

	case TIOCCDTR:
		s = splcom();
		i8251_bclr_cmdr(ic, ic->ic_dtr);
		splx(s);
		break;

	case TIOCMSET:
		m = tiocm_xxx2cmd(*(int *)data);
		s = splcom();
		CLR(ic->ic_cmdr, CMD8251_DTR | CMD8251_RTS);
		SET(ic->ic_cmdr, m);
		i8251_write_cmdr(ic, ic->ic_cmdr);
		splx(s);
		break;

	case TIOCMBIS:
		s = splcom();
		i8251_bset_cmdr(ic, tiocm_xxx2cmd(*(int *)data));
		splx(s);
		break;

	case TIOCMBIC:
		s = splcom();
		i8251_bclr_cmdr(ic, tiocm_xxx2cmd(*(int *)data));
		splx(s);
		break;

	case TIOCMGET:
		bits = 0;
		m = ic->ic_cmdr;
		if (ISSET(m, CMD8251_DTR))
			SET(bits, TIOCM_DTR);
		if (ISSET(m, CMD8251_RTS))
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
 * seri params
 ****************************************************/
int
i8251param(sc, tp, t)
	struct com_softc *sc;
	struct tty *tp;
	struct termios *t;
{
	struct i8251_softc *ic = (void *) sc;
	struct comspeed *spt;
	int ospeed, s, carrier;
	u_int8_t cmdr, lcr = 0;

	if (ISSET(sc->sc_cflags, COM_MODE_FIXED))
		comsetfake(sc, t, &ospeed);
	else
		ospeed = t->c_ospeed;

	spt = comfindspeed(sc->sc_speedtab, ospeed);
	if (spt->cs_div < 0 ||
	    (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return EINVAL;

	switch (ISSET(t->c_cflag, CSIZE))
	{
	case CS5:
		lcr = MOD8251_5BITS;
		break;
	case CS6:
		lcr = MOD8251_6BITS;
		break;
	case CS7:
		lcr = MOD8251_7BITS;
		break;
	case CS8:
		lcr = MOD8251_8BITS;
		break;
	}

	if (ISSET(t->c_cflag, PARENB))
	{
		SET(lcr, MOD8251_PENAB);
		if (!ISSET(t->c_cflag, PARODD))
			SET(lcr, MOD8251_PEVEN);
	}

	if (ISSET(t->c_cflag, CSTOPB))
		SET(lcr, MOD8251_STOPB);

	s = splcom();
	cmdr = ic->ic_cmdr;
	sc->sc_rtscts = ISSET(t->c_cflag, CHWFLOW);

	if (ISSET(t->c_cflag, CRTSCTS))
	{
		ic->ic_mcts = 0; /* automatic cts flow control hardware */
		ic->ic_mrts = CMD8251_RTS;
		ic->ic_dtr = CMD8251_DTR;
	}
	else if (ISSET(t->c_cflag, MDMBUF))
	{
		ic->ic_mcts = MSR_DCD;
		ic->ic_mrts = CMD8251_DTR;
		ic->ic_dtr = 0;
	}
	else
	{
		ic->ic_mcts = ic->ic_mrts = 0;
		ic->ic_dtr = CMD8251_DTR | CMD8251_RTS;
	}

	if (ospeed == 0)
	{
		sc->sc_ospeed = ospeed;
		CLR(cmdr, ic->ic_dtr);
	}
	else
	{
		if (sc->sc_ospeed != ospeed || ic->ic_lcr != lcr)
			i8251_init(ic, ospeed, ic->ic_cmdr, lcr);
		SET(cmdr, ic->ic_dtr);
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

	if (cmdr != ic->ic_cmdr)
	{
		ic->ic_cmdr = cmdr;
		i8251_write_cmdr(ic, cmdr);
	}

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	(void) (*linesw[tp->t_line].l_modem) (tp, carrier);
	splx(s);

	return 0;
}

/****************************************************
 * errmap
 ****************************************************/
u_int
i8251errmap(sc, c, stat)
	struct com_softc *sc;
	u_int c;
	u_int8_t stat;
{

#ifdef DDB
	if (ISSET(stat, LSR_BI) && 
	    ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
	{
		Debugger();
		/* remaining break characters in the fifo should be cleared */
		return RBFLUSH;
	}
#endif	/* DDB */

	if (ISSET(stat, (LSR_BI | LSR_FE)))
		SET(c, TTY_FE);
	if (ISSET(stat, LSR_PE))
		SET(c, TTY_PE);
	if (ISSET(stat, LSR_OE))
		comoerr(sc, COM_HOVF);
	return c;
}

/*****************************************************
 * Reset and Setup
 ****************************************************/
void
i8251_reset(iot, ioh, mode, flags)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t mode;
	int flags;
{

	if (flags != 0)
	{
		bus_space_write_1(iot, ioh, seri_cmdr, 0);
		delay(30);
		bus_space_write_1(iot, ioh, seri_cmdr, 0);
		delay(30);
		bus_space_write_1(iot, ioh, seri_cmdr, 0);
		delay(30);
	}

	bus_space_write_1(iot, ioh, seri_cmdr, CMD8251_RESET);
	delay(30);
	bus_space_write_1(iot, ioh, seri_modr, mode | MOD8251_BAUDR);
	delay(30);
}

void
i8251_init(ic, ospeed, cmd, mode)
	struct i8251_softc *ic;
	int ospeed;
	u_int8_t cmd;
	u_int8_t mode;
{
	struct com_softc *sc = &ic->ic_com;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	bus_space_handle_t spioh = ic->ic_spioh;
	int hwflags = sc->sc_hwflags;

	/* interrupt line disenable */
	(*ic->ic_control_intr) (sc, 0);
	if (ISSET(hwflags, COM_HW_ACCEL))
		i8251f_cr_write_1(iot, spioh, hwflags, serf_fcr, 0);

	/* setup serial clock */
	if (sc->sc_ospeed != ospeed)
	{
		(*sc->sc_speedfunc) (sc, ospeed);
		sc->sc_ospeed = ospeed;
	}

	/* reset and init */
	i8251_reset(iot, ioh, ic->ic_lcr = mode, 0);
	bus_space_write_1(iot, ioh, seri_cmdr, ic->ic_cmdr = cmd);

	if (ISSET(hwflags, COM_HW_ACCEL))
		i8251f_cr_write_1(iot, spioh, hwflags, serf_fcr, ic->ic_triger);

	/* recover interrupt line */
	(*ic->ic_control_intr) (sc, ic->ic_ier);
}
