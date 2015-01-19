/*	$NecBSD: mpu_qws.c,v 1.11 1999/07/31 15:18:34 honda Exp $	*/
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

static int	mpu_qws_match __P((struct device *, struct cfdata *, void *));
void	mpu_qws_attach __P((struct device *, struct device *, void *));
int	mpu_qws_map __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t *));

struct mpu_qws_softc {
	struct mpu401_softc sc_mpu401;
};

struct cfattach mpu_qws_ca = {
	sizeof(struct mpu_qws_softc), mpu_qws_match, mpu_qws_attach
};

static int
mpu_qws_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct qws_attach_args *qa = aux;
	bus_space_handle_t mpuh;
	bus_space_tag_t iot = qa->qa_iot;
	pisa_device_handle_t dh = qa->qa_dh;
	int rv = 0;

	if (strcmp("mpu", qa->qa_name) != 0)
		return rv;
	if (mpu_qws_map(dh, iot, &mpuh))
		return rv;
	if (mpu401_probesubr(iot, mpuh) >= 0)
		rv = 1;
	pisa_space_unmap(dh, iot, mpuh);
	return rv;
}

void
mpu_qws_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mpu_qws_softc *sc = (void *) self;
	struct mpu401_softc *msc = &sc->sc_mpu401;
	struct qws_attach_args *qa = aux;
	slot_device_res_t dr = PISA_RES_DR(qa->qa_dh);
	bus_space_handle_t mpuh;

	printf(": UART mode\n");

	if (mpu_qws_map(qa->qa_dh, qa->qa_iot, &mpuh))
		return;

	qa->qa_sc = sc;
	msc->sc_iot = qa->qa_iot;
	msc->sc_ioh = mpuh;
	mpu401_attachsubr(msc);

	if (PISA_DR_IRQN(dr, PISA_PIN1) != SLOT_DEVICE_UNKVAL)
		pisa_intr_establish(qa->qa_dh, PISA_PIN1, IPL_AUDIO,
					  mpu401_intr, msc);
}

int
mpu_qws_map(dh, iot, mpuhp)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	bus_space_handle_t *mpuhp;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_iat_t iatp;
	bus_addr_t iobase;
	u_int dvcfg;
	int idx;

	/* notyet:
	 * check AT's mpu401 io port allocation strategy.
	 */
 	dvcfg = 0;			/* 98 allocation */

	for (idx = 0; idx < SLOT_DEVICE_NIO; idx ++)
	{
		iobase = PISA_DR_IMB(dr, idx) & 0xf0f0;
		if (iobase != 0xe0d0 && iobase != 0xe0e0)
			continue;
		if (PISA_DR_IMS(dr, idx) == 2 || PISA_DR_IMS(dr, idx) == 4)
			break;
	}
	if (idx == SLOT_DEVICE_NIO)
		return EINVAL;

	if ((dvcfg & 1) == 0)
	{
		PISA_DR_IMS(dr, idx) = 4;	/* XXX */
		iatp = BUS_SPACE_IAT_2;
	}
	else
		iatp = BUS_SPACE_IAT_1;

	return pisa_space_map_load(dh, idx, 2, iatp, mpuhp);
}
