/*	$NecBSD: pd4990.c,v 1.22 1998/12/31 02:38:18 honda Exp $	*/
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
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/isa/isareg.h>

#include <machine/bus.h>
#include <machine/syspmgr.h>
#include <machine/rtcvar.h>

#include <i386/isa/timerreg.h>
#include <i386/isa/pd4990reg.h>
#include <i386/isa/pd4990var.h>

struct rtc_softc rtc_gsc;

static void rtc_set_sbit __P((struct rtc_softc *, u_int, u_int));
static u_int rtc_get_sbit __P((struct rtc_softc *));
static void rtc_set_scmd __P((struct rtc_softc *, int));
static void rtc_out __P((struct rtc_softc *, u_int));
static u_int rtc_in __P((struct rtc_softc *));

/*********************************************
 * hw port access
 *********************************************/
static void
rtc_set_sbit(sc, bit, ebit)
	struct rtc_softc *sc;
	u_int bit, ebit;
{
	register bus_space_tag_t iot = sc->sc_iot;

	bit = (bit & 0x01) ? (SR_DI | SR_CMASK) : SR_CMASK;

	bus_space_write_1(iot, sc->sc_sdt, 0, bit);
	delay(2);
	bus_space_write_1(iot, sc->sc_sdt, 0, bit | ebit);
	delay(2);
	bus_space_write_1(iot, sc->sc_sdt, 0, bit);
	delay(2);
}

static u_int
rtc_get_sbit(sc)
	struct rtc_softc *sc;
{
	register bus_space_tag_t iot = sc->sc_iot;
	register u_int bit;

	bit = (u_int) bus_space_read_1(sc->sc_portBt, sc->sc_portBh, 0);
	bit = bit & DR_MASK;

	bus_space_write_1(iot, sc->sc_sdt, 0, SR_CLK | SR_CMASK);
	delay(2);
	bus_space_write_1(iot, sc->sc_sdt, 0, SR_CMASK);
	delay(2);

	return bit;
}

/*********************************************
 * low level
 *********************************************/
static void
rtc_set_scmd(sc, cmd)
	struct rtc_softc *sc;
	int cmd;
{

	rtc_set_sbit(sc, cmd >> 0, SR_CLK);
	rtc_set_sbit(sc, cmd >> 1, SR_CLK);
	rtc_set_sbit(sc, cmd >> 2, SR_CLK);
	rtc_set_sbit(sc, cmd >> 3, SR_CLK);
	rtc_set_sbit(sc, 0, SR_STB);
}

static u_int
rtc_in(sc)
	struct rtc_softc *sc;
{
	int i;
	u_int data = 0;

	for (i = 0; i < NBBY; i ++)
		data += (rtc_get_sbit(sc) << i);

	return data;
}

static void
rtc_out(sc, data)
	struct rtc_softc *sc;
	u_int data;
{
	int i;

	for (i = 0; i < NBBY; i++)
		rtc_set_sbit(sc, data >> i, SR_CLK);
}

/*********************************************
 * upper interface
 *********************************************/
void
pd4990_init(rt)
	rtc_tag_t rt;
{
	struct rtc_softc *sc = rt;
	bus_space_tag_t iot;
	int error;

	sc->sc_iot = iot = I386_BUS_SPACE_IO;
	error = bus_space_map(iot, IO_RTC, 1, 0, &sc->sc_sdt);
	if (error)
		panic("pd4990: io map failed(set)");
	syspmgr_alloc_systmph(&sc->sc_portBt, &sc->sc_portBh, SYSPMGR_PORTB);
}

void
pd4990_gettod(rt, regs)
	rtc_tag_t rt;
	mc_todregs *regs;
{
	struct rtc_softc *sc = rt;
	u_int s;

	rtc_set_scmd(sc, RTC_READ_TIME);
	rtc_set_scmd(sc, RTC_REG_SHIFT);
	delay(40);
	(*regs)[MC_SEC] = rtc_in(sc);
	(*regs)[MC_MIN] = rtc_in(sc);
	(*regs)[MC_HOUR] = rtc_in(sc);
	(*regs)[MC_DOM] = rtc_in(sc);
	s = rtc_in(sc);
	(*regs)[MC_DOW] = (s & 0x0f);
	s = ((s & 0xf0) >> 4);
	if (s >= 10)
		s = 0x10 + (s - 10);
	(*regs)[MC_MONTH] = s;
	(*regs)[MC_YEAR] = rtc_in(sc);
}

void
pd4990_puttod(rt, regs)
	rtc_tag_t rt;
	mc_todregs *regs;
{
	struct rtc_softc *sc = rt;
	u_int s;

	rtc_set_scmd(sc, RTC_REG_SHIFT);
	rtc_out(sc, (*regs)[MC_SEC]);
	rtc_out(sc, (*regs)[MC_MIN]);
	rtc_out(sc, (*regs)[MC_HOUR]);
	rtc_out(sc, (*regs)[MC_DOM]);
	s = (*regs)[MC_MONTH];
	if (s & 0xf0)
		s = 10 + (s & 0x0f);
	s <<= 4;
	s += ((*regs)[MC_DOW] & 0x0f);
	rtc_out(sc, s);
	rtc_out(sc, (*regs)[MC_YEAR]);
	rtc_set_scmd(sc, RTC_TIMER_SET);
	rtc_set_scmd(sc, RTC_REG_HOLD);
}
