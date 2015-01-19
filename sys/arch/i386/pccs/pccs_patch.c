/*	$NecBSD: pccs_patch.c,v 1.10.6.2 1999/09/18 17:40:58 honda Exp $	*/
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

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

struct cis_patch_data {
	struct cis_patch_data *cd_next;
	u_long cd_offset;

#define	CD_BLKSIZE	8
	u_int8_t cd_data[CD_BLKSIZE];
};

struct cis_patch_entry {
	u_char *cp_name;
	struct cis_patch_entry *cp_next;

	struct cis_patch_data *cp_cd;
};

#define	PATCH_ENTRY	static struct cis_patch_entry
#define	PATCH_DATA	static struct cis_patch_data

/********************************************************
 * patch data section
 ********************************************************/
/* TDK DFL5610WS  patch data */
PATCH_DATA pdata_DFL5610WS_0x30 = {
 	NULL,					/* next patch data */
	0x30,					/* offset */
	{0, 0, 0, 0, 0, 0, 0xea ^ 0xf0, 0}	/* address line full */
};

/* APEX DATA MULTICARD ETHERNET-MODEM A patch data */
PATCH_DATA pdata_APEX_MULTICARD_130000_0xa0 = {
 	NULL,					/* next patch data */
	0xa0,					/* offset */
	{0x04 ^ 0x40, 0, 0, 0x0d ^ 0x00, 0, 0, 0, 0 }
};

/* NEC PC-9801N-J02/J02R patch data */
/* XXX: should more? */
PATCH_DATA pdata_PC9801NJ02_0x38 = {
 	NULL,					/* next patch data */
	0x38,					/* offset */
	{0, 0, 0x10 ^ 0x20, 0x20 ^ 0x00, 0, 0, 0, 0 }
};

/* TDK NetPartner DFL9610 patch data */
PATCH_DATA pdata_DFL9610_0xe0 = {
 	&pdata_PC9801NJ02_0x38,			/* next patch data */
	0xe0,					/* offset */
	{0, 0, 0, 0 ^ 0x08, 0x02 ^ 0x01, 0, 0, 0 }
};

PATCH_DATA pdata_DFL9610_0xd8 = {
 	&pdata_DFL9610_0xe0,			/* next patch data */
	0xd8,					/* offset */
	{0, 0, 0xff ^ 0x06, 0, 0, 0, 0 ^ 0xe8, 0x01 ^ 0x00}
};

/* Logitec LPM-LAN/FM patch data */
PATCH_DATA pdata_LPMLANFM_0xa0 = {
 	NULL,					/* next patch data */
	0xa0,					/* offset */
	{0x24 ^ 0x64, 0x0, 0xca ^ 0xea, 0x0, 0x0, 0x0, 0x07 ^ 0x1f, 0x0}
};

PATCH_DATA pdata_LPMLANFM_0x58 = {
 	&pdata_LPMLANFM_0xa0,			/* next patch data */
	0x58,					/* offset */
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x24 ^ 0x64, 0x0, 0xa3 ^ 0x83}
};

/* TDK Global Networker DFL3410 patch data */
PATCH_DATA pdata_DFL3410_0xd8 = {
 	NULL,					/* next patch data */
	0xd8,					/* offset */
	{0, 0, 0xff ^ 0x06, 0, 0, 0, 0, 0}
};

/********************************************************
 * patch entry section
 ********************************************************/
/* TDK DFL5610WS  */
PATCH_ENTRY entry_DFL5610WS = {
	"TDK DFL5610WS   ",
	NULL,					/* next patch entry */

	&pdata_DFL5610WS_0x30,			/* patch data pointer */
};

/* APEX DATA MULTICARD ETHERNET-MODEM A */
PATCH_ENTRY entry_APEX_MULTICARD_130000 = {
	"APEX DATA MULTICARD ETHERNET-MODEM",
	&entry_DFL5610WS,			/* next patch entry */

	&pdata_APEX_MULTICARD_130000_0xa0,	/* patch data pointer */
};

/* NEC PC-9801N-J02/J02R patch entry */
PATCH_ENTRY entry_PC9801NJ02 = {
	"NEC    PC-9801N-J02",			/* card name */
	&entry_APEX_MULTICARD_130000,		/* next patch entry */

	&pdata_PC9801NJ02_0x38,			/* patch data pointer */
};

/* TDK Mobile Networker DFL3200 patch entry */
PATCH_ENTRY entry_DFL3200 = {
	"TDK MobileNetworker DFL3200",		/* card name */
	&entry_PC9801NJ02,			/* next patch entry */

	/* same patch as DFL3410 */
	&pdata_DFL3410_0xd8,			/* patch data pointer */
};

/* TDK NetPartner DFL9610 patch entry */
PATCH_ENTRY entry_DFL9610 = {
	"TDK NetPartner DFL9610",		/* card name */
	&entry_DFL3200,				/* next patch entry */

	&pdata_DFL9610_0xd8,			/* patch data pointer */
};
	
/* Logitec LPM-LAN/FM patch entry */
PATCH_ENTRY entry_LPMLANFM = {
	"Logitec LPM-LAN/FM",			/* card name */
	&entry_DFL9610,				/* patch data pointer */

	&pdata_LPMLANFM_0x58,			/* patch data pointer */
};

/* TDK Global Networker DFL3410 patch entry */
PATCH_ENTRY entry_DFL3410 = {
	"TDK GlobalNetworker 3410/3412",	/* card name */
	&entry_LPMLANFM,			/* next patch entry */

	&pdata_DFL3410_0xd8,			/* patch data pointer */
};
	
/* entry head */
PATCH_ENTRY entry_head = {
	NULL,				/* name */
	&entry_DFL3410,			/* next patch entry */

	NULL				/* patch data pointer */
};

/********************************************************
 * util
 ********************************************************/
void cis_patch_apply __P((struct cis_patch_entry *, u_int8_t *, int));

void
cis_patch_apply(cp, buf, size)
	struct cis_patch_entry *cp;
	u_int8_t *buf;
	int size;
{
	struct cis_patch_data *cd = cp->cp_cd;
	u_long offset;
	int i;

	for ( ; cd != NULL; cd = cd->cd_next)
	{
		offset = cd->cd_offset;
		if (offset >= size - CD_BLKSIZE)
			continue;

		offset &= ~(CD_BLKSIZE - 1);
		for (i = 0; i < CD_BLKSIZE; i ++)
			buf[offset + i] ^= cd->cd_data[i];
	}
}

int
cis_patch_scan(ssc)
	struct slot_softc *ssc;
{
	struct card_info *ci = ssc->sc_mci;
	struct cis_patch_entry *cp = &entry_head;
	int error = ENOENT;

	if (ssc->sc_aoffs != CIS_ATTR_ADDR(0) || ci->ci_product[0] == 0)
		return error;

	for ( ; cp != NULL; cp = cp->cp_next)
	{
		if (cp->cp_name == NULL)
			continue;

		if (!strncmp(cp->cp_name, ci->ci_product, strlen(cp->cp_name)))
		{
			cis_patch_apply(cp, ssc->sc_atbp, ssc->sc_asize);
			return 0;
		}
	}

	return error;
}
