/*	$NecBSD: i8251reg.h,v 1.19 1998/12/31 02:38:50 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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

#ifndef	_I8251REG_H_
#define	_I8251REG_H_

/* interrupt identification register */
#define	IIR_IMASK	0x6
#define	IIR_RXTOUT	0x8
#define	IIR_RLS		0x6	/* Line status change */
#define	IIR_RXRDY	0x4	/* Receiver ready */
#define	IIR_TXRDY	0x2	/* Transmitter ready */
#define	IIR_NOPEND	0x1	/* Transmitter ready */
#define	IIR_MLSC	0x0	/* Modem status */

/* fifo control register */
#define	FIFO_ENABLE	0x01	/* Turn the FIFO on */
#define	FIFO_RCV_RST	0x02	/* Reset RX FIFO */
#define	FIFO_XMT_RST	0x04	/* Reset TX FIFO */
#define	FIFO_LSR_EN	0x08
#define	FIFO_MSR_EN	0x10
#define	FIFO_TRIGGER_1	0x00	/* Trigger RXRDY intr on 1 character */
#define	FIFO_TRIGGER_4	0x40	/* ibid 4 */
#define	FIFO_TRIGGER_8	0x80	/* ibid 8 */
#define	FIFO_TRIGGER_14	0xc0	/* ibid 14 */

/* I8251 line status register */
#define	LSR_TXRDY	0x01
#define	LSR_RXRDY	0x02
#define	LSR_TXE		0x04
#define	LSR_PE		0x08
#define	LSR_OE		0x10
#define	LSR_FE		0x20
#define	LSR_BI		0x40
#define	LSR_RCV_ERR	(LSR_BI | LSR_FE | LSR_PE | LSR_OE)

/* I8251F line status register */
#define	FLSR_TXRDY	0x02			/* XXX */
#define	FLSR_TXE	0x01			/* XXX */
#define	FLSR_RXRDY	0x04			/* XXX */
#define	FLSR_PE		LSR_PE
#define	FLSR_OE		LSR_OE
#define	FLSR_FE		LSR_FE
#define	FLSR_BI		LSR_BI
#define	FLSR_RCV_ERR	LSR_RCV_ERR

/* modem status register */
#define	MSR_DCD		0x80	/* Current Data Carrier Detect */
#define	MSR_RI		0x40	/* Current Ring Indicator */
#define	MSR_DSR		0x20	/* Current Data Set Ready */
#define	MSR_CTS		0x10	/* Current Clear to Send */
#define	MSR_DDCD	0x08	/* DCD has changed state */
#define	MSR_TERI	0x04	/* RI has toggled low to high */
#define	MSR_DDSR	0x02	/* DSR has changed state */
#define	MSR_DCTS	0x01	/* CTS has changed state */

/* define command and status code */
#define	CMD8251_TxEN	0x01	/* transmit enable */
#define	CMD8251_DTR	0x02	/* assert DTR */
#define	CMD8251_RxEN	0x04	/* receive enable */
#define	CMD8251_SBRK	0x08	/* send break */
#define	CMD8251_ER	0x10	/* error reset */
#define	CMD8251_RTS	0x20	/* assert RTS */
#define	CMD8251_RESET	0x40	/* internal reset */
#define	CMD8251_EH	0x80	/* enter hunt mode (only synchronous mode) */

#define	STS8251_TxRDY	0x01	/* transmit READY */
#define	STS8251_RxRDY	0x02	/* data exists in receive buffer */
#define	STS8251_TxEMP	0x04	/* transmit buffer EMPTY */
#define	STS8251_PE	0x08	/* perity error */
#define	STS8251_OE	0x10	/* overrun error */
#define	STS8251_FE	0x20	/* framing error */
#define	STS8251_BD_SD	0x40	/* break detect (async) / sync detect (sync) */
#define	STS8251_DSR	0x80	/* DSR is asserted */

#define	MOD8251_5BITS	0x00
#define	MOD8251_6BITS	0x04
#define	MOD8251_7BITS	0x08
#define	MOD8251_8BITS	0x0c
#define	MOD8251_PENAB	0x10
#define	MOD8251_PEVEN	0x20
#define	MOD8251_STOPB	(0x80 | 0x40)	/* 2 stop bit */
#define	MOD8251_BAUDR	0x02		/* x16 */

/* modem control register */
#define	MCR_LOOPBACK	0x10
#define	MCR_IENABLE	0x08
#define	MCR_DRS		0x04
#define	MCR_RTS		0x02
#define	MCR_DTR		0x01
#endif	/* !_I8251REG_H_ */
