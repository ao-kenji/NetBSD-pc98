/*	$NecBSD: mpu401.c,v 1.14 1999/08/02 17:17:40 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
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
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/pio.h>

#include <dev/midi_if.h>
#include <dev/packet_midi.h>

#include <i386/systm/sndtmvar.h>
#include <i386/Cbus/dev/mpu401var.h>

#include "sndtm.h"
#include "pmidi.h"
#include "midi.h"

#define	mpu401_data	0			/* data port(R/W) */
#define	MPU401_ACK		0xfe		/* ack */

#define	mpu401_stat	1			/* status port(R) */
#define	MPU401_SR_REMP		0x80		/* empty for read */
#define	MPU401_SR_WBSY		0x40		/* busy for write */
#define	MPU401_SR_TIMEOUT	0x80000000	/* timed out */

#define	mpu401_cmd	1			/* cmd port(W) */
#define	MPU401_CMD_RESET	0xff		/* reset cmd */
#define	MPU401_CMD_UART		0x3f		/* mode UART cmd */

#define	MPU401_TIMEOUT		0x8000		/* timeout loops */

extern struct cfdriver mpu_cd;

static void mpu401_write_cmd __P((bus_space_tag_t, bus_space_handle_t, u_int8_t));
static u_int mpu401_read_data __P((bus_space_tag_t, bus_space_handle_t, int));
static int mpu401_drain_data __P((struct mpu401_softc *, int));
static int mpu401_init __P((bus_space_tag_t, bus_space_handle_t));

static int mpu401_open __P((void *, int, void (*)__P((void *, int)), void (*)__P((void *)), void *));
static void mpu401_close __P((void *));
static int mpu401_output __P((void *, int));
static void mpu401_getinfo __P((void *, struct midi_info *));
static int mpu401_write_intr __P((void *));
/* static int mpu401_ioctl __P((u_long, caddr_t, int, struct proc *)); */

struct midi_hw_if mpu401_hw_if = {
	mpu401_open,
	mpu401_close,
	mpu401_output,
	mpu401_getinfo,
	NULL,
};

struct midi_info mpu401_info = {
	"mpu401",
	0
};

/******************************************
 * primitive
 ******************************************/
static void
mpu401_write_cmd(iot, ioh, data)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t data;
{
	int timeout = MPU401_TIMEOUT;

	while (bus_space_read_1(iot, ioh, mpu401_stat) & MPU401_SR_WBSY)
		if (timeout -- < 0)
			return;

	bus_space_write_1(iot, ioh, mpu401_cmd, data);
}

static u_int
mpu401_read_data(iot, ioh, timeout)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int timeout;
{

	while (bus_space_read_1(iot, ioh, mpu401_stat) & MPU401_SR_REMP)
		if (-- timeout < 0)
			return MPU401_SR_TIMEOUT;

	return ((u_int) bus_space_read_1(iot, ioh, mpu401_data));
}

static int
mpu401_init(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	int s, error = 0;

	mpu401_write_cmd(iot, ioh, MPU401_CMD_RESET);
	(void) mpu401_read_data(iot, ioh, MPU401_TIMEOUT);

	mpu401_write_cmd(iot, ioh, MPU401_CMD_RESET);
	(void) mpu401_read_data(iot, ioh, MPU401_TIMEOUT);

	s = splaudio();
	mpu401_write_cmd(iot, ioh, MPU401_CMD_UART);
	delay(10);
	if (bus_space_read_1(iot, ioh, mpu401_data) != MPU401_ACK)
		error = ENXIO;
	splx(s);

	return error;
}

static int
mpu401_drain_data(sc, on)
	struct mpu401_softc *sc;
	int on;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int maxloop;
	u_int ch;

	for (maxloop = 128; maxloop > 0; maxloop --)
	{
		ch = mpu401_read_data(iot, ioh, 0);
		if (ch & MPU401_SR_TIMEOUT)
			break;

		if (on != 0 && sc->sc_read != NULL)
			sc->sc_read(sc->sc_midi, ch);
	}

	return 0;
}
/******************************************
 * probe attach subr
 ******************************************/
int
mpu401_probesubr(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{

	if (mpu401_init(iot, ioh) != 0)
		return -1;

	return 0;
}

void
mpu401_attachsubr(sc)
	struct mpu401_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	(void) mpu401_init(iot, ioh);

#if	NMIDI > 0
	midi_attach_mi(&mpu401_hw_if, sc, &sc->sc_dev);
#endif  /* NPMIDI > 0 */
#if	NPMIDI > 0
	pmidi_attach_mi(&mpu401_hw_if, sc, &sc->sc_dev);
#endif	/* NPMIDI > 0 */
}

int
mpu401_activate(arg)
	void *arg;
{
	struct mpu401_softc *sc = arg;

	return mpu401_init(sc->sc_iot, sc->sc_ioh);
}

int
mpu401_intr(arg)
	void *arg;
{
	struct mpu401_softc *sc = arg;

	mpu401_drain_data(sc, 1);
	return 1;
}
	
static int
mpu401_write_intr(arg)
	void *arg;
{
	struct mpu401_softc *sc = arg;
	
	sc->sc_intr(sc->sc_midi);
	return 0;
}
/******************************************
 * upper interface
 ******************************************/
static int 
mpu401_open(arg, flags, iintr, ointr, a)
	void *arg;
	int flags;
	void (*iintr) __P((void *, int));
	void (*ointr) __P((void *));
	void *a;
{
	struct mpu401_softc *sc = arg;
	struct device *dvp = a;
	int s;
	
	if (sc->sc_read != NULL && sc->sc_read != iintr)
		return EBUSY;

	mpu401_drain_data(sc, 0);
	
	s = splaudio();
	sc->sc_midi = a;
	sc->sc_read = iintr;
	sc->sc_intr = ointr;
#if	NSNDTM > 0
	if (sc->sc_timer == 0)
	{
		if (strncmp(dvp->dv_xname, "pmidi", 4) == 0 &&
		    systm_sound_timer_add(sc, mpu401_write_intr, 1000) == 0)
			sc->sc_timer = 1;
	}
#endif	NSNDTM > 0
	splx(s);

	return 0;
}

static void
mpu401_close(arg)
	void *arg;
{
	struct mpu401_softc *sc = arg;
	int s;

	s = splaudio();
	sc->sc_read = NULL;
#if	NSNDTM > 0
	if (sc->sc_timer > 0)
	{
		sc->sc_timer = 0;
		systm_sound_timer_remove(sc);
	}
#endif	NSNDTM > 0
	splx(s);
}

static int
mpu401_output(arg, c)
	void *arg;
	int c;
{
	struct mpu401_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int timeout = MPU401_TIMEOUT;

	/* drain comming data */
	mpu401_drain_data(sc, 1);

	/* output a byte now */
	while (bus_space_read_1(iot, ioh, mpu401_stat) & MPU401_SR_WBSY)
		if (timeout -- < 0)
			return EBUSY;

	bus_space_write_1(iot, ioh, mpu401_data, (u_int8_t) c);

	return 0;
}
	
static void
mpu401_getinfo(addr, mi)
	void *addr;
	struct midi_info *mi;
{

	mi->name = "MPU-401 MIDI UART";
	mi->props = 0;
}
