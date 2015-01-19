/*	$NecBSD: ippres.c,v 1.10 1998/03/14 07:06:26 kmatsuda Exp $	*/
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
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <i386/Cbus/pnp/pnpreg.h>
#include <i386/Cbus/pnp/ippiio.h>

#define	IPPRES_READ_1(bp) (*(u_int8_t *) bp)
#define	IPPRES_READ_2(bp) (*(u_int16_t *) bp)
#define	IPPRES_READ_4(bp) (*(u_int32_t *) bp)

int ippres_read __P((u_int8_t *, int, int, void *));
int ippres_next_item __P((u_int8_t *, int, u_int8_t));
int ippres_parse_lrdt __P((u_int8_t *, int, ippres_res_t));
int ippres_parse_items __P((u_int8_t *, struct ippres_request *, ippres_res_t));

int
ippres_read(ap, offs, size, bp)
	u_int8_t *ap;
	int offs, size;
	void *bp;
{

	if (size < 0 || offs < 0 || offs + size >= MAX_RDSZ)
		return EINVAL;

	bcopy(ap + offs, bp, size);
	return 0;
}

int
ippres_parse_lrdt(ap, offs, irr)
	u_int8_t *ap;
	int offs;
	ippres_res_t irr;
{
	struct lrdt_entry le;
	struct lrdt_32fmrd l32fm;
	struct lrdt_32mrd l32m;
	struct lrdt_mrd lm;
	slot_device_res_t dr = &irr->irr_dr;
	int nmem;

	if (ippres_read(ap, offs, sizeof(le), &le))
		return EINVAL;

	switch (LRDT_NAME(le.le_type))
	{
	case LRDT_MRD:
		if ((nmem = dr->dr_nmem) >= SLOT_DEVICE_NMEM)
			return 0;
		if (ippres_read(ap, offs, sizeof(lm), &lm))
			return EIO;
		if (lm.lm_meminf & (MEMINF_16BITS | MEMINF_ANYBITS))
			dr->dr_mem[nmem].im_flags |= 
				(SDIM_BUS_WIDTH16 | MCR_BUS16);
		dr->dr_mem[nmem].im_hwbase = ((u_long)lm.lm_loaddr) << NBBY;	
		dr->dr_mem[nmem].im_size = ((u_long) lm.lm_blen * 256);
		irr->irr_mem[nmem].mem_hi = ((u_long) lm.lm_hiaddr) << NBBY;
		irr->irr_mem[nmem].mem_al = (u_long) lm.lm_align;
		irr->irr_mem[nmem].mem_bussz = 16;
		dr->dr_nmem ++;
		break;

	case LRDT_32MRD:
		if ((nmem = dr->dr_nmem) >= SLOT_DEVICE_NMEM)
			return 0;
		if (ippres_read(ap, offs, sizeof(l32m), &l32m))
			return EIO;
		if (l32m.l32m_meminf & (MEMINF_16BITS | MEMINF_ANYBITS))
			dr->dr_mem[nmem].im_flags |= 
			(SDIM_BUS_WIDTH32 | SDIM_BUS_WIDTH16 | MCR_BUS16);
		dr->dr_mem[nmem].im_hwbase = l32m.l32m_loaddr;
		dr->dr_mem[nmem].im_size = l32m.l32m_len;
		irr->irr_mem[nmem].mem_hi = l32m.l32m_hiaddr;
		irr->irr_mem[nmem].mem_al = l32m.l32m_align;
		irr->irr_mem[nmem].mem_bussz = 32;
		dr->dr_nmem ++;

	case LRDT_32FMRD:
		if ((nmem = dr->dr_nmem) >= SLOT_DEVICE_NMEM)
			return 0;
		if (ippres_read(ap, offs, sizeof(l32fm), &l32fm))
			return EIO;
		if (l32fm.l32fm_meminf & (MEMINF_16BITS | MEMINF_ANYBITS))
			dr->dr_mem[nmem].im_flags |= 
			(SDIM_BUS_WIDTH32 | SDIM_BUS_WIDTH16 | MCR_BUS16);
		dr->dr_mem[nmem].im_hwbase = l32fm.l32fm_offset;
		dr->dr_mem[nmem].im_size = l32fm.l32fm_len;
		irr->irr_mem[nmem].mem_hi = l32fm.l32fm_offset;
		irr->irr_mem[nmem].mem_al = 0;
		irr->irr_mem[nmem].mem_bussz = 32;
		dr->dr_nmem ++;
		break;

	default:
		break;
	}

	return 0;
}

int
ippres_next_item(ap, offs, code)
	u_int8_t *ap;
	int offs;
	u_int8_t code;
{	
	struct lrdt_entry le;

	if (SRDT_LTAG(code) == 0)
	{
		return offs + (SRDT_LEN(code) + sizeof(code));
	}
	else
	{
		if (ippres_read(ap, offs, sizeof(le), &le))
			return -1;
		return offs + (le.le_size + sizeof(le));
	}
}

