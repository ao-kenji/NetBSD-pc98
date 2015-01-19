/*	$NecBSD: vpd.c,v 3.20 1999/07/31 22:51:24 honda Exp $	*/
/*	$NetBSD: vpd.c,v 1.26 1996/03/30 23:06:11 christos Exp $	*/

#define	VPD_SAFE_SERVER
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
 *	 NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 */
/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: vn.c 1.13 94/04/02$
 *
 *	@(#)vn.c	8.6 (Berkeley) 4/1/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/kthread.h>

#include <miscfs/specfs/specdev.h>

#include <dev/isa/ccbque.h>
#include <machine/vpdio.h>
#include <i386/i386/autoscan.h>

#define	VPD_NMAXIO	64
#define	VPD_MAX_UNITS	8

#define	vpdunit(x)	DISKUNIT(x)
#define	vpdmajor(x)	major((x))
#define	vpdpart(x)	DISKPART((x))

struct vpdbuf {
	struct buf	vb_buf;		/* strategy buf */
	struct buf	*vb_obp;	/* shadowed buffer */

	TAILQ_ENTRY(vpdbuf) vb_chain;	/* free buffer chain */
};

struct vpd_softc {
	struct device	sc_dev;		/* device sosftc */
	struct disk	sc_dkdev;	/* generic disk device info */

	struct lock	sc_lock;

#define	VNF_ALIVE	0x00001
#define VNF_INITED	0x00002
#define	VNF_WLABEL	0x01000
#define	VNF_LABELLING	0x02000
#define	VNF_ALLPART	0x04000
#define	VNF_RDONLY	0x10000
#define	VNF_SSLP	0x20000
#define	VNF_LOADED	0x40000
	u_int		sc_flags;	/* flags */
#define	VNFW_UBUF	0x00001
	u_int		sc_wflags;	/* flags (requested) */
	int		sc_open;	/* open flags */
	struct vnode	*sc_vp;		/* vnode */

	int		sc_nbp;		/* dummy count */
	int		sc_nbuf;	/* num of the current io */
	int		sc_maxnbuf;	/* max buffers */
	int		sc_depth;	/* io depth */
	dev_t		sc_devn;

	struct buf	sc_btab;	/* buffers tab */
};

void vpdattach __P((int));
void vpdiodone __P((struct buf *));
int vpdgetdisklabel __P((struct vpd_softc *, struct proc *));

cdev_decl(vpd);
bdev_decl(vpd);

#define	VPD_SOFTC(unit)	(vpd_cd.cd_devs[(unit)])
#define	b_cylin	b_resid

void *vpd_devs[VPD_MAX_UNITS];
struct cfdriver vpd_cd = {
	NULL, "vpd", DV_DISK, NULL,
};

#define	vpdlock(sc) lockmgr(&(sc)->sc_lock, LK_EXCLUSIVE, NULL)
#define	vpdunlock(sc) lockmgr(&(sc)->sc_lock, LK_RELEASE, NULL)

GENERIC_CCB_ASSERT(vpd, vpdbuf)
GENERIC_CCB_STATIC_ALLOC(vpd, vpdbuf)
GENERIC_CCB(vpd, vpdbuf, vb_chain)

void
vpdattach(num)
	int num;
{
	struct vpd_softc *sc;
	u_int i;

	if (num <= 0)
		return;
	if (num >= VPD_MAX_UNITS)
		num = VPD_MAX_UNITS;

	vpd_cd.cd_ndevs = num;
	vpd_cd.cd_devs = vpd_devs;
	for (i = 0; i < vpd_cd.cd_ndevs; i ++)
	{
		sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK);
		if (sc == NULL)
			return;
		memset(sc, 0, sizeof(*sc));
		VPD_SOFTC(i) = sc;
		sc->sc_dev.dv_unit = i;
		sprintf(sc->sc_dev.dv_xname, "vpd%d", i);
		sc->sc_dkdev.dk_name = sc->sc_dev.dv_xname;
		lockinit(&sc->sc_lock, PLOCK | PCATCH, "vpdlk", 0, 0);
	}
}

