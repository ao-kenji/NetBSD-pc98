/*	$NecBSD: pccscis.c,v 1.27.4.3 1999/09/18 17:40:59 honda Exp $	*/
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

/*
 * Here list up buggy card vendors.
 */
#define	BUGGY_LOGITEC_CIS
#define	BUGGY_RATOC_CIS
#define	BUGGY_INDEX_CIS

#ifndef	CARDINFO
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>
#endif	/* !CARDINFO */

/* static */
static int read_cis_mem __P((pccs_cis_tag_t, cis_addr_t, cis_size_t, u_int8_t *));
static cis_addr_t move_next_link __P((pccs_cis_tag_t, struct tuple_request *, struct tuple_data *));
static void tuple_init __P((struct tuple_data *));
static int get_next_tuple __P((pccs_cis_tag_t, struct tuple_request *, struct tuple_data *));
static u_int read_data __P((u_int8_t **, u_int, u_int8_t *));

static int pccscis_anal_power __P((struct card_info *, u_int8_t **, u_int8_t *));
static int pccscis_anal_timing __P((struct card_info *, u_int8_t **, u_int8_t *));
static int pccscis_anal_io __P((struct card_info *, u_int8_t **, u_int8_t *));
static int pccscis_anal_irq __P((struct card_info *, u_int8_t **, u_int8_t *));
static int pccscis_anal_mem __P((struct card_info *, u_int8_t **, u_int8_t *, int));

static int pccscis_anal_centry __P((struct card_info *, u_int8_t *, u_int8_t *));
static int pccscis_anal_config __P((struct card_info *, u_int8_t *, u_int8_t *));
static int pccscis_anal_vers_1 __P((pccs_cis_tag_t, struct tuple_data *, struct card_info *));

#define CIS_GET_ADDR(addr) ((cis_addr_t)(*(addr)))
#define	CIS_MAXLOOP		1024
#define	PCCSCIS_PHYS_START	NBPG		/* skip 0 page */
/********************************************
 * tuple read
 *******************************************/
#ifdef	_KERNEL
static int update_cis_cache __P((pccs_cis_tag_t, cis_addr_t));

static int
update_cis_cache(ap, offs)
	pccs_cis_tag_t ap;
	cis_addr_t offs;
{
	struct slot_softc *ssc = ap;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);

	/* check cache */
	if (CIS_IS_ATTR(offs) != CIS_IS_ATTR(ssc->sc_aoffs) ||
	    PCCSHW_TRUNC_PAGE(pp, offs) != CIS_COMM_ADDR(ssc->sc_aoffs))
	{
#ifdef	DIAGNOSTIC
		printf("update cis cache\n");
#endif	/* DIAGNOSTIC */
		/* reload */
		if (pccs_load_config(ssc, offs, 0) != 0)
			return EIO;
	}
	return 0;
}

static int
read_cis_mem(ap, offs, len, buf)
	pccs_cis_tag_t ap;
	cis_addr_t offs;
	cis_size_t len;
	u_int8_t *buf;
{
	cis_addr_t offset;
	struct slot_softc *ssc = ap;
	
	update_cis_cache(ap, offs);

 	offset = CIS_COMM_ADDR(offs) - CIS_COMM_ADDR(ssc->sc_aoffs);
	if (offset + len > ssc->sc_asize)
		return EIO;

	if (buf != NULL)
		bcopy(ssc->sc_atbp + offset , buf, len);
	return 0;
}
#endif	/* _KERNEL */

static void
tuple_init(tp)
	struct tuple_data *tp;
{

	memset(tp, 0, sizeof(*tp));

	tp->tp_code = CIS_END;
	tp->tp_offs = CIS_ATTR_ADDR(0);
	tp->tp_Aoffs = CIS_ATTR_ADDR(0);
	tp->tp_Coffs = CIS_COMM_ADDR(0);
	tp->tp_Moffs = CIS_ATTR_ADDR(0);
	tp->tp_plnkid = tp->tp_lnkid = PUNKMFCID;
}

static cis_addr_t
move_next_link(ap, r, tp)
	pccs_cis_tag_t ap;
	struct tuple_request *r;
	struct tuple_data *tp;
{
	cis_addr_t offs;
	struct cis_linktarget lt;
	struct cis_longlink_mfc_entry mt;

