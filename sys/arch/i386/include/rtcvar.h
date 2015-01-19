/*	$NecBSD: rtcvar.h,v 3.5 1998/03/14 07:07:59 kmatsuda Exp $	*/
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

#ifndef _I386_RTCVAR_H_
#define _I386_RTCVAR_H_

#define	MC_SEC		0x0	/* Time of year: seconds (0-59) */
#define	MC_ASEC		0x1	/* Alarm: seconds */
#define	MC_MIN		0x2	/* Time of year: minutes (0-59) */
#define	MC_AMIN		0x3	/* Alarm: minutes */
#define	MC_HOUR		0x4	/* Time of year: hour (see above) */
#define	MC_AHOUR	0x5	/* Alarm: hour */
#define	MC_DOW		0x6	/* Time of year: day of week (1-7) */
#define	MC_DOM		0x7	/* Time of year: day of month (1-31) */
#define	MC_MONTH	0x8	/* Time of year: month (1-12) */
#define	MC_YEAR		0x9	/* Time of year: year in century (0-99) */
#define	MC_NTODREGS	0xa	/* 10 of those regs are for TOD and alarm */

typedef u_int mc_todregs[MC_NTODREGS];

#ifdef	_KERNEL
#include <i386/isa/pd4990var.h>

#define	rtc_init(t)		pd4990_init((t))
#define	rtc_puttod(t, regs)	pd4990_puttod((t), (regs))
#define	rtc_gettod(t, regs)	pd4990_gettod((t), (regs))
#endif	/* _KERNEL */
#endif	/* !_I386_RTCVAR_H_ */
