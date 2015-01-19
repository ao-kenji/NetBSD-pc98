/*	$NecBSD: c16res.c,v 1.22.6.4 1999/08/24 23:58:59 honda Exp $	*/
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

/*
 * Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/slot_device.h>
#include <machine/devlist.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>

#ifndef	PCCSHW_WAIT
#define	PCCSHW_WAIT	1000		/* 1 sec */
#endif	/* !PCCSHW_WAIT */

static int pccs16_fixup
	__P((struct card_info *, u_long, u_int32_t *));
static void pccs16_setup_dlq
	__P((struct card_info *, struct devlist_query *, int));

static void
pccs16_setup_dlq(ci, dlqp, mfcid)
	struct card_info *ci;
	struct devlist_query *dlqp;
	int mfcid;
{
	slot_device_res_t dr;

 	dr = &ci->ci_res;
	dlqp->dlq_prot = "PCCS16";
	dlqp->dlq_id = (u_long) ci->ci_manfid;
	dlqp->dlq_func = mfcid;
	dlqp->dlq_class = ci->ci_function;
	dlqp->dlq_mkey0 = dr->dr_io[0].im_base;	/* pass card base addr */
	dlqp->dlq_mkey1 = dr->dr_io[1].im_base;
	dlqp->dlq_skey = ci->ci_product;
}

int
pccs16_attach_card(ssc, mfcid)
	struct slot_softc *ssc;
	int mfcid;
{
	struct cfdriver *cd;
	struct pcdv_attach_args cadev;
	struct card_info oinfo;
	struct card_info *kci;
	slot_device_res_t dr;
	struct devlist_query dlq;
	struct devlist *dlp;
	devlist_res_magic_t drm;
	int fixup, priority, error = EINVAL;
	u_int32_t disable = 0, ntry = 0;
	u_int8_t index;

 	dr = &oinfo.ci_res;
	if (ssc->sc_state < SLOT_READY)
		return error;

	kci = ssc->sc_mci;
	if (mfcid == 0)
	{
		ssc->sc_hwflags &= ~HW_SHAREWIN;
	}
	else if (mfcid > 0 && mfcid >= kci->ci_nmfc)
	{
		if ((ssc->sc_hwflags & HW_SHAREWIN) == 0)
			return error;
	}

	/* XXX: QUIRK
	 * The following code aims at detecting the configuraions
	 * of illegal shared type multi function cards.
	 */
	pccs_init_ci(&oinfo);
	PCCS_SET_MFCID(&oinfo, mfcid);
	pccscis_parse_cis(ssc, &oinfo, PCCS_NOT_FOLLOW_LINK);

	pccs16_setup_dlq(&oinfo, &dlq, mfcid);
	printf("%s: function %d: %s(0x%lx)\n", ssc->sc_dev.dv_xname, mfcid,
		dlq.dlq_skey, dlq.dlq_id);

	devlist_init_res_magic(DEVLIST_RES_DEFAULT_TAG, &drm);
	dlp = devlist_query_resource(DEVLIST_RES_DEFAULT_TAG, &drm, 
				     DLQ_PRI_HIGH, &dlq);
	if (dlp == NULL)
	{
		disable = 0;
	}
	else
	{
		cadev.ca_ci = *kci;
		if (pccs16_fixup(&cadev.ca_ci, dlp->dl_buscfg, &disable))
		{
			printf("%s: shared type card fixup(0x%x)\n",
				ssc->sc_dev.dv_xname, disable);
		}
	}
	
	/*
	 * find a preferable configuration of the target card.
	 */
	pccs_init_ci(&oinfo);
	PCCS_SET_MFCID(&oinfo, mfcid);

	if (ssc->sc_ndev > 0 && (ssc->sc_hwflags & HW_SHAREWIN))
	{
		/* XXX: case I:
		 * illegal multifunction cards which share the same cf.
		 * I hope such a foolish card would disappear in the world.
		 */
		index = PCCS_GET_INDEX(kci);
		PCCS_SET_INDEX(&oinfo, index);
	}
	else
	{
		/* case II:
		 * normal single or multifunction cards
		 */
		index = PUNKINDEX;
		PCCS_SET_INDEX(&oinfo, index);
	}

	error = pccscis_find_config(ssc, &oinfo, disable);
	if (error != 0)
		return error;

	/*
	 * allocated shared pin info
	 */
	oinfo.ci_delay = PCCSHW_WAIT;
	dr->dr_pin[0].dc_chan = PAUTOIRQ;
	error = pccss_shared_pin(ssc, &oinfo);
	if (error != 0)
		return 0;

	/*
	 * search a device information
	 */
	pccs16_setup_dlq(&oinfo, &dlq, mfcid);
	devlist_init_res_magic(DEVLIST_RES_DEFAULT_TAG, &drm);
	for (priority = 0; priority < DLQ_PRI_NPRI; )
	{
		fixup = 0;
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
		dr->dr_dvcfg = dlp->dl_dvcfg;

		printf("%s: %s(0x%lx) with %s at %s\n",
			ssc->sc_dev.dv_xname, dlq.dlq_skey, dlq.dlq_id,
			cd->cd_name, PCCSHW_TAG(ssc)->pp_sb->sb_name);

		/*
		 * try to attach a device
		 */
		cadev.ca_ci = oinfo;
		fixup = pccs16_fixup(&cadev.ca_ci, dlp->dl_buscfg, &disable);
		strncpy(cadev.ca_name, cd->cd_name, 16);

#define	C16RES_SHOW_INFO
#ifdef	C16RES_SHOW_INFO
		show_card_info(&cadev.ca_ci);
#endif	/* C16RES_SHOW_INFO */
	
		ntry ++;
		error = pccs_connect_bus(ssc, &cadev);
		if (error == 0)
		{
			if (fixup != 0)
				ssc->sc_hwflags |= HW_SHAREWIN;
			return error;
		}
	}

	if (ntry == 0)
		printf("%s: NO device configuration in kernel\n",
			ssc->sc_dev.dv_xname);
	else
		printf("%s: device probe failed\n",
			ssc->sc_dev.dv_xname);
	return error;
}

