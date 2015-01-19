/*	$NecBSD: fd.c,v 3.83 1999/08/01 23:55:52 honda Exp $	*/
/*	$NetBSD: fd.c,v 1.90.4.3 1997/01/26 01:35:48 rat Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Kouichi Matsuda.  All rights reserved.
 */
/*-
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
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
 *	@(#)fd.c	7.4 (Berkeley) 5/25/91
 */

#define	FD_CF_FIXED	0x01	/* auto density check */
#define	FD_CF_EXT144	0x02	/* force 144 mode */

#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/disk.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/lock.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/conf.h>
#include <machine/intr.h>

#include <dev/cons.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>
#include <dev/isa/isadmavar.h>

#include <i386/isa/pc98spec.h>
#include <i386/isa/fdreg.h>

#include <machine/fdio.h>
#if NRND > 0
#include <sys/rnd.h>
#endif
#include <machine/systmbusvar.h>

#define b_cylin b_resid

enum fdc_state {
	DEVIDLE = 0,
	MOTORWAIT,
	DOSEEK,
	SEEKWAIT,
	SEEKTIMEDOUT,
	SEEKCOMPLETE,
	DOIO,
	IOCOMPLETE,
	IOTIMEDOUT,
	DORESET,
	RESETCOMPLETE,
	RESETTIMEDOUT,
	DORECAL,
	RECALWAIT,
	RECALTIMEDOUT,
	RECALCOMPLETE,
	DOFORMAT,
	FORMATCOMPLETE,
	FORMATTIMEOUT,
	BLOCKIO,
	BLOCKIOCOMPLETE,
	BLOCKIOTIMEOUT,
};

/* software state, per controller */
struct fdc_softc {
	struct device sc_dev;		/* boilerplate */
	void *sc_ih;
	isa_chipset_tag_t sc_ic;

	bus_space_tag_t sc_iot;		/* ISA space identifier */
	bus_space_tag_t sc_memt;

	bus_space_handle_t   sc_ioh;	/* ISA io handle */
	bus_space_handle_t   sc_fdsioh;
	bus_space_handle_t   sc_fdemsioh;

	int sc_drq;

	struct fd_softc *sc_fd[4];	/* pointers to children */
	TAILQ_HEAD(drivehead, fd_softc) sc_drives;
	enum fdc_state sc_state;
	int sc_errors;			/* number of retries so far */
	u_char sc_status[7];		/* copy of registers */

#define	FDC_INACTIVE	0x80000000
	u_int sc_flags;
	int sc_lockcnt;

	struct fd_type *sc_imode;	/* interface mode */
	u_int sc_shift;			/* sector shift */

	u_char sc_readbuf[FDC_BSIZE * 2];
};

/* controller driver configuration */
static int fdcmatch __P((struct device *, struct cfdata *, void *));
static int fdprint __P((void *, const char *));
static void fdcattach __P((struct device *, struct device *, void *));

struct cfattach fdc_ca = {
	sizeof(struct fdc_softc), fdcmatch, fdcattach
};

extern struct cfdriver fdc_cd;

/*
 * Floppies come in various flavors, e.g., 1.2MB vs 1.44MB; here is how
 * we tell them apart.
 */
struct fd_type {
	int	sectrac;	/* sectors per track */
	int	heads;		/* number of heads */
	int	seccyl;		/* sectors per cylinder */
	int	secsize;	/* size code for sectors */
	int	datalen;	/* data len when secsize = 0 */
	int	steprate;	/* step rate and head unload time */
	int	gap1;		/* gap len between sectors */
	int	gap2;		/* formatting gap */
	int	tracks;		/* total num of tracks */
	int	size;		/* size of disk in sectors */
	int	step;		/* steps per cylinder */
	int	rate;		/* transfer speed code */
	char	*name;
	#define	MODE_HD		0x0
	#define	MODE_DD		0x1
	#define	MODE_144	0x2
	int	imode;		/* interface mode. 1.44,HD or DD */
	int	interleave;	/* interleave factor. default=1 */
};

struct fd_type fd_types[] = {
{18,2,36,2,0xff,0xaf,0x1b,0x6c,80,2880,1,FDC_500KBPS, "1.44MB",   MODE_144,1},
{15,2,30,2,0xff,0xaf,0x1b,0x54,80,2400,1,FDC_500KBPS, "1.2MB 2HC",MODE_HD,1},
{8, 2,16,3,0xff,0xaf,0x35,0x74,77,1232,1,FDC_500KBPS, "1.2MB 2HD",MODE_HD,1},
{9, 2,18,2,0xff,0xaf,0x1b,0x50,80,1440,1,FDC_300KBPS, "720KB/x",  MODE_DD,1},
#ifdef	WE_NEEDS_640K
{8, 2,16,2,0xff,0xaf,0x1b,0x50,80,1280,1,FDC_300KBPS, "640KB/x",  MODE_DD,1},
#endif	/* WE_NEEDS_640K */
};

/* software state, per disk (with up to 4 disks per ctlr) */
struct fd_softc {
	struct device sc_dev;
	struct disk sc_dk;

	struct lock sc_lock;

	struct fd_type *sc_deftype;	/* default type descriptor */
	struct fd_type *sc_type;	/* current type descriptor */

	daddr_t	sc_blkno;	/* starting block number */
	int sc_bcount;		/* byte count left */
	int sc_skip;		/* bytes already transferred */
	int sc_nblks;		/* number of blocks currently tranferring */
	int sc_nbytes;		/* number of bytes currently tranferring */

	int sc_drive;		/* physical unit number */
	int sc_flags;
#define	FD_OPEN		0x0001		/* it's open */
#define	FD_MOTOR	0x0002		/* motor should be on */
#define	FD_MOTOR_WAIT	0x0004		/* motor coming up */
#define	FD_R_AGAIN	0x0100
#define	FD_EXT_144M	0x0200
#define	FD_D_CHECK	0x0400
#define	FD_B_EXTRA	0x0800
#define	FD_CHKREQ	0x4000
#define	FD_DUNK		0x8000
	int sc_cylin;		/* where we think the head is */

	void *sc_sdhook;	/* saved shutdown hook for drive. */

	TAILQ_ENTRY(fd_softc) sc_drivechain;
	int sc_ops;		/* I/O ops since last switch */
	struct buf sc_q;	/* head of buf chain */

	struct fd_type *sc_type_backup;
	struct buf *sc_fbp, *sc_chkbuf;

	int sc_mwcnt;
	int sc_acc;

#if NRND > 0
	rndsource_element_t	rnd_source;
#endif

#define	FST3_WP		0x40
#define	FST3_RDY	0x20
	u_int8_t sc_st3;
};

/* floppy driver configuration */
static int fdmatch __P((struct device *, struct cfdata *, void *));
static void fdattach __P((struct device *, struct device *, void *));

struct cfattach fd_ca = {
	sizeof(struct fd_softc), fdmatch, fdattach
};

extern struct cfdriver fd_cd;

#define	FDUNIT(dev)	DISKUNIT(dev)
#define	FDPART(dev)	DISKPART(dev)
#define	MAKEFDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)
#define	FDLABELDEV(dev)	(MAKEFDDEV(major(dev), FDUNIT(dev), RAW_PART))

#define	FDC_MOTOR_TIMEOUT (15 * hz)
#define	FDC_MOTOR_WC	15			/* 1.5s */
#define	FDC_BSHIFT	9
#define	DEV2FDCSHIFT	(DEV_BSHIFT - FDC_BSHIFT)

#define	ISFORMAT(bp)	((bp) == fd->sc_fbp)
#define	IS2HD(XXX)	((XXX)->secsize == 3)
#define	FD_TYPE_144	(&fd_types[0])
#define	FD_TYPE_NORMAL	(&fd_types[1])
#define	FD_TABLE_SIZE	(sizeof(fd_types) / sizeof(struct fd_type))
#define	FD_TABLE_LAST	(&fd_types[FD_TABLE_SIZE])

