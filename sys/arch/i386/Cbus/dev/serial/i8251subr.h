/*	$NecBSD: i8251subr.h,v 1.2 1999/07/23 20:57:30 honda Exp $	*/
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

#ifndef	_I8251SUBR_H_
#define	_I8251SUBR_H_

#include <dev/ic/i8251reg.h>

/* probe priority */
#define	SERI_PROBE_PRI	1
#define	SERF_PROBE_PRI	2

/* default setup */
#define	I8251_DEF_MODE (MOD8251_8BITS | MOD8251_STOPB)
#define	I8251_DEF_CMD  (CMD8251_TxEN | CMD8251_RxEN | CMD8251_DTR | CMD8251_RTS)

#define	I8251F_FIFO_TRIG (FIFO_TRIGGER_8)
#define	I8251F_FIFO_INTR (FIFO_MSR_EN)

#define	I8251F_FCLR	0x01
#define	I8251F_FXCLR	0x02
#define	I8251F_FALLCLR	(I8251F_FCLR | I8251F_FXCLR)

/* structure */
struct i8251_softc {
	struct com_softc ic_com;

	bus_space_tag_t ic_iot;			/* serial ic tag */
	bus_space_handle_t ic_ioh;		/* serial chip ioh */
	bus_space_handle_t ic_spioh;		/* serial fifo ioh */ 

	bus_space_tag_t ic_icrt;		/* icr io handle */
	bus_space_handle_t ic_icrh;
#define	I8251_RxINTR_ENABLE	0x01
#define	I8251_TxINTR_ENABLE	0x02
	void (*ic_control_intr) __P((struct com_softc *, u_int8_t));

	bus_space_tag_t ic_msrt;		/* msr io handle */
	bus_space_handle_t ic_msrh;
	u_int8_t (*ic_read_msr) __P((struct com_softc *));

	/* chip specific port registers */
	u_int8_t ic_dtr;			/* dtr */
	u_int8_t ic_cmdr;			/* cmd register */
	u_int8_t ic_lcr;			/* line control register */
	u_int8_t ic_ier;			/* interrupt line register */
	u_int8_t ic_triger;			/* fifo control register */
	u_int8_t ic_mrts;			/* rts mask */	
	u_int8_t ic_mcts;			/* cts mask */

	int ic_maxspeed;			/* max speed */
};

/*****************************************************
 * Local port index (relocated)
 *****************************************************/
#define	seri_data	0		/* I8251 N/F */
#define	seri_modr	1
#define	seri_cmdr	2
#define	seri_lsr	3

#define	serf_cridx	0		/* I8251F index access board */
#define	serf_crdata	1
#define	serf_base	2		/* I8251F plain access board */
#define	serf_data	(serf_base + 0)
#define	serf_lsr	(serf_base + 1)
#define	serf_msr	(serf_base + 2)
#define	serf_iir	(serf_base + 3)
#define	serf_fcr	(serf_base + 4)
#define	serf_div	(serf_base + 5)

/*****************************************************
 * Proto
 *****************************************************/
/* cmd register access */
static __inline void i8251_write_cmdr __P((struct i8251_softc *, u_int8_t));
static __inline void i8251_bset_cmdr __P((struct i8251_softc *, u_int8_t));
static __inline void i8251_bclr_cmdr __P((struct i8251_softc *, u_int8_t));
static __inline void i8251_rtsoff __P((struct i8251_softc *));
static __inline void i8251_rtson __P((struct i8251_softc *));

/* i8251F fifo control register */
static __inline u_int8_t i8251f_cr_read_1 __P((bus_space_tag_t, bus_space_handle_t, u_int, bus_addr_t));
static __inline void i8251f_cr_write_1 __P((bus_space_tag_t, bus_space_handle_t, u_int, bus_addr_t, u_int8_t));

/* common utils */
void i8251_init __P((struct i8251_softc *, int, u_int8_t, u_int8_t));
void i8251_reset __P((bus_space_tag_t, bus_space_handle_t, u_int8_t mode, int));
int i8251ioctl __P((struct com_softc *, u_long, caddr_t, int, struct proc *));
u_int i8251errmap __P((struct com_softc *, u_int, u_int8_t));
int i8251param __P((struct com_softc *, struct tty *, struct termios *));
int _seriprobe __P((bus_space_tag_t, bus_space_handle_t, bus_space_handle_t, u_int));
int _serfprobe __P((bus_space_tag_t, bus_space_handle_t, bus_space_handle_t, u_int));
void i8251_setup_softc __P((struct commulti_attach_args *, struct i8251_softc *));

/*****************************************************
 * I8251F fifo control register read/write 1
 *****************************************************/