/**************************************************************
 * bus action overwrite for buggy cards!
 **************************************************************/
#define	PCCS16_FIXUP_BUSCFG_IODS16	0x000001
#define	PCCS16_FIXUP_BUSCFG_IOCS16	0x000002
#define	PCCS16_FIXUP_BUSCFG_MEMDS16	0x000004
#define	PCCS16_FIXUP_BUSCFG_MINTEN	0x000100
#define	PCCS16_FIXUP_BUSCFG_MIOBEN	0x000200
#define	PCCS16_FIXUP_BUSCFG_IOWMASK	0x070000
#define	PCCS16_FIXUP_BUSCFG_IOW		0x080000
#define	PCCS16_FIXUP_BUSCFG_MEMWMASK	0x700000
#define	PCCS16_FIXUP_BUSCFG_MEMW	0x800000
#define	PCCS16_FIXUP_BUSCFG_IOWB(fl)	(((fl) >> 16) & 0x07)
#define	PCCS16_FIXUP_BUSCFG_MEMWB(fl)	(((fl) >> 20) & 0x07)

static int
pccs16_fixup(ci, flags, disablep)
	struct card_info *ci;
	u_long flags;
	u_int32_t *disablep;
{
	struct card_registers *cr = &ci->ci_cr;
	slot_device_res_t dr = &ci->ci_res;
	int start, target, fixup = 0;

	*disablep = 0;
	if (flags & PCCS16_FIXUP_BUSCFG_MINTEN)
		cr->cr_ccmor |= CCOR_MIEN;

	if (flags & PCCS16_FIXUP_BUSCFG_MIOBEN)
		cr->cr_mask |= (BCISR_IOB0MASK | BCISR_IOL0);

	if (flags & (PCCS16_FIXUP_BUSCFG_IOW | PCCS16_FIXUP_BUSCFG_MEMW))
	{
		fixup = 1;
		*disablep = -1;
		if (flags & PCCS16_FIXUP_BUSCFG_IOW) 
		{
			start = 0;
			target = PCCS16_FIXUP_BUSCFG_IOWB(flags);
			if (target >= 0 && target < SLOT_DEVICE_NIO)
			{
				*disablep &= ~(1 << target);
				dr->dr_io[0] = dr->dr_io[target];
				start ++;
			}
			for (target = start; target < SLOT_DEVICE_NIO;
			     target ++)
				pccs_init_iomem(&dr->dr_io[target]);
		}

		if (flags & PCCS16_FIXUP_BUSCFG_MEMW)
		{
			start = 0;
			target = PCCS16_FIXUP_BUSCFG_MEMWB(flags);
			if (target >= 0 && target < SLOT_DEVICE_NMEM)
			{
				*disablep &= ~(1 << (target + SLOT_DEVICE_NIO));
				dr->dr_mem[0] = dr->dr_mem[target];
				start ++;
			}
			for (target = start; target < SLOT_DEVICE_NMEM;
			     target ++)
				pccs_init_iomem(&dr->dr_mem[target]);
		}
	}

	if (flags & PCCS16_FIXUP_BUSCFG_IODS16)
		dr->dr_io[0].im_flags |= SDIM_BUS_WIDTH16;
	if (flags & PCCS16_FIXUP_BUSCFG_IOCS16)
		dr->dr_io[0].im_flags |= SDIM_BUS_AUTO;
	if (flags & PCCS16_FIXUP_BUSCFG_MEMDS16)
		dr->dr_mem[0].im_flags |= SDIM_BUS_WIDTH16;

	return fixup;
}
