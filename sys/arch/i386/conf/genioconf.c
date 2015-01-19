/*	$NecBSD: genioconf.c,v 3.30.2.1 1999/08/17 06:31:19 honda Exp $	*/
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

/*****************************************************
 * main ioconf
 *****************************************************/
#include "ioconf.c"

/*****************************************************
 * devlist (PnP resources)
 *****************************************************/
#include <lib/libkern/libkern.h>
#include <machine/devlist.h>
#include "gendevlist.h"

static int dlq_id_match __P((struct devlist_query *, struct devlist *, int));
static inline u_long dlq_id_aliases __P((struct devlist *));
#define	DLQ_FULL_MASK	((u_long) -1)

static inline u_long
dlq_id_aliases(dlp)
	struct devlist *dlp;
{

	return dlp->dl_idmask;
}

static int
dlq_id_match(dlqp, dlp, pri)
	struct devlist_query *dlqp;
	struct devlist *dlp;
	int pri;
{
	u_long ckmask;

	if (pri == DLQ_PRI_MIDDLE)
		ckmask = dlq_id_aliases(dlp);
	else
		ckmask = DLQ_FULL_MASK;

	return (dlqp->dlq_id & ckmask) != (dlp->dl_id & ckmask);
}

struct devlist *
devlist_query_resource(dtag, stp, pri, dlqp)
	devlist_res_tag_t dtag;
	devlist_res_magic_t *stp;
	int pri;
	struct devlist_query *dlqp;
{
	struct devlist *dlp;
	int start, slevel;

	if (pri >= DLQ_PRI_NPRI)
		return NULL;

	start = *stp;
	if (start < 0)
		return NULL;

	dlp = &devlist[start];
	for ( ;dlp->dl_prot != NULL; dlp ++, start ++)
	{
		if (strcmp(dlqp->dlq_prot, dlp->dl_prot) != 0)
			continue;

		if (dlp->dl_skey != NULL)
		{
			slevel = DLQ_PRI_HIGH;
		}
		else if (dlp->dl_id != 0)
		{
			if (dlq_id_aliases(dlp) != DLQ_FULL_MASK)
				slevel = DLQ_PRI_MIDDLE;
			else	
				slevel = DLQ_PRI_HIGH;
		}
		else
			slevel = DLQ_PRI_LOW;

		if (slevel != pri)
			continue;

		/*
		 * functions and device classes should have the same values.
		 */
		if (dlqp->dlq_func != dlp->dl_func ||
		    dlqp->dlq_class != dlp->dl_class)
			continue;

		/*
		 * identify 
		 */
		if (slevel == DLQ_PRI_LOW)
		{
			/* PRI_LOW (week match)
			 * magic key 0 and 1 should exactly 
			 * take the same values.
			 */
			if (dlp->dl_mkey0 != 0 && 
			    dlqp->dlq_mkey0 != dlp->dl_mkey0)
				continue;
			if (dlp->dl_mkey1 != 0 && 
			    dlqp->dlq_mkey1 != dlp->dl_mkey1)
				continue;
		}
		else
		{
			/* 
			 * PRI_MIDDLE OR PRI_HIGH (strict match)
			 * id and str should be the same values.
			 */
			if (dlp->dl_id != 0 &&
			    dlq_id_match(dlqp, dlp, pri) != 0)
				continue;

			if (dlp->dl_skey != NULL)
			{
				if (dlqp->dlq_skey == NULL)
					continue;
				if (strncmp(dlqp->dlq_skey, dlp->dl_skey,
					    strlen(dlp->dl_skey)) != 0)
					continue;
			}
		}

		*stp = start + 1;
		return dlp;
	}

	*stp = -1;
	return NULL;
}

#include "opt_magic.h"
#ifdef	CONFIG_DEVICES
/*****************************************************
 * option magic
 *****************************************************/
#include <i386/Cbus/dev/magicvar.h>
#include "locators.h"

int
magic_is_isadev(cf)
	struct cfdata *cf;
{
	u_short pvec = *cf->cf_parents;

	return (cfdata[pvec].cf_driver == &isa_cd);
}

struct cfdata *
magic_next_dev(cf)
	struct cfdata *cf;
{

	return (cf + 1);
}

int
magic_is_sameG(resname, ocf, dcf)
	u_char *resname;
	struct cfdata *ocf, *dcf;
{

	if (dcf->cf_driver == NULL ||
	    strcmp(ocf->cf_driver->cd_name, dcf->cf_driver->cd_name) != 0 ||
	    ocf->cf_parents != dcf->cf_parents)
		return 1;
	return 0;
}

struct cfdata *
magic_find_dev(resname, dvname)
	u_char *resname;
	u_char *dvname;
{
	struct cfdriver *cf;
	struct cfdata *cdp;

	for (cdp = &cfdata[0]; (cf = cdp->cf_driver) != NULL; cdp ++)
	{
		if (strcmp(cf->cd_name, dvname) == 0)
			return cdp;
	}

	return NULL;
}
#endif	/* CONFIG_DEVICES */
