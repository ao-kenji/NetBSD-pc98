/*	$NecBSD: sera.c,v 1.5 1999/07/23 05:39:10 kmatsuda Exp $	*/
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

#define	SERA_DEBUG(msg)

#include "opt_sera.h"

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
#include <i386/Cbus/dev/serial/servar.h>
#include <i386/Cbus/dev/serial/sersubr.h>
#include <i386/Cbus/dev/serial/ns165subr.h>

#include "locators.h"

/****************************************************
 * NS16550 Hardware definition
 ****************************************************/
/* local port index definitions */
#define	com_data_write	8		/* RSA98 I/II/III only */
#define	com_fsr		9
#define	com_data_read	10
#define	com_frr		11
#define	com_fmsc	12
#define	com_fic		13

#define	com_tir		14
#define	com_tsr		15
#define	com_tstr	15

#define	IS_RSA98(sc) ((sc)->sc_subtype == COM_HW_RSA98)

#define	RSA98_TIMEOUT_MAX	(1024 * 80)	/* 100 m sec max */
#define	RSA98_TIMEOUT_MIN	(1024 * 3)	/* 3.6 m sec */
#define	RSA98_TIMEOUT_LOW	(1024 * 10)	/* 12 m sec */
#define	RSA98_TIMEOUT_INC	1

/*  RSA98 I/II/III */
#define	RSA98_FIFO_SIZE		0x800	/* 2Kb */
#define	RSA98_FRAG_SIZE		0x100	/* 256 */

/* hardware fifo status register */
#define	RSA98_SFR_SEMPTY	0x01
#define	RSA98_SFR_SFULL		0x02
#define	RSA98_SFR_SHALF		0x04
#define	RSA98_SFR_SAFAE		0x08
#define	RSA98_SFR_SMAKS		0x0f
#define	RSA98_SFR_REMPTY	0x10
#define	RSA98_SFR_RFULL		0x20
#define	RSA98_SFR_RHALF		0x40
#define	RSA98_SFR_RAFAE		0x80
#define	RSA98_SFR_EFM		(RSA98_SFR_SEMPTY | RSA98_SFR_SHALF | RSA98_SFR_SAFAE)

#define	RSA98c_SFR_SEMPTY	0x01
#define	RSA98c_SFR_SHALF	0x02
#define	RSA98c_SFR_SFULL	0x04
#define	RSA98c_SFR_REMPTY	0x08
#define	RSA98c_SFR_RHALF	0x10
#define	RSA98c_SFR_RFULL	0x20
#define	RSA98c_SFR_EFM		(RSA98_SFR_SEMPTY | RSA98_SFR_SHALF)

/* hardware fifo reset register */
#define	RSA98_FRR_S		0x01
#define	RSA98_FRR_R		0x02

/* hardware mode select register */
#define	RSA98c_FMSC_EFEN	0x04
#define	RSA98c_FMSC_RTSCTS	0x08

/* hardware interrupt register */
#define	RSA98c_FIC_RFH		0x01
#define	RSA98c_FIC_SFE		0x04
#define	RSA98c_FIC_RCT		0x08
#define	RSA98c_FIC_HT		0x10

/* hardware timer register */
#define	RSA98_TSR_DIS		0x02
#define	RSA98c_TSR_DIS		0x00
#define	RSA98c_TSR_EN		0x01

/* hardware timer status register */
#define	TSTR_STATUS_MASK	0x01
#define	TSTR_OPEN		0x01

