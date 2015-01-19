/*	$NecBSD: ser_pc98_Cbus.c,v 1.2 1999/04/15 01:36:18 kmatsuda Exp $	*/
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
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/ser_pc98_Cbus.h>
#include "serf.h"
#include "seri.h"
#include "sera.h"
#include "serb.h"
#include "sern.h"
#include "serh.h"

#define	serdecl(c, name)	extern struct com_hw name
#define	serent(c, name)		(c > 0 ? &##name : NULL)

serdecl((NSERI + NSERF),com_hw_i8251);
serdecl((NSERI + NSERF),com_hw_i8251_ext1);
serdecl((NSERI + NSERF),com_hw_i8251_ext2);
serdecl(NSERI,com_hw_i8251_LNDSPT_ext1);
serdecl(NSERI,com_hw_i8251_LNDSPT_ext2);
serdecl(NSERA,com_hw_RSA98);
serdecl(NSERN,com_hw_pure);
serdecl(NSERN,com_hw_NEC_2nd);
serdecl(NSERN,com_hw_MC16550);
serdecl(NSERN,com_hw_MCRS98);
serdecl(NSERB,com_hw_RSB2000);
serdecl(NSERB,com_hw_RSB4000);
serdecl(NSERN,com_hw_RSB384);
serdecl(NSERN,com_hw_nsmodem);
serdecl(NSERA,com_hw_RSA98III);
serdecl(NSERN,com_hw_emul_hayes);
serdecl(NSERH,com_hw_hayes_sample);

static dvcfg_hw_t com_hwsel_array[] = {
/* 0x00 */	serent((NSERI + NSERF),com_hw_i8251),
/* 0x01 */	serent((NSERI + NSERF),com_hw_i8251_ext1),
/* 0x02 */	serent((NSERI + NSERF),com_hw_i8251_ext2),
/* 0x03 */	serent(NSERI,com_hw_i8251_LNDSPT_ext1),
/* 0x04 */	serent(NSERI,com_hw_i8251_LNDSPT_ext2),
/* 0x05 */	NULL,
/* 0x06 */	NULL,
/* 0x07 */	NULL,
/* 0x08 */	NULL,
/* 0x09 */	NULL,
/* 0x0a */	NULL,
/* 0x0b */	NULL,
/* 0x0c */	NULL,
/* 0x0d */	NULL,
/* 0x0e */	NULL,
/* 0x0f */	NULL,
/* 0x10 */	serent(NSERA,com_hw_RSA98),
/* 0x11 */	serent(NSERN,com_hw_pure),
/* 0x12 */	serent(NSERN,com_hw_NEC_2nd),
/* 0x13 */	serent(NSERN,com_hw_MC16550),
/* 0x14 */	serent(NSERN,com_hw_MCRS98),
/* 0x15 */	serent(NSERB,com_hw_RSB2000),
/* 0x16 */	serent(NSERN,com_hw_RSB384),
/* 0x17 */	serent(NSERN,com_hw_nsmodem),
/* 0x18 */	serent(NSERA,com_hw_RSA98III),
/* 0x19 */	serent(NSERN,com_hw_emul_hayes),
/* 0x1a */	serent(NSERB,com_hw_RSB4000),
/* 0x1b */	NULL,
/* 0x1c */	NULL,
/* 0x1d */	NULL,
/* 0x1e */	NULL,
/* 0x1f */	NULL,
/* 0x20 */	serent(NSERH,com_hw_hayes_sample),
};

struct dvcfg_hwsel com_hwsel = {
	DVCFG_HWSEL_SZ(com_hwsel_array),
	com_hwsel_array
};

struct com_hw *
ser_find_hw(cfgflags)
	u_int cfgflags;
{
	struct com_hw *hw;

	hw = DVCFG_HW(&com_hwsel, DVCFG_MAJOR(cfgflags));
	return hw;
}

void
ser_setup_ca(ca, hw)
	struct commulti_attach_args *ca;
	struct com_hw *hw;
{

	ca->ca_type = hw->type;
	ca->ca_subtype = hw->subtype;
	ca->ca_hwflags = hw->hwflags;
	ca->ca_freq = hw->freq;
	ca->ca_speedtab = hw->spt;
	ca->ca_speedfunc = hw->speedfunc;
	ca->ca_h.cs_control_intr = hw->control_intr;
	ca->ca_h.cs_read_msr = hw->read_msr;
}