int
ippres_parse_items(ap, ir, irr)
	u_int8_t *ap;
	struct ippres_request *ir;
	ippres_res_t irr;
{
	slot_device_res_t dr = &irr->irr_dr;
	struct srdt_ldid sl;
	int error, offs, cldn, cdfn, loopx, found;
	u_int8_t code;
	
	found = 0;
	ir->ir_ndfn = 0;
	cdfn = cldn = -1;
	offs = sizeof(struct serial_id);
	error = EIO;

	dr->dr_nio = dr->dr_nmem = 0;
	dr->dr_npin = dr->dr_ndrq = 0;
	irr->irr_pri = 0;
	irr->irr_flags = 0;

	for (loopx = 0; loopx < MAX_RDSZ; loopx ++)
	{
		if (offs < 0 || ippres_read(ap, offs, sizeof(u_int8_t), &code))
			break;

		if (SRDT_LTAG(code) == 0)
		{
			switch(SRDT_NAME(code))
			{
			case SRDT_LDID:
				cldn ++;
				if (ir->ir_ldn == cldn)
					found = 1;
				if (ippres_read(ap, offs, sizeof(sl), &sl) != 0)
					goto out;
				if (sl.sl_id == ir->ir_cid)
					irr->irr_flags |= IPPRES_LDEV;
				if (sl.sl_rm0 & RM0_BOOTABLE)	
					irr->irr_flags |= IPPRES_BDEV;
				offs = ippres_next_item(ap, offs, code);
				continue;

			case SRDT_SDF:
				cdfn ++;
				ir->ir_ndfn ++;
				if (SRDT_LEN(code) >= 1)
				{
					struct srdt_sdf ss;

					if (ippres_read(ap, offs, sizeof(ss),
							&ss) != 0)
						goto out;
					irr->irr_pri = ss.ss_pri;
				}
				offs = ippres_next_item(ap, offs, code);
				continue;

			case SRDT_EDF:
				cdfn = -1;
				offs = ippres_next_item(ap, offs, code);
				continue;

			case SRDT_ET:
				error = (found != 0) ? 0 : ENOENT;
				goto out;
			}
		}

		if (ir->ir_ldn != cldn || (cdfn >= 0 && cdfn != ir->ir_dfn))
		{
			offs = ippres_next_item(ap, offs, code);
			continue;
		}

		if (SRDT_LTAG(code) != 0)
		{
			if (ippres_parse_lrdt(ap, offs, irr) != 0)
				goto out;

			offs = ippres_next_item(ap, offs, code);
			continue;
		}
			
		switch(SRDT_NAME(code))
		{
		case SRDT_IRQF:
		{
			struct srdt_irqf si;
			int npin;

			if ((npin = dr->dr_npin) >= SLOT_DEVICE_NPIN) 
				break;
			if (ippres_read(ap, offs, sizeof(si), &si))
				goto out;

			dr->dr_pin[npin].dc_chan = SLOT_DEVICE_UNKVAL;
			dr->dr_pin[npin].dc_aux = si.si_im;
			if (si.si_ifg & IFG_HIGH)
				dr->dr_pin[npin].dc_flags |= IRTR_HIGH;
			if ((si.si_ifg & (IFG_HIEDGE | IFG_LOEDGE)) == 0)
				dr->dr_pin[npin].dc_flags |= IRTR_LEVEL;
			dr->dr_npin ++;
			break;
		}

		case SRDT_DMAF:
		{
			struct srdt_dmaf sd;
			int ndrq;

			if ((ndrq = dr->dr_ndrq) >= SLOT_DEVICE_NDRQ) 
				break;
			if (ippres_read(ap, offs, sizeof(sd), &sd))
				goto out;

			dr->dr_drq[ndrq].dc_chan = 0;
			dr->dr_drq[ndrq].dc_aux = sd.sd_dm;
			dr->dr_ndrq ++;
			break;
		}			

		case SRDT_IOPD:
		{
			struct srdt_iopd si;
			int nio;

			if ((nio = dr->dr_nio) >= SLOT_DEVICE_NIO) 
				break;
			if (ippres_read(ap, offs, sizeof(si), &si))
				goto out;

			dr->dr_io[nio].im_hwbase = si.sio_loaddr;
			dr->dr_io[nio].im_size = si.sio_len;
			if (dr->dr_io[nio].im_size == 0)	/* XXX */
				dr->dr_io[nio].im_size = 8;	/* XXX */
			irr->irr_io[nio].io_hi = si.sio_hiaddr;
			irr->irr_io[nio].io_al = si.sio_align;
			dr->dr_nio ++;
			break;
		}

		case SRDT_FIOPD:
		{
			struct srdt_fiopd sf;
			int nio;

			if ((nio = dr->dr_nio) >= SLOT_DEVICE_NIO) 
				break;
			if (ippres_read(ap, offs, sizeof(sf), &sf))
				goto out;

			dr->dr_io[nio].im_hwbase = sf.sfio_offset;
			dr->dr_io[nio].im_size = sf.sfio_len;
			if (dr->dr_io[nio].im_size == 0)	/* XXX */
				dr->dr_io[nio].im_size = 8;	/* XXX */
			irr->irr_io[nio].io_hi = sf.sfio_offset;
			irr->irr_io[nio].io_al = 0;
			dr->dr_nio ++;
			break;
		}

		case SRDT_CDID:	
		{
			struct srdt_cdid sci;

			if (ippres_read(ap, offs, sizeof(sci), &sci) != 0)
				goto out;
			if (ir->ir_cid == sci.sc_id)
				irr->irr_flags |= IPPRES_CDEV;
			break;
		}

		case SRDT_VER:	
			break;
		}

		offs = ippres_next_item(ap, offs, code);
	}

out:
	return error;
}
