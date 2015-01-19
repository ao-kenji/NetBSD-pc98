/*	$NecBSD: serh.c,v 1.3 1999/07/23 05:39:11 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
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
/*
 * XXX: this driver completely broken.
 */
#include "opt_serh.h"

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
#include <dev/ic/hayespreg.h>

#include "locators.h"

#define	com_lcr	com_cfcr

int comprobeHAYESP __P((bus_space_tag_t, bus_space_handle_t));
void comh_hw_attach __P((struct com_softc *));
int comh_hw_activate __P((struct com_softc *));
int comh_hw_deactivate __P((struct com_softc *, int));
void comh_hw_fifo __P((struct com_softc *, int));
int comh_hw_drain __P((struct com_softc *, int));
void comhstart __P((struct com_softc *, struct tty *));
int comhintr __P((void *));

#ifndef	SERH_SPEED
#define	SERH_SPEED TTYDEF_SPEED
#endif	/* !SERH_SPEED */

static struct com_switch comh_switch = { 
	  comh_hw_attach,
	  comh_hw_activate,
	  comh_hw_deactivate,
	  comh_hw_drain,
	  comh_hw_fifo,

	  comhstart,
	  comhintr,
	  IPL_TTY,

	  comnsioctl,
	  comnsparam,
	  comnsmodem,
	  comnserrmap,

	  SERH_SPEED,
};

static int serhmatch __P((struct device *, struct cfdata *, void *));
void serhattach __P((struct device *, struct device *, void *));

struct cfattach serh_ca = {
	sizeof(struct ns16550_softc), serhmatch, serhattach
};

extern struct cfdriver serh_cd;

static int
serhmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct commulti_attach_args *ca = aux;
	bus_space_tag_t iot = ca->ca_h.cs_iot;
	bus_space_handle_t spioh = ca->ca_h.cs_spioh;

	if (ca->ca_type != COM_HW_HAYES)
		return 0;

	if (cf->cf_loc[SERCF_SLAVE] != SERCF_SLAVE_DEFAULT &&
	    cf->cf_loc[SERCF_SLAVE] != ca->ca_slave)
		return 0;

	if (spioh == NULL || comprobeHAYESP(iot, spioh) == 0)
		return 0;

	return SERH_PROBE_PRI;
}

void
serhattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct commulti_attach_args *ca = aux;
	struct ns16550_softc *ic = (void *) self;

	printf("\n");
	ic->ic_com.sc_cswp = &comh_switch;
	ic->ic_com.sc_ser = (void *) parent;
	ca->ca_m = self;
	comns_setup_softc(ca, ic);
	config_found(self, ca, NULL);
}

/***********************************************************
 * attach hardware
 ***********************************************************/
int
comprobeHAYESP(iot, hayespioh)
	bus_space_tag_t iot;
	bus_space_handle_t hayespioh;
{
	char	val, dips;

	/*
	 * Hayes ESP cards have two iobases.  One is for compatibility with
	 * 16550 serial chips, and at the same ISA PC base addresses.  The
	 * other is for ESP-specific enhanced features, and lies at a
	 * different addressing range entirely (0x140, 0x180, 0x280, or 0x300).
	 */

	/* Test for ESP signature */
	if ((bus_space_read_1(iot, hayespioh, 0) & 0xf3) == 0)
		return 0;

	/*
	 * ESP is present at ESP enhanced base address; unknown com port
	 */

	/* Get the dip-switch configurations */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETDIPS);
	dips = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1);

	/* Check ESP Self Test bits. */
	/* Check for ESP version 2.0: bits 4,5,6 == 010 */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETTEST);
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1);
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS2);
	if ((val & 0x70) < 0x20) {
		printf("-old (%o)", val & 0x70);
		/* we do not support the necessary features */
		return 0;
	}

	/* Check for ability to emulate 16550: bit 8 == 1 */
	if ((dips & 0x80) == 0) {
		printf(" slave");
		/* XXX Does slave really mean no 16550 support?? */
		return 0;
	}

	/*
	 * If we made it this far, we are a full-featured ESP v2.0 (or
	 * better), at the correct com port address.
	 */
	return 1;
}