/* useful macro */
#define	RSA98_SF_AF(regv) 	(((regv) & (RSA98_SFR_SHALF | RSA98_SFR_SAFAE)) == (RSA98_SFR_SHALF | RSA98_SFR_SAFAE))
#define	RSA98_SF_NAF(regv) 	(((regv) & (RSA98_SFR_SHALF | RSA98_SFR_SAFAE)) != (RSA98_SFR_SHALF | RSA98_SFR_SAFAE))
#define	RSA98_SF_AE(regv) 	(((regv) & (RSA98_SFR_SHALF | RSA98_SFR_SAFAE)) == (RSA98_SFR_SAFAE))
#define	RSA98_SF_NAE(regv) 	(((regv) & (RSA98_SFR_SHALF | RSA98_SFR_SAFAE)) != (RSA98_SFR_SAFAE))
#define	RSA98_RF_AF(regv) 	(((regv) & (RSA98_SFR_RHALF | RSA98_SFR_RAFAE)) == (RSA98_SFR_RHALF | RSA98_SFR_RAFAE))
#define	RSA98_RF_AE(regv) 	(((regv) & (RSA98_SFR_RHALF | RSA98_SFR_RAFAE)) == (RSA98_SFR_RAFAE))
#define	RSA98_RF_NAE(regv) 	(((regv) & (RSA98_SFR_RHALF | RSA98_SFR_RAFAE)) != (RSA98_SFR_RAFAE))
#define	RSA98_SF_E(regv)		(((regv) & RSA98_SFR_SEMPTY) == 0)
#define	RSA98_SF_NE(regv)	(((regv) & RSA98_SFR_SEMPTY))
#define	RSA98_SF_F(regv)		(((regv) & RSA98_SFR_SFULL) == 0)
#define	RSA98_SF_NF(regv)	(((regv) & RSA98_SFR_SFULL))
#define	RSA98_SF_HF(regv)	(((regv) & RSA98_SFR_SHALF))
#define	RSA98_SF_NHF(regv)	(((regv) & RSA98_SFR_SHALF) == 0)

#define	RSA98_RF_E(regv)		(((regv) & RSA98_SFR_REMPTY) == 0)
#define	RSA98_RF_F(regv)		(((regv) & RSA98_SFR_RFULL) == 0)
#define	RSA98_RF_NE(regv)	(((regv) & RSA98_SFR_REMPTY))
#define	RSA98_RF_HF(regv)	(((regv) & RSA98_SFR_RHALF))
#define	RSA98_RF_NHF(regv)	(((regv) & RSA98_SFR_RHALF) == 0)

#define	RSA98c_SF_E(regv)	(((regv) & RSA98c_SFR_SEMPTY) == 0)
#define	RSA98c_SF_NE(regv)	((regv) & RSA98c_SFR_SEMPTY)
#define	RSA98c_SF_F(regv)	(((regv) & RSA98c_SFR_SFULL) == 0)
#define	RSA98c_SF_NF(regv)	((regv) & RSA98c_SFR_SFULL)
#define	RSA98c_SF_HF(regv)	(((regv) & RSA98c_SFR_SHALF) == 0)
#define	RSA98c_SF_NHF(regv)	((regv) & RSA98c_SFR_SHALF)

#define	RSA98c_RF_E(regv)	(((regv) & RSA98c_SFR_REMPTY) == 0)
#define	RSA98c_RF_NE(regv)	((regv) & RSA98c_SFR_REMPTY)
#define	RSA98c_RF_F(regv)	(((regv) & RSA98c_SFR_RFULL) == 0)
#define	RSA98c_RF_NF(regv)	((regv) & RSA98c_SFR_RFULL)
#define	RSA98c_RF_HF(regv)	(((regv) & RSA98c_SFR_RHALF) == 0)
#define	RSA98c_RF_NHF(regv)	((regv) & RSA98c_SFR_RHALF) 

static void seratimerstart __P((struct ser_softc *));
static void seratimerstop __P((struct ser_softc *));
static void seraIIItimerstart __P((struct ser_softc *));
static void seraIIItimerstop __P((struct ser_softc *));

void sera_hw_attach __P((struct com_softc *));
int sera_hw_activate __P((struct com_softc *));
int sera_hw_deactivate __P((struct com_softc *, int));
int sera_hw_drain __P((struct com_softc *, int));
void sera_hw_fifo __P((struct com_softc *sc, int));
void serastart __P((struct com_softc *, struct tty *));
int seraintr __P((void *));

#ifndef	SERA_SPEED
#define	SERA_SPEED TTYDEF_SPEED
#endif	/* !SERA_SPEED */

