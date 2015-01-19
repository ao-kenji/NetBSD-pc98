/*	$NecBSD: serf.c,v 1.3 1999/07/23 05:39:11 kmatsuda Exp $	*/
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
#include <i386/Cbus/dev/serial/sersubr.h>
#include <i386/Cbus/dev/serial/i8251subr.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif	/* KGDB */

#include "locators.h"

/**************************************************
 * I8251 Hardware definition
 **************************************************/
void serf_hw_attach __P((struct com_softc *));
int serf_hw_activate __P((struct com_softc *));
int serf_hw_deactivate __P((struct com_softc *, int));
int serf_hw_drain __P((struct com_softc *, int));
void serf_hw_fifo __P((struct com_softc *, int));

void serfstart __P((struct com_softc *, struct tty *));
int serfintr __P((void *));
static void serfmodem __P((struct com_softc *, struct tty *));

#ifndef	SERF_SPEED
#define	SERF_SPEED TTYDEF_SPEED
#endif	/* !SERF_SPEED */

struct com_switch serf_switch = {
	  serf_hw_attach,
	  serf_hw_activate,
	  serf_hw_deactivate,
	  serf_hw_drain,
	  serf_hw_fifo,

	  serfstart,
	  serfintr,
	  IPL_COM,

	  i8251ioctl,
	  i8251param,
	  serfmodem,
	  i8251errmap,

	  SERF_SPEED,
};

static int serfmatch __P((struct device *, struct cfdata *, void *));
void serfattach __P((struct device *, struct device *, void *));

struct cfattach serf_ca = {
	sizeof(struct i8251_softc), serfmatch, serfattach
};

extern struct cfdriver serf_cd;

/****************************************************
 * serf probe attach
 ****************************************************/
static int
serfmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct commulti_attach_args *ca = aux;
	bus_space_tag_t iot = ca->ca_h.cs_iot;
	bus_space_handle_t ioh = ca->ca_h.cs_ioh;
	bus_space_handle_t spioh = ca->ca_h.cs_spioh;

	if (ca->ca_type != COM_HW_I8251 && ca->ca_type != COM_HW_I8251_F)
		return 0;

	if (cf->cf_loc[SERCF_SLAVE] != SERCF_SLAVE_DEFAULT &&
	    cf->cf_loc[SERCF_SLAVE] != ca->ca_slave)
		return 0;

	if (_seriprobe(iot, ioh, spioh, ca->ca_hwflags) == 0)
		return 0;

	if (_serfprobe(iot, ioh, spioh, ca->ca_hwflags) ==  0)
		return 0;

	return SERF_PROBE_PRI;	/* higher pri */
}

void
serfattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct commulti_attach_args *ca = aux;
	struct i8251_softc *ic = (void *) self;

	printf("\n");
	ca->ca_m = self;
	ic->ic_com.sc_ser = (void *) parent;
	ic->ic_com.sc_cswp = &serf_switch;
	SET(ic->ic_com.sc_hwflags, COM_HW_ACCEL);
	i8251_setup_softc(ca, ic);
	config_found(self, ca, NULL);
#ifdef	KGDB
	comkgdb_attach(&ic->ic_com);
#endif	/* KGDB */
}

/****************************************************
 * serf hardware attach
 ****************************************************/
void
serf_hw_attach(sc)
	struct com_softc *sc;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	int rv;

	ic->ic_ier = 0;
	(*ic->ic_control_intr) (sc, ic->ic_ier);

	rv = _serfprobe(iot, ioh, ic->ic_spioh, sc->sc_hwflags);
	sc->sc_maxputs = SERF_FIFO_HIWATER;
	ic->ic_maxspeed = (rv == COM_HW_I8251F_EDIV) ? 115200 : 76800;

	serf_hw_fifo(sc, I8251F_FALLCLR);
	i8251_init(ic, sc->sc_termios.c_ospeed, I8251_DEF_CMD, I8251_DEF_MODE);
	i8251_bset_cmdr(ic, CMD8251_DTR | CMD8251_RTS);

	printf (": I8251F fifo enable(16) max %d bps\n", ic->ic_maxspeed);
}

