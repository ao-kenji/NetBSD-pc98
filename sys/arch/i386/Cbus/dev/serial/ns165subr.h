/*	$NecBSD: ns165subr.h,v 1.2 1999/07/23 20:57:31 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef	_NS165SUBR_H_
#define	_NS165SUBR_H_

#include <i386/Cbus/dev/serial/ns165reg.h>

#define	COMNS_DEBUG(msg)

/* probe priority */
#define	SERA_PROBE_PRI	3
#define	SERB_PROBE_PRI	3
#define	SERH_PROBE_PRI	2
#define	SERN_PROBE_PRI	1

struct ns16550_softc {
	struct com_softc ic_com;

	bus_space_tag_t ic_iot;			/* hw resources */
	bus_space_handle_t ic_ioh;		/* serial chip ioh */
	bus_space_handle_t ic_spioh;		/* fifo ioh */

	/* chip specific port registers */
	u_int8_t ic_dtr;			/* dtr */
	u_int8_t ic_mcr;			/* modem register */
	u_int8_t ic_lcr;			/* line control register */
	u_int8_t ic_ier;			/* interrupt line register */
	u_int8_t ic_triger;			/* fifo register */
	u_int8_t ic_mrts;			/* rts mask */	
	u_int8_t ic_mcts;			/* cts mask */
};

/*****************************************************
 * Local Port index (relocated)
 *****************************************************/
#define	com_data	0
#define	com_dlbl	0
#define	com_dlbh	1
#define	com_ier		1
#define	com_iir		2
#define	com_fifo	2
#define	com_lctl	3
#define	com_cfcr	3
#define	com_lcr		com_cfcr
#define	com_mcr		4
#define	com_lsr		5
#define	com_msr		6
#define	com_scratch	7

/*****************************************************
 * Proto
 *****************************************************/
static __inline void comns_rtsoff __P((struct ns16550_softc *));
static __inline void comns_rtson __P((struct ns16550_softc *));

void comns_set_speed __P((struct com_softc *, int));
int comns_speed __P((long, u_long));
int _comnsprobe __P((bus_space_tag_t, bus_space_handle_t, bus_space_handle_t, u_int));
int comnsparam __P((struct com_softc *, struct tty *, struct termios *));
u_int comnserrmap __P((struct com_softc *, u_int, u_int8_t));
void comnsmodem __P((struct com_softc *, struct tty *));
int comnsioctl __P((struct com_softc *, u_long, caddr_t, int, struct proc *));
void comns_setup_softc __P((struct commulti_attach_args *, struct ns16550_softc *));

/* rts/cts flow control inline */
static __inline void
comns_rtsoff(ic)
	struct ns16550_softc *ic;
{
	int s = splcom();

	if (ISSET(ic->ic_mcr, ic->ic_mrts) != 0)
	{
		CLR(ic->ic_mcr, ic->ic_mrts);
		bus_space_write_1(ic->ic_iot, ic->ic_ioh, com_mcr, ic->ic_mcr);
	}
	splx(s);
}

static __inline void
comns_rtson(ic)
	struct ns16550_softc *ic;
{
	int s;

	if (ISSET(ic->ic_com.sc_cflags, COM_RTSOFF))
		return;

 	s = splcom();
	if (ISSET(ic->ic_mcr, ic->ic_mrts) != ic->ic_mrts)
	{
		SET(ic->ic_mcr, ic->ic_mrts);
		bus_space_write_1(ic->ic_iot, ic->ic_ioh, com_mcr, ic->ic_mcr);
	}
	splx(s);
}

/*****************************************************
 * txintr emulation
 ****************************************************/
static __inline void comns_xmit_terminate __P((struct com_softc *));

static __inline void
comns_xmit_terminate(sc)
	struct com_softc *sc;
{

	sc->ob_cc = -1;
	sc->sc_txintr = COM_TXINTR_ASSERT;
	setsofttty();
}
#endif	/* !_NS165SUBR_H_ */
