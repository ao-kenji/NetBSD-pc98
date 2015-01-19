/*	$NecBSD: nec86_pisa.c,v 1.13 1998/09/26 11:31:11 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <i386/Cbus/dev/nec86reg.h>
#include <i386/Cbus/dev/nec86hwvar.h>
#include <i386/Cbus/dev/nec86var.h>

static int nec86_pisa_match __P((struct device *, struct cfdata *, void *));
static void nec86_pisa_attach __P((struct device *, struct device *, void *));
static int nec86_activate __P((pisa_device_handle_t));
static int nec86_pisa_map __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t *, bus_space_handle_t *));
static void nec86_pisa_unmap __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, bus_space_handle_t));

struct nec86_pisa_softc {
	struct nec86_softc sc_nec86;
	
	pisa_device_handle_t sc_dh;
};

struct cfattach pcm_pisa_ca = {
	sizeof(struct nec86_pisa_softc), (cfmatch_t) nec86_pisa_match, nec86_pisa_attach
};

struct pisa_functions nec86_pd = {
	nec86_activate, NULL
};

static int
nec86_pisa_match(parent, match, aux)
    struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t n86h, coreh;
	int rv = 0;
    
	if (nec86_pisa_map(dh, iot, &coreh, &n86h))
		return rv;

    	if (nec86_probesubr(iot, coreh, n86h) >= 0)
		rv = 1;

	nec86_pisa_unmap(dh, iot, coreh, n86h);
    	return rv;
}

void
nec86_pisa_attach(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
	struct nec86_pisa_softc *sc = (void *) self;
    	struct nec86_softc *nsc = &sc->sc_nec86;
	struct nec86hw_softc *ysc = &nsc->sc_nec86hw;
    	struct pisa_attach_args *pa = aux;
    	pisa_device_handle_t dh = pa->pa_dh;
    	slot_device_res_t dr = PISA_RES_DR(dh);
    	bus_space_tag_t iot = pa->pa_iot;
    	bus_space_handle_t coreh, n86h;

    	if (nec86_pisa_map(dh, iot, &coreh, &n86h))
    	{
		printf("%s: can not map\n", ysc->sc_dev.dv_xname);
       		return;
    	}

	pisa_register_functions(dh, self, &nec86_pd);

    	sc->sc_dh = dh;
    	nsc->sc_n86iot = iot;
    	nsc->sc_n86ioh = n86h;
    	ysc->sc_iot = iot;
    	ysc->sc_ioh = coreh;
    	ysc->sc_cfgflags = PISA_DR_DVCFG(dr);

    	nec86_attachsubr(nsc);
    	pisa_intr_establish(dh, PISA_PIN0, IPL_AUDIO, nec86hw_intr, ysc);
}

static int
nec86_activate(dh)
	pisa_device_handle_t dh;
{
    	struct nec86_pisa_softc *sc = PISA_DEV_SOFTC(dh);
    	struct nec86_softc *nsc = &sc->sc_nec86;

    	nec86_attachsubr(nsc);
    	return 0;
}

static int
nec86_pisa_map(dh, iot, corehp, n86hp)
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

static void
nec86_pisa_unmap(dh, iot, coreh, n86h)
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