	if (tp->tp_stat & LINK_A)
	{
		offs = CIS_ATTR_ADDR(tp->tp_Aoffs);
		tp->tp_stat &= ~LINK_A;
	}
	else if (tp->tp_stat & LINK_C)
	{
		offs = CIS_COMM_ADDR(tp->tp_Coffs);
		tp->tp_stat &= ~LINK_C;
	}
	else if ((tp->tp_stat & LINK_MFC) && tp->tp_lnkid != PUNKMFCID &&
		 r->tr_mfcid < tp->tp_lnkcnt && r->tr_mfcid >= tp->tp_lnkid)
	{
		do
		{
			if (read_cis_mem(ap, tp->tp_Moffs, sizeof(mt), 
			    		 (u_int8_t *) &mt))
				return CIS_ADDRUNK;

			tp->tp_Moffs += sizeof(mt);
			tp->tp_lnkid ++;
			if (tp->tp_lnkid >= tp->tp_lnkcnt)
				tp->tp_stat &= ~LINK_MFC;

			offs = CIS_GET_ADDR(&mt.mt_offset);
			if ((mt.mt_flags & MFC_ENTRY_COMMON) == 0)
				offs = CIS_ATTR_ADDR(offs);
		}
		while (r->tr_mfcid >= tp->tp_lnkid);
	}
	else
		return CIS_ADDRUNK;

	if (read_cis_mem(ap, offs, sizeof(lt), (u_int8_t *) &lt))
		return CIS_ADDRUNK;

	if (lt.lt_code == CIS_LINKTARGET &&
	    lt.lt_size >= sizeof(lt.lt_str) &&
	    strncmp(lt.lt_str, "CIS", sizeof(lt.lt_str)) == 0)
		return CIS_DATAP(offs) + lt.lt_size;

	/* XXX:
	 * For a stupid pc cards (LOGITEC),
	 * we should consider extra cases.
	 */
#ifdef	BUGGY_LOGITEC_CIS
	/* XXX: BUGGY CARD! */
	if (lt.lt_code >= sizeof(lt.lt_str) &&
	    strncmp(&lt.lt_size, "CIS", sizeof(lt.lt_str)) == 0)
		return CIS_DATAP(offs) + lt.lt_code - 1;

	/* XXX: offset mistaken? */
	if (CIS_IS_ATTR(offs))
		offs = CIS_ATTR_ADDR(CIS_COMM_ADDR(offs) / 2);
	else
		offs = (CIS_COMM_ADDR(offs) / 2);
	if (read_cis_mem(ap, offs, sizeof(lt), (u_int8_t *) &lt))
		return CIS_ADDRUNK;

	if (lt.lt_code == CIS_LINKTARGET &&
	    lt.lt_size >= sizeof(lt.lt_str) &&
	    strncmp(lt.lt_str, "CIS", sizeof(lt.lt_str)) == 0)
		return CIS_DATAP(offs) + lt.lt_size;
#endif	/* BUGGY_LOGITEC_CIS */

	return CIS_ADDRUNK;
}

static int
get_next_tuple(ap, req, tp)
	pccs_cis_tag_t ap;
	struct tuple_request *req;
	struct tuple_data *tp;
{
	struct cis_longlink_mfc_header mh;
	struct cis_longlink ll;
	struct cis_header eh;
	cis_addr_t offs, toffs;
	u_int8_t code, *tbp;
	int i;

	if (req->tr_count ++ > CIS_MAXLOOP)
		return EIO;

	if (tp->tp_offs == CIS_ADDRUNK)
	{
		/* Scan a common memory after looking attribute mem. */
		offs = CIS_ATTR_ADDR(0);
#ifdef	FOLLOW_COMMON_AFTER_ATTRIBUTE
		tp->tp_stat |= LINK_C;
		tp->tp_Coffs = CIS_COMM_ADDR(0);
#endif	/* FOLLOW_COMMON_AFTER_ATTRIBUTE */
	}
	else if (tp->tp_offs == CIS_ATTR_ADDR(0) && tp->tp_size == 0)
		offs = CIS_ATTR_ADDR(0);
	else
		offs = CIS_DATAP(tp->tp_offs) + tp->tp_size;

