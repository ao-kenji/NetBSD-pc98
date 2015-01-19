/*	$NecBSD: boot_machdep.c,v 3.17 1998/12/31 02:38:17 honda Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>
#include <machine/systmbusvar.h>
#include <machine/syspmgr.h>
#include <machine/timervar.h>
#include <machine/rtcvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <i386/isa/pc98spec.h>
#include <i386/isa/icu.h>

static void hardware_init __P((void));
void pccs_shutdown __P((void));		/* XXX */
/*********************************************
 * minimal hardware setup
 *********************************************/
static void
hardware_init(void)
{
	extern void consinit __P((void));
	struct timer_mode tm;

	/* bus space */
	bus_space_init();

	/* initialize 8253 clock: for delay() funcs used in consinit */
	timer_init(SYSTM_TIMER_TAG);
	tm.tm_mode = TMM_RATEGEN;
	tm.tm_freq = hz;
	timer_set_mode(SYSTM_TIMER_TAG, TMC_CLOCK, &tm);

	/* system port */
	syspmgr_init(SYSTM_SYSPMGR_TAG);

	/* init console for panic msg */
	consinit();

	/* rtc */
	rtc_init(SYSTM_RTC_TAG);
}

void
shutdown_hw(void)
{

	disable_intr();

#include "pccs.h"
#if	NPCCS > 0
	pccs_shutdown();
#endif	/* NPCMCIA > 0 */

#ifdef	PANIC_HWHALT
	if (panicstr != NULL)
		while (1)
			;
#endif	/* PANIC_HWHALT */

	syspmgr(SYSTM_SYSPMGR_TAG, SYSPMGR_SHUT0_OFF);
	syspmgr(SYSTM_SYSPMGR_TAG, SYSPMGR_SHUT1_OFF);
	bus_space_write_1(I386_BUS_SPACE_IO, BUS_SPACE_SYSTM_HANDLE, 0xf0, 0);
	while (1)
		;
}

/*********************************************
 * boot up
 *********************************************/
void
startup_pc98(void)
{

	init_sysinfo();
	CPU_control_cpu_init();
	systmmsg_init();
	hardware_init();
}
