/*	$NecBSD: ipp.c,v 1.29 1998/09/12 14:11:06 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
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
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <i386/Cbus/pnp/pnpreg.h>
#include <i386/Cbus/pnp/ippiio.h>
#include <i386/Cbus/pnp/pnpvar.h>

#include <machine/devlist.h>

int
ippi_card_attach(sc, ib, il)
	struct ippi_softc *sc;
	struct ippi_board_info *ib;
	struct ippi_ld_info *il;
{
	struct slot_device_resources dr;
	struct serial_id *si;
	struct cfdriver *cd;
	u_int ldn;
	struct devlist_query dlq;
	devlist_res_magic_t drm;
	struct devlist *dlp;
	int priority, error = EINVAL;

	ldn = il->il_ldn;
	if (il->il_status != IPP_FOUND)
		return EINVAL;

	if ((il->il_flags & IPPI_LD_BOOTACTIVE) == 0)
	{
		ippi_initres(&dr);
		ippi_find_resources(sc, ib->ib_cis, ldn, &dr);
	}
	else
		dr = il->il_bdr;

	si = (struct serial_id *) ib->ib_cis;
	dlq.dlq_prot = "IPPI";
	dlq.dlq_id = htonl((u_long) si->si_product);
	dlq.dlq_func = ldn;
	dlq.dlq_class = 0;
	dlq.dlq_mkey0 = dr.dr_io[0].im_hwbase;
	dlq.dlq_mkey1 = dr.dr_io[1].im_hwbase;
	dlq.dlq_skey = NULL;

	devlist_init_res_magic(DEVLIST_RES_DEFAULT_TAG, &drm);
	for (priority = 0; priority < DLQ_PRI_NPRI; )
	{
		dlp = devlist_query_resource(DEVLIST_RES_DEFAULT_TAG, &drm, 
					     priority, &dlq);
		if (dlp == NULL)
		{
			priority ++;
			devlist_init_res_magic(DEVLIST_RES_DEFAULT_TAG, &drm);
			error = ENOENT;
			continue;
		}

		cd = dlp->dl_cd;
		dr.dr_dvcfg = dlp->dl_dvcfg;

		printf("%s: ld(%d:%d) with %s at pisa\n",
			sc->sc_dev.dv_xname, ib->ib_csn, ldn, cd->cd_name);

		error = ippi_connect_pisa(sc, ib, il, &dr, cd->cd_name);
		if (error == 0)
			return error;
	}

	return error;
}