void fdstrategy __P((struct buf *));
void fdstart __P((struct fd_softc *));
void fd_set_motor __P((struct fdc_softc *fdc, int reset));
void fd_motor_off __P((void *arg));
void fd_motor_on __P((void *arg));
int fdcresult __P((struct fdc_softc *fdc));
int out_fdc __P((bus_space_tag_t iot, bus_space_handle_t ioh, u_char x));
void fdcstart __P((struct fdc_softc *fdc));
void fdcstatus __P((struct device *dv, int n, char *s));
void fdctimeout __P((void *arg));
void fdcpseudointr __P((void *arg));
int fdcintr __P((void *));
void fdcretry __P((struct fdc_softc *fdc));
void fdfinish __P((struct fd_softc *fd, struct buf *bp));
static __inline struct fd_type *fd_dev_to_type __P((struct fd_softc *, dev_t));
void fdc_motor_timeout __P((void *));

static void set_density __P((struct fdc_softc *, struct fd_softc *));
static int fd_check_range __P((struct fd_softc *, struct buf *));
static int fd_density2num __P((struct fd_softc *, struct fd_type *));
static struct fd_type *fd_num2density __P((struct fd_softc *, u_int));
static struct fd_type *fd_next_density __P((struct fd_softc *));
static struct buf *fd_dummy_read __P((struct fdc_softc *, struct fd_softc *, dev_t, struct buf *));
static void fd_dummy_read_finish __P((struct fdc_softc *, struct fd_softc *));
int fdcintrReal __P((void *));
static u_char fd_interleave_sector __P((u_int, u_int, u_int));
static int fdformat __P((dev_t, u_int));
int fd_check_density __P((dev_t));
static int fd_sense_device __P((struct fdc_softc *, struct fd_softc *));
static void fd_ready_and_check __P((struct fdc_softc *, struct fd_softc *));
int fdc_systmmsg __P((struct device *, systm_event_t));
static __inline struct fd_type *fd_dev_to_type __P((struct fd_softc *, dev_t));
void fdgetdisklabel __P((dev_t));
static void fdiocsmode __P((struct fd_softc *, u_int));

void	fd_mountroot_hook __P((struct device *));

struct dkdriver fddkdriver = { fdstrategy };

/**********************************************************
 * Fdc probe attach
 **********************************************************/
static int
fdcmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	register struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_space_handle_t fdsioh;
	bus_space_handle_t fdemsioh;
	int rv = 0;

	if (ia->ia_iobase == IOBASEUNK || ia->ia_drq == DRQUNK)
		return 0;

	iot = ia->ia_iot;

	/* Map the i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &ioh))
		return 0;
	if (bus_space_map_load(iot, ioh, 3, BUS_SPACE_IAT_2,
			       BUS_SPACE_MAP_FAILFREE))
		return 0;
	if (bus_space_map(iot, IO_FDPORT, 1, 0, &fdsioh))
	{
		bus_space_unmap(iot, ioh, FDC_NPORT);
		return 0;
	}
	if (bus_space_map(iot, 0x4be, 1, 0, &fdemsioh))
	{
		bus_space_unmap(iot, ioh, FDC_NPORT);
		bus_space_unmap(iot, fdsioh, 1);
		return 0;
	}

	/* see if it can handle a command */
	if (out_fdc(iot, ioh, NE7CMD_SPECIFY) < 0)
		goto out;
	out_fdc(iot, ioh, 0xaf);
	out_fdc(iot, ioh, 0x32);

	rv = 1;
	ia->ia_iosize = FDC_NPORT;
	ia->ia_msize = 0;

 out:
	bus_space_unmap(iot, ioh, FDC_NPORT);
	bus_space_unmap(iot, fdsioh, 1);
	bus_space_unmap(iot, fdemsioh, 1);
	return rv;
}

/*
 * Arguments passed between fdcattach and fdmatch.
 */
struct fdc_attach_args {
	int fa_drive;
	struct fd_type *fa_deftype;
};

static int
fdprint(aux, fdc)
	void *aux;
	const char *fdc;
{
	register struct fdc_attach_args *fa = aux;

	if (!fdc)
		printf(" drive %d", fa->fa_drive);
	return QUIET;
}

static void
fdcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fdc_softc *fdc = (void *)self;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	struct fdc_attach_args fa;

	iot = ia->ia_iot;

	/* Re-map the I/O space. */
	if (bus_space_map(iot, ia->ia_iobase, 0, 0, &ioh) ||
	    bus_space_map_load(iot, ioh, 3, BUS_SPACE_IAT_2, 0) ||
	    bus_space_map(iot, IO_FDPORT, 1, 0, &fdc->sc_fdsioh) ||
	    bus_space_map(iot, 0x4be, 1, 0, &fdc->sc_fdemsioh))
		panic("%s: could not map io space", fdc->sc_dev.dv_xname);

	fdc->sc_iot = iot;
	fdc->sc_memt = ia->ia_memt;
	fdc->sc_ioh = ioh;
	fdc->sc_ic = ia->ia_ic;

	fdc->sc_drq = ia->ia_drq;
	fdc->sc_state = DEVIDLE;
	fdc->sc_imode = NULL;

	TAILQ_INIT(&fdc->sc_drives);

	printf("\n");

	for (fa.fa_drive = 0; fa.fa_drive < 4; fa.fa_drive++)
	{
		fa.fa_deftype = (lookup_bios_param(BIOS_FD_EXT, 1) &
			(1 << fa.fa_drive)) ? FD_TYPE_144 : FD_TYPE_NORMAL;
		(void)config_found(self, (void *)&fa, fdprint);
	}

	systmmsg_bind(self, fdc_systmmsg);
	timeout(fdc_motor_timeout, fdc, FDC_MOTOR_TIMEOUT);

	if (isa_dmamap_create(fdc->sc_ic, fdc->sc_drq, FDC_MAXIOSIZE,
	    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
		printf("%s: can't set up ISA DMA map\n",
		    fdc->sc_dev.dv_xname);
		return;
	}

	fdc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
					IPL_BIO, fdcintrReal, fdc);
}

/**********************************************************
 * Fd probe attach
 **********************************************************/
static int
fdmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct fdc_attach_args *fa = aux;
	int drive = fa->fa_drive;

	if (cf->cf_loc[FDCCF_DRIVE] != FDCCF_DRIVE_DEFAULT && 
	    cf->cf_loc[FDCCF_DRIVE] != drive)
		return 0;
	return 1;
}

static void
fdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fdc_softc *fdc = (void *)parent;
	struct fd_softc *fd = (void *)self;
	struct fdc_attach_args *fa = aux;
	struct fd_type *type = fa->fa_deftype;
	int drive = fa->fa_drive;
	struct cfdata *cf = fd->sc_dev.dv_cfdata;

	/*
	 * Initialize lock
	 */
	lockinit(&fd->sc_lock, PLOCK | PCATCH, "fdlk", 0, 0);
	if (cf->cf_flags & FD_CF_EXT144)
		type = FD_TYPE_144;

	if (type)
		printf(": %s %d cyl, %d head, %d sec\n", type->name,
		    type->tracks, type->heads, type->sectrac);
	else
		printf(": density unknown\n");

	fd->sc_cylin = -1;
	fd->sc_drive = drive;
	fd->sc_deftype = type;
	fdc->sc_fd[drive] = fd;

	/*
	 * Initialize and attach the disk structure.
	 */
	fd->sc_dk.dk_name = fd->sc_dev.dv_xname;
	fd->sc_dk.dk_driver = &fddkdriver;
	disk_attach(&fd->sc_dk);

	if (type == FD_TYPE_144)
		fd->sc_flags |= FD_EXT_144M;
	if ((cf->cf_flags & FD_CF_FIXED) == 0)
		fd->sc_flags |= FD_D_CHECK;
	fd->sc_type_backup = fd->sc_type = type;

	mountroothook_establish(fd_mountroot_hook, &fd->sc_dev);

	/* Needed to power off if the motor is on when we halt. */
	fd->sc_sdhook = shutdownhook_establish(fd_motor_off, fd);

	/* also turn off the motor, here */
	fd_motor_off(fd);