static struct com_switch sera_switch = { 
	  sera_hw_attach,
	  sera_hw_activate,
	  sera_hw_deactivate,
	  sera_hw_drain,
	  sera_hw_fifo,

	  serastart,
	  seraintr,
	  IPL_TTY,

	  comnsioctl,
	  comnsparam,
	  comnsmodem,
	  comnserrmap,

	  SERA_SPEED,
};

static int seramatch __P((struct device *, struct cfdata *, void *));
void seraattach __P((struct device *, struct device *, void *));

struct cfattach sera_ca = {
	sizeof(struct ns16550_softc), seramatch, seraattach
};

extern struct cfdriver sera_cd;

/****************************************************
 * sera probe
 ****************************************************/
static int
seramatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct commulti_attach_args *ca = aux;
	bus_space_tag_t iot = ca->ca_h.cs_iot;
	bus_space_handle_t ioh = ca->ca_h.cs_ioh;
	bus_space_handle_t spioh = ca->ca_h.cs_spioh;

	if (ca->ca_type != COM_HW_SERA)
		return 0;

	if (cf->cf_loc[SERCF_SLAVE] != SERCF_SLAVE_DEFAULT &&
	    cf->cf_loc[SERCF_SLAVE] != ca->ca_slave)
		return 0;

	if (ca->ca_probepri == SER_PRI_PROBEHW && 
	    _comnsprobe(iot, ioh, spioh, ca->ca_hwflags) == 0)
		return 0;

	return SERA_PROBE_PRI;
}

void
seraattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct commulti_attach_args *ca = aux;
	struct ns16550_softc *ic = (void *) self;
	struct com_softc *sc = &ic->ic_com;

	printf("\n");
	sc->sc_ser = (void *) parent;
	sc->sc_cswp = &sera_switch;
	ca->ca_m = self;
	comns_setup_softc(ca, ic);

	if (IS_RSA98(sc) != 0)
	{
		sc->sc_ser->sc_start = seratimerstart;
		sc->sc_ser->sc_stop = seratimerstop;
	}
	else
	{
		sc->sc_ser->sc_start = seraIIItimerstart;
		sc->sc_ser->sc_stop = seraIIItimerstop;
	}

	config_found(self, ca, NULL);
}

/****************************************************
 * sera hardware attach
 ****************************************************/
void
sera_hw_attach(sc)
	struct com_softc *sc;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register u_int8_t clr;

	printf(": RSA I/II/III native mode (2048/2048 r/w fifo).\n");
	SET(sc->sc_hwflags, COM_HW_FIFO);

	clr = IS_RSA98(sc) ? RSA98_TSR_DIS : RSA98c_TSR_DIS;
	bus_space_write_1(iot, ioh, com_tsr, clr);

	bus_space_write_1(iot, ioh, com_fifo, 0);
	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);
}

/****************************************************
 * sera fifo control
 ****************************************************/
static void sera_clear_fifo __P((struct ns16550_softc *));

static void
sera_clear_fifo(ic)
	struct ns16550_softc *ic;
{
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	bus_space_write_1(iot, ioh, com_frr, 0);
	(void) bus_space_read_1(iot, ioh, com_lsr);
	(void) bus_space_read_1(iot, ioh, com_data);
}

void
sera_hw_fifo(sc, clear)
	struct com_softc *sc;
	int clear;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	if (IS_RSA98(sc) == 0)
	{
		register u_int8_t fcr;

	        fcr = RSA98c_FMSC_EFEN;
		if (ISSET(sc->sc_rtscts, CRTSCTS))
			SET(fcr, RSA98c_FMSC_RTSCTS);
		bus_space_write_1(iot, ioh, com_fmsc, fcr);
	}
	ic->ic_triger = 0;

	if (clear)
		sera_clear_fifo(ic);
	else
		bus_space_write_1(iot, ioh, com_fifo, ic->ic_triger);
}

/****************************************************
 * sera fifo drain
 ****************************************************/
