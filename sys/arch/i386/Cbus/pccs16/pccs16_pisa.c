/*	$NecBSD: pccs16_pisa.c,v 1.23.2.7 1999/09/26 08:03:29 kmatsuda Exp $	*/
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

#define	PCCS16_ZEROIDX_TRANSLATE	1

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/select.h>
#include <sys/signalvar.h>

#include <vm/vm.h>

#include <dev/cons.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>
#include <machine/systmbusvar.h>
#include <machine/dvcfg.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

static __inline int pccs16_lock_physwin __P((struct slot_softc *));
static __inline void pccs16_unlock_physwin __P((struct slot_softc *));
static int pccs16_space_uio __P((struct slot_softc *, struct uio *));
static int pccs16_map_mem __P((pccshw_tag_t, slot_handle_t, bus_addr_t, window_handle_t *, int));
static __inline u_int8_t pccs16_cr_read_1 __P((pccshw_tag_t, bus_addr_t, u_int));
static __inline void pccs16_cr_write_1 __P((pccshw_tag_t, bus_addr_t, u_int, u_int8_t));
int pccs16_load_config __P((struct slot_softc *, u_long, int));
int pccs16_setup_config __P((struct slot_softc *, struct card_info *));
int pccs16_clear_config __P((struct slot_softc *, struct card_info *));
int pccs16_config_intr_ack __P((struct slot_softc *, struct card_info *));
int pccs16_connect_pisa __P((struct slot_softc *, struct pcdv_attach_args *));
int pccs16_prefer_config __P((struct slot_softc *, struct card_prefer_config *));
slot_device_ident_t pccs16_pisa_mk_ident __P((struct slot_softc *ssc, u_char *, int, u_int8_t));

#define	pccs16_parse_cis pccscis_parse_cis

struct pccs_config_funcs pccs_config_16bits_funcs = {
	pccs16_prefer_config,		/* select prefer config */
	pccs16_load_config,		/* load 16b cis raw data */
	pccs16_parse_cis,		/* parse 16b cis data */
	pccs16_setup_config,		/* setup 16b card registers */
	pccs16_clear_config,		/* clear 16b card registers */
	pccs16_connect_pisa,		/* 16 bits bus connect */
	pccs16_attach_card,		/* 16 bits card attach */
	pccs16_config_intr_ack,		/* interrupt ack of card registers */
	pccs16_space_uio,		/* XXX */
};

struct pccs16_pisa_magic {
	u_char ma_str[PRODUCT_NAMELEN];
	u_int ma_magic0;
	u_int ma_magic1;
};

typedef struct pccs16_pisa_magic *pccs16_pisa_magic_t;

#define	PCCS_WRITE	0		/* write op */
#define	PCCS_READ	1		/* read op */

/**************************************************************
 * utilities
 **************************************************************/
slot_device_ident_t
pccs16_pisa_mk_ident(ssc, dv, mfcid, scr)
	struct slot_softc *ssc;	
	u_char *dv;
	int mfcid;
	u_int8_t scr;
{
	slot_device_ident_t di;
	pccs16_pisa_magic_t ma;

	di = slot_device_ident_allocate(PCCS_SLOT_TAG(ssc), dv, sizeof(*ma));
	if (di == NULL)
		return NULL;

	ma = di->di_magic;
	ma->ma_magic0 = mfcid;
	ma->ma_magic1 = (u_int) scr;
	bcopy(ssc->sc_mci->ci_product, ma->ma_str, PRODUCT_NAMELEN);
	return di;
}

int
pccs16_prefer_config(ssc, pcp)
	struct slot_softc *ssc;
	struct card_prefer_config *pcp;
{
	slot_device_ident_t di;
	slot_device_handle_t dh;
	int mfcid, error = 0;
	u_int8_t scr;

	mfcid = PCCS_GET_MFCID(&pcp->cp_ci);
	scr = PCCS_GET_SCR(&pcp->cp_ci);

	di = pccs16_pisa_mk_ident(ssc, pcp->cp_name, mfcid, scr);
	if (di == NULL)
		return EBUSY;	

