/*	$NecBSD: nec86_qws.c,v 1.6 1998/09/26 11:31:12 kmatsuda Exp $	*/
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
#include <i386/Cbus/dev/nec86hwvar.h>
#include <i386/Cbus/dev/nec86var.h>

#include <i386/Cbus/dev/qwsvar.h>

static int nec86_qws_match __P((struct device *, struct cfdata *, void *));
void nec86_qws_attach __P((struct device *, struct device *, void *));
int nec86_qws_map __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t *, bus_space_handle_t *));
void nec86_qws_unmap __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, bus_space_handle_t));

struct cfattach pcm_qws_ca = {
	sizeof(struct nec86_softc), nec86_qws_match, nec86_qws_attach,
};

static int
nec86_qws_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct qws_attach_args *qa = aux;
	bus_space_handle_t n86h, coreh;
	int rv = 0;

	if (strcmp("nec86", qa->qa_name) != 0)
		return rv;
	if (nec86_qws_map(qa->qa_dh, qa->qa_iot, &coreh, &n86h))
		return rv; 
    	if (nec86_probesubr(qa->qa_iot, coreh, n86h) >= 0)
		rv = 1;
	
	nec86_qws_unmap(qa->qa_dh, qa->qa_iot, coreh, n86h);
	return rv;
}

void
nec86_qws_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct qws_attach_args *qa = aux;	/* pisa attach arg */
	struct nec86_softc *nsc = (void *) self;
	struct nec86hw_softc *ysc = &nsc->sc_nec86hw;
	bus_space_handle_t n86h, coreh;

	if (nec86_qws_map(qa->qa_dh, qa->qa_iot, &coreh, &n86h))
		return;

	qa->qa_sc = nsc;
    	nsc->sc_n86iot = qa->qa_iot;
    	nsc->sc_n86ioh = n86h;
    	ysc->sc_iot = qa->qa_iot;
    	ysc->sc_ioh = coreh;
    	ysc->sc_cfgflags = qa->qa_cfgflags;

    	nec86_attachsubr(nsc);
    	pisa_intr_establish(qa->qa_dh, PISA_PIN0, IPL_AUDIO, nec86hw_intr, ysc);
}

int
nec86_qws_map(dh, iot, corehp, n86hp)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	bus_space_handle_t *corehp, *n86hp;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_size_t offset, size;
	int idx;

	/*
	 * find pcm core windows
	 */
	for (idx = 0; idx < SLOT_DEVICE_NIO; idx ++)
		if (NEC86_BASE_VALID((PISA_DR_IMB(dr, idx) & 0xfff0))) 
			break;
	if (idx == SLOT_DEVICE_NIO)
		return EINVAL;
	
	/*
	 * calcuate core offset
	 */
	if ((PISA_DR_IMB(dr, idx) & 0xf) > NEC86_COREOFFSET)
		return EINVAL;
	offset = NEC86_COREOFFSET - (PISA_DR_IMB(dr, idx) & 0xf);

	/*
	 * special huck for illegal resource cards.
	 */
	if (offset == NEC86_COREOFFSET && PISA_DR_IMS(dr, idx) != NEC86_PORT)
		PISA_DR_IMS(dr, idx) = NEC86_PORT;

	/*
	 * calcuate core size.
	 */
	if (PISA_DR_IMS(dr, idx) <= offset)
		return EINVAL;
	size = PISA_DR_IMS(dr, idx) - offset;

	/*
	 * assert now!
	 */
	if (offset != NEC86_COREOFFSET)
	{
		if (pisa_space_map(dh, idx, corehp))
			return ENOSPC;
		n86hp = NULL;
	}
	else
	{
		if (pisa_space_map(dh, idx, n86hp))
			return ENOSPC;
		if (bus_space_subregion(iot, *n86hp, offset, size, corehp))
		{
			pisa_space_unmap(dh, iot, *n86hp);
			return ENOSPC;
		}
	}
	return 0;
}

void
nec86_qws_unmap(dh, iot, coreh, n86h)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	bus_space_handle_t coreh, n86h;
{

	if (n86h == NULL)
	{
		pisa_space_unmap(dh, iot, coreh);
	}
	else
	{
		bus_space_unmap(iot, coreh, NEC86_CORESIZE);
		pisa_space_unmap(dh, iot, n86h);
	}
}
