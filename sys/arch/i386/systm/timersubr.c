/*	$NecBSD: timersubr.c,v 1.11 1999/07/23 05:40:25 kmatsuda Exp $	*/
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

#include "opt_cputype.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>
#include <machine/cpufunc.h>
#include <machine/bus.h>
#include <machine/timervar.h>

#include <dev/isa/isareg.h>
#include <i386/isa/timerreg.h>
#include <i386/isa/spkrreg.h>
#include <i386/isa/pc98spec.h>

struct timer_state {
	u_long ts_freq;
	struct timer_mode ts_tm;
};

struct timer_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct timer_state sc_st[TMC_MAX + 1];
} timer_softc;

#define	tr_mod	3

void
timer_init(tt)
	timer_tag_t tt;
{
	struct timer_softc *sc = &timer_softc;
	struct timer_state *tsp = sc->sc_st;
	bus_space_tag_t iot = I386_BUS_SPACE_IO;
	bus_space_handle_t ioh;

	tsp[0].ts_freq = TIMER_FREQ;
	tsp[1].ts_freq = TIMER_FREQ;
	tsp[2].ts_freq = TIMER_FREQ / 16;

	strncpy("timer", sc->sc_dev.dv_xname, 16);
	if (bus_space_map(iot, IO_TIMER, 0, 0, &ioh) ||
	    bus_space_map_load(iot, ioh, 4, BUS_SPACE_IAT_2, 0))
		panic("timer: can not map io space");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
};

int
timer_set_mode(tt, chan, mp)
	timer_tag_t tt;
	timer_channel_t chan;
	struct timer_mode *mp;
{
	struct timer_softc *sc = &timer_softc;
	struct timer_state *tsp;
	u_long div;
	u_int8_t mode;

	if (chan < 0 || chan > TMC_MAX)
		return EINVAL;
 	tsp = &sc->sc_st[chan];

	if (mp->tm_freq == 0 || mp->tm_freq >= tsp->ts_freq)
		return EINVAL;

	div = (tsp->ts_freq * 2) / mp->tm_freq;
	div = (div / 2) + (div & 0x1);
	if (div <= 1)
		return EINVAL;

	switch (mp->tm_mode)
	{
	case TMM_RATEGEN:
		mode = TIMER_RATEGEN | TIMER_16BIT;
		break;
	case TMM_SQWAVE:
		mode = TIMER_SQWAVE | TIMER_16BIT;
		break;
	default:
		return EINVAL;
	}

	mode |= (chan << 6);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, tr_mod, mode);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, chan, div % 256);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, chan, div / 256);

	tsp->ts_tm = *mp;
	return 0;
}

int
timer_get_mode(tt, chan, mp)
	timer_tag_t tt;
	timer_channel_t chan;
	struct timer_mode *mp;
{
	struct timer_softc *sc = &timer_softc;
	struct timer_state *tsp;

	if (chan < 0 || chan > TMC_MAX)
		return EINVAL;

 	tsp = &sc->sc_st[chan];
	*mp = tsp->ts_tm;
	return 0;
}

#if	defined(I586_CPU) || defined(I686_CPU)
#define	SECTONSEC (1000ULL * 1000ULL * 1000ULL)
#define	SHIFT32	(0x100000000ULL)

int Pcpu_synch_done, Pcpu_init_clock;
u_int Pcpu_base_usec, Pcpu_base_sec, Pcpu_clock_factor;
u_int clock_factor, clock_divisor;

void
Pcpu_setup_clock(void)
{
	u_quad_t cpuns, clkns, cend;

	cend = (u_quad_t) Pcpu_get_cycle();
	if (cend == 0ULL)
		panic("clock: no system clock count");

	cpuns = (SECTONSEC * SHIFT32) / cend;
	Pcpu_clock_factor = (u_int) (cpuns / 1000ULL);

	clkns = (SECTONSEC * SHIFT32) / ((u_quad_t) (TIMER_FREQ));
	clock_factor = (u_int) (clkns  / 1000ULL);

	clock_divisor = TIMER_DIV(hz);

	printf("clock: use Pentium cpu: %d Khz (a clk %u.%u ns) rat %d\n", 
		(u_int) (cend / 1000ULL), 
		(u_int) (cpuns / SHIFT32), 
		(u_int) (((cpuns % SHIFT32) * 1000000ULL) / SHIFT32), 
		Pcpu_clock_factor);

	Pcpu_init_clock = 1;
}
#endif	/* PENTIUM_CPU_CLOCK */
