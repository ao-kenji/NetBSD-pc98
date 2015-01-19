/*	$NecBSD: syspmgr.c,v 1.9 1998/12/31 02:38:30 honda Exp $	*/
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

#define	BUS_SPACE_MAP_INIAT

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
#include <machine/syspmgr.h>
#include <machine/timervar.h>

#include <dev/isa/isareg.h>
#include <i386/isa/timerreg.h>
#include <i386/isa/spkrreg.h>

struct systmport_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_Aioh;
	bus_space_handle_t sc_Bioh;
	bus_space_handle_t sc_Cioh;
	bus_space_handle_t sc_MCioh;
	bus_space_handle_t sc_delayioh;
} systmport_softc;	

#define	sysp_A	0
#define	sysp_B	1
#define	sysp_C	2
#define	sysp_MC 3

void
syspmgr_init(st)
	syspmgr_tag_t st;
{
	struct systmport_softc *sc = &systmport_softc;
	bus_space_tag_t iot = I386_BUS_SPACE_IO;

	strncpy("syspmgr", sc->sc_dev.dv_xname, 16);
	if (bus_space_subregion(iot, BUS_SPACE_SYSTM_HANDLE,
				IO_PORTA, 1, &sc->sc_Aioh) ||
	    bus_space_subregion(iot, BUS_SPACE_SYSTM_HANDLE,
				IO_PORTB, 1, &sc->sc_Bioh) ||
	    bus_space_subregion(iot, BUS_SPACE_SYSTM_HANDLE,
				IO_PORTC, 1, &sc->sc_Cioh) ||
	    bus_space_subregion(iot, BUS_SPACE_SYSTM_HANDLE,
				IO_PORTMC, 1, &sc->sc_MCioh) ||
	    bus_space_map(iot, 0x5f, 1, 0, &sc->sc_delayioh))
		panic("%s: init fail", sc->sc_dev.dv_xname);
	sc->sc_iot = iot;
}

int
syspmgr(st, cmd)
	syspmgr_tag_t st;
	u_int cmd;
{
	struct systmport_softc *sc = &systmport_softc;

	bus_space_write_1(sc->sc_iot, sc->sc_MCioh, 0, (u_int8_t) cmd);
	return 0;
}

int
syspmgr_alloc_delayh(iotp, iohp)
	bus_space_tag_t *iotp;
	bus_space_handle_t *iohp;
{
	struct systmport_softc *sc = &systmport_softc;

	*iotp = sc->sc_iot;
	*iohp = sc->sc_delayioh;
	return 0;
}

int
syspmgr_alloc_systmph(iotp, iohp, chan)
	bus_space_tag_t *iotp;
	bus_space_handle_t *iohp;
	syspmgr_channel_t chan;
{
	struct systmport_softc *sc = &systmport_softc;

	*iotp = sc->sc_iot;
	switch (chan)
	{
	case SYSPMGR_PORTA:
		*iohp = sc->sc_Aioh;
		break;
	case SYSPMGR_PORTB:
		*iohp = sc->sc_Bioh;
		break;
	case SYSPMGR_PORTC:
		*iohp = sc->sc_Cioh;
		break;
	case SYSPMGR_PORTMC:
		*iohp = sc->sc_MCioh;
		break;
	default:
		return EINVAL;
	}
	return 0;
}
