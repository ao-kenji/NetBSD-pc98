/*	$NecBSD: sern.c,v 1.3 1999/07/23 05:39:12 kmatsuda Exp $	*/
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
#include <i386/Cbus/dev/serial/sersubr.h>
#include <i386/Cbus/dev/serial/ns165subr.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif	/* KGDB */

#include "locators.h"

/****************************************************
 * NS16550 Hardware definition
 ****************************************************/
static void sern_check_fifo __P((struct ns16550_softc *));
static void sern_clear_fifo __P((struct ns16550_softc *));

void sern_hw_attach __P((struct com_softc *));
int sern_hw_activate __P((struct com_softc *));
int sern_hw_deactivate __P((struct com_softc *, int));
int sern_hw_drain __P((struct com_softc *, int));
void sern_hw_fifo __P((struct com_softc *sc, int));
void sernstart __P((struct com_softc *, struct tty *));
int sernintr __P((void *));

#ifndef	SERN_SPEED
#define	SERN_SPEED TTYDEF_SPEED
#endif	/* !SERN_SPEED */

struct com_switch sern_switch = { 
	  sern_hw_attach,
	  sern_hw_activate,
	  sern_hw_deactivate,
	  sern_hw_drain,
	  sern_hw_fifo,

	  sernstart,
	  sernintr,
	  IPL_COM,

	  comnsioctl,
	  comnsparam,
	  comnsmodem,
	  comnserrmap,

	  SERN_SPEED,
};

static int sernmatch __P((struct device *, struct cfdata *, void *));
void sernattach __P((struct device *, struct device *, void *));

struct cfattach sern_ca = {
	sizeof(struct ns16550_softc), sernmatch, sernattach
};

extern struct cfdriver sern_cd;

static int
sernmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct commulti_attach_args *ca = aux;
	bus_space_tag_t iot = ca->ca_h.cs_iot;
	bus_space_handle_t ioh = ca->ca_h.cs_ioh;
	bus_space_handle_t spioh = ca->ca_h.cs_spioh;

	if (ca->ca_type != COM_HW_NS16550)
		return 0;

	if (cf->cf_loc[SERCF_SLAVE] != SERCF_SLAVE_DEFAULT &&
	    cf->cf_loc[SERCF_SLAVE] != ca->ca_slave)
		return 0;

	if (ca->ca_probepri == SER_PRI_PROBEHW && 
	    _comnsprobe(iot, ioh, spioh, ca->ca_hwflags) == 0)
		return 0;

	return SERN_PROBE_PRI;
}

void
sernattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct commulti_attach_args *ca = aux;
	struct ns16550_softc *ic = (void *) self;

	printf("\n");
	ic->ic_com.sc_cswp = &sern_switch;
	ic->ic_com.sc_ser = (void *) parent;
	ca->ca_m = self;
	comns_setup_softc(ca, ic);
	config_found(self, ca, NULL);

#ifdef	KGDB
	comkgdb_attach(&ic->ic_com);
#endif	/* KGDB */
}

/****************************************************
 * sern fifo check
 ****************************************************/
static void
sern_check_fifo(ic)
	struct ns16550_softc *ic;
{
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register u_int8_t val;
	u_char *s;

	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE | FIFO_RCV_RST |
					      FIFO_XMT_RST | FIFO_TRIGGER_14);
	delay(100);
	val = bus_space_read_1(iot, ioh, com_iir);
	if (ISSET(val, IIR_FIFO_MASK) == IIR_FIFO_MASK)
	{
		val = bus_space_read_1(iot, ioh, com_fifo);
		if (ISSET(val, FIFO_TRIGGER_14) == FIFO_TRIGGER_14)
		{
			SET(ic->ic_com.sc_hwflags, COM_HW_FIFO);
			s = ": ns16550a, working";
		}
		else
			s = ": ns16550, broken";
	}
	else
		s = ": ns8250 or ns16450, no";

	printf("%s fifo\n", s);
}

/****************************************************
 * sern open
 ****************************************************/
void
sern_hw_attach(sc)
	struct com_softc *sc;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	sern_check_fifo(ic);
	bus_space_write_1(iot, ioh, com_fifo, 0);
	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);
}

/****************************************************
 * sern fifo control
 ****************************************************/