	for (i = 0; i < CIS_MAXLOOP; i++)
	{
		if (read_cis_mem(ap, offs, sizeof(eh), (u_int8_t *) &eh))
			return ENOENT;

		code = ((eh.eh_size == (u_int8_t) -1) ? CIS_END : eh.eh_code);
#ifdef	_PCCS_DEBUG
		printf("offs 0x%x: code 0x%x size 0x%x\n",
			offs, eh.eh_code, eh.eh_size);
#endif	/* _PCCS_DEBUG */
		switch (code)
		{
		case CIS_LINKTARGET:
			if ((tp->tp_stat & (LINK_A | LINK_C | LINK_MFC)) == 0)
				break;

		case CIS_END:
			if ((tp->tp_stat & (LINK_A | LINK_C)) == 0 && 
			    (req->tr_flags & NOT_FOLLOW_LINK))
				return ENOENT;

			offs = move_next_link(ap, req, tp);
			if (offs == CIS_ADDRUNK)
				return ENOENT;
			continue;

		case CIS_LONGLINK_A:
			tbp = (u_int8_t *) &ll;
			if (read_cis_mem(ap, offs, sizeof(ll), tbp) != 0)
				return ENOENT;
			if (tp->tp_stat & LINK_A)
				break;
			tp->tp_stat |= LINK_A;
			toffs =	CIS_GET_ADDR(&ll.ll_offset);
			tp->tp_Aoffs = CIS_ATTR_ADDR(toffs);
			break;

		case CIS_LONGLINK_C:
			tbp = (u_int8_t *) &ll;
			if (read_cis_mem(ap, offs, sizeof(ll), tbp) != 0)
				return ENOENT;
			if (tp->tp_stat & LINK_C)
				break;
			tp->tp_stat |= LINK_C;
			tp->tp_Coffs = CIS_GET_ADDR(&ll.ll_offset);
			break;	

		case CIS_LONGLINK_MFC:
			tbp = (u_int8_t *) &mh;
			if (read_cis_mem(ap, offs, sizeof(mh), tbp) != 0)
				return ENOENT;
			if (tp->tp_stat & LINK_MFC)
				break;

			tp->tp_stat |= LINK_MFC;
			tp->tp_lnkid = 0;
			tp->tp_Moffs = offs + sizeof(mh);
			/* tp->tp_lnkcnt = mh.mh_cnt; */ /* for a BUGGY CARD */
			tp->tp_lnkcnt = (mh.mh_size - sizeof(mh.mh_cnt)) /
					sizeof(struct cis_longlink_mfc_entry);
			if (tp->tp_lnkcnt >= PCCS_MAXMFC)
				tp->tp_lnkcnt = PCCS_MAXMFC;
			break;

		case CIS_NO_LINK:
			break;
		}

		if (req->tr_code == CIS_END || eh.eh_code == req->tr_code)
			break;
		offs = CIS_DATAP(offs) + eh.eh_size;
	}

	tp->tp_code = code;
	tp->tp_offs = offs;
	tp->tp_size = eh.eh_size;
	return 0;
}

/********************************************
 * CIS
 *******************************************/
/* misc */
static u_int
read_data(addr, size, maxaddr)
	u_int8_t **addr;
	u_int size;
	u_int8_t *maxaddr;
{
	static u_int32_t mask[5] = {0, 0xff, 0xffff, 0xffffff, 0xffffffff};
	u_int8_t *data = *addr;

	*addr = data + size;
	if (size > sizeof(u_int32_t) || data + size >= maxaddr)
		return 0;
	return (*(u_int32_t *) data) & mask[size];
}

/* power */
static int
pccscis_anal_power(ci, data, maddr)
	struct card_info *ci;
	u_int8_t **data;
	u_int8_t *maddr;
{
	u_int mask;
	u_int8_t desc = read_data(data, sizeof(desc), maddr);

	for (mask = 1; mask < 256; mask <<= 1)
		if (desc & (u_int8_t) mask)
			while (read_data(data, 1, maddr) & 0x80)
				;

	return 0;
}

/* timing */
static int
pccscis_anal_timing(ci, data, maddr)
	struct card_info *ci;
	u_int8_t **data;
	u_int8_t *maddr;
{
	u_int8_t desc = read_data(data, sizeof(desc), maddr);

	if ((desc & 0x03) != 0x03)
		read_data(data, 1, maddr);
	if (((desc >> 2) & 0x07) != 0x07)
		read_data(data, 1, maddr);
	if (((desc >> 5) & 0x07) != 0x07)
		read_data(data, 1, maddr);
	return 0;
}