	dh = slot_device_handle_lookup(PCCS_SLOT_TAG(ssc), di,
				       DH_CREATE | DH_ACTIVE | DH_BUSY);
	slot_device_ident_deallocate(di);
	if (dh != NULL && dh->dh_ci != NULL)
	{
		pcp->cp_pri = 0;
		pcp->cp_ci = *((struct card_info *) dh->dh_ci);
	}
	else
	{
		pcp->cp_pri = 1;
		error = pccscis_find_config(ssc, &pcp->cp_ci, pcp->cp_disable);
	}

	if (error == 0)
	{
		pcp->cp_ci.ci_res.dr_pin[0].dc_chan = PAUTOIRQ;
		error = pccss_shared_pin(ssc, &pcp->cp_ci);	
	}

	return error;
}

/**************************************************************
 * device attach
 **************************************************************/
int
pccs16_connect_pisa(ssc, ca)
	struct slot_softc *ssc;
	struct pcdv_attach_args *ca;
{
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	slot_device_slot_tag_t st = PCCS_SLOT_TAG(ssc);
	struct slot_func_info *si;
	struct card_info *fci, *ci;
	slot_device_handle_t dh;
	slot_device_ident_t di;
	int mfcid, error;
	u_int8_t scr;
	struct pisa_slot_device_attach_args psa;

	ca->ca_unit = -1;
	/*
	 * check arguments
	 */
 	ci = &ca->ca_ci;
	mfcid = PCCS_GET_MFCID(ci);
	if (mfcid < 0 || mfcid >= PCCS_MAXMFC)
		return EINVAL;

	/*
	 * allocate a kernel internal card function info structure
	 */
	if ((error = pccs_alloc_si(ssc, mfcid)) != 0)
		return error;

	si = ssc->sc_si[mfcid];
	if (si->si_dh != NULL)
	{
		bcopy(si->si_xname, ca->ca_name, PCCS_NAMELEN);
		return 0;	/* XXX */
	}

	/*
	 * do special io address swap and mapping
	 */
	pccshw_swapspace(pp, ssc->sc_id, &ci->ci_res, &ci->ci_res);

	/*
	 * allocate common interrupt pin. 
	 * all functions share one interrupt pin.
	 * pccs16 has only one pin.
	 */
	if ((error = pccss_shared_pin(ssc, ci)) != 0)
		return error;

	/* 
	 * not yet support dma extension (Pc card standard)
	 */
	ci->ci_res.dr_drq[0].dc_chan = SLOT_DEVICE_UNKVAL;

	/*
	 *  allocate device handle.
	 */
	scr = PCCS_GET_SCR(ci);
	if ((di = pccs16_pisa_mk_ident(ssc, ca->ca_name, mfcid, scr)) == NULL)
		return EBUSY;	

	error = slot_device_handle_allocate(st, di, 0, &dh);
	if (error)
	{
		slot_device_ident_deallocate(di);
		return 0;
	}
	si->si_dh = dh;

	/*
	 *  allocate card_info of this function
	 */
	fci = dh->dh_ci;
	if (fci == NULL)
	{
		fci = malloc(sizeof(*fci), M_DEVBUF, M_NOWAIT);
		if (fci == NULL)
		{
			error = ENOMEM;
			goto bad;
		}

		dh->dh_ci = fci;
		*fci = *ci;
	}
	else
		fci->ci_res.dr_pin[0] = ci->ci_res.dr_pin[0];

	/*
	 *  pointer update
	 */
 	ci = dh->dh_ci;	
	SLOT_DEVICE_IDENT_IDS(dh) = ci->ci_product;
	SLOT_DEVICE_IDENT_IDN(dh) = ci->ci_manfid;

	/*
	 *  attach a device
	 */
	psa.psa_pdr = &ci->ci_res;			/* device map */
	if ((dh->dh_flags & DH_CREATE) == 0)
	{
		slot_device_ident_deallocate(di);
		error = slot_device_activate(st, dh, &psa);
		if (error != 0)
			goto bad1;
	}
	else
	{
		error = slot_device_attach(st, dh, &psa);
		if (error != 0)
			goto bad;
	}