#if NRND > 0
	rnd_attach_source(&fd->rnd_source, fd->sc_dev.dv_xname,
			  RND_TYPE_DISK, 0);
#endif
}

static __inline struct fd_type *
fd_dev_to_type(fd, dev)
	struct fd_softc *fd;
	dev_t dev;
{

	return fd->sc_type;
}

/**********************************************************
 * Strategy Io
 **********************************************************/
void
fdstrategy(bp)
	struct buf *bp;
{
	struct fd_softc *fd = fd_cd.cd_devs[FDUNIT(bp->b_dev)];
	struct fdc_softc *fdc = (struct fdc_softc *) fd->sc_dev.dv_parent;
 	int s;

	/* check write protect */
	if ((bp->b_flags & B_READ) == 0 && (fd->sc_st3 & FST3_WP))
	{
		bp->b_error = EROFS;
		goto bad;
	}

	if (bp->b_bcount == 0)
		goto done;

	if (bp->b_blkno < 0 || 
	    (ISFORMAT(bp) == 0 && (bp->b_bcount & (FDC_BSIZE - 1)) != 0))
	{
		bp->b_error = EINVAL;
		goto bad;
	}

	s = splbio();
	if ((fd->sc_flags & FD_CHKREQ) != 0 &&
	    fd->sc_q.b_active == 0 && fd->sc_chkbuf == NULL)
	{
		fd->sc_flags &= ~FD_CHKREQ;
		if (ISFORMAT(bp) == 0 && (fd->sc_flags & FD_D_CHECK))
			fd->sc_chkbuf = fd_dummy_read(fdc, fd, bp->b_dev, NULL);
	}				
	splx(s);

 	bp->b_cylin = bp->b_blkno;		/* XXX */

	s = splbio();
	disksort(&fd->sc_q, bp);
	if (!fd->sc_q.b_active)
		fdstart(fd);
	splx(s);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

void
fdstart(fd)
	struct fd_softc *fd;
{
	struct fdc_softc *fdc = (void *)fd->sc_dev.dv_parent;
	int active = fdc->sc_drives.tqh_first != 0;

	/* Link into controller queue. */
	fd->sc_q.b_active = 1;
	TAILQ_INSERT_TAIL(&fdc->sc_drives, fd, sc_drivechain);

	/* If controller not already active, start it. */
	if (!active)
		fdcstart(fdc);
}

void
fdfinish(fd, bp)
	struct fd_softc *fd;
	struct buf *bp;
{
	struct fdc_softc *fdc = (void *)fd->sc_dev.dv_parent;

	if (fd->sc_chkbuf != NULL)
	{
		fd_dummy_read_finish(fdc, fd);
		if (fd->sc_type_backup != fd->sc_type)
			log(LOG_WARNING, "%s: the current density is %s\n",
			    fd->sc_dev.dv_xname, fd->sc_type->name);
		fd->sc_type_backup = fd->sc_type;
		fdc->sc_state = DEVIDLE;
		return;
	}
	fd->sc_type_backup = fd->sc_type;

	/*
	 * Move this drive to the end of the queue to give others a `fair'
	 * chance.  We only force a switch if N operations are completed while
	 * another drive is waiting to be serviced, since there is a long motor
	 * startup delay whenever we switch.
	 */
	if (fd->sc_drivechain.tqe_next && ++fd->sc_ops >= 8) {
		fd->sc_ops = 0;
		TAILQ_REMOVE(&fdc->sc_drives, fd, sc_drivechain);
		if (bp->b_actf) {
			TAILQ_INSERT_TAIL(&fdc->sc_drives, fd, sc_drivechain);
		} else
			fd->sc_q.b_active = 0;
	}
	bp->b_resid = fd->sc_bcount;
	fd->sc_skip = 0;
	fd->sc_q.b_actf = bp->b_actf;

#if NRND > 0
	rnd_add_uint32(&fd->rnd_source, bp->b_blkno);
#endif

	biodone(bp);