/* io */
static int
pccscis_anal_io(ci, data, maddr)
	struct card_info *ci;
	u_int8_t **data;
	u_int8_t *maddr;
{
	u_int8_t desc = read_data(data, sizeof(desc), maddr);
	struct slot_device_iomem *wp;
	u_int busflags, iozone, ios, idx;

	/* desc bits 0->4: address decode lines */
	iozone = ios = desc & TPCE_IO_DESC_DECODE;
 	if (ios > MAX_IOBITS)
		ios = MAX_IOBITS;
	if (ios < MIN_IOBITS)
		ios = MIN_IOBITS;
	if (ios > ci->ci_iobits)
		ci->ci_iobits = ios;

	/* desc bits 5, 6: bus flags */
#ifdef	PCCSHW_DEFAULT_WACCESS
	busflags = SDIM_BUS_WEIGHT;
#else
	busflags = 0;
#endif
	ci->ci_cr.cr_ccsr &= ~CCSR_BUS8;
	switch (TPCE_IO_DESC_BUS(desc))
	{
	case 0:
		break;
	case 1:
		ci->ci_cr.cr_ccsr |= CCSR_BUS8;
		break;
	case 2:
		busflags |= SDIM_BUS_AUTO;
		break;
	case 3:
		busflags |= SDIM_BUS_AUTO | SDIM_BUS_WIDTH16;
		break;
	}

	wp = &ci->ci_res.dr_io[0];
	if (desc & TPCE_IO_DESC_EXIST)
	{
		u_int8_t bb;
		u_int num, pb, sb;
		
 		bb = read_data(data, sizeof(bb), maddr);
		num = TPCE_IO_RANGE_NENT(bb);
		pb = TPCE_IO_RANGE_AFSZ(bb);
		sb = TPCE_IO_RANGE_RFSZ(bb);
		for (idx = 0 ; num > 0 && idx < SLOT_DEVICE_NIO;
		     wp ++, idx ++, num --)
		{
			wp->im_flags = busflags | SDIM_BUS_ACTIVE;
			wp->im_hwbase = (u_long) read_data(data, pb, maddr);
			wp->im_base = wp->im_hwbase;
			wp->im_size = (u_long) read_data(data, sb, maddr);
			wp->im_type = SLOT_DEVICE_SPIO;
			if (wp->im_size != PUNKSZ)
				wp->im_size ++;
		}

		for ( ; num > 0; num --)
		{
			(void) read_data(data, pb, maddr);
			(void) read_data(data, sb, maddr);
		}
	}
	else if (iozone != 0)
	{
		wp->im_flags = busflags | SDIM_BUS_ACTIVE | WFIO_ZONE;
		wp->im_size = (1 << iozone);
		wp->im_base = wp->im_hwbase = 0;
		wp->im_type = SLOT_DEVICE_SPIO;
	}
	return 0;
}

/* irq */
static int
pccscis_anal_irq(ci, data, maddr)
	struct card_info *ci;
	u_int8_t **data;
	u_int8_t *maddr;
{
	u_int8_t desc = read_data(data, sizeof(desc), maddr);
	struct slot_device_channel *dc = &ci->ci_res.dr_pin[0];
	u_int i, mask;

	/* default setup */
	dc->dc_chan = PUNKIRQ;
	dc->dc_aux = -1;
	dc->dc_flags = 0;

	/* check the level triggered */
	if (desc & TPCE_IR_LEVEL)
		dc->dc_flags |= INTR_EDGE;

	/* check irq masks */
	if (desc & TPCE_IR_HASMASK)
		dc->dc_aux &= read_data(data, sizeof(u_int16_t), maddr);

	/* get minimum irq */
	for (i = 0, mask = 1; i < 16; i++, mask <<= 1)
	{
		if (dc->dc_aux & mask)
		{
			dc->dc_chan = i;
			break;
		}
	}
	dc->dc_aux = -1;
	return 0;
}

/* mem */
static int
pccscis_anal_mem(ci, data, maddr, type)
	struct card_info *ci;
	u_int8_t **data;
	u_int8_t *maddr;
	int type;
{
	u_int mls, mas, mhs, maxwin, idx, busflags;
	struct slot_device_iomem *wp;
	u_int8_t desc;

 	busflags = SDIM_BUS_ACTIVE;
	if (ci->ci_tpce.if_ifm == 0 || (ci->ci_tpce.if_flags & IF_MWAIT))
		busflags |= SDIM_BUS_WEIGHT;

 	wp = &ci->ci_res.dr_mem[0];
	switch (type)
	{
	case 1:
		wp->im_size = read_data(data, 2, maddr) << NBBY;
		wp->im_base = 0;
		wp->im_hwbase = PCCSCIS_PHYS_START;
		wp->im_flags |= busflags;
		wp->im_type = SLOT_DEVICE_SPMEM;
		break;

	case 2:
		wp->im_size = read_data(data, 2, maddr) << NBBY;
		wp->im_base = read_data(data, 2, maddr) << NBBY;
		wp->im_hwbase = PCCSCIS_PHYS_START;
		wp->im_flags |= busflags;
		wp->im_type = SLOT_DEVICE_SPMEM;
		break;

	case 3:
		desc = read_data(data, sizeof(desc), maddr);
		maxwin = TPCE_MEM_RANGE_NENT(desc);
		mls = TPCE_MEM_RANGE_RFSZ(desc);
		mas = TPCE_MEM_RANGE_AFSZ(desc);
		mhs = (desc & TPCE_MEM_RANGE_HAF) ? mas : 0;

		for (idx = 0; maxwin > 0 && idx < SLOT_DEVICE_NMEM; 
		     wp ++, idx ++, maxwin --)
		{
			wp->im_size = NBPG;
			wp->im_hwbase = PCCSCIS_PHYS_START;
			wp->im_base = 0;
			wp->im_flags |= busflags;
			wp->im_type = SLOT_DEVICE_SPMEM;

			if (mls != 0)
				wp->im_size = 
					read_data(data, mls, maddr) << NBBY;

			if (mas != 0)
				wp->im_base = 
					read_data(data, mas, maddr) << NBBY;

			if (mhs != 0)
				wp->im_hwbase =
					read_data(data, mhs, maddr) << NBBY;
		}

		for ( ; maxwin > 0; maxwin --)
		{
			(void) read_data(data, mls, maddr);
			(void) read_data(data, mas, maddr);
			(void) read_data(data, mhs, maddr);
		}
		break;
	}

	return 0;
}