	ca->ca_unit = ssc->sc_ndev;
	bcopy(dh->dh_dv->dv_xname, si->si_xname, sizeof(si->si_xname));
	bcopy(si->si_xname, ca->ca_name, PCCS_NAMELEN);
	ssc->sc_state = SLOT_HASDEV;
	ssc->sc_ndev ++;

bad1:
	slot_device_handle_unbusy(dh);
	if (error != 0)
		si->si_dh = NULL;
	return error;

bad:
	if (dh->dh_ci != NULL)
		free(dh->dh_ci, M_DEVBUF);
	slot_device_handle_deallocate(st, dh);
	si->si_dh = NULL;
	return error;	
}

/**************************************************************
 * attribute memory control 
 **************************************************************/
static __inline int
pccs16_lock_physwin(ssc)
	struct slot_softc *ssc;
{
	register struct pccs_softc *sc = (void *) ssc->sc_dev.dv_parent;

	return lockmgr(&sc->sc_lock, LK_EXCLUSIVE, NULL);
}

static __inline void
pccs16_unlock_physwin(ssc)
	struct slot_softc *ssc;
{
	register struct pccs_softc *sc = (void *) ssc->sc_dev.dv_parent;

	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
}

/* read write from user context */
static int
pccs16_space_uio(ssc, uio)
	struct slot_softc *ssc;
	struct uio *uio;
{
	struct card_info *ci = ssc->sc_mci;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	struct slot_device_iomem win;
	vaddr_t offs;
	window_handle_t wh;
	u_int8_t *tbp;
	u_long count;
	int error = 0;

	if (uio->uio_rw != UIO_READ)
		return EIO;

	/* XXX:
	 * Uio not yet implemented in bus space.
	 * Currently use a temp buffer and do indirectly.
	 */
	tbp = malloc(pp->pp_psz, M_TEMP, M_WAITOK);

	/* map window */
	win = pp->pp_csim;
	win.im_size = pp->pp_psz;
	win.im_flags |= SDIM_BUS_ACTIVE;
	if (CIS_IS_ATTR(uio->uio_offset))
		win.im_flags |= (WFMEM_ATTR | SDIM_BUS_WEIGHT);
	else if (ci->ci_tpce.if_ifm == 0 || (ci->ci_tpce.if_flags & IF_MWAIT))
		win.im_flags |= SDIM_BUS_WEIGHT;

	/* lock phys win */
	pccs16_lock_physwin(ssc);

	/* ok */
	while (uio->uio_resid && error == 0)
	{
		offs = PCCSHW_TRUNC_PAGE(pp, uio->uio_offset);
		win.im_base = offs;
		if ((error = pccshw_map(pp, ssc->sc_id, &win, &wh, 0)) != 0)
		{
			error = EIO;
			goto bad;
		}

#if	0
		bus_space_read_region_4(pp->pp_csbst, pp->pp_csbsh, 0,
			(u_int32_t *) tbp, pp->pp_psz / sizeof(u_int32_t));
#else
		bus_space_read_region_1(pp->pp_csbst, pp->pp_csbsh, 0,
			(u_int8_t *) tbp, pp->pp_psz);
#endif

		offs = CIS_COMM_ADDR(uio->uio_offset) - offs;
		count = pp->pp_psz - offs;
		if (count > uio->uio_resid)
			count = uio->uio_resid;
		error = uiomove(tbp + offs, count, uio);
		pccshw_unmap(pp, ssc->sc_id, wh);
	}

bad:
	pccs16_unlock_physwin(ssc);
	free(tbp, M_TEMP);
	return error;
}

static int
pccs16_map_mem(pp, sid, ofs, whp, attr)
	pccshw_tag_t pp;
	slot_handle_t sid;
	bus_addr_t ofs;
	window_handle_t *whp;
	int attr;
{
	struct slot_device_iomem win;
	int error;

	win = pp->pp_csim;
	win.im_flags |= (SDIM_BUS_ACTIVE | SDIM_BUS_WEIGHT);
	if (attr != 0)
		win.im_flags |= WFMEM_ATTR;
	win.im_base = PCCSHW_TRUNC_PAGE(pp, ofs);
	win.im_size = pp->pp_psz;

	error = pccshw_map(pp, sid, &win, whp, 0);

	delay(100);					/* XXX */
	return error;	
}