int
vpdopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit;
	struct vpd_softc *sc;
	int error, part, pmask;

	unit = vpdunit(dev);
	if (unit >= vpd_cd.cd_ndevs)
		return ENXIO;
	if ((sc = VPD_SOFTC(unit)) == NULL)
		return ENXIO;

	if ((error = vpdlock(sc)) != 0)
		return error;

	part = DISKPART(dev);
	pmask = (1 << part);

	if ((sc->sc_flags & VNF_INITED) == 0)
	{
		if (part != RAW_PART)
			error = ENXIO;
		goto bad;
	}

	if ((flags & FWRITE) && (sc->sc_flags & VNF_RDONLY))
	{
		error = EROFS;
		goto bad;
	}

	if (sc->sc_dkdev.dk_openmask == 0)
	{
		if ((sc->sc_flags & VNF_LOADED) == 0)
		{
			error = vpdgetdisklabel(sc, p);
			if (error)
				goto bad;
			sc->sc_flags |= VNF_LOADED;
		}
	}
	else
	{
		if ((sc->sc_flags & VNF_LOADED) == 0)
		{
			error = EIO;
			goto bad;
		}
	}

	if (part != RAW_PART &&
	    (part >= sc->sc_dkdev.dk_label->d_npartitions ||
	     sc->sc_dkdev.dk_label->d_partitions[part].p_fstype == FS_UNUSED))
	{
		error = ENXIO;
		goto bad;
	}

	switch (mode)
	{
	case S_IFCHR:
		sc->sc_dkdev.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		sc->sc_dkdev.dk_bopenmask |= pmask;
		break;
	}
	sc->sc_dkdev.dk_openmask =
	    sc->sc_dkdev.dk_copenmask | sc->sc_dkdev.dk_bopenmask;

bad:
	vpdunlock(sc);
	return error;
}

int
vpdclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = vpdunit(dev);
	struct vpd_softc *sc = VPD_SOFTC(unit);
	int error, part;

	if ((error = vpdlock(sc)) != 0)
		return (error);

	part = DISKPART(dev);

	switch (mode)
	{
	case S_IFCHR:
		sc->sc_dkdev.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		sc->sc_dkdev.dk_bopenmask &= ~(1 << part);
		break;
	}
	sc->sc_dkdev.dk_openmask =
	    sc->sc_dkdev.dk_copenmask | sc->sc_dkdev.dk_bopenmask;

	if (sc->sc_dkdev.dk_openmask == 0)
	{
		if ((sc->sc_flags & VNF_WLABEL) == 0)
			sc->sc_flags &= ~VNF_LOADED;
	}
	vpdunlock(sc);
	return (0);
}

#ifdef	VPD_SAFE_SERVER
#include <sys/signalvar.h>
#include <vm/vm.h>
void vpdsafeserver __P((void *));

