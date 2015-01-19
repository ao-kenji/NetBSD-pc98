/*	$NecBSD: yms.c,v 1.4 1999/07/23 11:04:42 honda Exp $	*/
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
#include <machine/pio.h>
#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/ic/ym2203reg.h>
#include <i386/Cbus/dev/ymsvar.h>
#include <i386/Cbus/dev/nec86reg.h>

#ifndef NEC86HW_NPOLL
#define NEC86HW_NPOLL	1000
#endif	/* !NEC86HW_NPOLL */

#ifndef OPNA_DELAY
/*
 * XXX -  Delaytime in microsecond after an access
 *	  to the OPNA addressing I/O port.
 */
#define OPNA_DELAY	10
#endif	/* !OPNA_DELAY */

/*
 * ISA device declaration
 */

extern struct cfdriver yms_cd;

int
yms_probesubr(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{

	if (opna_wait(iot, ioh) < 0)
		return -1;

	return 0;
}

void
yms_attachsubr(sc)
	struct yms_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	(void) opna_wait(iot, ioh);
	opna_disable(iot, ioh);
	sc->sc_enabled = 0;

	/* not yet */
}

int
yms_activate(sc)
	struct yms_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (opna_wait(iot, ioh) < 0)
		return EBUSY;

	opna_disable(iot, ioh);
	sc->sc_enabled = 0;
	return 0;
}

int
yms_deactivate(sc)
	struct yms_softc *sc;
{

	return 0;
}

/*
 * Wait for the OPNA to be ready, using polling.
 */
int
opna_wait(iot, ioh)
    bus_space_tag_t iot;
    bus_space_handle_t ioh;
{
    register int i = NEC86HW_NPOLL;

    while((bus_space_read_1(iot, ioh, OPN_STAT) & OPN_BUSY) && (i-- > 0))
	delay(10);

    /* If i is negative, then we may consider the OPNA is not present. */
    return i;
}

/*
 * Disable OPNA timer register
 */
void
opna_disable(iot, ioh)
   bus_space_tag_t iot;
   bus_space_handle_t ioh;
{

    /* Reset the OPNA timer register. */
    opna_write(iot, ioh, FM_MODE_TIMER, 0x30);
    (void) opna_wait(iot, ioh);
}

/*
 * Read/Write a byte from/to the OPNA register.
 */
u_int8_t
opna_read(iot, ioh, reg)
    bus_space_tag_t iot;
    bus_space_handle_t ioh;
    bus_addr_t reg;
{

    (void) opna_wait(iot, ioh);

    bus_space_write_1(iot, ioh, OPN_ADDR, reg);
    delay(OPNA_DELAY);
    return bus_space_read_1(iot, ioh, OPN_DATA);
}

void
opna_write(iot, ioh, reg, data)
    bus_space_tag_t iot;
    bus_space_handle_t ioh;
    bus_addr_t reg;
    u_int8_t data;
{

    (void) opna_wait(iot, ioh);

    bus_space_write_1(iot, ioh, OPN_ADDR, reg);
    delay(OPNA_DELAY);
    bus_space_write_1(iot, ioh, OPN_DATA, data);
}
