/*	$NecBSD: tipcreg.h,v 1.4 1998/03/14 07:10:19 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 * Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting stuff. All rights reserved.
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

#define	TIPC_DCR_BASE			0x90
#define TIPC_DCR_IMODE_MASK		0x00060000
#define TIPC_DCR_IMODE_ISA		0x00020000
#define TIPC_DCR_IMODE_SERIRQ		0x00040000

/* should move cardbus definitions */
#define CARDBUS_BRIDGE_CONTROL		0x3c
#define CARDBUS_BCR_PARITY_ENA		0x00010000
#define CARDBUS_BCR_SERR_ENA		0x00020000
#define CARDBUS_BCR_ISA_ENA		0x00040000
#define CARDBUS_BCR_VGA_ENA		0x00080000
#define CARDBUS_BCR_MABORT		0x00200000
#define CARDBUS_BCR_CB_RESET		0x00400000
#define CARDBUS_BCR_ISA_IRQ		0x00800000
#define CARDBUS_BCR_PREFETCH_0		0x01000000
#define CARDBUS_BCR_PREFETCH_1		0x02000000
#define CARDBUS_BCR_WRITE_POST		0x04000000

#define CARDBUS_SUBSYSTEM_VENDOR_ID	0x40
#define CARDBUS_SUBSYSTEM_ID		0x42
#define CARDBUS_LEGACY_MODE_BASE	0x44
