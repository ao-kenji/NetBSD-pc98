/*	$NecBSD: autoscan.h,v 1.20.10.1 1999/08/28 02:24:05 honda Exp $	*/
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

#ifndef _I386_AUTOSCAN_H_
#define _I386_AUTOSCAN_H_

#include <vm/vm.h>
#include <machine/sysinfo.h>
#include <machine/diskutil.h>

struct dos_check_args {
	struct disklabel *lp;		/* IN: disklabel pointer */
	struct dos_partition *dp;	/* IN: cpulabel pointer */

	struct buf *bp;			/* IN: data buffer */
	void (*start) __P((struct buf *));	/* IN: strategy routine */

#define	GEO_CHG_EN	0x01
	u_int flag;			/* IN: control flags */

	u_int ssec;			/* OUT: FFS partition starting secs */
	u_int cyl;			/* OUT: FFS partition starting cyl */
	u_int nsecs;			/* OUT: FFS partition sec size */
	u_int fake;			/* OUT: fake entries */

#define	NODOSPART	0
#define	PC98_DOSPART	1
#define	PC98_FFSPART	2
#define	AT_DOSPART	3
#define	AT_FFSPART	4
	u_int kind;			/* OUT: result */
};

/* scsi disk geo */
struct scsi_disk_geometry {
	u_int16_t sdg_tracks;
	u_int16_t sdg_sectors;
};

struct sdg_conv_element {
	struct scsi_disk_geometry sce_org;
	struct scsi_disk_geometry sce_mod;
};

struct sdg_conv_table {
	u_int sct_sz;
	struct sdg_conv_element *sct_pt;
};

/* macro */
#define	SDG_CONV_ELEMENT_SZ(array) (sizeof(array)/sizeof(struct sdg_conv_element))

#ifdef	_KERNEL
u_char *make_fake_disklabel __P((struct disklabel *, struct dos_partition *, u_char *, int));
u_int check_dospart __P((struct dos_check_args *));
#endif /* _KERNEL */
#endif /* !_I386_AUTOSCAN_H_ */
