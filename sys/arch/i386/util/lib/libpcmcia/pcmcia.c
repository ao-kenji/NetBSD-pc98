/*	$NecBSD: pcmcia.c,v 1.16.6.1 1999/08/22 19:46:49 honda Exp $	*/
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
 * Pcmcia card information service program.
 * Written by N. Honda.
 * Version 0.0 ALPHA(0)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/slot_device.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>

static int DevFd;
static u_int pccs_show_info;
static int read_cis_mem __P((u_int8_t *, cis_addr_t, cis_size_t, u_int8_t *));
static void pccscis_merge_wininfo __P((struct slot_device_iomem *, struct slot_device_iomem *));
void bits_expand __P((u_int, u_char *));
int PcmciaParseCis __P((int, struct card_info *, int, int));

static cis_addr_t Cisoffset;

static int
read_cis_mem(abuf, offs, len, buf)
	u_int8_t *abuf;
	cis_addr_t offs;
	cis_size_t len;
	u_int8_t *buf;
{
	cis_addr_t offset, page;

	offset = offs & (CARD_ATTR_SZ - 1);
	page = offs - offset;

	if (page != Cisoffset)
	{
		lseek(DevFd, (off_t) page, 0);
		if (read(DevFd, abuf, CARD_ATTR_SZ) != CARD_ATTR_SZ)
			return EINVAL;

		Cisoffset = page;
	}
	
	if (offset + len >= CARD_ATTR_SZ)
		return EINVAL;

	if (buf != NULL)
		bcopy(abuf + offset, buf, len);
	return 0;
}

int
PcmciaParseCis(fd, ci, flags, showinfo)
	int fd;
	struct card_info *ci;
	int flags, showinfo;
{
	u_int8_t buf[CARD_ATTR_SZ];

	if (ioctl(fd, PCCS_IOG_ATTR, buf))
	{
		perror("get attribute");
		return EINVAL;
	}

	DevFd = fd;
	pccs_show_info = showinfo;
	Cisoffset = CIS_ATTR_ADDR(0);

	return pccscis_parse_cis(buf, ci, flags);
}


int
pccscis_match_config(kci, ci)
	struct card_info *kci, *ci;
{
	int mfcid = PCCS_GET_MFCID(ci);
	u_int mask, len, i;
	struct slot_device_iomem *wp, *kwp;

	if (mfcid != PCCS_GET_MFCID(kci) ||
	    PCCS_GET_INDEX(ci) != PCCS_GET_INDEX(kci))
		return ENOENT;

	len = strlen(kci->ci_product);
	if (len > 0)
	{
		if (strncmp(kci->ci_product, ci->ci_product, len) == 0)
			return 0;
		return ENOENT;
	}

	wp = &ci->ci_res.dr_io[0];
	kwp = &kci->ci_res.dr_io[0];
	for (i = 0; i < MAX_IOWF; i++, wp ++, kwp ++)
		if (kwp->im_base != PUNKADDR && wp->im_base != kwp->im_base)
			return ENOENT;

	if (kci->ci_function != ci->ci_function)
		return ENOENT;

	mask = (1 << kci->ci_res.dr_pin[0].dc_chan);
	if (kci->ci_res.dr_pin[0].dc_chan == PUNKIRQ ||
	    kci->ci_res.dr_pin[0].dc_chan == PAUTOIRQ ||
	    (ci->ci_res.dr_pin[0].dc_aux & mask))
		return 0;

	return ENOENT;
}

int
pccscis_merge_config(kc, mc, overwrite)
	struct card_info *kc, *mc;
	int overwrite;
{
	struct slot_device_resources *kr, *mr;
	struct card_registers *kcr;
	struct card_registers *mcr;
	int i;

	kr = &kc->ci_res;
	mr = &mc->ci_res;

	if (overwrite != 0)
	{
		/* bus resrouces overwirte */
		mr->dr_pin[0].dc_flags ^= kr->dr_pin[0].dc_flags;
		if (kr->dr_pin[0].dc_chan != PUNKIRQ)
			mr->dr_pin[0].dc_chan = kr->dr_pin[0].dc_chan;

		if (kr->dr_drq[0].dc_chan != PUNKDRQ)
			mr->dr_drq[0].dc_chan = kr->dr_drq[0].dc_chan;

		for (i = 0; i < MAX_IOWF; i++)
			pccscis_merge_wininfo(&kr->dr_io[i], &mr->dr_io[i]);

		for (i = 0; i < MAX_MEMWF; i++)
			pccscis_merge_wininfo(&kr->dr_mem[i], &mr->dr_mem[i]);
	}

	kcr = &kc->ci_cr;	
	mcr = &mc->ci_cr;	

	if (kcr->cr_offset != 0)
		mcr->cr_offset = kcr->cr_offset;
	mcr->cr_mask ^= kcr->cr_mask;
	mcr->cr_ccsr ^= kcr->cr_ccsr;
	mcr->cr_ccor = kcr->cr_ccor;
	mcr->cr_ccmor = kcr->cr_ccmor;
	mcr->cr_scr = kcr->cr_scr;

	if (kc->ci_delay > mc->ci_delay && kc->ci_delay <= 5000)
		mc->ci_delay = kc->ci_delay;

	return 0;
}

/* misc util */
static void
pccscis_merge_wininfo(kwp, mwp)
	struct slot_device_iomem *kwp, *mwp;
{

	if (kwp->im_hwbase != SLOT_DEVICE_UNKVAL)
		mwp->im_hwbase = kwp->im_hwbase;
	if (kwp->im_base != SLOT_DEVICE_UNKVAL)
		mwp->im_base = kwp->im_base;
	if (kwp->im_size != SLOT_DEVICE_UNKVAL)
		mwp->im_size = kwp->im_size;
	mwp->im_flags ^= kwp->im_flags;

	if (mwp->im_size != SLOT_DEVICE_UNKVAL && 
	    mwp->im_hwbase != SLOT_DEVICE_UNKVAL)
		mwp->im_flags |= SDIM_BUS_ACTIVE;
	else
		mwp->im_flags &= ~SDIM_BUS_ACTIVE;
	mwp->im_type = kwp->im_type;
}

void
bits_expand(ul, p)
	u_int ul;
	u_char *p;
{
	int tmp;
	u_char n;

	printf("0x%x", ul);
	p++;
	for (tmp = 0; n = *p++; )
	{
		if (ul & (1 << (n - 1)))
		{
			putchar(tmp ? ',' : '<');
			for (; (n = *p) > ' '; ++p)
				putchar(n);
			tmp = 1;
		}
		else
			for (; *p > ' '; ++p)
				continue;
	}
	if (tmp)
		putchar('>');
	printf("\n");
}

/********************************************
 * includes
 *******************************************/
#define	CARDINFO
#include "i386/pccs/pccs.c"
#include "i386/pccs/pccscis.c"
