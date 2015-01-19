/*	$NecBSD: seri_direct.c,v 1.4 1999/08/02 05:42:40 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (c) 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 * Copyright (c) 1998, 1999
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
#ifdef	SUPPORT_SERIAL

#include <sys/types.h>
#include <lib/libsa/stand.h>
#include <machine/pio.h>
#include <dev/ic/i8251reg.h>
#include "libi386.h"
#include "comio_direct.h"

#define	seri_data	0		/* I8251 Core */
#define	seri_icr	5
#define	IEN_Rx		0x01
#define	IEN_TxEMP	0x02
#define	IEN_Tx		0x04
#define	seri_modr	2
#define	seri_cmdr	2
#define	seri_lsr	2
#define	LSR_TXRDY	0x01
#define	LSR_RXRDY	0x02
#define	LSR_TXE		0x04
#define	LSR_PE		0x08
#define	LSR_OE		0x10
#define	LSR_FE		0x20
#define	seri_msr	3
#define	serf_data	0x100		/* I8251F Core */
#define	serf_lsr	0x102
#define	FLSR_TXRDY	0x02
#define	FLSR_TXE	0x01
#define	FLSR_RXRDY	0x04
#define	serf_icr	0x106
#define	serf_fcr	0x108
#define	FIFO_ENABLE	0x01	/* Turn the FIFO on */
#define	FIFO_RCV_RST	0x02	/* Reset RX FIFO */
#define	FIFO_XMT_RST	0x04	/* Reset TX FIFO */
#define	FIFO_LSR_EN	0x08
#define	FIFO_MSR_EN	0x10
#define	FIFO_TRIGGER_1	0x00	/* Trigger RXRDY intr on 1 character */
#define	FIFO_TRIGGER_4	0x40	/* ibid 4 */
#define	FIFO_TRIGGER_8	0x80	/* ibid 8 */
#define	FIFO_TRIGGER_14	0xc0	/* ibid 14 */
#define	serf_divr	0x10a

#define	timer_ctlr	0x77
#define	timer_c2r	0x75

#define XON	0x11
#define	XOFF	0x13
#define	SERBUFSIZE	16

static u_int8_t serbuf[SERBUFSIZE];
static int serbuf_read = 0;
static int serbuf_write = 0;
static int stopped = 0;

#define	I8251_MODEL_I8251	0
#define	I8251_MODEL_I8251F	1
#define	I8251_MODEL_I8251AF	2
static int i8251_model;

#define	ISSET(t,f)	((t) & (f))

static void i8251_init __P((int, u_long));
static int i8251f_probe	__P((int));
static int i8251_stat_rok	__P((int));
static int i8251_stat_wok	__P((int));
static u_int8_t i8251_read_data_1 __P((int));
static void i8251_write_data_1 __P((int, int));

/***********************************************************
 * Hareware basic funcs
 ***********************************************************/
static u_int8_t
i8251_read_data_1(iobase)
	int iobase;
{

	if (i8251_model >= I8251_MODEL_I8251F)
		return inb(iobase + serf_data);
	else
		return inb(iobase + seri_data);
}

static void
i8251_write_data_1(iobase, c)
	int iobase, c;
{

	if (i8251_model >= I8251_MODEL_I8251F)
		outb(iobase + serf_data, c);
	else
		outb(iobase + seri_data, c);
}

static int
i8251_stat_rok(iobase)
	int iobase;
{

	if (i8251_model >= I8251_MODEL_I8251F)
		return (ISSET(inb(iobase + serf_lsr), FLSR_RXRDY));
	else
		return (ISSET(inb(iobase + seri_lsr), LSR_RXRDY));
}

static int
i8251_stat_wok(iobase)
{

	if (i8251_model >= I8251_MODEL_I8251F)
		return (ISSET(inb(iobase + serf_lsr), FLSR_TXRDY));
	else
		return (ISSET(inb(iobase + seri_lsr), LSR_TXRDY));
}

static int
i8251f_probe(iobase)
	int iobase;
{
	u_int8_t stat, stat1, statd;
	int i;

	outb(iobase + serf_fcr, 0);
	delay(10);

	for (i = 0; i < 10; i ++)
	{
		stat = inb(iobase + serf_fcr);
		if ((stat & 1) == 0)
		{
			stat = inb(iobase + serf_lsr);

			outb(iobase + serf_fcr, FIFO_ENABLE);
			delay(100);

			stat1 = inb(iobase + serf_lsr);
			statd = inb(iobase + serf_divr);

			outb(iobase + serf_fcr, 0);
			delay(100);

			if (stat1 == stat)
				return I8251_MODEL_I8251;

			if (statd != (u_int8_t) -1)
				return I8251_MODEL_I8251AF;

			return I8251_MODEL_I8251F;
		}

		delay(10);
	}

	return I8251_MODEL_I8251;
}