static __inline u_int8_t
i8251f_cr_read_1(iot, ioh, hwflags, addr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int hwflags;
	bus_size_t addr;
{

	if (ISSET(hwflags, COM_HW_INDEXED) == 0)
	{
		return bus_space_read_1(iot, ioh, addr);
	}
	else
	{
		bus_space_write_1(iot, ioh, serf_cridx, addr);
		return bus_space_read_1(iot, ioh, serf_crdata);
	}
}

static __inline void
i8251f_cr_write_1(iot, ioh, hwflags, addr, val)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int hwflags;
	bus_size_t addr;
	u_int8_t val;
{

	if (ISSET(hwflags, COM_HW_INDEXED) == 0)
	{
		bus_space_write_1(iot, ioh, addr, val);
	}
	else
	{
		bus_space_write_1(iot, ioh, serf_cridx, addr);
		bus_space_write_1(iot, ioh, serf_crdata, val);
	}
}

/*****************************************************
 * cmd register access
 ****************************************************/
static __inline void
i8251_write_cmdr(ic, cmd)
	struct i8251_softc *ic;
	u_int8_t cmd;
{
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	bus_space_handle_t spioh = ic->ic_spioh;
	int hwflags = ic->ic_com.sc_hwflags;

	if (ISSET(hwflags, COM_HW_ACCEL))
	{
		i8251f_cr_write_1(iot, spioh, hwflags, serf_fcr, 0);
		bus_space_write_1(iot, ioh, seri_cmdr, cmd);
		i8251f_cr_write_1(iot, spioh, hwflags, serf_fcr, ic->ic_triger);
	}
	else
		bus_space_write_1(iot, ioh, seri_cmdr, cmd);
}

static __inline void
i8251_bset_cmdr(ic, cmd)
	struct i8251_softc *ic;
	u_int8_t cmd;
{

	SET(ic->ic_cmdr, cmd);
	i8251_write_cmdr(ic, ic->ic_cmdr);
}

static __inline void
i8251_bclr_cmdr(ic, cmd)
	struct i8251_softc *ic;
	u_int8_t cmd;
{

	CLR(ic->ic_cmdr, cmd);
	i8251_write_cmdr(ic, ic->ic_cmdr);
}

/*****************************************************
 * i8251 flow control
 ****************************************************/
static __inline void
i8251_rtsoff(ic)
	struct i8251_softc *ic;
{
	int sp = splcom();

	if (ISSET(ic->ic_cmdr, ic->ic_mrts) != 0)
	{
		CLR(ic->ic_cmdr, ic->ic_mrts);
		i8251_write_cmdr(ic, ic->ic_cmdr);
	}
	splx(sp);
}

static __inline void
i8251_rtson(ic)
	struct i8251_softc *ic;
{
	int sp;

	if (ISSET(ic->ic_com.sc_cflags, COM_RTSOFF))
		return;

	sp = splcom();
	if (ISSET(ic->ic_cmdr, ic->ic_mrts) != ic->ic_mrts)
	{
		SET(ic->ic_cmdr, ic->ic_mrts);
		i8251_write_cmdr(ic, ic->ic_cmdr);
	}
	splx(sp);
}

/*****************************************************
 * intr operations
 ****************************************************/
static __inline void i8251_bset_icr __P((struct i8251_softc *, u_int8_t));
static __inline void i8251_bclr_icr __P((struct i8251_softc *, u_int8_t));

static __inline void
i8251_bset_icr(ic, data)
	struct i8251_softc *ic;
	u_int8_t data;
{

	SET(ic->ic_ier, data);
	(*ic->ic_control_intr) (&ic->ic_com, ic->ic_ier);
}

static __inline void
i8251_bclr_icr(ic, data)
	struct i8251_softc *ic;
	u_int8_t data;
{

	CLR(ic->ic_ier, data);
	(*ic->ic_control_intr) (&ic->ic_com, ic->ic_ier);
}

/*****************************************************
 * msr operations
 ****************************************************/
static __inline u_int8_t i8251_read_msr __P((struct i8251_softc *));

static __inline u_int8_t
i8251_read_msr(ic)
	struct i8251_softc *ic;
{

	return (*ic->ic_read_msr) (&ic->ic_com);
}
/*****************************************************
 * txintr emulation
 ****************************************************/
static __inline void i8251_xmit_terminate __P((struct com_softc *));

static __inline void
i8251_xmit_terminate(sc)
	struct com_softc *sc;
{

	sc->ob_cc = -1;
	sc->sc_txintr = COM_TXINTR_ASSERT;
	setsofttty();
}

#endif	/* !_I8251SUBR_H_ */