int
sera_hw_drain(sc, flags)
	struct com_softc *sc;
	int flags;
{
	struct ns16550_softc *ic = (void *) sc;
	register u_int8_t lsr;

	if (flags)
	{
		sera_clear_fifo(ic);
	}
	else
	{
		if (sc->sc_ospeed <= 0 || !ISSET(sc->sc_msr, MSR_DSR))
			return -1;

		lsr = bus_space_read_1(ic->ic_iot, ic->ic_ioh, com_fsr);
		lsr = IS_RSA98(sc) ? RSA98_SF_NE(lsr) : RSA98c_SF_NE(lsr);
		return (int) lsr;
 	}
	return 0;
}

/****************************************************
 * sera hardware (de) activate
 ****************************************************/
int
sera_hw_activate(sc)
	struct com_softc *sc;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	ic->ic_mcr = MCR_DTR | MCR_RTS;
	sera_hw_fifo(sc, 1);

	if (IS_RSA98(sc))
		SET(ic->ic_mcr, MCR_IENABLE);
	else
		bus_space_write_1(iot, ioh, com_fic, 
			          RSA98c_FIC_RFH | RSA98c_FIC_SFE |
			          RSA98c_FIC_RCT | RSA98c_FIC_HT);

	bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
	sc->sc_nmsr = sc->sc_msr = bus_space_read_1(iot, ioh, com_msr);

	ic->ic_ier = IER_ERLS | IER_EMSC;
	bus_space_write_1(iot, ioh, com_ier, ic->ic_ier);

	return ISSET(sc->sc_msr, MSR_DCD);
}

int
sera_hw_deactivate(sc, hupctl)
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

	if (IS_RSA98(sc) == 0)
		bus_space_write_1(iot, ioh, com_fic, 0);

	if (hupctl != 0)
		bus_space_write_1(iot, ioh, com_mcr, 0);

	return 0;
}

/****************************************************
 * sera start output
 ****************************************************/
void
serastart(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register u_int8_t lsr;
	register int cc;

	/* 
	 * If sc->sc_rtscts on, RSA98 does automatic flow control.
	 */
	if (IS_RSA98(sc))
	{
		/*
		 * MCR_IENABLE bit enables data trasfer between 
		 * xmit fifo of RSA98 and ns16550 transmitter.
		 * It is important to turn the bit off during
		 * fifo write, because with the bit on, 
		 * the fifo control chip of RSA98 would have requests
		 * from cpu(us) and ns16550(transmitter) at the same time.
		 */

		CLR(ic->ic_mcr, MCR_IENABLE);
		bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
	}

	do
	{
		cc = ndqb(&tp->t_outq, 0);
		if (cc > RSA98_FRAG_SIZE)
			cc = RSA98_FRAG_SIZE;
		bus_space_write_multi_1(iot, ioh, com_data_write,
					tp->t_outq.c_cf, cc);
		ndflush(&tp->t_outq, cc);
		if (tp->t_outq.c_cc == 0)
			break;

		lsr = bus_space_read_1(iot, ioh, com_fsr);
		lsr = IS_RSA98(sc) ? RSA98_SF_NHF(lsr) : RSA98c_SF_NHF(lsr);
	}
	while (lsr != 0);

	if (IS_RSA98(sc))
	{
		SET(ic->ic_mcr, MCR_IENABLE);
		bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);

		if (sc->sc_ser->sc_period > RSA98_TIMEOUT_LOW)
			EMUL_INTR_REQUIRED(sc);
		sc->sc_ser->sc_period = RSA98_TIMEOUT_MIN;
	}

	sc->sc_cc = sc->ob_cc = 0;
}

/****************************************************
 * sera timer control
 ****************************************************/
static void
seratimerstart(sc)
	struct ser_softc *sc;
{
	u_int period = sc->sc_period;
	bus_space_tag_t iot = sc->sc_tbst;
	bus_space_handle_t ioh = sc->sc_tbsh;

	period >>= 10;	/* div by 1024 */
	period++;

