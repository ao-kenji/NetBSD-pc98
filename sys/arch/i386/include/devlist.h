/*	$NecBSD: devlist.h,v 3.9 1998/09/12 14:12:22 honda Exp $	*/
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

#ifndef _I386_DEVLIST_H_
#define _I386_DEVLIST_H_

#define	DLQ_PRI_NPRI	3
#define	DLQ_PRI_HIGH	0
#define	DLQ_PRI_MIDDLE	1
#define	DLQ_PRI_LOW	2

struct devlist {
	u_char *dl_prot;		/* protocol name */

	u_long dl_id;			/* ident number */
	u_long dl_idmask;		/* ident number mask */
	u_long dl_func;			/* function number */
	u_long dl_class;		/* device class */

	struct cfdriver *dl_cd;		/* device cfdriver */
	u_long dl_dvcfg;		/* device flags */
	u_long dl_buscfg;		/* device bus flags */

	u_long dl_mkey0;		/* misc option key0 */
	u_long dl_mkey1;		/* misc option key1 */
	u_char *dl_skey;		/* misc option str */
};

struct devlist_query {
	u_char *dlq_prot;		/* protocol name */

	u_long dlq_id;			/* ident number */
	u_long dlq_func;		/* function number */
	u_long dlq_class;		/* function class */

	u_long dlq_mkey0;		/* misc key */
	u_long dlq_mkey1;		/* misc key */
	u_char *dlq_skey;		/* misc str */
};

typedef	void *devlist_res_tag_t;
typedef	int devlist_res_magic_t;

#define	DEVLIST_RES_DEFAULT_TAG			((devlist_res_tag_t) (0))
#define	devlist_init_res_magic(tag, drp)	((*(drp)) = 0)

struct devlist *devlist_query_resource __P((devlist_res_tag_t,
	devlist_res_magic_t *, int, struct devlist_query *));
#endif /* _I386_DEVLIST_H_ */
