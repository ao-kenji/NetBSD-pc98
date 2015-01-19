/*	$NecBSD: mpu_pisa.c,v 1.12 1999/07/31 15:18:34 honda Exp $	*/
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

#include <i386/Cbus/dev/qwsvar.h>
#include <i386/Cbus/dev/mpu401var.h>

#include <i386/pccs/tuple.h>

static int	mpu_pisa_match __P((struct device *, struct cfdata *, void *));
void	mpu_pisa_attach __P((struct device *, struct device *, void *));
int	mpu_pisa_map __P((pisa_device_handle_t, bus_space_tag_t, int, bus_space_handle_t *));
void	mpu_pisa_init __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, int));
int	mpu_pisa_hw __P((pisa_device_handle_t));
int	mpu_pisa_activate __P((pisa_device_handle_t));
void 	scp_init __P((bus_space_tag_t, bus_space_handle_t));

struct mpu_pisa_softc {
	struct mpu401_softc sc_mpu401;

	int sc_model;
};

struct cfattach mpu_pisa_ca = {
	sizeof(struct mpu_pisa_softc), mpu_pisa_match, mpu_pisa_attach
};

struct pisa_functions mpu_pd = {
	mpu_pisa_activate, NULL
};

#define	MPU401_PISA_98		0
#define	MPU401_PISA_AT		1
#define	MPU401_PISA_QVISION	2
#define	MPU401_PISA_SCP		3

static int
mpu_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	bus_space_tag_t iot = pa->pa_iot;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t mpuh;
	int model, rv = 0;

	model = mpu_pisa_hw(dh);
	if (mpu_pisa_map(dh, iot, model, &mpuh))
		return rv;

	mpu_pisa_init(dh, iot, mpuh, model);
	if (mpu401_probesubr(iot, mpuh) >= 0)
		rv = 1;

	pisa_space_unmap(dh, iot, mpuh);
	return rv;
}

void
mpu_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	struct mpu_pisa_softc *sc = (void *) self;
	struct mpu401_softc *msc = &sc->sc_mpu401;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t mpuh;

	printf(": UART mode\n");

	sc->sc_model = mpu_pisa_hw(dh);
	if (mpu_pisa_map(dh, pa->pa_iot, sc->sc_model, &mpuh))
		return;

	pisa_register_functions(dh, self, &mpu_pd);

	mpu_pisa_init(dh, pa->pa_iot, mpuh, sc->sc_model);

	msc->sc_iot = pa->pa_iot;
	msc->sc_ioh = mpuh;
	mpu401_attachsubr(msc);

	/*
	 * In the future, kernel supports our mpu401 interface!
	 */
    	pisa_intr_establish(pa->pa_dh, PISA_PIN0, IPL_AUDIO, mpu401_intr, msc);
}

int
mpu_pisa_activate(dh)
	pisa_device_handle_t dh;
{
    	struct mpu_pisa_softc *sc = PISA_DEV_SOFTC(dh);
	struct mpu401_softc *msc = &sc->sc_mpu401;

	mpu_pisa_init(dh, msc->sc_iot, msc->sc_ioh, sc->sc_model);
	mpu401_activate(msc);
    	return 0;
}

int
mpu_pisa_map(dh, iot, model, mpuhp)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	int model;
	bus_space_handle_t *mpuhp;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_size_t size = PISA_DR_IMS(dr, PISA_IO0);
	bus_space_iat_t iatp;

	if (size > 16)
		return EINVAL;

	if ((model & 1) == 0)
	{
		if (size < 4)
			return EINVAL;
		size = size / 2;
		iatp = BUS_SPACE_IAT_2;
	}
	else
	{
		if (size < 2)
			return EINVAL;	
		if (model == MPU401_PISA_SCP && size != 0x0c)
			return EINVAL;
		iatp = BUS_SPACE_IAT_1;
	}

	return pisa_space_map_load(dh, PISA_IO0, size, iatp, mpuhp);
}

void
mpu_pisa_init(dh, iot, ioh, model)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int model;
{

	switch (model)
	{
	case MPU401_PISA_SCP:
		scp_init(iot, ioh);
		break;
#ifdef	notyet
	case MPU401_PISA_QVISION:
		qv_init(iot, ioh);
		break;
#endif	/* notyet */
	}
}

int 
mpu_pisa_hw(dh)
	pisa_device_handle_t dh;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_size_t size = PISA_DR_IMS(dr, PISA_IO0);
	u_char *cp;

	switch (dh->dh_stag->st_type)
	{
	case PISA_SLOT_PnPISA:
		if (SLOT_DEVICE_IDENT_IDN(dh) & 0x80000000) /* XXX */
			return MPU401_PISA_98;
		else
			return MPU401_PISA_AT;

	case PISA_SLOT_PCMCIA:
		cp = SLOT_DEVICE_IDENT_IDS(dh);
		if (strncmp(cp, "Roland PCMCIA", 12) == 0)
			return MPU401_PISA_SCP;
		if (size < 4)
			return MPU401_PISA_AT;
		else
			return MPU401_PISA_98;
	}
	return MPU401_PISA_AT;
}
	
/************************************************************
 * Roland SCP specific 
 ************************************************************/
/*
 * Remark that SCP's pcm part is CS4231!
 * We can not use AD1848 code because the code heavily depends on
 * DMA structure, (and quite dirty .... Ghaaa).
 * Please write down ad1848 pio code! (register offset 0x8 -> 0x0b)
 */

#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>
#undef AD1848_IADDR
#undef AD1848_IDATA
#undef AD1848_STATUS
#undef AD1848_PIO	
#define AD1848_IADDR		0x08
#define AD1848_IDATA		0x09
#define AD1848_STATUS		0x0a
#define AD1848_PIO		0x0b
#define	SCP_ICR	0x0a		/* interrupt line control */

#define	ICR_MPU	0x01		/* mpu intr enable */
#define	ICR_PCM	0x04		/* pcm intr enable */	

static void scp_write_reg __P((bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int8_t));
static void scp_wait_busy __P((bus_space_tag_t, bus_space_handle_t));

/* initial sequences */
static void
scp_write_reg(iot, ioh, reg, regv)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t reg;
	u_int8_t regv;
{

	bus_space_write_1(iot, ioh, AD1848_IADDR, reg);
	bus_space_write_1(iot, ioh, AD1848_IDATA, regv);
}

static void
scp_wait_busy(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	int timeout = 100000;

	while (timeout > 0 &&
	       (bus_space_read_1(iot, ioh, AD1848_IADDR) & SP_IN_INIT))
		timeout--;
}

void
scp_init(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{

	scp_wait_busy(iot, ioh);
	scp_write_reg(iot, ioh, SP_MISC_INFO, 0x80 | MODE2); /* select mode2 */

	scp_wait_busy(iot, ioh);
	scp_write_reg(iot, ioh, SP_LEFT_AUX1_CONTROL, GAIN_12);
	scp_write_reg(iot, ioh, SP_RIGHT_AUX1_CONTROL, GAIN_12);
	scp_write_reg(iot, ioh, CS_LEFT_LINE_CONTROL, GAIN_12);
	scp_write_reg(iot, ioh, CS_RIGHT_LINE_CONTROL, GAIN_12);

	bus_space_write_1(iot, ioh, SCP_ICR, ICR_MPU);
}
