/*	$NecBSD: i8251_pc98_Cbus.c,v 1.2 1999/04/15 01:36:15 kmatsuda Exp $	*/
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
#include <i386/Cbus/dev/serial/ser_pc98_Cbus.h>
#include <machine/timervar.h>
#include "serf.h"
#include "seri.h"

#if	(NSERI + NSERF)	> 0
static u_int8_t seri_read_msr __P((struct com_softc *));
static void seri_control_intrline __P((struct com_softc *, u_int8_t));

/*
 * Read msr via SYSTM_PORTB
 */
#define	CICSCD_CI	0x80	/* CI */
#define	CICSCD_CS	0x40	/* CS */
#define	CICSCD_CD	0x20	/* CD */

static u_int8_t
seri_read_msr(sc)
	struct com_softc *sc;
{
	struct i8251_softc *ic = (void *) sc;
	static u_int8_t msr_translate_tbl[] = { 0,
				MSR_DCD,
			        MSR_CTS,
				MSR_DCD | MSR_CTS,
				MSR_RI,
				MSR_RI | MSR_DCD,
				MSR_RI | MSR_CTS,
				MSR_RI | MSR_CTS | MSR_DCD };
	register u_int8_t msr, stat;

	stat = bus_space_read_1(ic->ic_msrt, ic->ic_msrh, 0) ^ 0xff;
	stat >>= 5;
	msr = msr_translate_tbl[stat];
	stat = bus_space_read_1(ic->ic_iot, ic->ic_ioh, seri_lsr);
	if (ISSET(stat, STS8251_DSR))
		SET(msr, MSR_DSR);
	return  msr;
}

/*
 * control intrline via SYSTM_PORTC 
 */
#define	IEN_Rx		0x01
#define	IEN_TxEMP	0x02
#define	IEN_Tx		0x04

static void
seri_control_intrline(sc, cmd)
	struct com_softc *sc;
	u_int8_t cmd;
{
	struct i8251_softc *ic = (void *) sc;
	register u_int8_t regv;
	static u_int8_t intr_enable_reg[] = 
		{0, IEN_Rx, IEN_Tx, IEN_Rx | IEN_Tx};

	regv = bus_space_read_1(ic->ic_icrt, ic->ic_icrh, 0);
	regv &= ~(IEN_Rx | IEN_TxEMP | IEN_Tx);
	regv |= intr_enable_reg[cmd];
	bus_space_write_1(ic->ic_icrt, ic->ic_icrh, 0, regv);
};

/*
 * Setup systm clock supplier.
 */
static void i8251_set_speed __P((struct com_softc *, int));
static void
i8251_set_speed(sc, speed)
	struct com_softc *sc;
	int speed;
{
	struct i8251_softc *ic = (void *) sc;
	int hwflags = sc->sc_hwflags;
	struct comspeed *spt;
	struct timer_mode tm;
	u_int8_t d;

	if (speed == 0)
		speed = 9600;

	spt = comfindspeed(sc->sc_speedtab, speed);
	if (spt->cs_div != 0 && ISSET(hwflags, COM_HW_ACCEL))
	{
		d = spt->cs_div;
		if (ISSET(hwflags, COM_HW_INTERNAL))
			d |= 0x80;
		i8251f_cr_write_1(ic->ic_iot, ic->ic_spioh, hwflags,
				  serf_div, d);
	}
	else if (ISSET(hwflags, COM_HW_INTERNAL))
	{
		if (ISSET(hwflags, COM_HW_ACCEL))
			i8251f_cr_write_1(ic->ic_iot, ic->ic_spioh, hwflags,
					  serf_div, 0);
		tm.tm_mode = TMM_SQWAVE;
		tm.tm_freq = (u_long) speed;
		if (timer_set_mode(SYSTM_TIMER_TAG, TMC_RS232C, &tm))
			printf("%s: can not set freq\n", sc->sc_dev.dv_xname);
	}
}

struct comspeed serispt[] = {
	{0,	0},
	{75,	0},
	{150,	0},
	{300,	0},
	{600,	0},
	{1200,	0},
	{2400,	0},
	{4800,	0},
	{9600,	0},
	{14400,	8},
	{19200,	0},
	{20800,	0},
	{28800,	4},
	{38400,	0},
	{41600,	0},
	{76800,	0},
	{57600,	2},
	{115200,1},
	{-1, -1}
};

static bus_addr_t com_hw_iat_i8251[] = {
	0,	/* data register (R/W) */
	2,	/* mode register (W) */
	2,	/* cmd control register (W) */
	2,	/* line status register (R) */
};

static bus_addr_t com_hw_iat_fifo_i8251F[] = {
	0x100,	/* I8251F: index register (R/W) */
	0x100,	/* I8251F: register port (R/W) */
	0x100,	/* I8251F: fifo data (R/W) */
	0x102,	/* I8251F: line status (R) */
	0x104,	/* I8251F: modem status (R) */
	0x106,	/* I8251F: interrupt line (R/W) */
	0x108,	/* I8251F: fifo control (R/W) */
	0x10a,	/* I8251F: clock divsor (R/W) */
};

struct com_hw com_hw_i8251 = {
	COM_HW_I8251,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	COM_HW_INTERNAL, /* hwflags */

	0,		/* freq */
	serispt,	/* speed tab */
	i8251_set_speed,	/* speedfunc */
	seri_read_msr,		/* modem status access method */
	seri_control_intrline,	/* interrupt line access method */

	NULL,		/* special initialization */