static int
pccscis_anal_centry(ci, data, maddr)
	struct card_info *ci;
	u_int8_t *data, *maddr;
{
	u_int8_t index, desc, type, val;

	/* get index byte */
	index = read_data(&data, sizeof(index), maddr);
#ifndef	BUGGY_INDEX_CIS
	if ((index & INDEX_MASK) == 0)
		return EINVAL;
#endif	/* BUGGY_INDEX_CIS */

	PCCS_SET_INDEX(ci, index & INDEX_MASK);
	if (index & TPCE_INDEX_HASIF)
	{
		/* get tpce_if byte */
		val = read_data(&data, sizeof(val), maddr);
		ci->ci_tpce.if_ifm = TPCE_IF_INTERFACE(val);
		ci->ci_tpce.if_flags = TPCE_IF_FLAGS(val);
	}

	/* get tpce_fs byte */
	desc = read_data(&data, sizeof(desc), maddr);

	/* bits 0,1 */
	type = (desc & TPCE_FS_POWER);
	while (type-- > 0)
	{
		if (pccscis_anal_power(ci, &data, maddr))
			return EINVAL;
	}

	/* bit 2 */
	if ((desc & TPCE_FS_TIMING) && pccscis_anal_timing(ci, &data, maddr))
			return EINVAL;

	/* bit 3 */
	if ((desc & TPCE_FS_IO) && pccscis_anal_io(ci, &data, maddr))
			return EINVAL;

	/* bit 4 */
	if ((desc & TPCE_FS_IRQ) && pccscis_anal_irq(ci, &data, maddr))
			return EINVAL;

	/* bit 5, 6 */
	type = (desc & TPCE_FS_MEM) >> 5;
	if (type != 0 && pccscis_anal_mem(ci, &data, maddr, type))
			return EINVAL;

	/* bit 7 */
	if (desc & TPCE_FS_EXT)
	{
		ci->ci_cr.cr_ccsr &= ~CCSR_SPKR;
		val = read_data(&data, sizeof(type), maddr);
		if (val & TPCE_EXT_AUDIO)
			ci->ci_cr.cr_ccsr |= CCSR_SPKR;
	}

	/*
	 * For a stupid pc card (RATOC), 
	 * Here we should recalculate address line masks.
	 */
#ifdef	BUGGY_RATOC_CIS
	{
	slot_device_res_t dr = &ci->ci_res;
	struct slot_device_iomem *imp;
	int idx;

	imp = &dr->dr_im[0];
	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++, imp ++)
	{
		u_int mask;

		if (imp->im_type != SLOT_DEVICE_SPIO || 
		    imp->im_hwbase == SLOT_DEVICE_UNKVAL)
			continue;

		for (mask = ci->ci_iobits; mask < sizeof(bus_addr_t) * NBBY; 
		     mask ++)
			if ((imp->im_hwbase & ~((1 << mask) - 1)) == 0)
				break;
		ci->ci_iobits = (mask + 1) & ~1;	/* should be even */
	}	
	}
#endif	/* BUGGY_RATOC_CIS */
	
	return 0;
}

/* vers 1 */
static int
pccscis_anal_vers_1(ap, tp, ci)
	pccs_cis_tag_t ap;
	struct tuple_data *tp;
	struct card_info *ci;
{
	cis_addr_t offs;
	int i, sz;

	memset(ci->ci_product, 0, PRODUCT_NAMELEN);
	sz = tp->tp_size - sizeof(struct v1_version);
	offs = CIS_DATAP(tp->tp_offs) + sizeof(struct v1_version);
	if (sz <= 0 || sz >= PRODUCT_NAMELEN)
		return EINVAL;

	if (read_cis_mem(ap, offs, sz, ci->ci_product) != 0)
	{
		memset(ci->ci_product, 0, PRODUCT_NAMELEN);
		return EINVAL;
	}