void
vpdstrategy(bp)
	register struct buf *bp;
{
	int unit = vpdunit(bp->b_dev);
	register struct vpd_softc *sc = VPD_SOFTC(unit);
	struct disklabel *lp;
	int part;

	if ((sc->sc_flags & VNF_INITED) == 0)
	{
		bp->b_error = ENXIO;
		goto bad;
	}

	if (bp->b_bcount == 0)
		goto done;

	lp = sc->sc_dkdev.dk_label;
	if (bp->b_blkno < 0 || (bp->b_bcount % lp->d_secsize) != 0)
	{
		bp->b_error = EINVAL;
		goto bad;
	}

	part = vpdpart(bp->b_dev);
	if (part != RAW_PART && bounds_check_with_label(bp, lp, 0) <= 0)
		goto done;

	if (((bp->b_flags & B_READ) == 0) &&
  	    (bp->b_cylin < 1 || part == RAW_PART || part == RAW_PART - 1))
	{
		bp->b_error = EROFS;
		goto bad;
	}

	disksort(&sc->sc_btab, bp);
	if (sc->sc_flags & VNF_SSLP)
	{
		sc->sc_flags &=~VNF_SSLP;
		wakeup((caddr_t) &sc->sc_nbp);
	}
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

void
vpdsafeserver(arg)
	void *arg;
{
	struct vpd_softc *sc = arg;
	struct buf *bp;
	register struct vpdbuf *nbp;
	register daddr_t bn;
	struct disklabel *lp;
	struct proc *p;
	int s, error;

	lp = sc->sc_dkdev.dk_label;
	p = curproc;				/* XXX */

ServerLoop:
	if ((sc->sc_flags & VNF_INITED) == 0)
	{
		kthread_exit(0);
	}

	if (sc->sc_btab.b_actf == NULL)
	{
		sc->sc_flags |= VNF_SSLP;
		error = tsleep((caddr_t) &sc->sc_nbp, PRIBIO, "vpdserv", 0);
		if (error)
			CLRSIG(p, CURSIG(p));
		goto ServerLoop;
	}

	bp = sc->sc_btab.b_actf;
	sc->sc_btab.b_actf = bp->b_actf;

	s = splbio();
	if ((bp->b_flags & B_READ) == 0)
		sc->sc_vp->v_numoutput++;
	sc->sc_nbuf ++;

	while (sc->sc_nbuf > sc->sc_maxnbuf)
	{
		sc->sc_wflags |= VNFW_UBUF;
		tsleep((caddr_t) &sc->sc_nbuf, PRIBIO, "vpdmwait", 0);
	}
	nbp = vpd_get_ccb(0);			/* sleep OK */
	VHOLD(sc->sc_vp);
	splx(s);

	bn = bp->b_blkno;
	if (vpdpart(bp->b_dev) != RAW_PART)
	{
		register struct partition *p;

		p = lp->d_partitions + vpdpart(bp->b_dev); 
		bn += p->p_offset * (lp->d_secsize / dbtob(1));
	}

	nbp->vb_buf.b_flags = bp->b_flags | B_CALL | B_NOCACHE;
	nbp->vb_buf.b_bcount = bp->b_bcount;
	nbp->vb_buf.b_bufsize = bp->b_bufsize;
	nbp->vb_buf.b_error = 0;
	nbp->vb_buf.b_data = bp->b_data;
	nbp->vb_buf.b_blkno = bn;
	nbp->vb_buf.b_proc = bp->b_proc;
	nbp->vb_buf.b_iodone = vpdiodone;

	nbp->vb_buf.b_resid = 0;		/* trick XXX */

	nbp->vb_buf.b_validoff = 0;
	nbp->vb_buf.b_validend = 0;
	nbp->vb_buf.b_dirtyoff = 0;
	nbp->vb_buf.b_dirtyend = bp->b_bcount;

	nbp->vb_buf.b_dev = sc->sc_vp->v_rdev;
	nbp->vb_buf.b_vp = sc->sc_vp;
	nbp->vb_buf.b_vnbufs.le_next = NOLIST;

	nbp->vb_obp = bp;

	VOP_STRATEGY(&nbp->vb_buf);
	goto ServerLoop;
}
#else	/* !VPD_SAFE_SERVER */
void
vpdstrategy(bp)
	register struct buf *bp;
{
	int unit = vpdunit(bp->b_dev);
	register struct vpd_softc *sc = VPD_SOFTC(unit);
	register struct vpdbuf *nbp;
	register daddr_t bn;
	struct disklabel *lp;
	struct partition *p;
	int s;

	if ((sc->sc_flags & VNF_INITED) == 0)
	{
		bp->b_error = ENXIO;
		goto bad;
	}

	if (bp->b_bcount == 0)
		goto done;

	lp = sc->sc_dkdev.dk_label;
	if (bp->b_blkno < 0 || (bp->b_bcount % lp->d_secsize) != 0)
	{
		bp->b_error = EINVAL;
		goto bad;
	}

	if (vpdpart(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, lp, 0) <= 0)
		goto done;

	if (((bp->b_flags & B_READ) == 0) &&
  	    (bp->b_cylin < 1 || part == RAW_PART || part == RAW_PART - 1))
	{
		bp->b_error = EROFS;
		goto bad;
	}

	if (sc->sc_depth > 0)
	{
		disksort(&sc->sc_btab, bp);
		return;
	}
	++ sc->sc_depth;

next:
	s = splbio();
	if ((bp->b_flags & B_READ) == 0)
		sc->sc_vp->v_numoutput++;
	sc->sc_nbuf ++;

	while (sc->sc_nbuf > sc->sc_maxnbuf)
	{
		sc->sc_wflags |= VNFW_UBUF;
		tsleep((caddr_t) &sc->sc_nbuf, PRIBIO, "vpdmwait", 0);
	}
	nbp = vpd_get_ccb(0);			/* sleep OK */
	VHOLD(sc->sc_vp);
	splx(s);

	p = lp->d_partitions + vpdpart(bp->b_dev); 
	bn = bp->b_blkno;
	if (vpdpart(bp->b_dev) != RAW_PART)
		bn += p->p_offset * (lp->d_secsize / dbtob(1));

	nbp->vb_buf.b_flags = bp->b_flags | B_CALL | B_NOCACHE;
	nbp->vb_buf.b_bcount = bp->b_bcount;
	nbp->vb_buf.b_bufsize = bp->b_bufsize;
	nbp->vb_buf.b_error = 0;
	nbp->vb_buf.b_data = bp->b_data;
	nbp->vb_buf.b_blkno = bn;
	nbp->vb_buf.b_proc = bp->b_proc;
	nbp->vb_buf.b_iodone = vpdiodone;

	nbp->vb_buf.b_resid = 0;		/* trick XXX */

	nbp->vb_buf.b_validoff = 0;
	nbp->vb_buf.b_validend = 0;
	nbp->vb_buf.b_dirtyoff = 0;
	nbp->vb_buf.b_dirtyend = bp->b_bcount;

	nbp->vb_buf.b_dev = sc->sc_vp->v_rdev;
	nbp->vb_buf.b_vp = sc->sc_vp;
	nbp->vb_buf.b_vnbufs.le_next = NOLIST;

	nbp->vb_obp = bp;

	VOP_STRATEGY(&nbp->vb_buf);

	if (sc->sc_btab.b_actf != NULL)
	{
		bp = sc->sc_btab.b_actf;
		sc->sc_btab.b_actf = bp->b_actf;
		goto next;
	}

	-- sc->sc_depth;
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}
#endif	/* !VPD_SAFE_SERVER */

void
vpdiodone(tbp)
	struct buf *tbp;
{
	register struct vpdbuf *vbp = (struct vpdbuf *) tbp;
	register struct buf *bp = vbp->vb_obp;
	register struct vpd_softc *sc = VPD_SOFTC(vpdunit(bp->b_dev));
	int s;

	if (tbp->b_flags & B_ERROR)
	{
		bp->b_flags |= B_ERROR;
		bp->b_error = biowait(tbp);
	}
	bp->b_resid = tbp->b_resid;

	s = splbio();
	if (tbp->b_vp)
		brelvp(tbp);
	vpd_free_ccb(vbp);
	sc->sc_nbuf --;
	if (sc->sc_wflags & VNFW_UBUF)
	{
		sc->sc_wflags &= ~VNFW_UBUF;
		wakeup((caddr_t) &sc->sc_nbuf);
	}
	splx(s);

	biodone(bp);
}

int
vpdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = vpdunit(dev);
	struct vpd_softc *sc = VPD_SOFTC(unit);

	if ((sc->sc_flags & VNF_INITED) == 0)
		return (ENXIO);

	return (physio(vpdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
vpdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = vpdunit(dev);
	struct vpd_softc *sc = VPD_SOFTC(unit);

	if ((sc->sc_flags & VNF_INITED) == 0)
		return (ENXIO);

	return (physio(vpdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
vpdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = vpdunit(dev);
	struct vpd_softc *sc = VPD_SOFTC(unit);
	struct vpd_ioctl *vio;
	struct vattr vattr;
	struct nameidata nd;
	int error, part, pmask, s;

	vio = (struct vpd_ioctl *) data;
	switch (cmd)
	{
	case VPDIOCSET:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			return (error);

		if ((error = vpdlock(sc)) != 0)
			return (error);

		if (sc->sc_flags & VNF_INITED)
		{
			vpdunlock(sc);
			return (EBUSY);
		}

		sc->sc_flags = 0;
		sc->sc_open = FREAD | FWRITE;
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, vio->vpd_file, p);
		if ((error = vn_open(&nd, sc->sc_open, 0)) != 0)
		{
			if (error == EROFS)
			{
				sc->sc_open = FREAD;
				sc->sc_flags |= VNF_RDONLY;
				error = vn_open(&nd, sc->sc_open, 0);
			}
		}

		if (error != 0)
		{
			vpdunlock(sc);
			return error;
		}

		error = VOP_GETATTR(nd.ni_vp, &vattr, p->p_ucred, p);
		VOP_UNLOCK(nd.ni_vp, 0);
		if (error)
			goto bad;

		sc->sc_vp = nd.ni_vp;
		if (sc->sc_vp->v_type != VBLK ||
		    vpdpart(sc->sc_vp->v_rdev) != RAW_PART)		    
		{
			error = EINVAL;
			goto bad;
		}

		if (sc->sc_maxnbuf == 0)
		{
			disk_attach(&sc->sc_dkdev);
			sc->sc_maxnbuf = VPD_NMAXIO;
			vpd_init_ccbque(sc->sc_maxnbuf);
		}

		sc->sc_devn = MAKEDISKDEV(vpdmajor(dev), unit, RAW_PART);

		if (vio->vpd_flags & VPD_ALLPART)
			sc->sc_flags |= VNF_ALLPART;
		error = vpdgetdisklabel(sc, p);
		if (error)
			goto bad;

		sc->sc_dkdev.dk_openmask = 0;
		sc->sc_dkdev.dk_copenmask = 0;
		sc->sc_dkdev.dk_bopenmask = 0;
		sc->sc_flags |= (VNF_INITED | VNF_LOADED);
		vpdunlock(sc);
#ifdef	VPD_SAFE_SERVER
		(void) kthread_create(vpdsafeserver, sc, NULL, "vpd_kthread");
#endif	/* VPD_SAFE_SERVER */
		return error;

bad:
		sc->sc_vp = NULL;
		(void) vn_close(nd.ni_vp, sc->sc_open, p->p_ucred, p);
		vpdunlock(sc);
		return error;

	case VPDIOCCLR:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			return (error);

		if ((error = vpdlock(sc)) != 0)
			return (error);

		if ((sc->sc_flags & VNF_INITED) == 0)
		{
			vpdunlock(sc);
			return (ENXIO);
		}

		part = DISKPART(dev);
		pmask = (1 << part);
		if ((sc->sc_dkdev.dk_openmask & ~pmask) ||
		    ((sc->sc_dkdev.dk_bopenmask & pmask) &&
		    (sc->sc_dkdev.dk_copenmask & pmask)))
		{
			vpdunlock(sc);
			return (EBUSY);
		}

		s = splbio();
		if (sc->sc_nbuf > 0)
		{
			splx(s);
			vpdunlock(sc);
			return EBUSY;
		}
		sc->sc_flags &= ~VNF_INITED;
		splx(s);

#ifdef	VPD_SAFE_SERVER
		sc->sc_flags &= ~VNF_SSLP;
		wakeup((caddr_t) &sc->sc_nbp);
#endif	/* VPD_SAFE_SERVER */

		(void) vn_close(sc->sc_vp, sc->sc_open, p->p_ucred, p);
		sc->sc_vp = NULL;
		vpdunlock(sc);
		return error;
	}

	if ((sc->sc_flags & VNF_INITED) == 0)
		return ENXIO;
	
	switch (cmd) 
	{
	case DIOCGDINFO:
		*(struct disklabel *) data = *(sc->sc_dkdev.dk_label);
		return 0;
	
	case DIOCGPART:
		((struct partinfo *) data)->disklab = sc->sc_dkdev.dk_label;
		((struct partinfo *) data)->part =
			&sc->sc_dkdev.dk_label->d_partitions[vpdpart(dev)];
		return 0;
	
	case DIOCWDINFO:
	case DIOCSDINFO:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			return (error);
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = vpdlock(sc)) != 0)
			return error;
		sc->sc_flags |= VNF_LABELLING;

		/* allow to edit the incore disklabel only */
		error = setdisklabel(sc->sc_dkdev.dk_label,
				     (struct disklabel *) data, 0, 
				     sc->sc_dkdev.dk_cpulabel);
		sc->sc_flags &= ~VNF_LABELLING;
		vpdunlock(sc);
		return error;
	
	case DIOCWLABEL:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			return (error);
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *) data)
			sc->sc_flags |= VNF_WLABEL;
		else
			sc->sc_flags &= ~VNF_WLABEL;
		return 0;

	case DIOCGDEFLABEL:
		/* not requried */
	default:
		return ENOTTY;
	}
}