static void
sern_clear_fifo(ic)
	struct ns16550_softc *ic;
{
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	u_int8_t triger = FIFO_RCV_RST | FIFO_XMT_RST | ic->ic_triger;

	if (ISSET(ic->ic_com.sc_hwflags, COM_HW_FIFO))
		bus_space_write_1(iot, ioh, com_fifo, triger);
	(void) bus_space_read_1(iot, ioh, com_lsr);
	(void) bus_space_read_1(iot, ioh, com_data);
}

void
sern_hw_fifo(sc, clear)
	struct com_softc *sc;
	int clear;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	if (!ISSET(sc->sc_hwflags, COM_HW_FIFO))
	{
		sc->sc_maxputs = 1;
		ic->ic_triger = 0;
		return;
	}
		
	sc->sc_maxputs = SERN_FIFO_HIWATER;
	ic->ic_triger = FIFO_ENABLE | NS165_FTL;
	if (ISSET(sc->sc_hwflags, COM_HW_AFE))
	{
		if (ISSET(sc->sc_rtscts, CRTSCTS))
			SET(ic->ic_mcr, MCR_AFE);
		else
			CLR(ic->ic_mcr, MCR_AFE);
	}

	if (clear != 0)
		sern_clear_fifo(ic);
	else
		bus_space_write_1(iot, ioh, com_fifo, ic->ic_triger);
}

/****************************************************
 * sern hardware (de) activate
 ****************************************************/
int
sern_hw_activate(sc)
	struct com_softc *sc;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	ic->ic_mcr = MCR_DTR | MCR_RTS;
	sern_hw_fifo(sc, 1);
	if (ISSET(sc->sc_hwflags, COM_HW_NOIEN) == 0)
		SET(ic->ic_mcr, MCR_IENABLE);
	bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);

	sc->sc_nmsr = sc->sc_msr = bus_space_read_1(iot, ioh, com_msr);

	ic->ic_ier = IER_ERLS | IER_EMSC | IER_ERXRDY;
	bus_space_write_1(iot, ioh, com_ier, ic->ic_ier);

	return ISSET(sc->sc_msr, MSR_DCD);
}

int
sern_hw_deactivate(sc, hupctl)
	struct com_softc *sc;
	int hupctl;
{
	struct ns16550_softc *ic = (void *) sc;
	register bus_space_tag_t iot = ic->ic_iot;
	register bus_space_handle_t ioh = ic->ic_ioh;

	CLR(ic->ic_lcr, LCR_SBREAK);
	bus_space_write_1(iot, ioh, com_lcr, ic->ic_lcr);
	ic->ic_ier = 0;
	bus_space_write_1(iot, ioh, com_ier, ic->ic_ier);

	if (hupctl != 0)
		bus_space_write_1(iot, ioh, com_mcr, 0);

	return 0;
}

/****************************************************
 * sern hardware fifo drain
 ****************************************************/
int
sern_hw_drain(sc, flags)
	struct com_softc *sc;
	int flags;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register u_int8_t lsr;
	int s;

	if (flags)
	{
		s = splcom();
		sern_clear_fifo(ic);
		splx(s);
	}
	else
	{
		if (sc->sc_ospeed <= 0 || !ISSET(sc->sc_msr, MSR_DSR))
			return -1;

		s = splcom();
		if (sc->ob_cc > 0)
		{
			splx(s);
			return sc->ob_cc;
		}	
		splx(s);

		lsr = bus_space_read_1(iot, ioh, com_lsr);
		return ISSET(lsr, LSR_TSRE) ? 0 : 1;
 	}
	return 0;
}		

/****************************************************
 * sern start output
 ****************************************************/
void
sernstart(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register int oc, n;
	int s;

	/*
	 * To block stray interrupt, we use a little strange trick.
	 * The main reason of stray interrupts is a race between
	 * fifo empty interrupt requests and data put operations,
	 * Thus we should keep IER_ETXRDY bit off. 
	 */
	sc->ob_cp = tp->t_outq.c_cf;
	n = sc->sc_cc = ndqb(&tp->t_outq, 0);
	oc = sc->sc_maxputs;

	s = splcom();
	if (ISSET(sc->sc_nmsr, ic->ic_mcts) == ic->ic_mcts)
	{
		do
		{
			bus_space_write_1(iot, ioh, com_data, *sc->ob_cp++);
			if (-- n == 0)
				break;
		}
		while (-- oc > 0);

		SET(ic->ic_ier, IER_ETXRDY);
		bus_space_write_1(iot, ioh, com_ier, ic->ic_ier);
	}
	sc->ob_cc = n;
	splx(s);
}