void
i8251_init(iobase, ospeed)
	int iobase;
	u_long ospeed;
{
	extern void conputc __P((int));
	u_int8_t icrs;
	int i, div, c = 'I';

	/* probe I8251F */
	i8251_model = i8251f_probe(iobase);

	/* disable interrupts */
	icrs = inb(iobase + seri_icr);
	icrs &= ~(IEN_Rx | IEN_TxEMP | IEN_Tx);
	outb(iobase + seri_icr, icrs);

	/* setup speed */
	if (ospeed == 0)
		ospeed = 9600;

	if (ospeed > 38400 && i8251_model == I8251_MODEL_I8251AF)
	{
		div = 115200 / ospeed;
		outb(iobase + serf_divr, 0x80 | div);
	}
	else
	{
		if ((get_biosparam(0x501) & 0x80) != 0)
			div = (9600 * 13) / ospeed;
		else
			div =  (38400 * 4) / ospeed;

		outb(iobase + serf_divr, 0);

		outb(timer_ctlr, 0xb6);		/* counter #2 mode 3 */
		outb(timer_c2r, div & 0xff);
		outb(timer_c2r, (div >> NBBY) & 0xff);
	}

	/* setup I8251 core */
	for (i = 0; i < 3; i ++)
	{
		outb(iobase + seri_cmdr, 0);
		delay(100);
	}

	outb(iobase + seri_cmdr, CMD8251_RESET);
	delay(100);
	outb(iobase + seri_modr, MOD8251_8BITS | MOD8251_STOPB | MOD8251_BAUDR);
	delay(100);
	outb(iobase + seri_cmdr, 
	     (CMD8251_TxEN | CMD8251_RxEN | CMD8251_DTR | CMD8251_RTS));
	delay(100);

	/* setup I8251F fifo */
	if (i8251_model >= I8251_MODEL_I8251F)
	{
		outb(iobase + serf_fcr, FIFO_ENABLE | FIFO_TRIGGER_4 |
				        FIFO_XMT_RST | FIFO_RCV_RST);
		c = (i8251_model == I8251_MODEL_I8251AF) ? 'A' : 'F';
	}

	conputc(c);
}

/***********************************************************
 * Generic IO
 ***********************************************************/
/*
 * get a character
 */
int
comgetc_d(iobase)
	int iobase;
{
	u_int8_t c;

	if (serbuf_read != serbuf_write)
	{
		c = serbuf[serbuf_read++];
		if (serbuf_read >= SERBUFSIZE)
			serbuf_read = 0;
		return c;
	}

	for (;;)
	{
		while (i8251_stat_rok(iobase) == 0)
			;
		c = i8251_read_data_1(iobase);
		if (c != XOFF)
		{
			stopped = 0;
			break;
		}
		stopped = 1;
	}
	return (c);
}

int
computc_d(c, iobase)
	int c;
	int iobase;
{
	int cc, timo;

	while (stopped != 0)
		comgetc_d(iobase);	

	if (comstatus_d(iobase) != 0)
	{
		cc = comgetc_d(iobase);
		serbuf[serbuf_write++] = cc;
		if (serbuf_write >= SERBUFSIZE)
			serbuf_write = 0;
	}

	timo = 50000;
	while (i8251_stat_wok(iobase) == 0 && -- timo)
		;
	if (timo <= 0)
		return 0;

	i8251_write_data_1(iobase, c);

	timo = 1500000;
	while (i8251_stat_wok(iobase) == 0 && -- timo)
		;
	if (timo <= 0)
		return 0;

	return 1;
}

void
cominit_d(iobase)
	int iobase;
{

	serbuf_read = 0;
	serbuf_write = 0;

	i8251_init(iobase, CONSPEED);
}

int
comstatus_d(iobase)
	int iobase;
{
	int c;

	if (serbuf_read != serbuf_write)
		return 1;

	if (i8251_stat_rok(iobase) != 0)
	{
		c = i8251_read_data_1(iobase);
		if (c == XOFF)
		{
			stopped = 1;
		}
		else
		{
			serbuf[serbuf_write++] = c;
			if (serbuf_write >= SERBUFSIZE)
				serbuf_write = 0;
			return 1;
		}
	}
	return 0;	
}
#endif	/* SUPPORT_SERIAL */