	for (i = 0; i < sz; i++)
	{
		if (ci->ci_product[i] < ' ')
			ci->ci_product[i] = ' ';
		if (ci->ci_product[i] == CISTD_END)
			ci->ci_product[i] = 0;
	}
	return 0;
}

/* config */
static int
pccscis_anal_config(ci, data, maddr)
	struct card_info *ci;
	u_int8_t *data;
	u_int8_t *maddr;
{
	struct card_registers *cr = &ci->ci_cr;
	u_int radr_sz, rmsk_sz;
	u_int8_t sz;

 	sz = read_data(&data, sizeof(sz), maddr);

	cr->cr_lastidx = read_data(&data, sizeof(u_int8_t), maddr);
	cr->cr_lastidx &= INDEX_MASK;

	radr_sz = TPCC_SZ_RADR(sz);
	cr->cr_offset = read_data(&data, radr_sz, maddr);

	rmsk_sz = TPCC_SZ_RMASK(sz);
	if (rmsk_sz > sizeof(cr->cr_mask))
		rmsk_sz = sizeof(cr->cr_mask);
	cr->cr_mask = read_data(&data, rmsk_sz, maddr);

	return 0;
}

#ifdef	PCCSCIS_REQURIED
/* device */
static int
pccscis_anal_device(dvdsc, data, maddr)
	struct dvdsc *dvdsc;
	u_int8_t *data;
	u_int8_t *maddr;
{
	u_int8_t val;

	val = read_data(&data, sizeof(val), maddr);
	dvdsc->wps = ((val & DEVID_WPS) != 0);
	dvdsc->type = DEVID_DTYPE(val);
	dvdsc->speed = DEVID_SPEED(val);
	if (dvdsc->speed == DEVID_SPEXT)
	{
		do
		{
			val = read_data(&data, sizeof(val), maddr);
		}
		while (data < maddr && val != CISTD_END && (val & DEVID_EXTSB));
	}

	val = read_data(&data, 1, maddr);
	dvdsc->unit = DEVSZ_SIZE(val);
	dvdsc->addr = DEVSZ_ADDR(val);

	/* XXX: ignore other device info */
	while (data < maddr && val != CISTD_END)
		val = read_data(&data, sizeof(val), maddr);

	return 0;
}
#endif	/* PCCSCIS_REQURIED */

/********************************************
 * CIS parse main
 *******************************************/
static void parse_start __P((struct tuple_data *, struct tuple_request *, u_int, u_int, u_int));

static void
parse_start(tp, req, tcode, tmfc, flags)
	struct tuple_data *tp;
	struct tuple_request *req;
	u_int tcode, tmfc, flags;
{

	if (tp != NULL)
		tuple_init(tp);
	memset(req, 0, sizeof(*req));
	req->tr_code = tcode;
	req->tr_mfcid = tmfc;
	req->tr_flags |= flags;
}

#define	TUPLE_BUFSZ 260
int
pccscis_parse_cis(ap, ci, flags)
	pccs_cis_tag_t ap;
	struct card_info *ci;
	int flags;
{
	struct tuple_data td;
	struct tuple_request req;
	struct card_info dci;
	u_int pflags;
	int mfcid;
	u_int8_t func, index, *data, *maddr;
	u_int8_t buf[TUPLE_BUFSZ + 1];

	mfcid = PCCS_GET_MFCID(ci);
	if (mfcid >= PCCS_MAXMFC || mfcid < 0)
		return EINVAL;

	index = PCCS_GET_INDEX(ci);
	if (index != PUNKINDEX)
		index &= INDEX_MASK;

	maddr = &buf[TUPLE_BUFSZ];
	pflags = 0;
	if (flags & PCCS_NOT_FOLLOW_LINK)
		pflags |= NOT_FOLLOW_LINK;

	dci = *ci;

	parse_start(&td, &req, CIS_END, mfcid, pflags);
	while (get_next_tuple(ap, &req, &td) == 0)
	{
		if (read_cis_mem(ap, CIS_DATAP(td.tp_offs), td.tp_size, buf))
			continue;

		data = buf;
		switch (td.tp_code)
		{
#ifdef	PCCSCIS_REQURIED
		case CIS_DEVICE:
			pccscis_anal_device(&dci.cdv, buf, maddr);
			continue;

		case CIS_DEVICE_A:
			pccscis_anal_device(&dci.adv, buf, maddr);
			continue;
#endif	/* PCCSCIS_REQUIRED */

		case CIS_CONFIG:
			pccscis_anal_config(&dci, buf, maddr);
			continue;

		case CIS_VERS_1:
			pccscis_anal_vers_1(ap, &td, &dci);
			continue;

		case CIS_CFTABLE_ENTRY:
			if (read_data(&data, 1, maddr) & TPCE_INDEX_DEFAULT)
				pccscis_anal_centry(&dci, buf, maddr);
			continue;

		case CIS_FUNCID:
			func = read_data(&data, 1, maddr);
			if (td.tp_lnkid == PUNKMFCID)
			{
				if (func != DV_FUNCID_MULTI)
				{
					td.tp_plnkid ++;	
					if (td.tp_plnkid == mfcid)
						dci.ci_function = func;
				}
			}
			else if (td.tp_lnkid == mfcid + 1)
				dci.ci_function = func;
			continue;

		case CIS_MANFID:
			dci.ci_manfid = read_data(&data, 2, maddr) << 16;
			dci.ci_manfid |= read_data(&data, 2, maddr);
			continue;
		}
	}

	dci.ci_nmfc = td.tp_lnkcnt;

	*ci = dci;
	if (index == PUNKINDEX && pccs_show_info == 0)
		return 0;

	parse_start(&td, &req, CIS_CFTABLE_ENTRY, mfcid, pflags);
	while (get_next_tuple(ap, &req, &td) == 0)
	{
		if (read_cis_mem(ap, CIS_DATAP(td.tp_offs), td.tp_size, buf))
			continue;
		if (flags == PCCS_FOLLOW_LINK && td.tp_lnkid <= 0)
			continue;

		*ci = dci;
		pccscis_anal_centry(ci, buf, maddr);
		if (PCCS_GET_INDEX(ci) == index)
			return 0;

		if (pccs_show_info != 0)
			show_card_info(ci);
	}

	return EINVAL;
}

