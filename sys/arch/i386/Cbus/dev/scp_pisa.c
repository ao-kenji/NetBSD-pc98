/*	$NecBSD: scp_pisa.c,v 1.9 1999/08/02 17:17:43 honda Exp $	*/
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

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>
#include <i386/Cbus/dev/scpvar.h>

#include <i386/pccs/tuple.h>

static int	scp_pisa_match __P((struct device *, struct cfdata *, void *));
void	scp_pisa_attach __P((struct device *, struct device *, void *));
int	scp_pisa_activate __P((pisa_device_handle_t));
int	scp_pisa_intr __P((void *));

struct scp_pisa_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	void *sc_wss;
	int (*sc_wss_activate) __P((void *));
	int (*sc_wss_intr) __P((void *));

	void *sc_mpu;
	int (*sc_mpu_activate) __P((void *));
	int (*sc_mpu_intr) __P((void *));

	u_int8_t sc_irr;
};

#define	SCP_ICR	0x0a		/* interrupt line control */
#define	ICR_MPU	0x01		/* mpu intr enable */
#define	ICR_PCM	0x04		/* pcm intr enable */	

extern struct cfdriver scp_cd;

struct cfattach scp_pisa_ca = {
	sizeof(struct scp_pisa_softc), scp_pisa_match, scp_pisa_attach
};

struct pisa_functions scp_pd = {
	scp_pisa_activate, NULL
};

static int
scp_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	bus_space_tag_t iot = pa->pa_iot;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t scph;
	int rv = 0;

	if (pisa_space_map(dh, PISA_IO0, &scph))
		return rv;

	rv = 1;
	pisa_space_unmap(dh, iot, scph);
	return rv;
}

void
scp_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	struct scp_pisa_softc *sc = (void *) self;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t scph, adh;
	bus_addr_t ad1848iat[4] = {2, 3, 0, 1};
	struct scp_attach_args sa;
	u_int8_t regv = 0;

	printf("\n");

	if (pisa_space_map(dh, PISA_IO0, &scph))
	{
		printf("%s: can not map\n", sc->sc_dev.dv_xname);
		return;
	}

	pisa_register_functions(dh, self, &scp_pd);

	sc->sc_iot = iot;
	sc->sc_ioh = scph;

	/* allocate subregion with bus map load */
	if (bus_space_subregion(iot, scph, 6, 0, &adh) ||
	    bus_space_map_load(iot, adh, 4, ad1848iat, BUS_SPACE_MAP_FAILFREE))
	{
		printf("%s: can not map(wss)\n", sc->sc_dev.dv_xname);
		return;
	}

	sa.sa_iot = iot;
	sa.sa_ic = pa->pa_ic;

	sa.sa_ioh = adh;
	sa.sa_name = "wss";
	sa.sa_arg = sa.sa_intr = sa.sa_activate = NULL;
	config_found(self, &sa, NULL);
	sc->sc_wss = sa.sa_arg;
	sc->sc_wss_activate = sa.sa_activate;
	sc->sc_wss_intr = sa.sa_intr;
	if (sc->sc_wss_intr != NULL)
		regv |= ICR_PCM;

	sa.sa_ioh = scph;
	sa.sa_name = "mpu";
	sa.sa_arg = sa.sa_intr = sa.sa_activate = NULL;
	config_found(self, &sa, NULL);
	sc->sc_mpu = sa.sa_arg;
	sc->sc_mpu_activate = sa.sa_activate;
	sc->sc_mpu_intr = sa.sa_intr;
	if (sc->sc_mpu_intr != NULL)
		regv |= ICR_MPU;

	bus_space_write_1(iot, scph, SCP_ICR, 0);
	bus_space_write_1(iot, scph, SCP_ICR, regv);
	sc->sc_irr = regv;

	pisa_intr_establish(dh, PISA_PIN0, IPL_AUDIO, scp_pisa_intr, sc);
}

int
scp_pisa_activate(dh)
	pisa_device_handle_t dh;
{
    	struct scp_pisa_softc *sc = PISA_DEV_SOFTC(dh);

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SCP_ICR, 0);

	if (sc->sc_wss_activate != NULL)
		(*sc->sc_wss_activate) (sc->sc_wss);

	if (sc->sc_mpu_activate != NULL)
		(*sc->sc_mpu_activate) (sc->sc_mpu);
	
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SCP_ICR, sc->sc_irr);
    	return 0;
}

int
scp_pisa_intr(arg)
	void *arg;
{
	struct scp_pisa_softc *sc = arg;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SCP_ICR, 0);

	if (sc->sc_wss_intr != NULL)
		(*sc->sc_wss_intr) (sc->sc_wss);

	if (sc->sc_mpu_intr != NULL &&
	    (bus_space_read_1(sc->sc_iot, sc->sc_ioh, 1) & 0x80) == 0)
		(*sc->sc_mpu_intr) (sc->sc_mpu);

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SCP_ICR, sc->sc_irr);
	return 0;
}
