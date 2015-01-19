/*	$NecBSD: diskutil.h,v 3.3 1999/07/23 20:47:03 honda Exp $	*/
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

/* disk address conversion functions */
static __inline u_long disk_abs_sector	\
	__P((struct disklabel *, u_long, u_long, u_long));
static __inline u_long disk_start_sector	\
	__P((struct disklabel *, struct dos_partition *));
static __inline u_long disk_end_sector	\
	__P((struct disklabel *, struct dos_partition *));
static __inline u_long disk_size_sector \
	__P((struct disklabel *, struct dos_partition *));

static __inline u_long
disk_abs_sector(lp, cyl, head, sec)
	struct disklabel *lp;
	u_long cyl, head, sec;
{

	return cyl * lp->d_secpercyl + head * lp->d_nsectors + sec;
}

static __inline u_long
disk_start_sector(lp, dp)
	struct disklabel *lp;
	struct dos_partition *dp;
{

	return disk_abs_sector(lp, dp->dp_scyl, dp->dp_shd, dp->dp_ssect);
}

static __inline u_long
disk_end_sector(lp, dp)
	struct disklabel *lp;
	struct dos_partition *dp;
{
	if (dp->dp_ehd != 0 || dp->dp_esect != 0)
	{
#ifdef	DIAGNOSTIC
		printf("end header and sector not ZERO\n");
#endif	/* DIAGNOSTIC */
		return disk_abs_sector(lp, dp->dp_ecyl, dp->dp_ehd, dp->dp_esect);
	}
	else
		return disk_abs_sector(lp, dp->dp_ecyl + 1, 0, 0) - 1;
}

static __inline u_long
disk_size_sector(lp, dp)
	struct disklabel *lp;
	struct dos_partition *dp;
{

	return disk_end_sector(lp, dp) - disk_start_sector(lp, dp) + 1;
}