/********************************************
 * Exported
 *******************************************/
#ifdef	_KERNEL
#include <sys/malloc.h>
static int pccscis_space_prefer __P((pccshw_tag_t, struct card_info *ci, u_int32_t));

#ifndef	PCCS_IOADDR_BASE
#define	PCCS_IOADDR_BASE	0x4000
#endif	/* PCCS_IOADDR_BASE */

static int
pccscis_space_prefer(pp, ci, disable)
	pccshw_tag_t pp;
	struct card_info *ci;
	u_int32_t disable;
{
	bus_space_tag_t t;
	slot_device_res_t dr;
	struct slot_device_iomem *imp;
	int idx, error = ENOENT;
	u_long skip, addr, base, hi, boundary;

	dr = &ci->ci_res;

#ifdef	PCCS_DEBUG
	printf("pccscis_prefer: IN\n");
	show_card_info(ci);
#endif	/* PCCS_DEBUG */

	t = pp->pp_memt;
	imp = &dr->dr_im[0];
	for (idx = 0; idx < pp->pp_nim; idx ++, imp ++)
	{
		if (imp->im_type != SLOT_DEVICE_SPMEM)
			continue;
		if (disable & (1 << idx))
			continue;

		addr = imp->im_hwbase;
		if (addr == SLOT_DEVICE_UNKVAL)
			continue;

		boundary = imp->im_size;
		if (boundary == 0)
			boundary = pp->pp_psz;
		boundary = ((boundary + pp->pp_psz - 1) & (~(pp->pp_psz - 1)));

		if (addr < pp->pp_lowmem)
			addr = pp->pp_lowmem;
		addr = ((addr + boundary - 1) & (~(boundary - 1)));

		hi = pp->pp_highmem - boundary;

		error = bus_space_map_prefer(t, &addr, hi, boundary, imp->im_size);
		if (error != 0)
			return error;

		imp->im_hwbase = addr;
	}

	if (ci->ci_iobits <= DEFAULT_IOBITS)
	{
		skip = (1 << DEFAULT_IOBITS);
		base = pp->pp_startio;
	}
	else
	{
		skip = (1 << ci->ci_iobits);
		base = pp->pp_lowio;
	}

	t = pp->pp_iot;
	for ( ; base < pp->pp_highio; base += skip)
	{
		imp = &dr->dr_im[0];
		for (idx = 0; idx < pp->pp_nim; idx ++, imp ++)
		{
			if (imp->im_type != SLOT_DEVICE_SPIO)
				continue;
			if (disable & (1 << idx))
				continue;

			addr = imp->im_hwbase;
			if (addr == SLOT_DEVICE_UNKVAL)
				continue;

			addr = (addr & (skip - 1)) + base;
			error = bus_space_map_prefer(t, &addr, addr, 0,
					     	     imp->im_size);
#ifdef	PCCS_DEBUG
			printf("check addr %lx size %lx ok %d\n",
			        addr, imp->im_size, error);
#endif	/* PCCS_DEBUG */

			if (error != 0)
				break;

			imp->im_hwbase = addr;
		}

		if (idx == pp->pp_nim)
		{
#ifdef	PCCS_DEBUG
			printf("pccscis_prefer: OUT\n");
			show_card_info(ci);
#endif	/* PCCS_DEBUG */
			return 0;
		}
	}

	return ENOSPC;
}

