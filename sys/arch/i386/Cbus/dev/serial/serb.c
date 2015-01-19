/*	$NecBSD: serb.c,v 1.3 1999/07/23 05:39:10 kmatsuda Exp $	*/
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

#include "opt_serb.h"

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

#include "locators.h"

/****************************************************
 * Hardware definition
 ****************************************************/
/* local port index definitions */
#define	com_emr		com_msr 	/* RSB-2000 or 3000 only */

/* RSB-2000 or 3000 */
#define	RSB2000_FIFO_SIZE	0x400	/* 1kb */

/* hardware fifo control register */
#define	FIFO_TRIGGER_32		0x20
#define	FIFO_TRIGGER_48		0x60
#define	FIFO_TRIGGER_80		0xa0
#define	FIFO_TRIGGER_144	0xe0

#define	FIFO_FL_80		0x00
#define	FIFO_FL_144		0x08
#define	FIFO_FL_272		0x10
#define	FIFO_FL_528		0x18

/* hardware ext mode register */
#define	EMR_EXBUFF		0x04
#define	EMR_CTSFLW		0x08
#define	EMR_DSRFLW		0x10
#define	EMR_RTSFLW		0x20
#define	EMR_DTRFLW		0x40
#define	EMR_EFMODE		0x80

void serb_hw_attach __P((struct com_softc *));
int serb_hw_activate __P((struct com_softc *));
int serb_hw_deactivate __P((struct com_softc *, int));
void serb_hw_fifo __P((struct com_softc *sc, int));
int serb_hw_drain __P((struct com_softc *, int));
void serbstart __P((struct com_softc *, struct tty *));
int serbintr __P((void *));

#ifndef	SERB_SPEED
#define	SERB_SPEED TTYDEF_SPEED
#endif	/* !SERB_SPEED */

static struct com_switch serb_switch = { 
	  serb_hw_attach,
	  serb_hw_activate,
	  serb_hw_deactivate,
	  serb_hw_drain,
	  serb_hw_fifo,

	  serbstart,
	  serbintr,
	  IPL_TTY,

	  comnsioctl,
	  comnsparam,
	  comnsmodem,
	  comnserrmap,

	  SERB_SPEED,
};

static int serbmatch __P((struct device *, struct cfdata *, void *));
void serbattach __P((struct device *, struct device *, void *));

struct cfattach serb_ca = {
	sizeof(struct ns16550_softc), serbmatch, serbattach
};

extern struct cfdriver serb_cd;

/****************************************************
 * serb probe
 ****************************************************/
static int
serbmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct commulti_attach_args *ca = aux;
	bus_space_tag_t iot = ca->ca_h.cs_iot;
	bus_space_handle_t spioh = ca->ca_h.cs_spioh;
	bus_space_handle_t ioh = ca->ca_h.cs_ioh;

	if (ca->ca_type != COM_HW_SERB)
		return 0;

	if (cf->cf_loc[SERCF_SLAVE] != SERCF_SLAVE_DEFAULT &&
	    cf->cf_loc[SERCF_SLAVE] != ca->ca_slave)
		return 0;

	if (_comnsprobe(iot, ioh, spioh, ca->ca_hwflags) == 0)
		return 0;

	return SERB_PROBE_PRI;
}

void
serbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct commulti_attach_args *ca = aux;
	struct ns16550_softc *ic = (void *) self;

	printf("\n");

	ca->ca_m = self;
	ic->ic_com.sc_ser = (void *) parent;
	ic->ic_com.sc_cswp = &serb_switch;
	comns_setup_softc(ca, ic);
	config_found(self, ca, NULL);
}

/****************************************************
 * serb hardware attach
 ****************************************************/
void
serb_hw_attach(sc)
	struct com_softc *sc;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	printf(": Earth chip native ext mode (1024/1024 r/w fifo).\n");
	SET(sc->sc_hwflags, COM_HW_FIFO);

	bus_space_write_1(iot, ioh, com_fifo, 0);
	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);
}

/****************************************************
 * serb fifo control
 ****************************************************/
static void serb_clear_fifo __P((struct ns16550_softc *ic));

static void
serb_clear_fifo(ic)
	struct ns16550_softc *ic;
{
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	u_int8_t triger = FIFO_RCV_RST | FIFO_XMT_RST | ic->ic_triger;

	bus_space_write_1(iot, ioh, com_fifo, triger);
	(void) bus_space_read_1(iot, ioh, com_lsr);
	(void) bus_space_read_1(iot, ioh, com_data);
}

