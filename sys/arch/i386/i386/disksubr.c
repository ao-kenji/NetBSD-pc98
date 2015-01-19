/*	$NecBSD: disksubr.c,v 3.36 1999/07/26 06:37:49 honda Exp $	*/
/*	$NetBSD: disksubr.c,v 1.21 1996/05/03 19:42:03 christos Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <i386/i386/autoscan.h>

#define	b_cylin	b_resid

/*
 * Attempt to read a disk label from a device
 * using the indicated stategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 *
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Also, if bad block
 * table needed, attempt to extract it as well. Return buffer
 * for use in signalling errors if requested.
 *
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat) __P((struct buf *));
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct dos_partition *dp;
	struct dkbad *bdp;
	struct buf *bp;
	struct disklabel *dlp;
	char *msg = NULL;
	int error, i, size;
	u_int factor;
	struct dos_check_args dos;

	/*****************************************************
 	 * minimal setup
	 *****************************************************/
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;

	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++)
	{
		lp->d_partitions[i].p_fstype = FS_UNUSED;
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}

	if (lp->d_partitions[i].p_size == 0)
		lp->d_partitions[i].p_size = 0x1fffffff;
	lp->d_partitions[i].p_offset = 0;

	lp->d_flags &= ~D_MISCFAKE;

	size = ((int) lp->d_secsize) < NBPG ? NBPG : lp->d_secsize;
	bp = geteblk(size);
	bp->b_dev = dev;

	/*****************************************************
 	 * check dos partition.
	 *****************************************************/
	memset(&dos, 0, sizeof(dos));
	dos.start = strat;
	dos.lp = lp;
	dos.bp = bp;

	if (osdep != NULL && (dp = osdep->dosparts) != NULL)
	{
		dos.dp = dp;
		bp->b_blkno = DOSBBSECTOR;
		bp->b_cylin = DOSBBSECTOR / lp->d_secpercyl;
		bp->b_bcount = size;
		bp->b_flags = B_BUSY | B_READ;

		(*strat) (bp);
		error = biowait(bp);
		if (error)
		{
			msg = "dos partition I/O error";
			goto done;
		}

		dos.flag |= GEO_CHG_EN;
		(void) check_dospart(&dos);
	}
	
	/*****************************************************
 	 * obtain a BSD disklabel
	 *****************************************************/
	if (lp->d_secsize == 0 || (lp->d_secsize % DEV_BSIZE))
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = 100;
	factor = lp->d_secsize / DEV_BSIZE;

	bp->b_blkno = dos.ssec + LABELSECTOR;
	bp->b_cylin = bp->b_blkno / lp->d_secpercyl;
	bp->b_blkno *= factor;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;

	(*strat) (bp);
	error = biowait(bp);
	if (error)
	{
		msg = "disk label I/O error";
		goto done;
	}

	for (i = 0; i <= lp->d_secsize - sizeof(*dlp); i += sizeof(long))
	{
	        dlp = (struct disklabel *) (bp->b_data + i);

		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC)
		{
			if (msg == NULL)
				msg = "no disk label";
		}
		else if (dlp->d_npartitions <= MAXPARTITIONS && !dkcksum(dlp))
		{
			*lp = *dlp;
			msg = NULL;
			break;
		}
		else
			msg = "disk label corrupted";
	}

	if (dos.fake)
		msg = make_fake_disklabel(lp, osdep->dosparts, msg, 0);

	if (msg != NULL || (lp->d_flags & D_MISCFAKE))
		goto done;

	/*****************************************************
 	 * obtain BAD144 data 
	 *****************************************************/
#define DKBAD_MAGIC 0x4321

	if (osdep != NULL && (bdp = &osdep->bad) != NULL &&
	    (lp->d_flags & D_BADSECT) != 0)
	{
		struct dkbad *db;
		daddr_t lastsec;

		if (lp->d_partitions[0].p_offset > 0)
			lastsec = lp->d_partitions[2].p_offset +
				  lp->d_partitions[2].p_size;
		else
			lastsec = lp->d_secperunit;

		for (i = 0; i < 10 && i < lp->d_nsectors; i += 2)
		{
			bp->b_flags = B_BUSY | B_READ;
			bp->b_blkno = lastsec - lp->d_nsectors + i;
			bp->b_cylin = bp->b_blkno / lp->d_secpercyl;
			bp->b_blkno *= factor;
			bp->b_bcount = lp->d_secsize;

			(*strat) (bp);
			error = biowait(bp);
			if (error)
			{
				msg = "bad sector table I/O error";
			}
			else
			{
				db = (struct dkbad *)(bp->b_data);
				if (!db->bt_mbz && db->bt_flag == DKBAD_MAGIC)
				{
					msg = NULL;
					*bdp = *db;
					break;
				}
				else
					msg = "bad sector table corrupted";
			}

			if (bp->b_flags & B_ERROR)
				break;
		} 
	}

