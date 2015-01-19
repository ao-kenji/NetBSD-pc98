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

#ifndef _I386_VSC_KBDREG_H_
#define _I386_VSC_KBDREG_H_
#include <dev/ic/i8251reg.h>

/*
 * KBDC port defintions
 */
/* status */
#define	kbd_status	(1)
#define	KBST_READY	LSR_RXRDY
#define	KBST_ERRMASK	(LSR_PE | LSR_OE | LSR_FE)

/* ctrl */
#define	kbd_ctrl	(1)
#define	KBCR_TEN	(CMD8251_TxEN)
#define	KBCR_DTR	(CMD8251_DTR)
#define	KBCR_REN	(CMD8251_RxEN)
#define	KBCR_ECL	(CMD8251_ER)
#define	KBCR_RESET	(CMD8251_RESET)

/* data port */
#define	KBDR_MAKEBIT	0x80
#define	KBDR_CODEMASK	0x7f
#define	kbd_data	(0)

/*
 * KBDC kbd command defintions
 */
#define	KBC_ANS_ACK	0xfa
#define	KBC_ANS_NACK	0xfc

#define	KBC_CMD_LED	0x9d
#define	CANALED		0x08
#define	CAPSLED		0x04
#define	NUMLED		0x01
#define	BASELED		0x70
#endif /* !_I386_VSC_KBDREG_H_ */