	1,	/* max unit */
	0,	/* port skip */
	6,	/* max ports */

	com_hw_iat_i8251,
	BUS_SPACE_IAT_SZ(com_hw_iat_i8251),

	com_hw_iat_fifo_i8251F,
	BUS_SPACE_IAT_SZ(com_hw_iat_fifo_i8251F),
};

static struct comspeed serfextspt[] = {
	{75, 3},
	{150, 4},
	{300, 5},
	{600, 6},
	{1200, 7},
	{2400, 8},
	{4800, 9},
	{9600, 10},
	{14400,	11},
	{19200,	12},
	{38400,	13},
	{57600,	14},
	{115200, 15},
	{-1, -1}
};

static bus_addr_t com_hw_iat_i8251_ext1[] = {
	1,	/* data register (R/W) */
	3,	/* com_cfcr line control register (R/W) */
	3,	/* modem control register (R/W) */
	3,	/* line status register (R/W) */
};

static bus_addr_t com_hw_iat_fifo_i8251F_ext1[] = {
	4,	/* I8251F: index register (R/W) */
	5,	/* I8251F: register port (R/W) */
};

struct com_hw com_hw_i8251_ext1 = {
	COM_HW_I8251,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	COM_HW_INDEXED,	/* hwflags */

	0,		/* freq */
	serfextspt,	/* speed tab */
	i8251_set_speed,	/* speedfunc */
	seri_read_msr,		/* modem status access method */
	seri_control_intrline,	/* interrupt line access method */

	NULL,		/* special initialization */

	1,	/* max unit */
	0,	/* port skip */
	4,	/* max ports */

	com_hw_iat_i8251_ext1,
	BUS_SPACE_IAT_SZ(com_hw_iat_i8251_ext1),

	com_hw_iat_fifo_i8251F_ext1,
	BUS_SPACE_IAT_SZ(com_hw_iat_fifo_i8251F_ext1),
};

static bus_addr_t com_hw_iat_i8251_ext2[] = {
	7,	/* data register (R/W) */
	9,	/* com_cfcr line control register (R/W) */
	9,	/* modem control register (R/W) */
	9,	/* line status register (R/W) */
	0,	/* modem status register (R/W) */
	0,	/* interrupt line control register (R/W) */
};

static bus_addr_t com_hw_iat_fifo_i8251F_ext2[] = {
	4,	/* I8251F: index register (R/W) */
	5,	/* I8251F: register port (R/W) */
};

struct com_hw com_hw_i8251_ext2 = {
	COM_HW_I8251,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	COM_HW_INDEXED,	/* hwflags */

	0,		/* freq */
	serfextspt,	/* speed tab */
	i8251_set_speed,	/* speedfunc */
	seri_read_msr,		/* modem status access method */
	seri_control_intrline,	/* interrupt line access method */

	NULL,		/* special initialization */

	1,	/* max unit */
	0,	/* port skip */
	10,	/* max ports */

	com_hw_iat_i8251_ext2,
	BUS_SPACE_IAT_SZ(com_hw_iat_i8251_ext2),

	com_hw_iat_fifo_i8251F_ext2,
	BUS_SPACE_IAT_SZ(com_hw_iat_fifo_i8251F_ext2),
};
#endif	/* NSERI + NSERF > 0 */

#if	NSERI > 0
static void seri_set_speed_LNDSPT __P((struct com_softc *, int));
static void
seri_set_speed_LNDSPT(sc, speed)
	struct com_softc *sc;
	int speed;
{
	struct i8251_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh = ic->ic_ioh;
	u_int rate;

	if (speed == 0)
		speed = 9600;

	rate = sc->sc_freq / speed;
	if (ISSET(sc->sc_hwflags, COM_HW_AFE))
		SET(rate, 0x8000);

	i8251_reset(iot, ioh, I8251_DEF_MODE, 1);

	bus_space_write_1(iot, ioh, seri_cmdr, CMD8251_RESET | CMD8251_EH);
	delay(200);
	bus_space_write_1(iot, ioh, seri_cmdr, (rate >> 8) & 0xff);
	delay(200);
	bus_space_write_1(iot, ioh, seri_cmdr, rate & 0xff);
	delay(400);
}

struct com_hw com_hw_i8251_LNDSPT_ext1 = {
	COM_HW_I8251_C,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	COM_HW_AFE,	/* hwflags */

	460800,		/* freq */
	serispt,	/* speed tab */
	seri_set_speed_LNDSPT,	/* speedfunc */
	seri_read_msr,		/* modem status access method */
	seri_control_intrline,	/* interrupt line access method */

	NULL,		/* special initialization */

	1,	/* max unit */
	0,	/* port skip */
	4,	/* max ports */

	com_hw_iat_i8251_ext1,
	BUS_SPACE_IAT_SZ(com_hw_iat_i8251_ext1),

	NULL,
	0
};

struct com_hw com_hw_i8251_LNDSPT_ext2 = {
	COM_HW_I8251_C,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	COM_HW_AFE,	/* hwflags */

	460800,		/* freq */
	serispt,	/* speed tab */
	seri_set_speed_LNDSPT,	/* speedfunc */
	seri_read_msr,		/* modem status access method */
	seri_control_intrline,	/* interrupt line access method */

	NULL,		/* special initialization */

	1,	/* max unit */
	0,	/* port skip */
	10,	/* max ports */

	com_hw_iat_i8251_ext2,
	BUS_SPACE_IAT_SZ(com_hw_iat_i8251_ext2),

	NULL,
	0
};
#endif	/* NSERI > 0 */