	fdc->sc_state = DEVIDLE;
}

int
fdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (physio(fdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
fdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (physio(fdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/**********************************************************
 * Open Close
 **********************************************************/
static int fd_lock __P((struct fd_softc *, struct proc *));
static void fd_unlock __P((struct fd_softc *, struct proc *));

static int
fd_lock(fd, p)
	struct fd_softc *fd;
	struct proc *p;
{
	struct fdc_softc *fdc = (void *) fd->sc_dev.dv_parent;
	int error;

	if ((error = lockmgr(&fd->sc_lock, LK_EXCLUSIVE, NULL)) != 0)
		return error;

	fdc->sc_lockcnt ++;
	return 0;
}

static void
fd_unlock(fd, p)
	struct fd_softc *fd;
	struct proc *p;
{
	struct fdc_softc *fdc = (void *) fd->sc_dev.dv_parent;

	lockmgr(&fd->sc_lock, LK_RELEASE, NULL);
	fdc->sc_lockcnt --;
}

int
fdopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
 	int unit, error;
	struct fd_softc *fd;
	struct fd_type *type;
	struct fdc_softc *fdc;
	u_int part = FDPART(dev);

	unit = FDUNIT(dev);
	if (unit >= fd_cd.cd_ndevs)
		return ENXIO;

	fd = fd_cd.cd_devs[unit];
	if (fd == 0)
		return ENXIO;
	fdc = (void *) fd->sc_dev.dv_parent;

	type = fd_dev_to_type(fd, dev);
	if (type == NULL)
		return ENXIO;

	if ((error = fd_lock(fd, p)) != 0)
		return error;

	if (fd->sc_dk.dk_openmask == 0)
	{
		fd->sc_cylin = -1;	/* XXX */
		fd->sc_flags |= FD_CHKREQ;
		fd->sc_flags &= ~FD_DUNK;
		fd->sc_st3 = 0;

		fd_ready_and_check(fdc, fd);
		if ((fd->sc_st3 & FST3_RDY) == 0)
		{
			error =  ENODEV;
			goto done;
		}
	}

	if ((flags & FWRITE) && (fd->sc_st3 & FST3_WP))
	{
		error = EROFS;
		goto done;
	}

	switch (mode)
	{
	case S_IFCHR:
		fd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		fd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}

	fd->sc_dk.dk_openmask = fd->sc_dk.dk_copenmask |
				fd->sc_dk.dk_bopenmask;

done:
	fd_unlock(fd, p);
	return error;
}

int
fdclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct fd_softc *fd = fd_cd.cd_devs[FDUNIT(dev)];
	u_int part = FDPART(dev);
	int error;

	if ((error = fd_lock(fd, p)) != 0)
		return error;

	switch (mode)
	{
	case S_IFCHR:
		fd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		fd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}

	fd->sc_dk.dk_openmask = fd->sc_dk.dk_copenmask |
				fd->sc_dk.dk_bopenmask;

	fd_unlock(fd, p);
	return error;
}

void
fdcstart(fdc)
	struct fdc_softc *fdc;
{

#ifdef DIAGNOSTIC
	/* only got here if controller's drive queue was inactive; should
	   be in idle state */
	if (fdc->sc_state != DEVIDLE) {
		printf("fdcstart: not idle\n");
		return;
	}
#endif
	(void) fdcintr(fdc);
}

/**********************************************************
 * Intr functions
 **********************************************************/
#define	st0 fdc->sc_status[0]
#define	st1 fdc->sc_status[1]
#define	cyl fdc->sc_status[1]

void
fdctimeout(arg)
	void *arg;
{
	struct fdc_softc *fdc = arg;
	struct fd_softc *fd = fdc->sc_drives.tqh_first;
	int s;

	s = splbio();
	fdcstatus(&fd->sc_dev, 0, "timeout");

	if (fd->sc_q.b_actf)
		fdc->sc_state++;
	else
		fdc->sc_state = DEVIDLE;

	(void) fdcintr(fdc);
	splx(s);
}

void
fdcpseudointr(arg)
	void *arg;
{
	int s;

	/* Just ensure it has the right spl. */
	s = splbio();
	(void) fdcintr(arg);
	splx(s);
}

int
fdcintrReal(arg)
	void *arg;
{
	struct fdc_softc *fdc = arg;
	bus_space_tag_t iot = fdc->sc_iot;
	bus_space_handle_t ioh = fdc->sc_ioh;

	if ((bus_space_read_1(iot, ioh, fdsts) & NE7_CB) == 0)
	{
		out_fdc(iot, ioh, NE7CMD_SENSEI);
		fdcresult(fdc);
		if ((fdc->sc_status[0] & 0xe0) == 0xc0)
			return 1;
	}

	fdcintr(fdc);
	return 1;
}

#define	SEQ_CONTINUE(STATE) {fdc->sc_state = (STATE); continue;}
#define	SEQ_RETURN(STATE) {fdc->sc_state = (STATE); return 1;}

int
fdcintr(arg)
	void *arg;
{
	struct fdc_softc *fdc = arg;
	struct fd_softc *fd;
	struct buf *bp;
	bus_space_tag_t iot = fdc->sc_iot;
	bus_space_handle_t ioh = fdc->sc_ioh;
	int read, head, sec, i, nblks;
	struct fd_type *type;

loop:
	if (fdc->sc_flags & FDC_INACTIVE)
		return 0;

	if ((fd = fdc->sc_drives.tqh_first) == NULL)
		SEQ_RETURN(DEVIDLE);

	fd->sc_acc ++;
	if ((bp = fd->sc_chkbuf) == NULL)
	{
		if ((bp = fd->sc_q.b_actf) == NULL)
		{
			fd->sc_ops = 0;
			TAILQ_REMOVE(&fdc->sc_drives, fd, sc_drivechain);
			fd->sc_q.b_active = 0;
			goto loop;
		}

		if ((fd->sc_flags & FD_DUNK) ||
		    ((fd->sc_st3 & FST3_WP) && (bp->b_flags & B_READ) == 0))
		{
			bp->b_flags |= B_ERROR;
			bp->b_error = (fd->sc_flags & FD_DUNK) ? EIO : EROFS;
			fd->sc_bcount = bp->b_bcount;
			fdfinish(fd, bp);
			goto loop;
		}
	}

    do
    {
	switch (fdc->sc_state) 
	{
	case DEVIDLE:
		if (fd_check_range(fd, bp))
		{
			fdfinish(fd, bp);
			goto loop;
		}

		if (fdc->sc_imode != fd->sc_type)
			set_density(fdc, fd);

		fdc->sc_errors = 0;
		fd->sc_skip = 0;
		fd->sc_bcount = bp->b_bcount;
		fd->sc_blkno = bp->b_blkno / (FDC_BSIZE / DEV_BSIZE);

		if ((fd->sc_flags & FD_MOTOR_WAIT) != 0)
			SEQ_RETURN(MOTORWAIT)

		if ((fd->sc_flags & FD_MOTOR) == 0)
		{
			fd->sc_cylin = -1;
			fd->sc_flags |= FD_MOTOR | FD_MOTOR_WAIT;
			fd_set_motor(fdc, 0);
			timeout(fd_motor_on, fd, hz / 10);
			SEQ_RETURN(MOTORWAIT)
		}
		fd_set_motor(fdc, 0);

	case DOSEEK:
		if (ISFORMAT(bp))
		{
			if (bp->b_blkno == 0)
				SEQ_CONTINUE(DORECAL)
			else if (fd->sc_cylin == bp->b_cylin)
				SEQ_CONTINUE(DOFORMAT)
		}
		else
		{
			if (fd->sc_cylin == bp->b_cylin)
				SEQ_CONTINUE(DOIO)
			else if (fd->sc_cylin == -1)
				SEQ_CONTINUE(DORECAL)
		}

		out_fdc(iot, ioh, NE7CMD_SEEK);	/* seek function */
		out_fdc(iot, ioh, fd->sc_drive);	/* drive number */
		out_fdc(iot, ioh, bp->b_cylin * fd->sc_type->step);

		fd->sc_cylin = -1;

		fd->sc_dk.dk_seek++;
		disk_busy(&fd->sc_dk);

		timeout(fdctimeout, fdc, 4 * hz);
		SEQ_RETURN(SEEKWAIT);

	case DOIO:
		/* The strategies:
		 * bp->b_cyl & fd->sc_cyl	real physical cyl
		 * fd->sc_nblks			512 emulation
		 * fd->sc_blkno			512 emulation
		 */
		type = fd->sc_type;
		if (IS2HD(type) != 0)
		{
			if ((fd->sc_blkno & 1) || 
			    (fd->sc_bcount & (2 * FDC_BSIZE - 1)))
				SEQ_CONTINUE(BLOCKIO)

			sec = (fd->sc_blkno / 2) % type->seccyl;
		}
		else
			sec = fd->sc_blkno % type->seccyl;

		nblks = type->seccyl - sec;
		nblks = min(nblks, (fd->sc_bcount) >> fdc->sc_shift);
		nblks = min(nblks, FDC_MAXIOSIZE >> fdc->sc_shift);
		if (IS2HD(type) != 0)
			nblks *= 2;
		fd->sc_nblks = nblks;
		fd->sc_nbytes = nblks * FDC_BSIZE;
		head = sec / type->sectrac;
		sec -= head * type->sectrac;
		read = bp->b_flags & B_READ ? DMAMODE_READ : DMAMODE_WRITE;
		isa_dmastart(fdc->sc_ic, fdc->sc_drq,
		    bp->b_data + fd->sc_skip, fd->sc_nbytes,
		    NULL, read, BUS_DMA_NOWAIT);
		if (read)
			out_fdc(iot, ioh, NE7CMD_READ);	/* READ */
		else
			out_fdc(iot, ioh, NE7CMD_WRITE);	/* WRITE */
		out_fdc(iot, ioh, (head << 2) | fd->sc_drive);
		out_fdc(iot, ioh, fd->sc_cylin);		/* track */
		out_fdc(iot, ioh, head);
		out_fdc(iot, ioh, sec + 1);		/* sector +1 */
		out_fdc(iot, ioh, type->secsize);	/* sector size */
		out_fdc(iot, ioh, type->sectrac);	/* sectors/track */
		out_fdc(iot, ioh, type->gap1);		/* gap1 size */
		out_fdc(iot, ioh, type->datalen);	/* data length */

		disk_busy(&fd->sc_dk);

		timeout(fdctimeout, fdc, 2 * hz);
		SEQ_RETURN(IOCOMPLETE);

	case SEEKWAIT:
		untimeout(fdctimeout, fdc);
		timeout(fdcpseudointr, fdc, hz / 50);
		SEQ_RETURN(SEEKCOMPLETE);

	case SEEKCOMPLETE:
		disk_unbusy(&fd->sc_dk, 0);	/* no data on seek */

		if ((st0 & 0xf8) != 0x20 || cyl != bp->b_cylin)
		{
			if (fd->sc_chkbuf == NULL)
				fdcstatus(&fd->sc_dev, 2, "seek failed");
			fdcretry(fdc);
			goto loop;
		}
		fd->sc_cylin = bp->b_cylin;
		fdc->sc_state = ISFORMAT(bp) ? DOFORMAT : DOIO;
		continue;

	case FORMATTIMEOUT:
	case BLOCKIOTIMEOUT:
	case IOTIMEDOUT:
		isa_dmaabort(fdc->sc_ic, fdc->sc_drq);

	case SEEKTIMEDOUT:
	case RECALTIMEDOUT:
	case RESETTIMEDOUT:
		fdcretry(fdc);
		goto loop;

	case IOCOMPLETE: /* IO DONE, post-analyze */
		untimeout(fdctimeout, fdc);
		disk_unbusy(&fd->sc_dk, fd->sc_nbytes);

		if (fdcresult(fdc) != 7 || (st0 & 0xf8) != 0)
			goto fdc_dmaout;

		read = bp->b_flags & B_READ ? DMAMODE_READ : DMAMODE_WRITE;
		isa_dmadone(fdc->sc_ic, fdc->sc_drq);

		fdc->sc_errors = 0;
		fd->sc_blkno += fd->sc_nblks;
		fd->sc_skip += fd->sc_nbytes;
		fd->sc_bcount -= fd->sc_nbytes;
		if (fd->sc_bcount <= 0)
		{
			fdfinish(fd, bp);
			goto loop;
		}

		if (IS2HD(fd->sc_type))
			bp->b_cylin = (fd->sc_blkno / 2) / fd->sc_type->seccyl;
		else
			bp->b_cylin = fd->sc_blkno / fd->sc_type->seccyl;
		SEQ_CONTINUE(DOSEEK);

	case DORESET:
		fd_set_motor(fdc, 1);
		delay(100);
		fd_set_motor(fdc, 0);
		timeout(fdctimeout, fdc, hz / 2);
		SEQ_RETURN(RESETCOMPLETE);

	case RESETCOMPLETE:
		untimeout(fdctimeout, fdc);
		for (i = 0; i < 4; i++)
		{
			out_fdc(iot, ioh, NE7CMD_SENSEI);
			(void) fdcresult(fdc);
		}

		/* fall through */
	case DORECAL:
		out_fdc(iot, ioh, NE7CMD_RECAL);	/* recalibrate function */
		out_fdc(iot, ioh, fd->sc_drive);
		timeout(fdctimeout, fdc, 5 * hz);
		SEQ_RETURN(RECALWAIT);

	case RECALWAIT:
		untimeout(fdctimeout, fdc);
		timeout(fdcpseudointr, fdc, hz / 30);
		SEQ_RETURN(RECALCOMPLETE);

	case RECALCOMPLETE:
		if ((st0 & 0xf8) != 0x20 || cyl != 0)
		{
			if (fd->sc_chkbuf == NULL)
				fdcstatus(&fd->sc_dev, 2, "recalibrate failed");
			fdcretry(fdc);
			goto loop;
		}

		if ((fd->sc_flags & FD_R_AGAIN) == 0)
		{
			fd->sc_flags |= FD_R_AGAIN;
			SEQ_CONTINUE(DORECAL)
		}

		fd->sc_flags &= ~FD_R_AGAIN;
		fd->sc_cylin = 0;
		if (ISFORMAT(bp) == 0 || bp->b_blkno != 0)
			SEQ_CONTINUE(DOSEEK)

	case DOFORMAT:
		if (ISFORMAT(bp) == 0)
		{
			/* protect against unexpected enterance. */
			fdcretry(fdc);
			goto loop;
		}

		isa_dmastart(fdc->sc_ic, fdc->sc_drq,
		    bp->b_data, bp->b_bcount, NULL, DMAMODE_WRITE, BUS_DMA_NOWAIT);
		head = (bp->b_blkno % fd->sc_type->heads);
		out_fdc(iot, ioh, NE7CMD_FORMAT);
		out_fdc(iot, ioh, (head << 2) | fd->sc_drive);
		out_fdc(iot, ioh, fd->sc_type->secsize);
		out_fdc(iot, ioh, fd->sc_type->sectrac);
		out_fdc(iot, ioh, fd->sc_type->gap2);
		out_fdc(iot, ioh, 0xe5);

		disk_busy(&fd->sc_dk);
		timeout(fdctimeout, fdc, 2 * hz);

		SEQ_RETURN(FORMATCOMPLETE);

	case FORMATCOMPLETE:
		untimeout(fdctimeout, fdc);
		disk_unbusy(&fd->sc_dk, 0);	/* no data on seek */

		isa_dmadone(fdc->sc_ic, fdc->sc_drq);
		fdcresult(fdc);
		if (st0 & 0x40)
		{
			fdcretry(fdc);
			goto loop;
		}
		fd->sc_skip = bp->b_bcount;
		fdfinish(fd, bp);
		goto loop;

	case BLOCKIO:
		type = fd->sc_type;
		fd->sc_nblks = 1;
		fd->sc_nbytes = FDC_BSIZE;
		sec = (fd->sc_blkno / 2) % type->seccyl;
		head = sec / type->sectrac;
		sec -= head * type->sectrac;
		read = ((fd->sc_flags & FD_B_EXTRA) ? 
			((bp->b_flags & B_READ) ? DMAMODE_READ:DMAMODE_WRITE) :
			DMAMODE_READ);
		isa_dmastart(fdc->sc_ic, fdc->sc_drq,
		    fdc->sc_readbuf, FDC_BSIZE *2, NULL, read, BUS_DMA_NOWAIT);

		out_fdc(iot, ioh, read ? NE7CMD_READ : NE7CMD_WRITE);
		out_fdc(iot, ioh, (head << 2) | fd->sc_drive);
		out_fdc(iot, ioh, fd->sc_cylin);
		out_fdc(iot, ioh, head);
		out_fdc(iot, ioh, sec + 1);
		out_fdc(iot, ioh, type->secsize);
		out_fdc(iot, ioh, type->sectrac);
		out_fdc(iot, ioh, type->gap1);	
		out_fdc(iot, ioh, type->datalen);

		disk_busy(&fd->sc_dk);
		timeout(fdctimeout, fdc, 2 * hz);
		SEQ_RETURN(BLOCKIOCOMPLETE);

	case BLOCKIOCOMPLETE:
		untimeout(fdctimeout, fdc);
		disk_unbusy(&fd->sc_dk, fd->sc_nbytes);

		if (fdcresult(fdc) != 7 || (st0 & 0xf8) != 0)
		{
			fd->sc_flags &= ~FD_B_EXTRA;
			goto fdc_dmaout;
		}

		read = ((fd->sc_flags & FD_B_EXTRA) ?
			((bp->b_flags & B_READ) ? DMAMODE_READ:DMAMODE_WRITE) :
			DMAMODE_READ);
		isa_dmadone(fdc->sc_ic, fdc->sc_drq);
		read = (bp->b_flags & B_READ);
		if (read != 0 || (fd->sc_flags & FD_B_EXTRA))
		{
			if (read)
				bcopy(fdc->sc_readbuf + (fd->sc_blkno & 0x01) *
				      FDC_BSIZE, bp->b_data + fd->sc_skip,
				      FDC_BSIZE);
			fd->sc_flags &= ~FD_B_EXTRA;
			fd->sc_blkno += fd->sc_nblks;
			fd->sc_skip += fd->sc_nbytes;
			fd->sc_bcount -= fd->sc_nbytes;
		}
		else
		{
			bcopy(bp->b_data + fd->sc_skip,
			      fdc->sc_readbuf + (fd->sc_blkno & 0x01) * 
			      FDC_BSIZE, FDC_BSIZE);
			fd->sc_flags |= FD_B_EXTRA;
			SEQ_CONTINUE(BLOCKIO);
		}

		if (fd->sc_bcount > 0)
		{
			bp->b_cylin = (fd->sc_blkno / 2) / fd->sc_type->seccyl;
			SEQ_CONTINUE(DOSEEK);
		}

		fdfinish(fd, bp);
		goto loop;

	case MOTORWAIT:
		if (fd->sc_flags & FD_MOTOR_WAIT)
			return 1;		/* time's not up yet */
		SEQ_CONTINUE(DOSEEK);

	default:
		fdcstatus(&fd->sc_dev, 0, "stray interrupt");
		return 1;
	}
   }
   while(1);

fdc_dmaout:
	isa_dmaabort(fdc->sc_ic, fdc->sc_drq);
	if ((st1 & 0x02) && fd->sc_chkbuf == NULL)
	{
		bp->b_flags |= B_ERROR;
		bp->b_error = EROFS;
		fd->sc_bcount = bp->b_bcount;
		fd->sc_st3 |= FST3_WP;
		fdfinish(fd, bp);
	}
	else
		fdcretry(fdc);
	goto loop;
}
#undef	st0
#undef	st1
#undef	cyl

void
fdcretry(fdc)
	struct fdc_softc *fdc;
{
	struct fd_softc *fd = fdc->sc_drives.tqh_first;
	struct fd_type *type;
	struct buf *bp;

	fd->sc_flags &= ~(FD_B_EXTRA | FD_R_AGAIN);
	bp = fd->sc_q.b_actf;
	switch (fdc->sc_errors)
	{
	case 1:
		if (ISFORMAT(bp) == 0 && fd->sc_chkbuf != NULL)
		{
			bp = fd->sc_chkbuf;
			if ((type = fd_next_density(fd)) != NULL)
			{
				fdc->sc_errors = 0;
				fdc->sc_state = DEVIDLE;
				fd->sc_type = type;
				fd_dummy_read(fdc, fd, bp->b_dev, bp);
			}
			else
			{
				fd->sc_flags |= FD_DUNK;
				fd_dummy_read_finish(fdc, fd);
				log(LOG_WARNING, "%s: no acceptable density.\n",
				    fd->sc_dev.dv_xname);
			}
			return;
		}
		
		/* fall */
	case 4:
	case 3:
	case 2:
	case 0:
		fdc->sc_errors++;
		fdc->sc_state = DORECAL;
		break;

	default:
		diskerr(bp, "fd", "hard error", LOG_PRINTF,
			fd->sc_skip >> fdc->sc_shift, (struct disklabel *)NULL);
		printf(" (st0 %b st1 %b st2 %b cyl %d head %d sec %d)\n",
			fdc->sc_status[0], NE7_ST0BITS,
			fdc->sc_status[1], NE7_ST1BITS,
			fdc->sc_status[2], NE7_ST2BITS,
			fdc->sc_status[3], fdc->sc_status[4],
			fdc->sc_status[5]);

		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		fdfinish(fd, bp);
	}
}

/**********************************************************
 * Misc
 **********************************************************/
int
fdsize(dev)
	dev_t dev;
{

	/* Swapping to floppies would not make sense. */
	return -1;
}

int
fddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}

/**********************************************************
 * Fd Format
 **********************************************************/
static u_char
fd_interleave_sector(index, spc, interleave)
	u_int index;
	u_int spc;
	u_int interleave;
{

	interleave &= 0xff;
	if (interleave != 0)
		return ((interleave * index) % spc) + 1;
	else
		return index + 1;
}

static int
fdformat(dev, blkno)
	dev_t dev;
	u_int blkno;
{
	struct fd_softc *fd = fd_cd.cd_devs[FDUNIT(dev)];
	struct fd_type *type = fd->sc_type;
	u_int interleave = type->interleave;
	struct buf *bp;
	u_int error = 0, index, cyls, heads, spc;
	u_char *f;

	cyls = blkno / type->heads;
	heads = blkno % type->heads;
	spc = (u_int) type->sectrac;
	if (cyls >= (u_int)type->tracks)
		return 0;

	bp = geteblk(spc * 4);
	bp->b_dev = dev;
	bp->b_flags |= (B_NOCACHE | B_INVAL | B_BUSY);
	bp->b_blkno = blkno;
	bp->b_bcount = spc * 4;
	f = (u_char *) bp->b_data;
	for (index = 0; index < spc; index++)
	{
		f[4 * index] = cyls;
		f[4 * index + 1] = heads;
		f[4 * index + 2] = fd_interleave_sector(index, spc, interleave);
		f[4 * index + 3] = type->secsize;
	}

	while (fd->sc_fbp != NULL)
	{
		error = tsleep(&fd->sc_fbp, PZERO | PCATCH, "fdfwait", 0);
		if (error)
			goto out;
	}
	fd->sc_fbp = bp;
	fdstrategy(bp);
	error = biowait(bp);
	fd->sc_fbp = NULL;
	wakeup(&fd->sc_fbp);

out:
	brelse(bp);
	return error;
}

/**********************************************************
 * Disklabel
 **********************************************************/
void
fdgetdisklabel(dev)
	dev_t dev;
{
	struct fd_softc *fd = fd_cd.cd_devs[FDUNIT(dev)];
	struct disklabel *lp = fd->sc_dk.dk_label;
	struct fd_type *type = fd->sc_type;

	memset(lp, 0, sizeof(*lp));

	strncpy(lp->d_typename, "floppy disk", 16);
	lp->d_type = DTYPE_FLOPPY;
	lp->d_secsize = FDC_BSIZE;
	lp->d_nsectors = type->sectrac;
	lp->d_ntracks = type->heads;
	lp->d_secpercyl = type->seccyl;
	lp->d_ncylinders = type->tracks;
	lp->d_secperunit = type->size;
	lp->d_rpm = 3600;			/* XXX */
	lp->d_interleave = type->interleave;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_npartitions = RAW_PART + 1;
	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	readdisklabel(FDLABELDEV(dev), fdstrategy, lp, NULL);
}

/**********************************************************
 * Ioctl functions
 **********************************************************/
static void
fdiocsmode(fd, flags)
	struct fd_softc *fd;
	u_int flags;
{
	struct fd_type *type = fd->sc_type;

	fd->sc_flags &= ~(FD_D_CHECK | FD_EXT_144M);
	if (flags & FDIOM_144M)
		fd->sc_flags |= FD_EXT_144M;
	if (flags & FDIOM_ADCHK)
		fd->sc_flags |= FD_D_CHECK;

	type->interleave = (flags & FDIOM_IRMASK) ?  (flags & FDIOM_IRMASK) : 1;

	if ((fd->sc_flags & FD_EXT_144M) == 0 && fd->sc_type == FD_TYPE_144)
		fd->sc_type = fd->sc_type_backup = FD_TYPE_NORMAL;
}

int
fdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct fd_softc *fd = fd_cd.cd_devs[FDUNIT(dev)];
	struct fd_type *type;
	u_int nmode;
	int error;

	switch (cmd) {
	case FDMIOC:
		return fd_check_density(dev);

	case FDIOCFORMAT:
		return fdformat(dev, *(u_int *)addr);

	case FDIOCSMODE:
		fdiocsmode(fd, *(u_int *) addr);
		return 0;

	case FDIOCGMODE:
		type = fd->sc_type;
		nmode = type->interleave & FDIOM_IRMASK;
		if (fd->sc_flags & FD_EXT_144M)
			nmode |= FDIOM_144M;
		if (fd->sc_flags & FD_D_CHECK)
			nmode |= FDIOM_ADCHK;
		*(u_int *)addr = nmode;
		return 0;

	case FDIOCGDENSITY:
		*(int *) addr = fd_density2num(fd, fd->sc_type);
		return 0;

	case FDIOCSDENSITY:
		if ((type = fd_num2density(fd, *(u_int *) addr)) == NULL)
			return EINVAL;
		fd->sc_type = fd->sc_type_backup = type;
		return 0;

	case DIOCGDINFO:
	case DIOCGPART:
		if (fd->sc_flags & FD_D_CHECK)
		{
			if ((error = fd_check_density(dev)) != 0)
				return error;
		}

		fdgetdisklabel(dev);
		if (cmd == DIOCGDINFO)
		{
			*(struct disklabel *) addr = *fd->sc_dk.dk_label;
		}
		else
		{
			((struct partinfo *) addr)->disklab = 
				fd->sc_dk.dk_label;
			((struct partinfo *) addr)->part = 
				&fd->sc_dk.dk_label->d_partitions[FDPART(dev)];
		}
		return 0;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		return 0;

	case DIOCWDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		error = setdisklabel(fd->sc_dk.dk_label, 
				     (struct disklabel *) addr, 0, NULL);
		if (error)
			return error;

		error = writedisklabel(FDLABELDEV(dev), fdstrategy, 
				       fd->sc_dk.dk_label, NULL);
		return error;

	default:
		return ENOTTY;
	}
}

/**********************************************************
 * System functions
 **********************************************************/
int
fdc_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct fdc_softc *fdc = (struct fdc_softc *) dv;
	struct fd_softc *fd;
	int no, restart;
 
