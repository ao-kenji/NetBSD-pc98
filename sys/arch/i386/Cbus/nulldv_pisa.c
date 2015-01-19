/*	$NecBSD: nulldv_pisa.c,v 1.6 1999/07/26 06:33:42 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
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
 * Place holder for IO and MEMORY spaces
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>
#include <dev/isa/pisavar.h>

#include <vm/vm.h>

struct nulldv_softc {
	struct device sc_dev;
};

static int nulldv_pisa_match __P((struct device *, struct cfdata *, void *));
static void nulldv_pisa_attach __P((struct device *, struct device *, void *));
static int nulldv_space_map __P((bus_space_tag_t, bus_space_tag_t, bus_space_handle_t *, pisa_device_handle_t));
static int nulldv_space_unmap __P((bus_space_tag_t, bus_space_tag_t, bus_space_handle_t *, pisa_device_handle_t));

struct cfattach nulldv_pisa_ca = {
	sizeof(struct nulldv_softc), nulldv_pisa_match, nulldv_pisa_attach
};

struct pisa_functions nulldv_pd = {
};

static int
nulldv_space_map(iot, memt, imhp, dh)
	bus_space_tag_t iot, memt;
	bus_space_handle_t *imhp;
	pisa_device_handle_t dh;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_handle_t bah;
	int idx, error;

	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
	{
		if (PISA_DR_IMB(dr, idx) == SLOT_DEVICE_UNKVAL)
			continue;
		error = pisa_space_map(dh, idx, &bah);
		if (error != 0)
			return error;
		imhp[idx] = bah;
	}

	return 0;	
}

static int
nulldv_space_unmap(iot, memt, imhp, dh)
	bus_space_tag_t iot, memt;
	bus_space_handle_t *imhp;
	pisa_device_handle_t dh;
{
	bus_space_tag_t t;
	int idx;

	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
	{
		if (imhp[idx] == NULL)
			continue;
		t = idx < SLOT_DEVICE_NIO ? iot : memt;
		pisa_space_unmap(dh, t, imhp[idx]);
	}
	return 0;	
}

int
nulldv_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	bus_space_handle_t im[SLOT_DEVICE_NIM];
	int rv = 1;

	memset(im, 0, sizeof(im));
	if (nulldv_space_map(pa->pa_iot, pa->pa_memt, im, pa->pa_dh))	
		rv = 0;
	nulldv_space_unmap(pa->pa_iot, pa->pa_memt, im, pa->pa_dh);
	return rv;
}

void
nulldv_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	bus_space_handle_t im[SLOT_DEVICE_NIM];

	printf("\n");

	pisa_register_functions(pa->pa_dh, self, &nulldv_pd);

	memset(im, 0, sizeof(im));
	if (nulldv_space_map(pa->pa_iot, pa->pa_memt, im, pa->pa_dh))	
		panic("nulldv: can not map\n");
}