int
vpdsize(dev)
	dev_t dev;
{
	int unit;
	struct vpd_softc *sc;
	struct disklabel *lp;
	int size = -1, part = vpdpart(dev);

	unit = vpdunit(dev);
	if (unit >= vpd_cd.cd_ndevs)
		return size;

	sc = VPD_SOFTC(unit);
	if (sc == NULL || (sc->sc_flags & VNF_INITED) == 0)
		return size;
	
	if ((sc->sc_flags & VNF_LOADED) == 0)
	{
		sc->sc_flags |= VNF_LOADED;
		vpdgetdisklabel(sc, curproc);
	}
	
	lp = sc->sc_dkdev.dk_label;
	if (part == RAW_PART || part == RAW_PART - 1)
		return size;

	if (lp->d_partitions[part].p_fstype == FS_SWAP &&
	    lp->d_partitions[part].p_offset >= 1) 
		size = lp->d_partitions[part].p_size * (lp->d_secsize / dbtob(1));

	return size;
}

int
vpddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	return ENXIO;
}

int
vpdgetdisklabel(sc, p)
	struct vpd_softc *sc;
	struct proc *p;
{
	struct disklabel *lp = sc->sc_dkdev.dk_label;
	struct vnode *vp = sc->sc_vp;
	u_char *buf, *s;
	int error, resid, i;
	struct partinfo dpart;

