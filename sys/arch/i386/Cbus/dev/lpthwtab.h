/*	$NecBSD: lpthwtab.h,v 1.1 1998/10/01 05:38:02 honda Exp $	*/
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

/*
 * LPT HW DEFINITIONS
 * By N. Honda
 */

void output_NECTRAD __P((void *));
void output_PS2 __P((void *));
int not_ready_NECTRAD __P((void *));
int not_ready_PS2 __P((void *));
int not_ready_error_PS2 __P((void *));

static bus_addr_t lpt_hw_iat_TRADNEC[] = {
	0,	/* data */
	2,	/* status */
	6,	/* control */
	0,	/* X PnP config A */
	0,	/* X PnP config B */
	0,	/* X mode */
	0,	/* X ext mode */
};

static struct lpt_hw lpt_hw_TRADNEC = {
	LPT_HW_TRADNEC,	/* subtype */

	0x40,	/* fixed iobase */
	8,	/* fixed irq */

	8,	/* ports */

	lpt_hw_iat_TRADNEC,
	BUS_SPACE_IAT_SZ(lpt_hw_iat_TRADNEC),

	output_NECTRAD,
	not_ready_NECTRAD,
	not_ready_NECTRAD,	/* the same as above */
};

static bus_addr_t lpt_hw_iat_generic[] = {
	0,	/* data */
	1,	/* status */
	2,	/* control */
	0x0c,	/* PnP config A */
	0x0d,	/* PnP config B */
	9,	/* mode */
	0x0e,	/* ext mode */
};

static struct lpt_hw lpt_hw_NEC = {
	LPT_HW_NEC,	/* subtype */

	0x140,	/* fixed iobase */
	14,	/* fixed irq */

	0x10,	/* ports */

	lpt_hw_iat_generic,
	BUS_SPACE_IAT_SZ(lpt_hw_iat_generic),

	output_PS2,
	not_ready_PS2,
	not_ready_error_PS2,
};

static struct lpt_hw lpt_hw_IBM = {
	LPT_HW_PS2,	/* subtype */

	0,	/* fixed iobase */
	0,	/* fixed irq */

	0x10,	/* ports */

	lpt_hw_iat_generic,
	BUS_SPACE_IAT_SZ(lpt_hw_iat_generic),

	output_PS2,
	not_ready_PS2,
	not_ready_error_PS2,
};

static dvcfg_hw_t lpt_hwsel_array[] = {
/* 0x01 */	&lpt_hw_TRADNEC,
/* 0x02 */	&lpt_hw_NEC,
/* 0x03 */	&lpt_hw_IBM
};

struct dvcfg_hwsel lpt_hwsel = {
	DVCFG_HWSEL_SZ(lpt_hwsel_array),
	lpt_hwsel_array
};