/****************************************************
 * serf (de) activate
 ****************************************************/
void
serf_hw_fifo(sc, flags)
	struct com_softc *sc;
	int flags;
{
	struct i8251_softc *ic = (void *) sc;
	register u_int8_t trig;

	SET(ic->ic_triger, FIFO_ENABLE | I8251F_FIFO_TRIG);
	trig = ic->ic_triger;

	if (ISSET(flags, I8251F_FCLR))
		SET(trig, FIFO_RCV_RST);	
	if (ISSET(flags, I8251F_FXCLR))
		SET(trig, FIFO_XMT_RST);	
	i8251f_cr_write_1(ic->ic_iot, ic->ic_spioh, sc->sc_hwflags,
			  serf_fcr, trig);
}

int
serf_hw_activate(sc)
	struct com_softc *sc;
{
	struct i8251_softc *ic = (void *) sc;
	register bus_space_tag_t iot = ic->ic_iot;
	register bus_space_handle_t spioh = ic->ic_spioh;

	/* block rw intr */
	ic->ic_ier = 0;
	(*ic->ic_control_intr) (sc, ic->ic_ier);

	/* block fifo status intr */
	CLR(ic->ic_triger, I8251F_FIFO_INTR);
	serf_hw_fifo(sc, I8251F_FALLCLR);

	/* init cmd register */
	i8251_bset_cmdr(ic, I8251_DEF_CMD);

	sc->sc_msr = i8251f_cr_read_1(iot, spioh, sc->sc_hwflags, serf_msr);
	sc->sc_nmsr = sc->sc_msr;

	/* allow rec intr */
	i8251_bset_icr(ic, I8251_RxINTR_ENABLE);

	/* allow fifo status intr */
	SET(ic->ic_triger, I8251F_FIFO_INTR);
	serf_hw_fifo(sc, 0);

	return ISSET(sc->sc_msr, MSR_DCD);
}

int
serf_hw_deactivate(sc, hupctl)
	struct com_softc *sc;
	int hupctl;
{
	struct i8251_softc *ic = (void *) sc;

	CLR(ic->ic_triger, I8251F_FIFO_INTR);
	serf_hw_fifo(sc, 0);

	i8251_bclr_cmdr(ic, CMD8251_SBRK);

	ic->ic_ier = 0;
	(*ic->ic_control_intr) (sc, ic->ic_ier);

	if (hupctl != 0)
		i8251_bclr_cmdr(ic, CMD8251_DTR | CMD8251_RTS);
	return 0;
}

/****************************************************
 * serf drain
 ****************************************************/
int
serf_hw_drain(sc, flags)
	struct com_softc *sc;
	int flags;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t spioh = ic->ic_spioh;
	int sp, cnt;
	u_int8_t lsr;

	if (flags)
	{
		sp = splcom();
		serf_hw_fifo(sc, I8251F_FALLCLR);
		splx(sp);
	}
	else 
	{
		if (sc->sc_ospeed <= 0 || !ISSET(sc->sc_msr, MSR_DSR))
			return -1;

		sp = splcom();
		if ((cnt = sc->ob_cc) < 0)
			cnt = 0;
		lsr = i8251f_cr_read_1(iot, spioh, sc->sc_hwflags, serf_lsr);
		if (ISSET(lsr, FLSR_TXE) == 0)
				cnt += 16;
		splx(sp);
		return cnt;
	}
	return 0;
}

/*****************************************************
 * modem status
 ****************************************************/