int
pccscis_find_config(ssc, ci, disable)
	struct slot_softc *ssc;
	struct card_info *ci;
	u_int32_t disable;
{
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	pccs_cis_tag_t ap = ssc;
	struct tuple_request req;
	struct tuple_data td;
	int mfcid, error;
	u_int8_t index;
	u_int8_t bp[TUPLE_BUFSZ + 1];

	mfcid = PCCS_GET_MFCID(ci);
	if (mfcid >= PCCS_MAXMFC || mfcid < 0)
		return EINVAL;

	index = PCCS_GET_INDEX(ci);

	/* step 1 */
	error = pccscis_parse_cis(ap, ci, PCCS_FOLLOW_LINK);
	if (error == 0 && PCCS_GET_INDEX(ci) != PUNKINDEX)
	{
		error = pccscis_space_prefer(pp, ci, disable);
		if (error == 0)
			return error;
	}

	/* step 2 */
	PCCS_SET_INDEX(ci, index);
	error = pccscis_parse_cis(ap, ci, PCCS_NOT_FOLLOW_LINK);
	if (error == 0 && PCCS_GET_INDEX(ci) != PUNKINDEX)
	{
		error = pccscis_space_prefer(pp, ci, disable);
		if (error == 0)
			return error;
	}
	
	/* step 3 */
	if (index != PUNKINDEX)
		return EINVAL;

	parse_start(&td, &req, CIS_CFTABLE_ENTRY, mfcid, 0);
	while (get_next_tuple(ap, &req, &td) == 0)
	{
		if (read_cis_mem(ap, CIS_DATAP(td.tp_offs), td.tp_size, bp))
			continue;
		if (td.tp_lnkid <= 0)
			continue;
		pccs_initres(&ci->ci_res);
		pccscis_anal_centry(ci, bp, bp + TUPLE_BUFSZ);
		if (pccscis_space_prefer(pp, ci, disable) == 0)
			return 0;
	}	

	/* step 4 */
	parse_start(&td, &req, CIS_CFTABLE_ENTRY, mfcid, NOT_FOLLOW_LINK);
	while (get_next_tuple(ap, &req, &td) == 0)
	{
		if (read_cis_mem(ap, CIS_DATAP(td.tp_offs), td.tp_size, bp))
			continue;
		pccs_initres(&ci->ci_res);
		pccscis_anal_centry(ci, bp, bp + TUPLE_BUFSZ);
		if (pccscis_space_prefer(pp, ci, disable) == 0)
			return 0;
	}	

	return ENOSPC;
}

u_int8_t *
pccscis_cis_tget(dh, code)
	slot_device_handle_t dh;
	u_int code;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	pccs_cis_tag_t ap;
	struct slot_softc *ssc;
	struct card_info *ci;
	struct tuple_data *tp;
	struct tuple_request req;
	u_int reqno, subno;
	u_int8_t *pos;
	u_long offs;
	int mfcid;

	ssc = st->st_sc;
	ap = ssc;
	if (ssc->sc_atbp == NULL)
		return NULL;

	if ((ci = dh->dh_ci) == NULL)
		return NULL;

	mfcid = PCCS_GET_MFCID(ci);
	tp = &ssc->sc_si[mfcid]->si_tdata;
	reqno = CIS_MAJOR_REQCODE(code);
	subno = CIS_MINOR_REQCODE(code);

	switch (reqno)
	{
	case CIS_IDSTRING:
		return ci->ci_product;

	case CIS_REQINIT:
		tuple_init(tp);
		offs = CIS_ATTR_ADDR(subno * NBPG);
		if (update_cis_cache(ap, offs))
			return NULL;
		return ssc->sc_atbp;

	default:
		break;
	}

	parse_start(NULL, &req, reqno, mfcid, 0);
	while (get_next_tuple(ap, &req, tp) == 0)
	{
		pos = ssc->sc_atbp + 
		      CIS_COMM_ADDR(tp->tp_offs) - CIS_COMM_ADDR(ssc->sc_aoffs);
		if (subno == CIS_MINOR_REQCODE(CIS_REQUNK))
			return pos;

		if (CIS_CODE(CIS_DATAP(pos)) != subno)
			continue;
		return (u_int8_t *) CIS_DATAP(pos);
	}

	return NULL;
}
#endif	/* _KERNEL */
