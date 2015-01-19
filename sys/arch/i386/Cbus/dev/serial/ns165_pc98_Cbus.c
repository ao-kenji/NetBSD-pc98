/*	$NecBSD: ns165_pc98_Cbus.c,v 1.1 1998/12/31 02:33:02 honda Exp $	*/
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
#include <i386/Cbus/dev/serial/ns165subr.h>
#include <i386/Cbus/dev/serial/ser_pc98_Cbus.h>

#include "sera.h"
#include "serb.h"
#include "sern.h"
#include "serh.h"

#define	COMNS_DIOT_SZ	8

#if	NSERN > 0
/*  NS16550 IBM PORT OFFSET */
struct com_hw com_hw_pure = {
	COM_HW_NS16550,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	0,		/* hwflags */

	1843200,	/* freq: 16-bit baud rate divisor */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	4,	/* max unit */
	8,	/* port skip */
	32,	/* max ports */

	BUS_SPACE_IAT_1,
	COMNS_DIOT_SZ,

	NULL,
	0
};

struct com_hw com_hw_emul_hayes = {
	COM_HW_NS16550,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	0,		/* hwflags */

	1843200,	/* freq: 16-bit baud rate divisor */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	1,	/* XXX: ??? max unit */
	1,	/* XXX: ??? port skip */
	8,	/* max ports */

	BUS_SPACE_IAT_2,	/* even port handle */
	COMNS_DIOT_SZ,

	NULL,
	0
};

struct com_hw com_hw_nsmodem = {
	COM_HW_NS16550,	/* type */
	COM_HW_NSMODEM,	/* subtype */

	0,		/* hwflags */

	1843200,	/* freq: 16-bit baud rate divisor */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	1,	/* max unit */
	8,	/* port skip */
	8,	/* max ports */

	BUS_SPACE_IAT_1,
	COMNS_DIOT_SZ,

	NULL,
	0
};
#endif	/* NSERN > 0 */

#if	NSERA > 0
static bus_addr_t com_hw_iat_RSA98[] = {
	0x10,	/* data register (R/W) */
	0x12,	/* divisor latch high (W) */
	0x14,	/* interrupt identification (R) */
	0x16,	/* line control register (R/W) */
	0x18,	/* modem control register (R/W) */
	0x1a,	/* line status register (R/W) */
	0x1c,	/* modem status register (R/W) */
	0x1e,	/* scratch register (R/W) */
	0,	/* data register (W) */
	0,	/* status register (R) */
	2,	/* data read (R) */
	4,	/* fifo reset */
	0,	/* dummy */
	0,	/* dummy */
	2,	/* timer interval (W) */
	6,	/* timer, enable = 0 disenable = 2 (W) */
};

static bus_addr_t com_hw_iat_RSA98III[] = {
	0x8,	/* data register (R/W) */
	0x9,	/* divisor latch high (W) */
	0xa,	/* interrupt identification (R) */
	0xb,	/* line control register (R/W) */
	0xc,	/* modem control register (R/W) */
	0xd,	/* line status register (R/W) */
	0xe,	/* modem status register (R/W) */
	0xf,	/* scratch register (R/W) */
	0x8,	/* data write register (W) */
	0x2,	/* status register (R) */
	0x8,	/* data read register (R) */
	0x2,	/* fifo reset (W) */
	0x0,	/* mode control */
	0x1,	/* interrupt control */
	3,	/* timer interval (W) */
	4,	/* timer, enable = 1 disenable = 0 (W) */
};

struct com_hw com_hw_RSA98 = {
	COM_HW_SERA,	/* type */
	COM_HW_RSA98,	/* subtype */

	COM_HW_TIMER,	/* hwflags */

	(1843200 * 8),	/* RSA98 base clock */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	0x02,	/* max unit */
	0x01,	/* port skip */
	32,	/* ports */

	com_hw_iat_RSA98,
	BUS_SPACE_IAT_SZ(com_hw_iat_RSA98),

	NULL,
	0
};

struct com_hw com_hw_RSA98III = {
	COM_HW_SERA,	/* type */
	COM_HW_RSA98III,	/* subtype */

	COM_HW_TIMER,		/* hwflags */

	(1843200 * 8),	/* RSA98III base clock */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	0x02,	/* max unit */
	0x100,	/* port skip */
	32,	/* ports */

	com_hw_iat_RSA98III,
	BUS_SPACE_IAT_SZ(com_hw_iat_RSA98III),

	NULL,
	0
};
#endif	/* NSERA > 0 */

#if	NSERN > 0
struct com_hw com_hw_NEC_2nd = {
	COM_HW_NS16550,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	0,		/* hwflags */

	1843200,	/* freq: 16-bit baud rate divisor */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	1,	/* max unit */
	0,	/* port skip */
	8,	/* max ports */