	bus_space_write_1(iot, ioh, com_tsr, RSA98_TSR_DIS);
	bus_space_write_1(iot, ioh, com_tir, period);
	bus_space_write_1(iot, ioh, com_tsr, 0);

	if (sc->sc_period < RSA98_TIMEOUT_MAX)
		sc->sc_period += RSA98_TIMEOUT_INC;
}

static void
seratimerstop(sc)
	struct ser_softc *sc;
{

	bus_space_write_1(sc->sc_tbst, sc->sc_tbsh, com_tsr, RSA98_TSR_DIS);
}

static void
seraIIItimerstart(sc)
	struct ser_softc *sc;
{
	bus_space_tag_t iot = sc->sc_tbst;
	bus_space_handle_t ioh = sc->sc_tbsh;

	bus_space_write_1(iot, ioh, com_tir, 50);
	bus_space_write_1(iot, ioh, com_tsr, RSA98c_TSR_EN);
}

static void
seraIIItimerstop(sc)
	struct ser_softc *sc;
{

	bus_space_write_1(sc->sc_tbst, sc->sc_tbsh, com_tsr, 0);
}

/****************************************************
 * sera intr
 ****************************************************/
int
seraintr(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register u_int8_t fsr;
	struct v_ring_buf *rb;
	int rsa98;

	rsa98 = IS_RSA98(sc);
	if (sc->sc_vicr == 0)
	{
		fsr = bus_space_read_1(iot, ioh, com_fsr);
		if (rsa98)
		{
			if (RSA98_RF_NE(fsr))
				bus_space_write_1(iot, ioh, com_frr, RSA98_FRR_S);
		}
		else
		{
			if (RSA98c_RF_NE(fsr))
				bus_space_write_1(iot, ioh, com_frr, 0);
		}

		if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
			return 0;

		(void) bus_space_read_1(iot, ioh, com_lsr);
		(void) bus_space_read_1(iot, ioh, com_msr);
		return 0;
	}

 	rb = &sc->sc_invrb;
	if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_IMASK) == IIR_RLS)
	{
		register u_int8_t lsr = bus_space_read_1(iot, ioh, com_lsr);

		rb_input(rb, sc->sc_rdata, sc->sc_rstat, 0,
			 ISSET(lsr, LSR_RCV_ERR));
		if (rb_highwater(rb))
			comns_rtsoff(ic);
		SERA_DEBUG("err");
	}

	fsr = bus_space_read_1(iot, ioh, com_fsr);
	fsr = rsa98 ? RSA98_RF_NE(fsr) : RSA98c_RF_NE(fsr);
	if (fsr != 0)
	{
		register int tout = RSA98_FIFO_SIZE;

		do
		{
			register u_int8_t data;

			data = bus_space_read_1(iot, ioh, com_data_read);
			rb_input(rb, sc->sc_rdata, sc->sc_rstat, data, 0);
			if (rb_highwater(rb))
			{
				comns_rtsoff(ic);
				SERA_DEBUG("off");
			}
			fsr = bus_space_read_1(iot, ioh, com_fsr);
			fsr = rsa98 ? RSA98_RF_NE(fsr) : RSA98c_RF_NE(fsr);
		}
		while (fsr != 0 && tout-- > 0);

		if (rsa98)
		{
			sc->sc_ser->sc_period = RSA98_TIMEOUT_MIN;
			setsofttty();
		}
	}

	sc->sc_nmsr = bus_space_read_1(iot, ioh, com_msr);
	if (sc->sc_msr != sc->sc_nmsr)
		setsofttty();

	if (sc->ob_cc == 0)
	{
		fsr = bus_space_read_1(iot, ioh, com_fsr);
		fsr = rsa98 ? RSA98_SF_AE(fsr) : RSA98c_SF_E(fsr);
		if (fsr != 0)
			comns_xmit_terminate(sc);
	}

	fsr = bus_space_read_1(iot, ioh, com_fsr);
	fsr = rsa98 ? RSA98_RF_NE(fsr) : RSA98c_RF_NE(fsr);
	return (fsr || !ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND));
}