void
serb_hw_fifo(sc, clear)
	struct com_softc *sc;
	int clear;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	u_int8_t lcr, fcr;

	lcr = bus_space_read_1(iot, ioh, com_lcr);
	bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
	fcr = EMR_EXBUFF | EMR_EFMODE;
	if (ISSET(sc->sc_rtscts, CRTSCTS))
		SET(fcr, EMR_CTSFLW | EMR_RTSFLW);
	if (ISSET(sc->sc_rtscts, CDTRCTS))
		SET(fcr, EMR_CTSFLW | EMR_DTRFLW);
	bus_space_write_1(iot, ioh, com_emr, fcr);
	bus_space_write_1(iot, ioh, com_lcr, lcr);
	ic->ic_triger = FIFO_ENABLE | FIFO_FL_528 | FIFO_TRIGGER_144;

	if (clear)
		serb_clear_fifo(ic);
	else
		bus_space_write_1(iot, ioh, com_fifo, ic->ic_triger);
}

/****************************************************
 * serb activate
 ****************************************************/
int
serb_hw_activate(sc)
	struct com_softc *sc;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	ic->ic_mcr = MCR_DTR | MCR_RTS;
	serb_hw_fifo(sc, 1);

	if (ISSET(sc->sc_hwflags, COM_HW_NOIEN) == 0)
		SET(ic->ic_mcr, MCR_IENABLE);

	bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
	sc->sc_nmsr = sc->sc_msr = bus_space_read_1(iot, ioh, com_msr);

#ifdef	SERB_BLKINTR
	ic->ic_ier = IER_ERLS | IER_EMSC;
#else	/* !SERB_BLKINTR */
	ic->ic_ier = IER_ERLS | IER_EMSC | IER_ERXRDY;
#endif	/* !SERB_BLKINTR */
	bus_space_write_1(iot, ioh, com_ier, ic->ic_ier);

	return ISSET(sc->sc_msr, MSR_DCD);
}

int
serb_hw_deactivate(sc, hupctl)
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
 * serb hardware fifo drain
 ****************************************************/
int
serb_hw_drain(sc, flags)
	struct com_softc *sc;
	int flags;
{
	struct ns16550_softc *ic = (void *) sc;
	register u_int8_t lsr;

	if (flags)
	{
		serb_clear_fifo(ic);
	}
	else
	{
		if (sc->sc_ospeed <= 0 || !ISSET(sc->sc_msr, MSR_DSR))
			return -1;

		lsr = bus_space_read_1(ic->ic_iot, ic->ic_ioh, com_lsr);
		return ISSET(lsr, LSR_TSRE) ? 0 : 1;
 	}
	return 0;
}		

/****************************************************
 * serb start
 ****************************************************/
void
serbstart(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register int n, cc;
	register u_int8_t lsr;

	/* 
	 * If sc->sc_rtscts on, RSB2000 does automatic flow control.
	 */
	lsr = bus_space_read_1(iot, ioh, com_lsr);
	do
	{
		n = ISSET(lsr, LSR_TSRE) ? RSB2000_FIFO_SIZE: 16;
		cc = ndqb(&tp->t_outq, 0);
		if (cc > n)
			cc = n;
		bus_space_write_multi_1(iot, ioh, com_data, tp->t_outq.c_cf, cc);
		ndflush(&tp->t_outq, cc);
		if (tp->t_outq.c_cc == 0)
			break;

		delay(1);
		lsr = bus_space_read_1(iot, ioh, com_lsr);
	}
	while (ISSET(lsr, LSR_TXRDY));

	sc->sc_cc = sc->ob_cc = 0;
	EMUL_INTR_REQUIRED(sc);
}

/****************************************************
 * serb intr
 ****************************************************/
int
serbintr(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	struct v_ring_buf *rb;
	u_int8_t lsr;
	int tout;

	if (sc->sc_vicr == 0)
	{
		if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
			return 0;

		(void) bus_space_read_1(iot, ioh, com_msr);
		serb_clear_fifo(ic);
		return 0;
	}

 	rb = &sc->sc_invrb;
	for (tout = RSB2000_FIFO_SIZE * 2; tout > 0; tout --)
	{
		lsr = bus_space_read_1(iot, ioh, com_lsr);
		if (ISSET(lsr, LSR_RXRDY) != 0) 
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

		sc->sc_nmsr = bus_space_read_1(iot, ioh, com_msr);
		if (sc->sc_msr != sc->sc_nmsr)
			setsofttty();

		if (sc->ob_cc == 0)
		{
			lsr = bus_space_read_1(iot, ioh, com_lsr);
			if (ISSET(lsr, LSR_TXRDY))
				comns_xmit_terminate(sc);
		}

		if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
			return 0;
	}

	printf("%s too many loops", sc->sc_dev.dv_xname);
	serb_clear_fifo(ic);
	return 0;
}