	BUS_SPACE_IAT_1,
	COMNS_DIOT_SZ,

	NULL,
	0
};

static bus_addr_t com_hw_iat_MicroCore[] = {
	0x000,
	0x100,
	0x200,
	0x300,
	0x400,
	0x500,
	0x600,
	0x700,
	0x1000,	/* only valid MCRS98 */
};

static int init_MCRS98 __P((struct com_softc *, u_long, u_long));
static int
init_MCRS98(sc, iobase, irq)
	struct com_softc *sc;
	u_long iobase;
	u_long irq;
{
	struct ns16550_softc *ic = (void *) sc;
	bus_space_tag_t iot = ic->ic_iot;
	bus_space_handle_t ioh;
	u_int8_t irqval;
	static u_int8_t comMCRS_irqval[] =
	/*0  1  2  3  4  5  6  7  8  9 10  11 12 13 14 15  */
	{ 0, 0, 0, 4, 0, 5, 6, 0, 0, 0, 0, 0, 7, 0, 0, 0 };

	iobase = 0x1000 | (iobase & 0xff);
	if (bus_space_map(iot, iobase, 1, 0, &ioh) != 0)
		return EINVAL;
	if (irq > 15)
		return EINVAL;

	irqval = comMCRS_irqval[irq];
	bus_space_write_1(iot, ioh, 0, irqval);
	return 0;
}

struct com_hw com_hw_MC16550 = {
	COM_HW_NS16550,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	COM_HW_SHAREDINT,/* hwflags */

	1843200,	/* freq: 16-bit baud rate divisor */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	init_MCRS98,		/* special initialization */

	2,	/* XXX: max unit */
	0x800,	/* XXX: port skip */
	8,	/* XXX: max ports */

	com_hw_iat_MicroCore,
	BUS_SPACE_IAT_SZ(com_hw_iat_MicroCore),

	NULL,
	0
};


struct com_hw com_hw_MCRS98 = {
	COM_HW_NS16550,	/* type */
	COM_HW_MCRS98,	/* subtype */

	COM_HW_SHAREDINT,/* hwflags */

	(1843200 * 4),	/* MCRS98 base clock */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	init_MCRS98,		/* special initialization */

	2,	/* XXX: max unit */
	0x800,	/* XXX: port skip */
	8,	/* XXX: max ports */

	com_hw_iat_MicroCore,
	BUS_SPACE_IAT_SZ(com_hw_iat_MicroCore) - 1,

	NULL,
	0
};
#endif	/* NSERN > 0 */

#if	NSERB > 0
struct com_hw com_hw_RSB2000 = {
	COM_HW_SERB,	/* type */
	COM_HW_RSB2000,	/* subtype */

	0, 		/* hwflags */

	(1843200 * 10),	/* RSB2000 or 3000 base clock */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	2,	/* XXX: max unit */
	1,	/* XXX: port skip */
	16,	/* XXX: max ports */

	BUS_SPACE_IAT_2,
	COMNS_DIOT_SZ,

	NULL,
	0
};

struct com_hw com_hw_RSB4000 = {
	COM_HW_SERB,	/* type */
	COM_HW_RSB2000,	/* subtype */

	0, 		/* hwflags */

	(1843200 * 16),	/* ???: check please */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	2,	/* XXX: max unit */
	1,	/* XXX: port skip */
	16,	/* XXX: max ports */

	BUS_SPACE_IAT_2,
	COMNS_DIOT_SZ,

	NULL,
	0
};
#endif	/* NSERB > 0 */

#if	NSERN > 0
struct com_hw com_hw_RSB384 = {
	COM_HW_NS16550,	/* type */
	COM_HW_RSB384,	/* subtype */

	0, /* hwflags */

	(1843200 * 10),	/* RSB2000 or 3000 base clock */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,

	1,	/* XXX: max unit */
	1,	/* XXX: port skip */
	16,	/* XXX: max ports */

	BUS_SPACE_IAT_2,
	COMNS_DIOT_SZ,

	NULL,
	0
};
#endif	/* NSERN > 0 */

#if	NSERH > 0
/* only sample: need changes */
struct com_hw com_hw_hayes_sample = {
	COM_HW_HAYES,	/* type */
	COM_HW_DEFAULT,	/* subtype */

	0,		/* hwflags */

	1843200,	/* freq: 16-bit baud rate divisor */
	NULL,		/* speed tab */
	comns_set_speed,	/* speedfunc */
	NULL,
	NULL,

	NULL,		/* special initialization */

	4,	/* max unit */
	8,	/* port skip */
	32,	/* max ports */

	BUS_SPACE_IAT_1,
	COMNS_DIOT_SZ,

	NULL,
	0
};
#endif	/* NSERH > 0 */