static __inline u_int8_t
pccs16_cr_read_1(pp, ofs, reg)
	pccshw_tag_t pp;
	u_long ofs;
	u_int reg;
{
	bus_addr_t addr = CIS_COMM_ADDR(ofs + reg);

	addr = addr - PCCSHW_TRUNC_PAGE(pp, addr);
	return bus_space_read_1(pp->pp_csbst, pp->pp_csbsh, addr);
}

static __inline void
pccs16_cr_write_1(pp, ofs, reg, val)
	pccshw_tag_t pp;
	bus_addr_t ofs;
	u_int reg;
	u_int8_t val;
{
	bus_addr_t addr = CIS_COMM_ADDR(ofs + reg);

	addr = addr - PCCSHW_TRUNC_PAGE(pp, addr);
	bus_space_write_1(pp->pp_csbst, pp->pp_csbsh, addr, val);
}

/**************************************************************
 * Pc card 16bits config
 **************************************************************/
int
pccs16_load_config(ssc, offs, flags)
	struct slot_softc *ssc;
	u_long offs;
	int flags;
{
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	slot_handle_t sid = ssc->sc_id;
	window_handle_t wh;
	int attr, i, len;
	u_long page, gap;

	attr = CIS_IS_ATTR(offs);
	gap = sizeof(u_int16_t);	/* dubious in case of COMMON area */

	/* calculate the hardware page boundary */
	page = PCCSHW_TRUNC_PAGE(pp, offs);
	offs = page * gap;
	if (offs >= PCC16_MEM_MAXADDR)
		return EINVAL;

	if ((flags & PCCS_LOAD_CONFIG_CACHEOK) &&
	    attr == CIS_IS_ATTR(ssc->sc_aoffs) &&
	    page == CIS_COMM_ADDR(ssc->sc_aoffs))
		return 0;

	/* calculate the length */
	len = pp->pp_psz / gap;
	if (len > ssc->sc_atbsz)
		len = ssc->sc_atbsz;

	/* lock a window and map it */
	pccs16_lock_physwin(ssc);
	if (pccs16_map_mem(pp, sid, offs, &wh, attr) != 0)
	{
		pccs16_unlock_physwin(ssc);
		return EIO;
	}

	for (i = 0; i < len; i ++)
		ssc->sc_atbp[i] = bus_space_read_1(pp->pp_csbst, pp->pp_csbsh, i * gap);

	/* unmap the target window */
	pccshw_unmap(pp, sid, wh);
	pccs16_unlock_physwin(ssc);

	ssc->sc_aoffs = attr ? CIS_ATTR_ADDR(page) : CIS_COMM_ADDR(page);
	ssc->sc_asize = len;

	cis_patch_scan(ssc);		/* XXX: apply a patch here */
	return 0;
}

int
pccs16_setup_config(ssc, ci)
	struct slot_softc *ssc;
	struct card_info *ci;
{
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	struct card_registers *cr;
	window_handle_t wh;
	u_int8_t index, tmp;
	u_long ofs;
	int attr, i, zone;

	cr = &ci->ci_cr;
	if (cr->cr_offset == 0)
		return 0;

	attr = 1;
	ofs = CIS_COMM_ADDR(cr->cr_offset);
	index = PCCS_GET_INDEX(ci);
#ifdef	PCCS16_ZEROIDX_TRANSLATE
	if ((index & CCOR_IDXMASK) == 0)
		index |= CCOR_MFEN | CCOR_MIEN;
#endif	/* PCCS16_ZEROIDX_TRANSLATE */
	PCCS_SET_INDEX(ssc->sc_mci, index);

	pccs16_lock_physwin(ssc);
	if (pccs16_map_mem(pp, ssc->sc_id, ofs, &wh, attr) != 0)
		goto out; 

	/* setup iobase of this function */
	zone = 0;
	cr->cr_iob = 0;
	for (i = 0; i < pp->pp_nio; i ++)
	{
		if (ci->ci_res.dr_io[i].im_base != PUNKADDR)
		{
	    		zone |= (ci->ci_res.dr_io[i].im_flags & WFIO_ZONE);
			cr->cr_iob = ci->ci_res.dr_io[i].im_base;
			break;
		}
	}