	switch(ev) 
	{
	case SYSTM_EVENT_SUSPEND:
		fdc->sc_flags |= FDC_INACTIVE;
		isa_dmaabort(fdc->sc_ic, fdc->sc_drq);
		bus_space_write_1(fdc->sc_iot, fdc->sc_ioh, fdctl, 0);
		untimeout(fdcpseudointr, fdc);
		untimeout(fdctimeout, fdc);
		untimeout(fdc_motor_timeout, fdc);
		break;

	case SYSTM_EVENT_RESUME:
		fdc->sc_flags &= ~FDC_INACTIVE;
		fdc->sc_state = DEVIDLE;
		fdc->sc_imode = NULL;
		restart = 0;

		for (no = 0; no < 4; no ++)
		{
			if ((fd = fdc->sc_fd[no]) == NULL)
				continue;
			untimeout(fd_motor_on, fd);
			fd->sc_cylin = -1;
			fd->sc_flags &= ~(FD_B_EXTRA | FD_R_AGAIN | 
					  FD_MOTOR | FD_MOTOR_WAIT);
			if (fd->sc_chkbuf != NULL || fd->sc_q.b_actf != NULL)
				restart = 1;
		}

		timeout(fdc_motor_timeout, fdc, FDC_MOTOR_TIMEOUT);
		if (restart)
			fdcstart(fdc);
		break;
	}

