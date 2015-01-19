/*	$NecBSD: mpu_isa.c,v 1.5 1999/07/31 15:18:33 honda Exp $	*/
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
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <dev/isa/isavar.h>
#include <i386/Cbus/dev/mpu401var.h>

#define	MPU401_NPORTS	2

struct mpuhw {
	int mh_flags;
	bus_space_iat_t mh_iat;
};

static int	mpu_isa_match __P((struct device *, struct cfdata *, void *));
void	mpu_isa_attach __P((struct device *, struct device *, void *));
int	mpu_isa_map __P((bus_space_tag_t, bus_addr_t, struct mpuhw *, bus_space_handle_t *));
extern  struct dvcfg_hwsel mpuhwsel;

struct mpu_isa_softc {
	struct mpu401_softc sc_mpu401;
};

struct cfattach mpu_isa_ca = {
	sizeof(struct mpu_isa_softc), mpu_isa_match, mpu_isa_attach
};

static int
mpu_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t mpuh;
	struct mpuhw *hw;
	int rv = 0;

	if ((hw = DVCFG_HW(&mpuhwsel, DVCFG_MAJOR(ia->ia_cfgflags))) == NULL)
		return EINVAL;

	if (mpu_isa_map(iot, ia->ia_iobase, hw, &mpuh))
		return rv;

	if (mpu401_probesubr(iot, mpuh) >= 0)
	{
		rv = 1;
		ia->ia_iosize = MPU401_NPORTS;
	}

	bus_space_unmap(iot, mpuh, MPU401_NPORTS);
	return rv;
}

void
mpu_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	struct mpu_isa_softc *sc = (void *) self;
	struct mpu401_softc *msc = &sc->sc_mpu401;
	bus_space_handle_t mpuh;
	struct mpuhw *hw;

	printf(": UART mode\n");

	hw = DVCFG_HW(&mpuhwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (mpu_isa_map(iot, ia->ia_iobase, hw, &mpuh))
	{
		printf("%s: can not map\n", msc->sc_dev.dv_xname);
		return;
	}

	msc->sc_iot = ia->ia_iot;
	msc->sc_ioh = mpuh;
	mpu401_attachsubr(msc);

	if (ia->ia_irq != IRQUNK)
		isa_intr_establish(ia->ia_ic, ia->ia_irq,
			 	   IST_EDGE, IPL_AUDIO, mpu401_intr, msc);
}

int
mpu_isa_map(iot, iobase, hw, mpuhp)
	bus_space_tag_t iot;
	bus_addr_t iobase;
	struct mpuhw *hw;
	bus_space_handle_t *mpuhp;
{

	if (iobase == IOBASEUNK)
		return EINVAL;

	if (bus_space_map(iot, iobase, 0, 0, mpuhp) ||
	    bus_space_map_load(iot, *mpuhp, MPU401_NPORTS, hw->mh_iat,
			       BUS_SPACE_MAP_FAILFREE))
		return ENOSPC;

	return 0;
}

/***************************************************
 * hw selection table
 ***************************************************/
static struct mpuhw mpuhw_mpu98 = {
	0,
	BUS_SPACE_IAT_2,
};

static struct mpuhw mpuhw_mpuAT = {
	0,
	BUS_SPACE_IAT_1,
};

static bus_addr_t mpuSB98_iat[] = {
	0x0,
	0x100
};

static struct mpuhw mpuhw_mpuSB98 = {
	0,
	mpuSB98_iat,
};

static dvcfg_hw_t mpuhwsel_array[] = {
/* 0x00 */	&mpuhw_mpu98,		/* default */
/* 0x01 */	&mpuhw_mpu98,		/* pc-98 map */
/* 0x02 */	&mpuhw_mpuAT,		/* at map */
/* 0x03 */	&mpuhw_mpuSB98,		/* sound blaster pc-98 map */
};

static struct dvcfg_hwsel mpuhwsel = {
	DVCFG_HWSEL_SZ(mpuhwsel_array),
	mpuhwsel_array
};