	/* setup io size of this function */
	cr->cr_iol = 0;
	for (i = 0; i < pp->pp_nio; i ++)
	{
		if (ci->ci_res.dr_io[i].im_size != PUNKSZ)
			cr->cr_iol += ci->ci_res.dr_io[i].im_size;
	}

#ifdef	PCCS_DEBUG
	printf("iob 0x%x iol 0x%x\n", cr->cr_iob, cr->cr_iol);
#endif	/* PCCS_DEBUG */

	if (ssc->sc_state < SLOT_HASDEV)
	{
		pccs16_cr_write_1(pp, ofs, cisr_ccor, CCOR_RST);
		pccs_wait(ci->ci_delay);
		pccs16_cr_write_1(pp, ofs, cisr_ccor, 0);
		pccs_wait(ci->ci_delay);
	}

	if (ci->ci_res.dr_pin[0].dc_flags & INTR_EDGE)
		index |= CCOR_INTR;
	if ((cr->cr_mask & BCISR_IOB0) != 0 && zone != 0 &&
	     cr->cr_iob != 0 && cr->cr_iol != 0)
		index |= CCOR_MIOBEN;
	index |= cr->cr_ccmor & CCOR_MMASK;

	pccs16_cr_write_1(pp, ofs, cisr_ccor, index);
	pccs_wait(1000);

	tmp = pccs16_cr_read_1(pp, ofs, cisr_ccor);
	if (tmp != index)
	{
		printf("%s: index register wrong (%x)!\n", 
			ssc->sc_dev.dv_xname, tmp);
		if (ci->ci_function == DV_FUNCID_SERIAL ||
		    ci->ci_function == DV_FUNCID_MULTI)
			pccs16_cr_write_1(pp, ofs, cisr_ccor, index);
		pccs_wait(1000);
	}

	if (cr->cr_mask & BCISR_CCSR)
	{
		u_int8_t ccsr = cr->cr_ccsr;

		if ((ssc->sc_hwflags & HW_SPKRUP) == 0)
			ccsr &= ~CCSR_SPKR;
		pccs16_cr_write_1(pp, ofs, cisr_ccsr, ccsr);
	}

	for (i = 0; i < sizeof(cr->cr_iob); i++)
	{
		if ((cr->cr_mask & (BCISR_IOB0 << i)) == 0)
			continue;

		/* little endian machine! */
		pccs16_cr_write_1(pp, ofs, cisr_iob0 + 2 * i,
				  cr->cr_iob >> (NBBY * i));
	}

	/* check if I/O Limit register exists */
	if ((cr->cr_mask & BCISR_IOL0) != 0)
	{
		if (cr->cr_iol > 0)
			cr->cr_iol --;
		pccs16_cr_write_1(pp, ofs, cisr_iol, cr->cr_iol);
	}

	pccshw_unmap(pp, ssc->sc_id, wh);

out:
	pccs16_unlock_physwin(ssc);
	return 0;
}

int
pccs16_clear_config(ssc, ci)
	struct slot_softc *ssc;
	struct card_info *ci;
{
#ifdef	PCCS16_CLEAR_INDEX
	struct pccs_softc *sc = (void *) ssc->sc_dev.dv_parent;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	struct card_registers *cr;
	window_handle_t wh;
	u_long ofs;
	int attr;

	cr = &ci->ci_cr;
	if (cr->cr_offset == 0)
		return 0;

	if (lockstatus(&sc->sc_lock) != 0)
	{
		printf("%s: phys win locked\n", ssc->sc_dev.dv_xname);
		return EBUSY;
	}

	attr = 1;
	ofs = CIS_COMM_ADDR(cr->cr_offset);

	pccs16_lock_physwin(ssc);
	if (pccs16_map_mem(pp, ssc->sc_id, ofs, &wh, attr) == 0)
	{
		pccs16_cr_write_1(pp, cr->cr_offset, cisr_ccsr, 0);
		pccshw_unmap(pp, ssc->sc_id, wh);
	}

	pccs16_unlock_physwin(ssc);
#endif	/* PCCS16_CLEAR_INDEX */
	return 0;
}

int
pccs16_config_intr_ack(ssc, ci)
	struct slot_softc *ssc;
	struct card_info *ci;
{

	/* not required, currently */
	return 0;
}