	return 0;
}

/**********************************************************
 * Low level Hw functions
 **********************************************************/
int
out_fdc(iot, ioh, x)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_char x;
{
	int i = 100000;

	while ((bus_space_read_1(iot, ioh, fdsts) & NE7_DIO) && i-- > 0);
	if (i <= 0)
		return -1;
	while ((bus_space_read_1(iot, ioh, fdsts) & NE7_RQM) == 0 && i-- > 0);
	if (i <= 0)
		return -1;
	bus_space_write_1(iot, ioh, fddata, x);
	return 0;
}

int
fdcresult(fdc)
	struct fdc_softc *fdc;
{
	bus_space_tag_t iot = fdc->sc_iot;
	bus_space_handle_t ioh = fdc->sc_ioh;
	u_char i;
	int j = 100000,
	    n = 0;

	for (; j; j--) {
		i = bus_space_read_1(iot, ioh, fdsts) &
		    (NE7_DIO | NE7_RQM | NE7_CB);
		if (i == NE7_RQM)
			return n;
		if (i == (NE7_DIO | NE7_RQM | NE7_CB)) {
			if (n >= sizeof(fdc->sc_status)) {
				log(LOG_ERR, "fdcresult: overrun\n");
				return -1;
			}
			fdc->sc_status[n++] = bus_space_read_1(iot, ioh, fddata);
		}
	}
	log(LOG_ERR, "fdcresult: timeout\n");
	return -1;
}

