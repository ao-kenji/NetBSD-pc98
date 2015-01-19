/*	$NecBSD: if_ephwtab.h,v 1.1 1998/10/01 05:38:01 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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
 * 3COM ETHERLINK III PC-98 DRIVER
 * written by N. Honda.
 */

#include <machine/dvcfg.h>

static struct ep_hw ep_hw_pure = {
	EP_HW_DEFAULT,	/* subtype */
	0x100,	/* id_iobase */
	0x200,	/* iobase */
	0x10,	/* ioskip */
	0x5090,	/* prodID */
};

static struct ep_hw ep_hw_3com98 = {
	EP_HW_3COM98,	/* subtype */
	0x70d0,	/* id_iobase */
	0x40d0,	/* iobase */
	0x100,	/* ioskip */
	0x5690,	/* prodID */
};

/* hardware tabs */
static dvcfg_hw_t ep_hwsel_array[] = {
/* 0x00 */	&ep_hw_3com98,
/* 0x01 */	&ep_hw_pure
};

struct dvcfg_hwsel ep_hwsel = {
	DVCFG_HWSEL_SZ(ep_hwsel_array),
	ep_hwsel_array
};
