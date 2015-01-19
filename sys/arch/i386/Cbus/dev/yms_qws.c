/*	$NecBSD: yms_qws.c,v 1.6 1998/09/26 11:31:25 kmatsuda Exp $	*/
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

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <i386/Cbus/dev/nec86reg.h>

#include <i386/Cbus/dev/ymsvar.h>

#include <i386/Cbus/dev/qwsvar.h>

/* YM230X */
static int	yms_qws_match __P((struct device *, struct cfdata *, void *));
void	yms_qws_attach __P((struct device *, struct device *, void *));
int	yms_qws_map __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t *));

struct cfattach yms_qws_ca = {
	sizeof(struct yms_softc), yms_qws_match, yms_qws_attach,
};

static int
yms_qws_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct qws_attach_args *qa = aux;
	bus_space_handle_t ymh;
	int rv = 0;

	if (strcmp("yms", qa->qa_name) != 0)
		return rv;
	if (yms_qws_map(qa->qa_dh, qa->qa_iot, &ymh))
		return rv;
	if (yms_probesubr(qa->qa_iot, ymh) >= 0)
		rv = 1;
	pisa_space_unmap(qa->qa_dh, qa->qa_iot, ymh);
	return rv;
}

void
yms_qws_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct yms_softc *sc = (void *) self;
	struct qws_attach_args *qa = aux;
	bus_space_handle_t ymh;

	printf("\n");

	if (yms_qws_map(qa->qa_dh, qa->qa_iot, &ymh))
		return;

	qa->qa_sc = sc;
	sc->sc_iot = qa->qa_iot;
	sc->sc_ioh = ymh;
	yms_attachsubr(sc);

#ifdef	notyet
    	pisa_intr_establish(qa->qa_dh, PISA_PIN0, IPL_AUDIO, yms_intr, sc);
#endif	/* notyet */
}

int
yms_qws_map(dh, iot, ymhp)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	bus_space_handle_t *ymhp;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_addr_t iobase;
	int idx;

	for (idx = 0; idx < SLOT_DEVICE_NIO; idx ++)
	{
		iobase = PISA_DR_IMB(dr, idx) & 0xfff0;
		if (iobase < 0x180 || iobase > 0x2f0)
			continue;
		if (PISA_DR_IMS(dr, idx) >= 4 && PISA_DR_IMS(dr, idx) <= 0x10)
			break;
	}
	if (idx == SLOT_DEVICE_NIO)
		return EINVAL;

	PISA_DR_IMB(dr, idx) = iobase + 0x08;		/* XXX */
	PISA_DR_IMS(dr, idx) = 8;			/* XXX */
	return pisa_space_map(dh, idx, ymhp);
}