void
fdcstatus(dv, n, s)
	struct device *dv;
	int n;
	char *s;
{
	struct fdc_softc *fdc = (void *)dv->dv_parent;

	if (n == 0) {
		out_fdc(fdc->sc_iot, fdc->sc_ioh, NE7CMD_SENSEI);
		(void) fdcresult(fdc);
		n = 2;
	}

	printf("%s: %s", dv->dv_xname, s);

	switch (n) {
	case 0:
		printf("\n");
		break;
	case 2:
		printf(" (st0 %b cyl %d)\n",
		    fdc->sc_status[0], NE7_ST0BITS,
		    fdc->sc_status[1]);
		break;
	case 7:
		printf(" (st0 %b st1 %b st2 %b cyl %d head %d sec %d)\n",
		    fdc->sc_status[0], NE7_ST0BITS,
		    fdc->sc_status[1], NE7_ST1BITS,
		    fdc->sc_status[2], NE7_ST2BITS,
		    fdc->sc_status[3], fdc->sc_status[4], fdc->sc_status[5]);
		break;
#ifdef DIAGNOSTIC
	default:
		printf("\nfdcstatus: weird size");
		break;
#endif
	}
}

/**********************************************************
 * Density Control
 **********************************************************/
static void
set_density(fdc, fd)
	struct fdc_softc *fdc;
	struct fd_softc *fd;
{
	int mode, flags = FDP_PORTEXC | FDP_EMTON;
	struct fd_type *type = fd->sc_type;

	/* 1.44 mode check */
	if (fd->sc_flags & FD_EXT_144M)
	{
		mode = (((fd->sc_drive & 0x3) << 5) | 0x10);
		if (type->imode == MODE_144)
			mode |= 0x01;	/* 1.44 MB access mode */
		bus_space_write_1(fdc->sc_iot, fdc->sc_fdemsioh, 0, mode);
	}

	/* interface mode check */
	if (type->imode != MODE_DD)
		flags |= FDP_FDDEXC;
	bus_space_write_1(fdc->sc_iot, fdc->sc_fdsioh, 0, flags);

	/* motor on */
	bus_space_write_1(fdc->sc_iot, fdc->sc_ioh, fdctl, FDC_DMAE | FDC_MTON);

	/* Initialize structure !! */
	fdc->sc_state = DEVIDLE;
	fd->sc_flags &= ~(FD_MOTOR | FD_MOTOR_WAIT);
	fd->sc_cylin = -1;
	fdc->sc_shift = FDC_BSHIFT + (IS2HD(type) ? 1 : 0);
	fdc->sc_imode = type;

	if (out_fdc(fdc->sc_iot, fdc->sc_ioh, NE7CMD_SPECIFY) >= 0)
	{
		out_fdc(fdc->sc_iot, fdc->sc_ioh, type->steprate);
		out_fdc(fdc->sc_iot, fdc->sc_ioh, 0x32);
	}
	else
		printf("%s: specify cmd failed\n", fd->sc_dev.dv_xname);
}

static int
fd_check_range(fd, bp)
	struct fd_softc *fd;
	struct buf *bp;
{
	daddr_t blkno;
	struct fd_type *type = fd->sc_type;
	int limit, error = 0;

	if (ISFORMAT(bp))
	{
		bp->b_cylin = bp->b_blkno / type->heads;
		return 0;
	}
	else
	{
		limit = (IS2HD(type) != 0) ? type->size * 2 : type->size;
		blkno = bp->b_blkno << DEV2FDCSHIFT;

		if (bp->b_bcount & (FDC_BSIZE - 1))
		{
			error = EIO;
		}
		else if (blkno + (bp->b_bcount >> FDC_BSHIFT) > limit)
		{
			if (blkno == limit)
			{
				fd->sc_bcount = bp->b_bcount;
				return 1;
			}
			else
				error = ENOSPC;
		}
		else
		{
			if (IS2HD(type) != 0)
				blkno /= 2;
			bp->b_cylin = (blkno / (type->sectrac * type->heads));
			return 0;
		}
	}