static void
serfmodem(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct i8251_softc *ic = (void *) sc;

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
serfstart(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t spioh = ic->ic_spioh;
	register int oc, n;
	int s;

	sc->ob_cp = tp->t_outq.c_cf;
	n = sc->sc_cc = ndqb(&tp->t_outq, 0);
	oc = sc->sc_maxputs;

	s = splcom();
	if (ISSET(sc->sc_nmsr, ic->ic_mcts) == ic->ic_mcts)
	{
		do
		{
			i8251f_cr_write_1(iot, spioh, sc->sc_hwflags,
					  serf_data, *sc->ob_cp++);
			if (-- n == 0)
				break;
		}
		while (-- oc > 0);

		i8251_bset_icr(ic, I8251_TxINTR_ENABLE);
	}
	sc->ob_cc = n;
	splx(s);
}

/****************************************************
 * intr
 ****************************************************/
int
serfintr(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	struct i8251_softc *ic = arg;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t spioh = ic->ic_spioh;
	struct v_ring_buf *rb;
	u_int8_t lsr, iir;
	int tout;

	if (sc->sc_vicr == 0)
	{
#ifdef	KGDB
		lsr = i8251f_cr_read_1(iot, spioh, sc->sc_hwflags, serf_lsr);
		if (ISSET(sc->sc_hwflags, COM_HW_KGDB) && 
		    ISSET(lsr, FLSR_RXRDY))
			kgdb_connect(1);
#endif	/* KGDB */

		serf_hw_fifo(sc, I8251F_FALLCLR);
		(void) i8251f_cr_read_1(iot, spioh, sc->sc_hwflags, serf_msr);
		return 0;
	}

	rb = &sc->sc_invrb;
	for (tout = COM_IBUFSIZE; tout > 0; tout --)
	{
		lsr = i8251f_cr_read_1(iot, spioh, sc->sc_hwflags, serf_lsr);
		if (ISSET(lsr, FLSR_RXRDY))
		do
		{
			register u_int8_t c;

			c = i8251f_cr_read_1(iot, spioh,
					     sc->sc_hwflags, serf_data);
			if (ISSET(lsr, FLSR_RCV_ERR))
				i8251_write_cmdr(ic, CMD8251_ER | ic->ic_cmdr);
			rb_input(rb, sc->sc_rdata, sc->sc_rstat, c,
				 ISSET(lsr, FLSR_RCV_ERR));
			if (rb_highwater(rb))
			{
				i8251_rtsoff(ic);
				SERF_DEBUG("off");
			}
			lsr = i8251f_cr_read_1(iot, spioh,
					       sc->sc_hwflags, serf_lsr);
		}
		while (ISSET(lsr, FLSR_RXRDY) && tout -- > 0);

		sc->sc_nmsr = i8251f_cr_read_1(iot, spioh,
					       sc->sc_hwflags, serf_msr);
		if (sc->sc_nmsr != sc->sc_msr)
			setsofttty();

		if (sc->ob_cc < 0)
			goto stop;

		lsr = i8251f_cr_read_1(iot, spioh, sc->sc_hwflags, serf_lsr);
		if (ISSET(lsr, FLSR_TXRDY) == 0)
			goto done;

		if (sc->ob_cc > 0)
		{
			register int oc;

			if (ISSET(sc->sc_nmsr, ic->ic_mcts) != ic->ic_mcts)
				goto stop;

 			oc = sc->sc_maxputs;
			do
			{
				i8251f_cr_write_1(iot, spioh, sc->sc_hwflags,
						  serf_data, *sc->ob_cp++);
				if (-- sc->ob_cc == 0)
					break;
			}
			while (-- oc > 0);

			if (ISSET(ic->ic_ier, I8251_TxINTR_ENABLE) == 0)
			{
				i8251_bset_icr(ic, I8251_TxINTR_ENABLE);
			}
		}
		else
		{
			i8251_xmit_terminate(sc);
stop:
			if (ISSET(ic->ic_ier, I8251_TxINTR_ENABLE) != 0)
			{
				i8251_bclr_icr(ic, I8251_TxINTR_ENABLE);
			}
		}

done:
		iir = i8251f_cr_read_1(iot, spioh, sc->sc_hwflags, serf_iir);
		if (ISSET(iir, IIR_NOPEND))
			return 1;
	}

	printf("%s: too many loops iir(0x%x)\n",
		sc->sc_dev.dv_xname, (u_int) iir);
	return 1;
}