	error = VOP_IOCTL(vp, DIOCGPART, (caddr_t) &dpart, FREAD, NOCRED, p);
	if (error)
		return error;

	memset(sc->sc_dkdev.dk_cpulabel, 0, sizeof(struct cpu_disklabel));
	*sc->sc_dkdev.dk_label = *dpart.disklab;

	strncpy(lp->d_typename, "vpd disk", 16);
	lp->d_type = DTYPE_VND;
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_flags = 0;

	buf = malloc(lp->d_secsize, M_TEMP, M_WAITOK);
	error = vn_rdwr(UIO_READ, vp, buf, lp->d_secsize, 
			DOSPARTOFF, UIO_SYSSPACE, 0, p->p_ucred, &resid, p);
	if (error)
	{
		free(buf, M_TEMP);
		return error;
	}

	for (i = 0; i < MAXPARTITIONS; i++)
	{
		if (i == RAW_PART || i == RAW_PART - 1)
			continue;
		lp->d_partitions[i].p_offset = 0;
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_fstype = FS_UNUSED;
	}
	lp->d_npartitions = MAXPARTITIONS;
	s = make_fake_disklabel(lp, (struct dos_partition *) buf, 
				"fake", sc->sc_flags & VNF_ALLPART);
	free(buf, M_TEMP);
#ifdef	VPD_ONLY_FAKE
	if (s != NULL)
		return EINVAL;
#endif	/* VPD_ONLY_FAKE */
	return error;
}