done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return (msg);
}

/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	int i;
	struct partition *opp, *npp;

	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0
		|| (nlp->d_secsize % DEV_BSIZE) != 0)
			return(EINVAL);

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);

	/* XXX missing check if other dos partitions will be overwritten */

	while (openmask != 0) {
		i = ffs(openmask) - 1;
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED &&
		    (olp->d_flags & D_MISCFAKE) == 0)
		{
			npp->p_fstype = opp->p_fstype;
			npp->p_fsize = opp->p_fsize;
			npp->p_frag = opp->p_frag;
			npp->p_cpg = opp->p_cpg;
		}
	}
	nlp->d_flags &= ~D_MISCFAKE;
 	nlp->d_checksum = 0;
 	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;
	return (0);
}


/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat) __P((struct buf *));
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct dos_partition *dp;
	struct buf *bp;
	struct disklabel *dlp;
	int error, size, i;
	struct dos_check_args dos;

	if (lp->d_flags & D_MISCFAKE)
		return EINVAL;

	size = ((int) lp->d_secsize) < NBPG ? NBPG : lp->d_secsize;
	bp = geteblk(size);
	bp->b_dev = dev;

	/*****************************************************
 	 * check dos partition.
	 *****************************************************/
	memset(&dos, 0, sizeof(dos));
	dos.start = strat;
	dos.lp = lp;
	dos.bp = bp;

	if (osdep != NULL && (dp = osdep->dosparts) != NULL)
	{
		dos.dp = dp;
		bp->b_blkno = DOSBBSECTOR;
		bp->b_cylin = DOSBBSECTOR / lp->d_secpercyl;
		bp->b_bcount = size;
		bp->b_flags = B_BUSY | B_READ;

		(*strat) (bp);
		error = biowait(bp);
		if (error)
			goto done;

		(void) check_dospart(&dos);
	}
	
	/*****************************************************
 	 * find a BSD partition and write it
	 *****************************************************/
	bp->b_blkno = dos.ssec + LABELSECTOR;
	bp->b_cylin = bp->b_blkno / lp->d_secpercyl;
	bp->b_blkno *= (lp->d_secsize / DEV_BSIZE);
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;

	(*strat) (bp);
	error = biowait(bp);
	if (error)
		goto done;

	for (i = 0; i <= lp->d_secsize - sizeof(*dlp); i += sizeof(long))
	{
	        dlp = (struct disklabel *) (bp->b_data + i);
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0)
		{
			*dlp = *lp;
			bp->b_flags = B_BUSY | B_WRITE;
			(*strat) (bp);
			error = biowait(bp);
			goto done;
		}
	}
	error = ESRCH;

done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return (error);
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	int wlabel;
{
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	daddr_t labelsector, sz, psize, poffs;
	daddr_t sc;

	sz = howmany(bp->b_bcount, DEV_BSIZE);
	labelsector = lp->d_partitions[2].p_offset + LABELSECTOR;

	if (lp->d_secsize == DEV_BSIZE)
	{
		sc = 1;
		psize = p->p_size;
		poffs = p->p_offset;
	}
	else
	{
		/* XXX:
		 * if secsize != DEV_BSIZE, should convert all partition addrs
		 * into values based on abstract device block size.
		 */
		sc = lp->d_secsize >> DEV_BSHIFT;
		if (sc < 1)
		{
			bp->b_error = EINVAL;
			goto bad;
		}
		psize = ((daddr_t) p->p_size) * sc;
		poffs = ((daddr_t) p->p_offset) * sc;
		labelsector *= sc;
	}

	/* Assumption:
	 * 1) bp->b_blkno < 0 already checked by a upper layer func().
	 * 2) bp->b_bcount alreay limited by minphys()!
	 */
	if (((u_long) bp->b_blkno) + ((u_long) sz) > ((u_long) psize))
	{
		sz = psize - bp->b_blkno;
		if (sz == 0)
		{
			bp->b_resid = bp->b_bcount;
			goto done;
		}

		if (sz < 0)
		{
			bp->b_error = EINVAL;
			goto bad;
		}

		bp->b_bcount = sz << DEV_BSHIFT;
	}

	poffs = bp->b_blkno + poffs;
	/* A remaining case to be checked. */
	if (poffs + sz < 0)
	{
		bp->b_error = EINVAL;
		goto bad;
	}

	if (poffs <= labelsector &&
#if LABELSECTOR != 0
	    poffs + sz > labelsector &&
#endif
	    (bp->b_flags & B_READ) == 0 && !wlabel)
	{
		bp->b_error = EROFS;
		goto bad;
	}

	bp->b_cylin = poffs / sc / lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
done:
	return (0);
}