	bp->b_flags |= B_ERROR;
	bp->b_error = error;
	fd->sc_bcount = bp->b_bcount;
	return 1;
}

static int
fd_density2num(fd, type)
	struct fd_softc *fd;
	struct fd_type *type;
{
	u_int num = (u_int)(type - fd_types);

	if (num >= FD_TABLE_SIZE || 
	    (num == 0 && (fd->sc_flags & FD_EXT_144M) == 0))
		return -1;
	return num;
}

static struct fd_type *
fd_num2density(fd, num)
	struct fd_softc *fd;
	u_int num;
{

	if (num >= FD_TABLE_SIZE || 
	    (num == 0 && (fd->sc_flags & FD_EXT_144M) == 0))
		return NULL;
	return &fd_types[num];
}

static struct fd_type *
fd_next_density(fd)
	struct fd_softc *fd;
{
	struct fd_type *type = fd->sc_type;

	/* get next one */
	if ((++type) == FD_TABLE_LAST)
		type = (fd->sc_flags & FD_EXT_144M) ? FD_TYPE_144 : FD_TYPE_NORMAL;

	/* check a cyclic point*/
	if (type == fd->sc_type_backup)
		type = NULL;

	return type;
}

#define	DUMMY_READ_CYL	2

static struct buf *
fd_dummy_read(fdc, fd, dev, bp)
	struct fdc_softc *fdc;
	struct fd_softc *fd;
	dev_t dev;
	struct buf *bp;
{
	struct fd_type *type = fd->sc_type;

	fd->sc_flags &= ~FD_DUNK;
	fd->sc_cylin = -1;

	if (bp == NULL)
		bp = geteblk(NBPG);

	bp->b_flags |= (B_READ | B_NOCACHE | B_INVAL | B_BUSY);
	bp->b_cylin = DUMMY_READ_CYL - 1;
	bp->b_blkno = DUMMY_READ_CYL * type->heads * type->sectrac - 1;
	bp->b_bcount = FDC_BSIZE;
	bp->b_dev = FDLABELDEV(dev);

	if (IS2HD(type))
	{
		bp->b_blkno *= 2;
		bp->b_bcount *= 2;
	}
	return bp;
}

void
fd_dummy_read_finish(fdc, fd)
	struct fdc_softc *fdc;
	struct fd_softc *fd;
{
	struct buf *bp = fd->sc_chkbuf;

	fd->sc_chkbuf = NULL;
	fd->sc_flags &= ~FD_CHKREQ;

	bp->b_resid = bp->b_bcount;
	bp->b_flags |= B_ERROR;
	bp->b_error = EIO;
	biodone(bp);
}

int
fd_check_density(dev)
	dev_t dev;
{
	struct buf *bp;
	int error;

	bp = geteblk(DEV_BSIZE);
	bp->b_flags |= (B_READ | B_NOCACHE | B_INVAL | B_BUSY);
	bp->b_dev = FDLABELDEV(dev);
	bp->b_blkno = 0;
	bp->b_bcount = DEV_BSIZE;
	fdstrategy(bp);
	error = biowait(bp);
	brelse(bp);
	return error;
}

/**********************************************************
 * Device sense, Motor control
 **********************************************************/
void
fdc_motor_timeout(arg)
	void *arg;
{
	struct fdc_softc *fdc = arg;
	struct fd_softc *fd;
	int i, s;

	s = splbio();
	if (fdc->sc_drives.tqh_first != NULL || fdc->sc_lockcnt > 0)
		goto out;

	for (i = 0; i < 4; i ++)
	{
		if ((fd = fdc->sc_fd[i]) == NULL)
			continue;

		if (fd->sc_acc > 0)
		{
			fd->sc_acc = 0;
			continue;
		}		

		if ((fd->sc_flags & FD_MOTOR) == 0)
			continue;
		
		fd_motor_off(fd);
	}

out:
	splx(s);
	timeout(fdc_motor_timeout, arg, FDC_MOTOR_TIMEOUT);
}

int
fd_sense_device(fdc, fd)
	struct fdc_softc *fdc;
	struct fd_softc *fd;
{
	bus_space_tag_t iot = fdc->sc_iot;
	bus_space_handle_t ioh = fdc->sc_ioh;
	
	if ((bus_space_read_1(iot, ioh, fdsts) & (NE7_CB | 0x0f)) != 0)
		return EIO;

	out_fdc(iot, ioh, NE7CMD_SENSED);
	out_fdc(iot, ioh, fd->sc_drive);
	if (fdcresult(fdc) != 1)
		return EIO;

	fd->sc_st3 = fdc->sc_status[0];
	return 0;
}

void
fd_ready_and_check(fdc, fd)
	struct fdc_softc *fdc;
	struct fd_softc *fd;
{
	int i, error;
	bus_space_tag_t iot = fdc->sc_iot;
	bus_space_handle_t ioh = fdc->sc_ioh;

	fdc->sc_lockcnt ++;
	for (i = 0; i < FDC_MOTOR_WC; i ++)
	{
		bus_space_write_1(iot, ioh, fdctl, (FDC_DMAE | FDC_MTON));
		fd_sense_device(fdc, fd);
		if (fd->sc_st3 & FST3_RDY)
		{
			fd->sc_flags |= FD_MOTOR;
			break;
		}

		error = tsleep(fdc, PZERO | PCATCH, "fdwait", hz / 10);
		if (error != EWOULDBLOCK)
			break;
	}
	fdc->sc_lockcnt --;
}

void
fd_set_motor(fdc, reset)
	struct fdc_softc *fdc;
	int reset;
{
	struct fd_softc *fd;
	u_char status;
	int n;

	if (reset != 0)
		return;

	status  = FDC_DMAE | FDC_MTON;
	if (fdc->sc_drives.tqh_first != NULL)
		goto out;

	for (n = 0; n < 4; n ++)
	{
		if ((fd = fdc->sc_fd[n]) == NULL)
			continue;
		if (fd->sc_flags & (FD_MOTOR | FD_MOTOR_WAIT))
			goto out;
	}
	status = 0;

out:
	bus_space_write_1(fdc->sc_iot, fdc->sc_ioh, fdctl, status);
}

void
fd_motor_off(arg)
	void *arg;
{
	struct fd_softc *fd = arg;
	int s;

	s = splbio();
	fd->sc_flags &= ~(FD_MOTOR | FD_MOTOR_WAIT);
	fd_set_motor((struct fdc_softc *)fd->sc_dev.dv_parent, 0);
	splx(s);
}

void
fd_motor_on(arg)
	void *arg;
{
	struct fd_softc *fd = arg;
	struct fdc_softc *fdc = (void *)fd->sc_dev.dv_parent;
	int s;

	s = splbio();
	fd->sc_st3 &= ~FST3_RDY;
	fd_sense_device(fdc, fd);
	if ((fd->sc_st3 & FST3_RDY) == 0 && fd->sc_mwcnt ++ < FDC_MOTOR_WC)
	{
		timeout(fd_motor_on, fd, hz / 10);
		splx(s);
		return;
	}
	fd->sc_st3 |= FST3_RDY;		/* XXX */
	fd->sc_mwcnt = 0;
	fd->sc_flags &= ~FD_MOTOR_WAIT;
	if ((fdc->sc_drives.tqh_first == fd) && (fdc->sc_state == MOTORWAIT))
		(void) fdcintr(fdc);
	splx(s);
}

/**********************************************************
 * Extra
 **********************************************************/
void
fd_mountroot_hook(dev)
	struct device *dev;
{
	int c;

	printf("Insert filesystem floppy and press return.");
	for (;;) {
		c = cngetc();
		if ((c == '\r') || (c == '\n')) {
			printf("\n");
			break;
		}
	}
}
