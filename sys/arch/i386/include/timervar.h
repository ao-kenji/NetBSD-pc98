/*	$NecBSD: timervar.h,v 3.8 1998/03/14 07:08:02 kmatsuda Exp $	*/
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
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef _I386_TIMERVAR_H_
#define _I386_TIMERVAR_H_

struct timer_tag {
	u_int tt_id;
};

typedef int timer_channel_t;
typedef struct timer_tag *timer_tag_t;

#define	SYSTM_TIMER_TAG	((timer_tag_t) 0)

#define	TMC_CLOCK	0
#define	TMC_SPKR	1
#define	TMC_RS232C	2
#define	TMC_MAX 	2

struct timer_mode {
	u_long tm_freq;

#define	TMM_RATEGEN	0x04
#define	TMM_SQWAVE	0x08
	int tm_mode;
};

#ifdef	_KERNEL
void timer_init __P((timer_tag_t));
int timer_set_mode __P((timer_tag_t, timer_channel_t, struct timer_mode *));
int timer_get_mode __P((timer_tag_t, timer_channel_t, struct timer_mode *));

#if	defined(I586_CPU) || defined(I686_CPU)
u_int Pcpu_get_cycle __P((void));
void Pcpu_setup_clock __P((void));
void Pcpu_clock_synch __P((void));

extern int Pcpu_synch_done, Pcpu_init_clock;
extern u_int Pcpu_base_usec, Pcpu_base_sec, Pcpu_clock_factor;
extern u_int clock_factor, clock_divisor;
#endif	/* I586_CPU || I686_CPU */
#endif	/* _KERNEL */
#endif /* _I386_TIMERVAR_H_ */