/****************************************************
 * sern intr
 ****************************************************/
#ifndef	SERN_POLLING_TIMEOUT
#define	SERN_POLLING_TIMEOUT	20
#endif	/* SERN_POLLING_TIMEOUT */

int
sernintr(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	struct ns16550_softc *ic = arg;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	struct v_ring_buf *rb;
	register u_int8_t lsr, iir;
	int tout;

	/*
	 * Step I. Check the interrupt status of our target.
	 *	   The routines called from a watch dog if sc_check == 1.
	 */
	iir = bus_space_read_1(iot, ioh, com_iir);
	if (sc->sc_check == 0)
	{
		if (ISSET(iir, IIR_NOPEND))
			return 0;

		if (sc->sc_vicr == 0)
		{
#ifdef	KGDB
			lsr = bus_space_read_1(iot, ioh, com_lsr);
			if (ISSET(sc->sc_hwflags, COM_HW_KGDB) &&
			    ISSET(lsr, LSR_RXRDY))
				kgdb_connect(1);
#endif	/* KGDB */

			(void) bus_space_read_1(iot, ioh, com_msr);
			sern_clear_fifo(ic);
			return 0;
		}
	}
	else 
	{
		if (iir == (u_int8_t) -1)
			return 0;

		sc->sc_check = 0;
	}

	/*
	 * Step II. do real processes.
	 */
 	rb = &sc->sc_invrb;
	for (tout = COM_IBUFSIZE * SERN_POLLING_TIMEOUT; tout > 0; tout --)
	{
		/*
		 * Step IIa. receive data.
		 */
		lsr = bus_space_read_1(iot, ioh, com_lsr);
		if (ISSET(lsr, LSR_RXRDY))
		do
		{
			register u_int8_t data;

			data = bus_space_read_1(iot, ioh, com_data);
			rb_input(rb, sc->sc_rdata, sc->sc_rstat, data,
				 ISSET(lsr, LSR_RCV_ERR));
			if (rb_highwater(rb))
			{
				comns_rtsoff(ic);
				COMNS_DEBUG("off");
			}
			lsr = bus_space_read_1(iot, ioh, com_lsr);
		}
		while (ISSET(lsr, LSR_RXRDY) && tout -- > 0);

		/*
		 * Step IIb. check modem status.
		 */
		sc->sc_nmsr = bus_space_read_1(iot, ioh, com_msr);
		if (sc->sc_msr != sc->sc_nmsr)
			setsofttty();

		/*
		 * Step IIc. put data in the send queue.
		 */
		if (sc->ob_cc < 0)
			goto stop;

		lsr = bus_space_read_1(iot, ioh, com_lsr);
		if (ISSET(lsr, LSR_TXRDY) == 0)
			goto done;

		if (sc->ob_cc > 0)
		{
			register int oc;

			if (ISSET(sc->sc_nmsr, ic->ic_mcts) != ic->ic_mcts)
				goto stop;

			if (ISSET(ic->ic_ier, IER_ETXRDY) == 0)
			{
				SET(ic->ic_ier, IER_ETXRDY);
				bus_space_write_1(iot, ioh, com_ier,
						  ic->ic_ier);
			}
				
 			oc = sc->sc_maxputs;
			do
			{
				bus_space_write_1(iot, ioh, com_data,
						  *sc->ob_cp++);
				if (-- sc->ob_cc == 0)
					break;
			}
			while (-- oc > 0);
		}
		else 
		{
			comns_xmit_terminate(sc);

stop:
			if (ISSET(ic->ic_ier, IER_ETXRDY))
				bus_space_write_1(iot, ioh, com_ier, 
						  CLR(ic->ic_ier, IER_ETXRDY));
		}

		/*
		 * Step IId. done 
		 */
done:
		if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
			return 1;
	}

	printf("%s too many loops\n", sc->sc_dev.dv_xname);
	printf("%s intr (0x%x)\n", sc->sc_dev.dv_xname,
	 	(u_int) bus_space_read_1(iot, ioh, com_iir));
	return 1;
}