void
comh_hw_attach(sc)
	struct com_softc *sc;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_handle_t ioh = ic->ic_ioh;
	bus_space_tag_t iot = ic->ic_iot;

	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);

	printf(": ESP, 1024 byte fifo\n");

	sc->sc_maxputs = 1024;
	if (sc->sc_maxputs > COM_OBUFSIZE)
		sc->sc_maxputs = COM_OBUFSIZE;
}

/***********************************************************
 * HW OPEN
 ***********************************************************/
int
comh_hw_activate(sc)
	struct com_softc *sc;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_handle_t hayespioh = ic->ic_spioh;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;

	bus_space_write_1(iot, ioh, com_fifo,
		     FIFO_DMA_MODE|FIFO_ENABLE|
		     FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_8);

	/* Set 16550 compatibility mode */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETMODE);
	bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
		     HAYESP_MODE_FIFO|HAYESP_MODE_RTS|
		     HAYESP_MODE_SCALE);

	/* Set RTS/CTS flow control */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETFLOWTYPE);
	bus_space_write_1(iot, hayespioh, HAYESP_CMD2, HAYESP_FLOW_RTS);
	bus_space_write_1(iot, hayespioh, HAYESP_CMD2, HAYESP_FLOW_CTS);

	/* Set flow control levels */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETRXFLOW);
	bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
		     HAYESP_HIBYTE(HAYESP_RXHIWMARK));
	bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
		     HAYESP_LOBYTE(HAYESP_RXHIWMARK));
	bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
		     HAYESP_HIBYTE(HAYESP_RXLOWMARK));
	bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
		     HAYESP_LOBYTE(HAYESP_RXLOWMARK));

	/* you turn me on, baby */
	ic->ic_mcr = MCR_DTR | MCR_RTS;
	if (!ISSET(sc->sc_hwflags, COM_HW_NOIEN))
		SET(ic->ic_mcr, MCR_IENABLE);

	bus_space_write_1(iot, ioh, com_mcr, ic->ic_mcr);
	ic->ic_ier = IER_ERXRDY | IER_ERLS | IER_EMSC;
	sc->sc_msr = bus_space_read_1(iot, ioh, com_msr);
	bus_space_write_1(iot, ioh, com_ier, ic->ic_ier);

	return ISSET(sc->sc_msr, MSR_DCD);
}

int
comh_hw_deactivate(sc, hupctl)
	struct com_softc *sc;
	int hupctl;
{
	struct ns16550_softc *ic = (void *) sc;
	register bus_space_tag_t iot = ic->ic_iot;
	register bus_space_handle_t ioh = ic->ic_ioh;

	CLR(ic->ic_lcr, LCR_SBREAK);
	bus_space_write_1(iot, ioh, com_lcr, ic->ic_lcr);
	bus_space_write_1(iot, ioh, com_ier, 0);

	if (hupctl != 0)
		bus_space_write_1(iot, ioh, com_mcr, 0);

	return 0;
}

/***********************************************************
 * fifo drain
 ***********************************************************/
int
comh_hw_drain(sc, flags)
	struct com_softc *sc;
	int flags;
{
	/* not yet */
	return 0;
}

void
comh_hw_fifo(sc, flags)
	struct com_softc *sc;
	int flags;
{

	/* nothing */
}

/***********************************************************
 * output
 ***********************************************************/
void
comhstart(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	register int n;
	int s;

	sc->ob_cp = tp->t_outq.c_cf;
	sc->sc_cc = n = ndqb(&tp->t_outq, 0);
	if (n > sc->sc_maxputs)
		n = sc->sc_maxputs;
	bus_space_write_multi_1(iot, ioh, com_data, sc->ob_cp, n);

	s = splcom();
	sc->ob_cp += n;
	sc->ob_cc = 0;
	splx(s);
}

int
comhintr(arg)
	void *arg;
{

	/* XXX */
	return 0;
}
