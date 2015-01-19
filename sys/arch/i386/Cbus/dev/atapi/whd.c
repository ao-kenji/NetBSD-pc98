/*	$NecBSD: whd.c,v 1.41 1999/08/01 23:55:22 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 * Copyright (c) 1995, 1996, 1997, 1998
 *	Isao Ohishi. All rights reserved.
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * DMA and multi-sector PIO handling are derived from code contributed by
 * Onno van der Linden.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/lock.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <vm/vm.h>

#include <machine/physio_proc.h>
#include <i386/Cbus/dev/atapi/wdcreg.h>
#include <i386/Cbus/dev/atapi/wdcvar.h>
#include <i386/Cbus/dev/atapi/wdchw.h>
#include <i386/Cbus/dev/atapi/whdio.h>
#include <dev/ata/atareg.h>

#include "locators.h"

#define	WDIORETRIES	5		/* number of retries before giving up */

#define	IDEDRV_FUNC_PREVENT	0x0000
#define	IDEDRV_FUNC_ALLOW	0x0001

#define	WDUNIT(dev)	DISKUNIT(dev)
#define	WDPART(dev)	DISKPART(dev)
#define	MAKEWDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)
#define	WDLABELDEV(dev)	(MAKEWDDEV(major(dev), WDUNIT(dev), RAW_PART))

/* state machine */
#define	INIT		0
#define	INIT_WAIT	1
#define	RECAL		2		/* recalibrate */
#define	RECAL_WAIT	3		/* done recalibrating */
#define	HWCTRL		4		/* hw mode setup */
#define	HWCTRL_WAIT	5		/* done */
#define	PIOMODE		6		/* piomode setup */
#define	PIOMODE_WAIT	7
#define	DMAMODE		8		/* dmamode setup */
#define	DMAMODE_WAIT	9
#define	GEOMETRY	10		/* upload geometry */
#define	GEOMETRY_WAIT	11		/* done uploading geometry */
#define	MULTIMODE	12		/* set multiple mode */
#define	MULTIMODE_WAIT	13		/* done setting multiple mode */
#define	OPEN		14		/* done with open */

struct wd_softc {
	struct device sc_dev;
	struct disk sc_dk;
	struct lock sc_lock;

	/* Information about the current transfer: */
	daddr_t sc_blkno;	/* starting block number */
	int sc_bcount;		/* byte count left */
	int sc_skip;		/* bytes already transferred */
	int sc_nblks;		/* number of blocks currently transferring */
	int sc_nbytes;		/* number of bytes currently transferring */

	/* Long-term state: */
	int sc_mode;			/* transfer mode */
#define	WDM_PIOSINGLE	0		/* single-sector PIO */
#define	WDM_PIOMULTI	1		/* multi-sector PIO */
#define	WDM_DMA		2		/* DMA */
#define	WDM_UDMA	3		/* UDMA */
	int sc_multiple;		/* multiple for WDM_PIOMULTI */
	int sc_flags;			/* drive characteistics found */
#define	WDF_WLABEL	0x04		/* label is writable */
#define	WDF_LABELLING	0x08		/* writing label */
#define	WDF_LOADED	0x10		/* parameters loaded */
#define	WDF_REMOVAL	0x20		/* removal disk */
#define	WDF_GEOCHK	0x40		/* check the hardware geometry */

	struct channel_link *sc_idec;
	struct buf sc_bufq;		/* buffered io q */

	u_int sc_hwsecs, sc_hwtrks, sc_hwcyls;	/* registered geometry */
	struct ataparams sc_params;	/* ESDI/ATA drive parameters */
	daddr_t	sc_badsect[127];	/* 126 plus trailing -1 marker */

	u_int sc_powt;				/* power save timeout */
	u_int sc_hwcreq, sc_hwc;		/* hw control */
#define	WDFHW_WCACHE		0x0001
#define	WDFHW_RCACHE		0x0002
#define	WDFHW_POWSAVE		0x0004

#if NRND > 0
	rndsource_element_t	rnd_source;
#endif
};

void wdstrategy __P((struct buf *));
static int wdmatch __P((struct device *, struct cfdata *, void *));
static void wdattach __P((struct device *, struct device *, void *));

struct cfattach wd_ca = {
	sizeof(struct wd_softc), wdmatch, wdattach
};

extern struct cfdriver wd_cd;

static int wdclink_wdintr __P((void *));
static int wdclink_wdstart __P((struct channel_link *));
static int wdclink_wdtimeout __P((struct channel_link *));
static int wdclink_wdactivate __P((struct channel_link *));
static int wdclink_wddeactivate __P((struct channel_link *));

struct channel_link_funcs channel_link_wd_funcs = {
	wdclink_wdstart,
	wdclink_wdintr,
	NULL,
	wdclink_wdtimeout,
	wdclink_wdactivate,
	wdclink_wddeactivate,
};

struct dkdriver wddkdriver = { wdstrategy };

static void wdfinish __P((struct wd_softc *, struct buf *));
static void wdgetdefaultlabel __P((struct wd_softc *, struct disklabel *));
static void wdgetdisklabel __P((struct wd_softc *));
static int wd_load_params __P((struct wd_softc *));
static int wdsetctlr __P((struct wd_softc *));
static void bad144intern __P((struct wd_softc *));
static void wderror __P((void *, struct buf *, char *));
static int wdctrlfeat __P((struct wd_softc *));
static void wd_print_params __P((struct wd_softc *));

int wdread __P((dev_t, struct uio *));
int wdwrite __P((dev_t, struct uio *));
int wdopen __P((dev_t, int, int, struct proc *));
int wdclose __P((dev_t, int, int, struct proc *));
int wdcontrol __P((struct wd_softc *));
int wdioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int wdsize __P((dev_t));
int wddump __P((dev_t, daddr_t, caddr_t, size_t));

static int idedrv_prevent __P((struct wd_softc *, u_int));
static int idedrv_test_unit_ready __P((struct wd_softc *));
static int idedrv_usrreq __P((struct wd_softc *, struct idedrv_ctrl *));
static int idedrv_usrans __P((struct wd_softc *, struct idedrv_ctrl *));

static __inline u_int8_t wd_precv __P((struct wd_softc *));

#define wdlock(wd) lockmgr(&(wd)->sc_lock, LK_EXCLUSIVE, NULL)
#define wdunlock(wd) lockmgr(&(wd)->sc_lock, LK_RELEASE, NULL)

/*******************************************************
 * probe attach
 *******************************************************/
static int
wdmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct ata_atapi_attach *aa = aux;
	struct cfdata *cf = match;

	if (aa->aa_type != T_ATA)
		return 0;

	if (cf->cf_loc[ATABUS_DIRECTCF_DRIVE] != ATABUS_DIRECTCF_DRIVE_DEFAULT
	    && cf->cf_loc[ATABUS_DIRECTCF_DRIVE] != aa->aa_drv_data->drive)
		return 0;

	return 1;
}

#define	WD_CFG(wd, parm) ((wd)->sc_params.atap_config & (parm))

static void
wdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ata_atapi_attach *aa = aux;
	struct atabus_softc *ata = (void *) parent;
	struct wd_softc *wd = (void *) self;
	struct ata_drive_datas *drvp = aa->aa_drv_data;
	struct channel_softc *chp = drvp->chnl_softc;
	struct channel_link *idec = ata->sc_idec[drvp->drive];
	u_int xfer_max, xfer_bits, xfer_pio, xfer_multi;
	u_char *xfer_type;

	printf("\n");

	drvp->drv_softc = self;

	wd->sc_idec = idec;
	wd->sc_dk.dk_driver = &wddkdriver;
	wd->sc_dk.dk_name = wd->sc_dev.dv_xname;

	idec->wk_isc = (void *) wd;
	idec->wk_funcs = &channel_link_wd_funcs;
	idec->wk_interface = MODE_IDE;

	disk_attach(&wd->sc_dk);

	wd_load_params(wd);
	wd_print_params(wd);

	/* probe caps */
	wdc_probe_and_setup_caps(idec, wd->sc_dev.dv_xname);

	/* setup */
	if ((chp->ch_flags & WDCF_REMOVAL) || WD_CFG(wd, ATA_CFG_REMOVABLE))
		wd->sc_flags |= WDF_REMOVAL;

	xfer_multi = wd->sc_params.atap_multi & 0xff;
	if (xfer_multi <= 1)
	{
		wd->sc_multiple = 1;
	}
	else
	{
		xfer_max = 16;
		if (chp->ch_memh != NULL)
			xfer_max = chp->ch_vsize / DEV_BSIZE;
		wd->sc_multiple = min(xfer_multi, xfer_max);
	}

	if (wd->sc_multiple > 1)
		wd->sc_mode = WDM_PIOMULTI;
	else
		wd->sc_mode = WDM_PIOSINGLE;
	if (drvp->drive_flags & DRIVE_DMA)
		wd->sc_mode = WDM_DMA;
	if (drvp->drive_flags & DRIVE_UDMA)
		wd->sc_mode = WDM_UDMA;

	lockinit(&wd->sc_lock, PLOCK | PCATCH, "wdlk", 0, 0);

	/* report our status */
	printf("%s:", wd->sc_dev.dv_xname);

	if (wd->sc_flags & WDF_REMOVAL)
		printf(" removal");

	printf(" using %d-sector", wd->sc_multiple);

	if ((wd->sc_params.atap_capabilities1 & WDC_CAP_LBA) != 0)
		printf(" lba addressing");
	else
		printf(" chs addressing");

	xfer_pio = (chp->wdc->cap & WDC_CAPABILITY_MODE) ? drvp->PIO_mode : -1;
	switch (wd->sc_mode)
	{
	case WDM_UDMA:
		printf(" ULTRA DMA mode(%d) transfer\n", drvp->UDMA_mode);
		break;
	case WDM_DMA:
		printf(" DMA mode(%d) transfer\n", drvp->DMA_mode);
		break;
	case WDM_PIOMULTI:
		printf(" multi secs mode(%d)", xfer_pio);
		goto pout;
		break;
	case WDM_PIOSINGLE:
		printf(" single sec mode(%d)", xfer_pio);
pout:
		xfer_bits = (drvp->drive_flags & DRIVE_CAP32) ? 32 : 16;
		xfer_type = (chp->ch_memh != NULL) ? "shm" : "pio";
		printf(" %d bits %s transfer\n", xfer_bits, xfer_type);
		break;
	}

#if NRND > 0
	rnd_attach_source(&wd->rnd_source, wd->sc_dev.dv_xname,
			  RND_TYPE_DISK, 0);
#endif
}

static void
wd_print_params(wd)
	struct wd_softc *wd;
{
	int i, blank;
	char buf[41], c, *p, *q;

	for (blank = 0, p = wd->sc_params.atap_model, q = buf, i = 0;
	     i < sizeof(wd->sc_params.atap_model); i++)
	{
		c = *p++;
		if (c == '\0')
			break;
		if (c != ' ') {
			if (blank) {
				*q++ = ' ';
				blank = 0;
			}
			*q++ = c;
		} else
			blank = 1;
	}
	*q++ = '\0';

	printf("%s: %dMB, %d cyl, %d head, %d sec, %d bytes/sec <%s>\n",
		wd->sc_dev.dv_xname,
		wd->sc_params.atap_cylinders *
		(wd->sc_params.atap_heads * wd->sc_params.atap_sectors) /
		(1048576 / DEV_BSIZE),
		wd->sc_params.atap_cylinders,
		wd->sc_params.atap_heads, wd->sc_params.atap_sectors,
	    	DEV_BSIZE, buf);
}

/*******************************************************
 * strategy
 *******************************************************/
/*
 * Read/write routine for a buffer.  Validates the arguments and schedules the
 * transfer.  Does not wait for the transfer to complete.
 */
static __inline u_int8_t
wd_precv(wd)
	struct wd_softc *wd;
{
	struct disklabel *lp = wd->sc_dk.dk_label;

	return ((lp->d_type == DTYPE_ST506) ? (lp->d_precompcyl / 4) : 0);
}

void
wdstrategy(bp)
	struct buf *bp;
{
	struct wd_softc *wd = wd_cd.cd_devs[WDUNIT(bp->b_dev)];
	struct channel_link *idec = wd->sc_idec;
	int s;

	/* Valid request?  */
	if (bp->b_blkno < 0 ||
	    (bp->b_bcount % wd->sc_dk.dk_label->d_secsize) != 0 ||
	    (bp->b_bcount / wd->sc_dk.dk_label->d_secsize) >= (1 << NBBY))
	{
		bp->b_error = EINVAL;
		goto bad;
	}

	/* If device invalidated (e.g. media change, door open), error. */
	if ((wd->sc_flags & WDF_LOADED) == 0)
	{
		bp->b_error = EIO;
		goto bad;
	}

	/* If it's a null transfer, return immediately. */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (WDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, wd->sc_dk.dk_label,
	    (wd->sc_flags & (WDF_WLABEL|WDF_LABELLING)) != 0) <= 0)
		goto done;

	/* Queue transfer on drive, activate drive and controller if idle. */
	s = splbio();
	disksort(&wd->sc_bufq, bp);
	WDC_DRIVE_HAS_JOB(idec);
	disk_busy(&wd->sc_dk);
	wdccmdstart(idec);
	splx(s);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	/* Toss transfer; we're done early. */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * Finish an I/O operation.  Clean up the drive and controller state, set the
 * residual count, and inform the upper layers that the operation is complete.
 */
static void
wdfinish(wd, bp)
	struct wd_softc *wd;
	struct buf *bp;
{

	bp->b_resid = wd->sc_bcount;
	wd->sc_skip = 0;
	wd->sc_bufq.b_actf = bp->b_actf;
	if (wd->sc_bufq.b_actf == NULL)
		WDC_DRIVE_HAS_NOJOB(wd->sc_idec);

	disk_unbusy(&wd->sc_dk, (bp->b_bcount - bp->b_resid));

#if NRND > 0
	rnd_add_uint32(&wd->rnd_source, bp->b_blkno);
#endif
	biodone(bp);
}

int
wdread(dev, uio)
	dev_t dev;
	struct uio *uio;
{

	return (physio(wdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
wdwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{

	return (physio(wdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Start I/O on a controller.  This does the calculation, and starts a read or
 * write operation.  Called to from wdstart() to start a transfer, from
 * wdcintr() to continue a multi-sector transfer or start the next transfer, or
 * wdcrestart() after recovering from an error.
 */
static __inline int _wdclink_wdstart __P((struct channel_link *, struct buf *));

static __inline int
_wdclink_wdstart(idec, bp)
	struct channel_link *idec;
	struct buf *bp;
{
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	struct wd_softc *wd = (struct wd_softc *) idec->wk_isc;
	struct disklabel *lp;
	int nblks;

	if (chp->ch_errors >= WDIORETRIES)
	{
		wderror(wd, bp, "too many retries");
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		wd->sc_bcount = bp->b_bcount; /* XXX */
		wdfinish(wd, bp);
		return WDCLINK_DONE;
	}

	/* reconfirm the registered geometry */
	lp = wd->sc_dk.dk_label;
	if (wd->sc_skip == 0)
	{
		if ((wd->sc_flags & WDF_GEOCHK) && (drvp->state > GEOMETRY) &&
		    (wd->sc_hwsecs != lp->d_nsectors || 
                     wd->sc_hwtrks != lp->d_ntracks))
			drvp->state = GEOMETRY;

		if (wd->sc_hwcreq != wd->sc_hwc && drvp->state > HWCTRL)
			drvp->state = HWCTRL;
	}

	/* Do control operations specially. */
	if (drvp->state < OPEN)
	{
		int error;

		/*
		 * Actually, we want to be careful not to mess with the control
		 * state if the device is currently busy, but we can assume
		 * that we never get to this point if that's the case.
		 */
		if ((error = wdcontrol(wd)) != WDCLINK_RESTART)
			return error;
	}

	/*
	 * WDCF_ERROR is set by wdcunwedge() and wdcintr() when an error is
	 * encountered.  If we are in multi-sector mode, then we switch to
	 * single-sector mode and retry the operation from the start.
	 */
	if (chp->ch_flags & WDCF_ERROR)
	{
		chp->ch_flags &= ~WDCF_ERROR;
		chp->ch_flags |= WDCF_SINGLE;
		wd->sc_skip = 0;
	}

	/* When starting a transfer... */
	if (wd->sc_skip == 0)
	{
		int part = WDPART(bp->b_dev);
		daddr_t blkno;

		wd->sc_bcount = bp->b_bcount;
		blkno = bp->b_blkno / (lp->d_secsize / DEV_BSIZE);
		if (part != RAW_PART)
			blkno += lp->d_partitions[part].p_offset;
		wd->sc_blkno = blkno;
	}

	/* When starting a multi-sector transfer, or doing single-sector
	    transfers... */
	if (wd->sc_skip == 0 || (chp->ch_flags & WDCF_SINGLE) != 0)
	{
		daddr_t blkno = wd->sc_blkno, altblk;
		long cylin, head, sector, blkdiff;
		int i, command = 0;

		if ((chp->ch_flags & WDCF_SINGLE) != 0)
			nblks = 1;
		else
			nblks = wd->sc_bcount / lp->d_secsize;

		if (lp->d_flags & D_BADSECT)
		{
#if defined(i386)
			/* XXX: see bad144.c */
			if (lp->d_partitions[0].p_offset > 0)
				altblk = lp->d_partitions[2].p_offset +
					 lp->d_partitions[2].p_size;
			else
#endif	/* i386 */
				altblk = lp->d_secperunit;

			for (i = 0; (blkdiff = wd->sc_badsect[i]) != -1; i++)
			{
				blkdiff -= blkno;
				if (blkdiff < 0)
					continue;

				if (blkdiff == 0)
					blkno = altblk - lp->d_nsectors - i - 1;

				if (blkdiff < nblks)
				{
					chp->ch_flags |= WDCF_SINGLE;
					nblks = 1;
				}
				break;
			}
		}

		if ((wd->sc_params.atap_capabilities1 & WDC_CAP_LBA) != 0)
		{
			sector = (blkno >> 0) & 0xff;
			cylin = (blkno >> 8) & 0xffff;
			head = (blkno >> 24) & 0xf;
			head |= WDSD_LBA;
		}
		else
		{
			sector = blkno % lp->d_nsectors;
			sector++;	/* Sectors begin with 1, not 0. */
			blkno /= lp->d_nsectors;
			head = blkno % lp->d_ntracks;
			blkno /= lp->d_ntracks;
			cylin = blkno;
			head |= WDSD_CHS;
		}

		if (wd->sc_mode == WDM_PIOSINGLE ||
		    (chp->ch_flags & WDCF_SINGLE) != 0)
		{
			wd->sc_nblks = 1;
			command = (bp->b_flags & B_READ) ?
				   WDCC_READ : WDCC_WRITE;
		}
		else if (wd->sc_mode == WDM_PIOMULTI)
		{
			wd->sc_nblks = min(nblks, wd->sc_multiple);
			command = (bp->b_flags & B_READ) ?
				   WDCC_READMULTI : WDCC_WRITEMULTI;
		}
		else
		{
			wd->sc_nblks = nblks;
			if ((bp->b_flags & B_READ) != 0)
			{
				command = WDCC_READDMA;
				chp->ch_dma = WDC_DMA_READ;
			}
			else
			{
				command = WDCC_WRITEDMA;
				chp->ch_dma = 0;
			}
			chp->ch_flags |= WDCF_DMASTART;
#ifdef	DIAGNOSTIC
			if (chp->wdc->dma_init == NULL)
			{
				panic("%s: DMA not present\n",
				      chp->wdc->sc_dev.dv_xname);
			}
#endif	/* DIAGNOSTIC */
		}

		wd->sc_nbytes = wd->sc_nblks * lp->d_secsize;

		if ((chp->ch_flags & WDCF_DMASTART) != 0)
		{
			if ((*chp->wdc->dma_init)(chp->wdc->dma_arg,
			    			  chp->channel, 
						  idec->wk_chan,
	        				  bp->b_data + wd->sc_skip,
						  wd->sc_nbytes,
						  chp->ch_dma) != 0)
			{
				wderror(wd, NULL, "wdcstart: DMA err");
				chp->ch_flags &= ~WDCF_DMASTART;
				return WDCLINK_ERROR_RETRY;
			}
		}

		/* Initiate command! */
		if (wdccommand(idec, command, cylin, head, sector, nblks,
			      wd_precv(wd)) != 0)
		{
			wderror(wd, NULL,
				"wdcstart: timeout waiting for unbusy");
			chp->ch_flags &= ~WDCF_DMASTART;
			return WDCLINK_RESET;
		}

		if ((chp->ch_flags & WDCF_DMASTART) != 0)
		{
			(*chp->wdc->dma_start) (chp->wdc->dma_arg,
						chp->channel, idec->wk_chan,
						chp->ch_dma);
			return WDCLINK_CONTINUE;
		}
	}
	else if (wd->sc_nblks > 1)
	{
		/* The number of blocks in the last stretch may be smaller. */
		nblks = wd->sc_bcount / lp->d_secsize;
		if (wd->sc_nblks > nblks)
		{
			wd->sc_nblks = nblks;
			wd->sc_nbytes = wd->sc_bcount;
		}
	}

	if ((bp->b_flags & (B_READ|B_WRITE)) == B_WRITE)
	{
		if (wait_for_drq(chp) < 0)
		{
			wderror(wd, NULL, "wdcstart: timeout waiting for drq");
			return WDCLINK_RESET;
		}

	        wdc_data_write(chp, (u_int8_t *) (bp->b_data + wd->sc_skip),
			       wd->sc_nbytes, drvp->drive_flags);
	}

	return WDCLINK_CONTINUE;
}

static int
wdclink_wdstart(idec)
	struct channel_link *idec;
{
	struct wd_softc *wd = (struct wd_softc *) idec->wk_isc;
	struct physio_proc *pp;
	struct buf *bp;
	int error;
	
	if ((bp = wd->sc_bufq.b_actf) == NULL)
		return WDCLINK_DONE;

	pp = physio_proc_enter(bp);
	error = _wdclink_wdstart(idec, bp);
	physio_proc_leave(pp);
	return error;
}


/*
 * Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start the
 * next request.  Also check for a partially done transfer, and continue with
 * the next chunk if so.
 */
static __inline int _wdclink_wdintr __P((struct channel_link *, struct buf *));

static __inline int
_wdclink_wdintr(idec, bp)
	struct channel_link *idec;
	struct buf *bp;
{
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	struct wd_softc *wd = idec->wk_isc;
	int dma_err = 0;

	if (drvp->state < OPEN)
	{
		if (wait_for_unbusy(chp) < 0)
		{
			wderror(wd, NULL, "wdcintr: timeout: drive setup");
			chp->ch_status |= WDCS_ERR;
		}
		return wdcontrol(wd);
	}

	/*
	 * Under DMA mode, ACARD ATP850UF's normal ide ports have
	 * no responses (thire images are all -1), 
	 * thus we should finish DMA mode first before normal accesses.
	 */
	if ((chp->ch_flags & WDCF_DMASTART) != 0)
	{
		if ((*chp->wdc->dma_finish)(chp->wdc->dma_arg,
		     			    chp->channel, idec->wk_chan,
					    chp->ch_dma) != 0)
		{
			dma_err |= WDCS_ERR;
			wderror(wd, bp, "DMA finish error");
		}
	}

	if (wait_for_unbusy(chp) < 0)
	{
		wderror(wd, NULL, "wdcintr: timeout waiting for unbusy");
		chp->ch_status |= WDCS_ERR;
	}
	chp->ch_status |= dma_err;

	if ((chp->ch_flags & WDCF_DMASTART) != 0)
	{
		chp->ch_flags &= ~WDCF_DMASTART;
		if ((chp->ch_status & WDCS_DRQ) != 0)
			chp->ch_status |= WDCS_ERR;

		if (chp->ch_status & WDCS_ERR)
			goto out;
	}
	else		
	{
		/* Have we an error? */
		if (chp->ch_status & WDCS_ERR)
			goto out;

		/* If this was a read and not using DMA, fetch the data. */
		if ((bp->b_flags & (B_READ|B_WRITE)) == B_READ)
		{
			if ((chp->ch_status & (WDCS_DRDY | WDCS_DSC | WDCS_DRQ))
			    != (WDCS_DRDY | WDCS_DSC | WDCS_DRQ))
			{
				wderror(wd, NULL, "wdcintr: intr before drq");
				return WDCLINK_RESET;
			}

			/* Pull in data. */
			wdc_data_read(chp, 
				      (u_int8_t *) (bp->b_data + wd->sc_skip),
				      wd->sc_nbytes, drvp->drive_flags);
		}
	}

	/* If we encountered any abnormalities, flag it as a soft error. */
	if (chp->ch_errors > 0 || (chp->ch_status & WDCS_CORR) != 0)
	{
		wderror(wd, bp, "soft error (corrected)");
	}

	/* Adjust pointers for the next block, if any. */
	wd->sc_blkno += wd->sc_nblks;
	wd->sc_skip += wd->sc_nbytes;
	wd->sc_bcount -= wd->sc_nbytes;

	/* See if this transfer is complete. */
	if (wd->sc_bcount > 0)
		return WDCLINK_RESTART;

	wdfinish(wd, bp);
	return WDCLINK_DONE;

out:
	if (chp->ch_flags & WDCF_SINGLE)
	{
		if (++ chp->ch_errors >= WDIORETRIES)
		{
			wderror(wd, bp, "<wdclink_wdintr> too many retries");
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			wdfinish(wd, bp);
			return WDCLINK_DONE;
		}
	}

	chp->ch_flags |= (WDCF_ERROR | WDCF_SINGLE);
	return WDCLINK_RESTART;
}

static int
wdclink_wdintr(arg)
	void *arg;
{
	struct channel_link *idec = arg;
	struct wd_softc *wd = idec->wk_isc;
	struct buf *bp = wd->sc_bufq.b_actf;
	struct physio_proc *pp;
	int error;

	pp = physio_proc_enter(bp);
	error = _wdclink_wdintr(idec, bp);
	physio_proc_leave(pp);
	return error;
}

/*******************************************************
 * open close
 *******************************************************/
/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
wdopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct wd_softc *wd;
	struct channel_softc *chp;
	struct channel_link *idec;
	int unit, part;
	int error;

	unit = WDUNIT(dev);
	if (unit >= wd_cd.cd_ndevs)
		return ENXIO;
	wd = wd_cd.cd_devs[unit];
	if (wd == 0)
		return ENXIO;

	idec = wd->sc_idec;
	chp = idec->wk_chp;
	if (idec->wk_flags & DRIVE_INACTIVE)
		return ENXIO;

	if ((error = wdlock(wd)) != 0)
		return error;

	if (wd->sc_dk.dk_openmask != 0)
	{
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((wd->sc_flags & WDF_LOADED) == 0)
		{
			error = EIO;
			goto bad3;
		}
	} 
	else
	{
		if (wd->sc_flags & WDF_REMOVAL)
		{
			error = idedrv_test_unit_ready(wd);
			if (error)
				goto bad;

			idedrv_prevent(wd, IDEDRV_FUNC_PREVENT);
		}

		if ((wd->sc_flags & WDF_LOADED) == 0)
		{
			wd->sc_flags |= WDF_LOADED;
			if (wd_load_params(wd) != 0)
			{
				error = ENXIO;
				wd->sc_flags &= ~WDF_LOADED;
				goto bad;
			}

			wdgetdisklabel(wd);
		}
	}

	part = WDPART(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= wd->sc_dk.dk_label->d_npartitions ||
	     wd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED))
	{
		error = ENXIO;
		goto bad;
	}

	switch (fmt)
	{
	case S_IFCHR:
		wd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	wd->sc_dk.dk_openmask = wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	wdunlock(wd);
	return 0;

bad:
	if (wd->sc_dk.dk_openmask == 0) 
	{
		wd->sc_flags &= ~WDF_LOADED;
		if (wd->sc_flags & WDF_REMOVAL)
			idedrv_prevent(wd, IDEDRV_FUNC_ALLOW);
	}

bad3:
	wdunlock(wd);
	return error;
}

int
wdclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct wd_softc *wd = wd_cd.cd_devs[WDUNIT(dev)];
	struct channel_softc *chp;
	struct channel_link *idec;
	int part = WDPART(dev);
	int error;

	if ((error = wdlock(wd)) != 0)
		return error;

	idec = wd->sc_idec;
	chp = idec->wk_chp;

	switch (fmt)
	{
	case S_IFCHR:
		wd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}

	wd->sc_dk.dk_openmask = wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;
	if (wd->sc_dk.dk_openmask == 0) 
	{
		wd->sc_flags &= ~WDF_LOADED;
		if (wd->sc_flags & WDF_REMOVAL)
			idedrv_prevent(wd, IDEDRV_FUNC_ALLOW);
	}

	wdunlock(wd);
	return 0;
}

/*******************************************************
 * drive geometry
 *******************************************************/
/*
 * Get the drive parameters, if ESDI or ATA, or create fake ones for ST506.
 */

static int
wd_load_params(wd)
	struct wd_softc *wd;
{
	struct channel_link *idec = wd->sc_idec;
	int error;

	error = wdc_load_params(idec, &wd->sc_params);
	if (error != 0)
	{
		/*
		 * We `know' there's a drive here; just assume it's old.
		 * This geometry is only used to read the MBR and print a
		 * (false) attach message.
		 */
		strncpy(wd->sc_dk.dk_label->d_typename, "ST506",
		    sizeof wd->sc_dk.dk_label->d_typename);
		wd->sc_dk.dk_label->d_type = DTYPE_ST506;

		strncpy(wd->sc_params.atap_model, "unknown",
			sizeof(wd->sc_params.atap_model));
		wd->sc_params.atap_config = ATA_CFG_FIXED;
		wd->sc_params.atap_cylinders = 1024;
		wd->sc_params.atap_heads = 8;
		wd->sc_params.atap_sectors = 17;
		wd->sc_params.atap_multi = 0;
		wd->sc_params.atap_capabilities1 = 0;
	}
	else
	{
		/* For over 8.4GB HDD */
		if (((wd->sc_params.atap_capabilities1 & WDC_CAP_LBA) != 0) &&
		    (wd->sc_params.atap_cylinders >= 16383))
		{
			u_int32_t totalsecs, secspercyl;

		    	totalsecs = wd->sc_params.atap_capacity[1];
			totalsecs <<= 16;
		    	totalsecs += wd->sc_params.atap_capacity[0];
			secspercyl = wd->sc_params.atap_heads *
				     wd->sc_params.atap_sectors;
			if (secspercyl == 0)
				secspercyl = 17 * 8;	/* XXX */
			wd->sc_params.atap_cylinders = totalsecs / secspercyl;
		}

		strncpy(wd->sc_dk.dk_label->d_typename, "ESDI/IDE",
		    sizeof(wd->sc_dk.dk_label->d_typename));
		wd->sc_dk.dk_label->d_type = DTYPE_ESDI;
	}
	return 0;
}

static void
wdgetdefaultlabel(wd, lp)
	struct wd_softc *wd;
	struct disklabel *lp;
{
	struct channel_link *idec = wd->sc_idec;
	struct channel_softc *chp = idec->wk_chp;

	memset(lp, 0, sizeof(struct disklabel));

	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = wd->sc_params.atap_heads;
	lp->d_nsectors = wd->sc_params.atap_sectors;
	lp->d_ncylinders = wd->sc_params.atap_cylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	lp->d_type = DTYPE_ST506;
	lp->d_subtype = chp->ch_hw->d_subtype;
	strncpy(lp->d_typename, "ST506 disk", 16);
	strncpy(lp->d_packname, wd->sc_params.atap_model, 16);
	lp->d_secperunit = lp->d_secpercyl * lp->d_ncylinders;
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * Fabricate a default disk label, and try to read the correct one.
 */
static void
wdgetdisklabel(wd)
	struct wd_softc *wd;
{
	struct disklabel *lp = wd->sc_dk.dk_label;
	struct channel_link *idec = wd->sc_idec;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	char *errstring;
	dev_t dev;

	memset(wd->sc_dk.dk_cpulabel, 0, sizeof(struct cpu_disklabel));

	wdgetdefaultlabel(wd, lp);

	dev = MAKEWDDEV(0, wd->sc_dev.dv_unit, RAW_PART);
	if (drvp->state > RECAL)
		drvp->state = RECAL;

	wd->sc_hwtrks = lp->d_ntracks;
	wd->sc_hwsecs = lp->d_nsectors;
	wd->sc_hwcyls = lp->d_ncylinders;
	wd->sc_badsect[0] = -1;

	wd->sc_flags |= WDF_GEOCHK;
	errstring = readdisklabel(dev, wdstrategy, lp, wd->sc_dk.dk_cpulabel);

	if (wd->sc_hwtrks != lp->d_ntracks ||
	    wd->sc_hwsecs != lp->d_nsectors ||
	    wd->sc_hwcyls != lp->d_ncylinders)
	{
		if (errstring == NULL)
			errstring = "BooBoo!";
		lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
		lp->d_checksum = 0;
		lp->d_checksum = dkcksum(lp);
	}

	if (errstring)
	{
		/*
		 * This probably happened because the drive's default
		 * geometry doesn't match the DOS geometry.  We
		 * assume the DOS geometry is now in the label and try
		 * again.  XXX This is a kluge.
		 */
		if (drvp->state > GEOMETRY)
			drvp->state = GEOMETRY;
		errstring = readdisklabel(dev, wdstrategy, lp,
					  wd->sc_dk.dk_cpulabel);
	}

	wd->sc_flags &= ~WDF_GEOCHK;
	if (errstring)
	{
		wd->sc_badsect[0] = -1;	/* delete badsect info! */
		printf("%s: %s\n", wd->sc_dev.dv_xname, errstring);
		return;
	}

	if (drvp->state > GEOMETRY)
		drvp->state = GEOMETRY;
	if ((lp->d_flags & D_BADSECT) != 0)
		bad144intern(wd);
}

/*******************************************************
 * drive control
 *******************************************************/
/*
 * Implement operations needed before read/write.
 * Returns 0 if operation still in progress, 1 if completed.
 */
#define	STATE_ERROR(str)	\
	{ s = (str); goto bad; }
#define	STATE_AGAIN(drvp, stat) \
	{ (drvp)->state = (stat); continue; }
#define	STATE_CONTINUE(drvp, stat) \
	{ (drvp)->state = (stat); return WDCLINK_CONTINUE; }

int
wdcontrol(wd)
	struct wd_softc *wd;
{
	struct disklabel *lp = wd->sc_dk.dk_label;
	struct channel_link *idec = wd->sc_idec;
	struct channel_softc *chp = idec->wk_chp;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	int error = 0;
	u_char *s, *nss;

     while (1) 
     {
	switch (drvp->state)
	{
	case INIT:
		wdccommandshort(chp, idec->wk_chan, WDCC_RECAL | WDCC_FORCE);
		STATE_CONTINUE(drvp, INIT_WAIT);
		break;

	case INIT_WAIT:
	case RECAL:
		if (wdccommandshort(chp, idec->wk_chan, WDCC_RECAL) != 0)
			STATE_ERROR("recal (1)");

		STATE_CONTINUE(drvp, RECAL_WAIT);
		break;

	case RECAL_WAIT:
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			STATE_ERROR("recal (2)");
		/* fall through */

	case HWCTRL:
		if (wd->sc_hwcreq == wd->sc_hwc)
			STATE_AGAIN(drvp, PIOMODE);

		if (wdctrlfeat(wd) != 0)
			STATE_ERROR("hwctrl (1)");
		STATE_CONTINUE(drvp, HWCTRL_WAIT);
		break;

	case HWCTRL_WAIT:
		if (chp->ch_status & WDCS_ERR)
		{
			wd->sc_hwcreq = wd->sc_hwc;
			STATE_ERROR("hwctrl (2)");
		}
		STATE_AGAIN(drvp, HWCTRL);
		
	case PIOMODE:
		if ((chp->wdc->cap & WDC_CAPABILITY_MODE) == 0 ||
		    (drvp->drive_flags & DRIVE_MODE) == 0)
			STATE_AGAIN(drvp, GEOMETRY);

		if (wdcsetfeature(chp, idec->wk_chan, 
				  WDCC_FEATURES, 0x08 | drvp->PIO_mode,
				  FEAT_SET_MODE) != 0)
		{
			STATE_ERROR("piomode setup (1)");
		}

		STATE_CONTINUE(drvp, PIOMODE_WAIT);
		break;

	case PIOMODE_WAIT:
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			STATE_ERROR("piomode setup (2)");
		/* fall through */

	case DMAMODE:
		if (drvp->drive_flags & DRIVE_UDMA)
		{
			error = wdcsetfeature(chp, idec->wk_chan,
					      WDCC_FEATURES,
			    		      0x40 | drvp->UDMA_mode,
					      FEAT_SET_MODE);
		}
		else if (drvp->drive_flags & DRIVE_DMA)
		{
			error = wdcsetfeature(chp, idec->wk_chan,
					      WDCC_FEATURES,
					      0x20 | drvp->DMA_mode,
					      FEAT_SET_MODE);
		} 
		else
			STATE_AGAIN(drvp, GEOMETRY);

		if (error != 0)
			STATE_ERROR("dma setup (1)");

		STATE_CONTINUE(drvp, DMAMODE_WAIT);
		break;

	case DMAMODE_WAIT:
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			STATE_ERROR("dma setup (2)");
		/* fall through */

	case GEOMETRY:
		if (wdsetctlr(wd) != 0)
			STATE_ERROR("geometry upload (1)");

		wd->sc_hwsecs = lp->d_nsectors;
		wd->sc_hwtrks = lp->d_ntracks;
		wd->sc_hwcyls = lp->d_ncylinders;
		STATE_CONTINUE(drvp, GEOMETRY_WAIT);
		break;

	case GEOMETRY_WAIT:
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			STATE_ERROR("geometry upload (2)");
		/* fall through */

	case MULTIMODE:
		if (wd->sc_multiple <= 1)
			STATE_AGAIN(drvp, OPEN);

		if (wdcsetfeature(chp, idec->wk_chan, WDCC_SETMULTI,
			          wd->sc_multiple, 0) != 0)
			STATE_ERROR("setmulti (1)");
		STATE_CONTINUE(drvp, MULTIMODE_WAIT);
		break;

	case MULTIMODE_WAIT:
		if (chp->ch_status & (WDCS_ERR | WDCS_DWF))
			STATE_ERROR("setmulti (2)");
		/* fall through */

	case OPEN:
		drvp->state = OPEN;
		return WDCLINK_RESTART;
	}

	return WDCLINK_CONTINUE;
    }

bad:
	nss = NULL;
	switch (wd->sc_mode)
	{
	case WDM_PIOSINGLE:
		drvp->drive_flags &= ~DRIVE_MODE;
		break;

	case WDM_PIOMULTI:
		if ((drvp->drive_flags & DRIVE_MODE) != 0)
		{
			drvp->drive_flags &= ~DRIVE_MODE;
			break;
		}
		else
		{
			wd->sc_mode = WDM_PIOSINGLE;
			wd->sc_multiple = 1;
			nss = "single sec";
		}
		break;

	case WDM_UDMA:
		wd->sc_mode = WDM_DMA;
		nss = "DMA";
		break;

	case WDM_DMA:
		wd->sc_mode = WDM_PIOMULTI;
		nss = "multi secs"; 
		break;
	}

	printf("%s: wdcontrol %s failed\n", wd->sc_dev.dv_xname, s);
	if (nss != NULL)
		printf("%s: wdcontrol fall to %s mode\n",
			wd->sc_dev.dv_xname, nss);

	return WDCLINK_RESET;
}

/*
 * Tell the drive what geometry to use.
 */
static int
wdsetctlr(wd)
	struct wd_softc *wd;
{
	struct channel_link *idec = wd->sc_idec;

#ifdef WDDEBUG
	printf("wd(%d,%d) C%dH%dS%d\n", wd->sc_dev.dv_unit, idec->wk_chan,
	    wd->sc_dk.dk_label->d_ncylinders, wd->sc_dk.dk_label->d_ntracks,
	    wd->sc_dk.dk_label->d_nsectors);
#endif

	if (wdccommand(idec, WDCC_IDP, wd->sc_dk.dk_label->d_ncylinders,
	    wd->sc_dk.dk_label->d_ntracks - 1, 0,
	    wd->sc_dk.dk_label->d_nsectors, wd_precv(wd)) != 0)
	{
		wderror(wd, NULL, "wdsetctlr: geometry upload failed");
		return -1;
	}

	return 0;
}

static int
wdctrlfeat(wd)
	struct wd_softc *wd;
{
	struct channel_link *idec = wd->sc_idec;
	struct channel_softc *chp = idec->wk_chp;
	register u_int8_t feat;
	u_int bit, mask, req;
	int error, chan = idec->wk_chan;
	u_char *s;

	req = wd->sc_hwcreq ^ wd->sc_hwc;
	if (req & WDFHW_RCACHE)
	{
		mask = ~WDFHW_RCACHE;
		bit = wd->sc_hwcreq & WDFHW_RCACHE;

		if (bit == 0)
			feat = FEAT_RCACHE_OFF;
		else
			feat = FEAT_RCACHE_ON;

		error = wdcsetfeature(chp, chan, WDCC_FEATURES, 0, feat);
		if (error == 0)
			goto done;
		s = "wdcontrol: rcache setup failed (1)";
	}
	else if (req & WDFHW_WCACHE)
	{
		mask = ~WDFHW_WCACHE;
		bit = wd->sc_hwcreq & WDFHW_WCACHE;

		if (bit == 0)
			feat = FEAT_WCACHE_OFF;
		else
			feat = FEAT_WCACHE_ON;

		error = wdcsetfeature(chp, chan, WDCC_FEATURES, 0, feat);
		if (error == 0)
			goto done;
		s = "wdcontrol: wcache setup failed (1)";
	}
	else if (req & WDFHW_POWSAVE)
	{
		mask = ~WDFHW_POWSAVE;
		bit = 0;

		error = wdcsetfeature(chp, chan, WDCC_POWSAVE, wd->sc_powt, 0);
		if (error == 0)
			goto done;
		s = "wdcontrol: power setup failed (1)";
	}
	else
		s = "wdcontrol: unknown cmd";

	wd->sc_hwcreq = wd->sc_hwc;		/* XXX */
	wderror(wd, NULL, s);
	return WDCLINK_RESET;

done:
	wd->sc_hwcreq &= mask;
	wd->sc_hwcreq |= bit;
	wd->sc_hwc &= mask;
	wd->sc_hwc |= bit;
	return 0;
}

/*******************************************************
 * ioctl
 *******************************************************/
int
wdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct wd_softc *wd = wd_cd.cd_devs[WDUNIT(dev)];
	struct channel_link *idec = wd->sc_idec;
	struct ata_drive_datas *drvp = idec->wk_drvp;
	int error;

	if ((wd->sc_flags & WDF_LOADED) == 0)
		return EIO;

	switch (cmd)
	{
	case DIOCSBAD:
		if ((flag & FWRITE) == 0)
			return EBADF;
		wd->sc_dk.dk_cpulabel->bad = *(struct dkbad *)addr;
		wd->sc_dk.dk_label->d_flags |= D_BADSECT;
		bad144intern(wd);
		return 0;

	case DIOCGDINFO:
		*(struct disklabel *)addr = *(wd->sc_dk.dk_label);
		return 0;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = wd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &wd->sc_dk.dk_label->d_partitions[WDPART(dev)];
		return 0;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = wdlock(wd)) != 0)
			return error;
		wd->sc_flags |= WDF_LABELLING;

		error = setdisklabel(wd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*wd->sc_dk.dk_openmask : */0,
		    wd->sc_dk.dk_cpulabel);
		if (error == 0)
		{
			if (drvp->state > GEOMETRY)
				drvp->state = GEOMETRY;
			if (cmd == DIOCWDINFO)
				error = writedisklabel(WDLABELDEV(dev),
				    wdstrategy, wd->sc_dk.dk_label,
				    wd->sc_dk.dk_cpulabel);
		}

		wd->sc_flags &= ~WDF_LABELLING;
		wdunlock(wd);
		return error;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *)addr)
			wd->sc_flags |= WDF_WLABEL;
		else
			wd->sc_flags &= ~WDF_WLABEL;
		return 0;

	case DIOCGDEFLABEL:
		wdgetdefaultlabel(wd, (struct disklabel *)addr);
		return 0;

	case IDEIOSMODE:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return error;
		error = idedrv_usrreq(wd, (struct idedrv_ctrl *) addr);
		if (error == 0 && (wd->sc_hwcreq != wd->sc_hwc))
		{
			/* XXX: dummy test read */
			struct disklabel *lp = wd->sc_dk.dk_label;
			struct buf *tbp;

			tbp = geteblk(lp->d_secsize);
			tbp->b_dev = WDLABELDEV(dev);
			tbp->b_blkno = 0;
			tbp->b_bcount = lp->d_secsize;
			tbp->b_flags |= B_BUSY | B_READ;
			wdstrategy(tbp);
			error = biowait(tbp);
			tbp->b_flags |= B_INVAL;
			brelse(tbp);
		}
		return error;

	case IDEIOGMODE:
		return idedrv_usrans(wd, (struct idedrv_ctrl *) addr);

	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("wdioctl: impossible");
#endif
}

/*******************************************************
 * swap device
 *******************************************************/
int
wdsize(dev)
	dev_t dev;
{
	struct wd_softc *wd;
	int part, unit, omask;
	int size;

	unit = WDUNIT(dev);
	if (unit >= wd_cd.cd_ndevs)
		return (-1);
	wd = wd_cd.cd_devs[unit];
	if (wd == NULL)
		return (-1);

	part = WDPART(dev);
	omask = wd->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && wdopen(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	if (wd->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = wd->sc_dk.dk_label->d_partitions[part].p_size *
		    (wd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && wdclose(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	return (size);
}

/* #define WD_DUMP_NOT_TRUSTED if you just want to watch */
static int wddoingadump;
static int wddumprecalibrated;

/*
 * Dump core after a system crash.
 */
int
wddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	struct wd_softc *wd;	/* disk unit to do the I/O */
	struct channel_softc *chp;	/* disk controller to do the I/O */
	struct channel_link *idec;
	struct ata_drive_datas *drvp;
	struct disklabel *lp;	/* disk's disklabel */
	int	unit, part;
	int	nblks;		/* total number of sectors left to write */

	/* Check if recursive dump; if so, punt. */
	if (wddoingadump)
		return EFAULT;
	wddoingadump = 1;

	unit = WDUNIT(dev);
	if (unit >= wd_cd.cd_ndevs)
		return ENXIO;
	wd = wd_cd.cd_devs[unit];
	if (wd == 0)
		return ENXIO;

	idec = wd->sc_idec;
	chp = idec->wk_chp;
 	drvp = idec->wk_drvp;
	part = WDPART(dev);

	/* Make sure it was initialized. */
	if (drvp->state < OPEN)
		return ENXIO;

	/* before dump, shutdown DMA */
	if ((chp->ch_flags & WDCF_DMASTART) != 0)
	{
		(*chp->wdc->dma_finish)(chp->wdc->dma_arg,
		     		        chp->channel, idec->wk_chan,
					chp->ch_dma);
		chp->ch_flags &= ~WDCF_DMASTART;
	}

	wdc_select_bank(chp, idec->wk_chan);

	/* Convert to disk sectors.  Request must be a multiple of size. */
	lp = wd->sc_dk.dk_label;
	if ((size % lp->d_secsize) != 0)
		return EFAULT;
	nblks = size / lp->d_secsize;
	blkno = blkno / (lp->d_secsize / DEV_BSIZE);

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || ((blkno + nblks) > lp->d_partitions[part].p_size))
		return EINVAL;

	/* Offset block number to start of partition. */
	blkno += lp->d_partitions[part].p_offset;

	/* Recalibrate, if first dump transfer. */
	if (wddumprecalibrated == 0)
	{
		wddumprecalibrated = 1;
		if (wdccommandshort(chp, idec->wk_chan, WDCC_RECAL) != 0 ||
		    wait_for_ready(chp) != 0 || wdsetctlr(wd) != 0 ||
		    wait_for_ready(chp) != 0)
		{
			wderror(wd, NULL, "wddump: recal failed");
			return EIO;
		}
	}

	while (nblks > 0)
	{
		daddr_t xlt_blkno = blkno, altblk;
		long cylin, head, sector, blkdiff;
		int i;

		if ((lp->d_flags & D_BADSECT) != 0)
		{
#if defined(i386)
			/* XXX: see bad144.c */
			if (lp->d_partitions[0].p_offset > 0)
				altblk = lp->d_partitions[2].p_offset +
					 lp->d_partitions[2].p_size;
			else
#endif	/* i386 */
				altblk = lp->d_secperunit;

			for (i = 0; (blkdiff = wd->sc_badsect[i]) != -1; i++)
			{
				blkdiff -= xlt_blkno;
				if (blkdiff < 0)
					continue;

				if (blkdiff == 0)
					xlt_blkno = altblk - 
						    lp->d_nsectors - i - 1;
				break;
			}
		}

		if ((wd->sc_params.atap_capabilities1 & WDC_CAP_LBA) != 0)
		{
			sector = (xlt_blkno >> 0) & 0xff;
			cylin = (xlt_blkno >> 8) & 0xffff;
			head = (xlt_blkno >> 24) & 0xf;
			head |= WDSD_LBA;
		}
		else
		{
			sector = xlt_blkno % lp->d_nsectors;
			sector++;	/* Sectors begin with 1, not 0. */
			xlt_blkno /= lp->d_nsectors;
			head = xlt_blkno % lp->d_ntracks;
			xlt_blkno /= lp->d_ntracks;
			cylin = xlt_blkno;
			head |= WDSD_CHS;
		}

#ifndef WD_DUMP_NOT_TRUSTED
		if (wdccommand(wd->sc_idec, WDCC_WRITE, cylin, head, sector, 1,
		           wd_precv(wd)) != 0 || wait_for_drq(chp) != 0)
		{
			wderror(wd, NULL, "wddump: write failed");
			return EIO;
		}

		wdc_data_write(chp, (u_int8_t *) va, lp->d_secsize, 0);

		/* Check data request (should be done). */
		if (wait_for_ready(chp) != 0)
		{
			wderror(wd, NULL, "wddump: timeout waiting for ready");
			return EIO;
		}
#else	/* WD_DUMP_NOT_TRUSTED */

		/* Let's just talk about this first... */
		printf("wd%d: dump addr 0x%x, cylin %d, head %d, sector %d\n",
		    unit, va, cylin, head, sector);
		delay(500 * 1000);	/* half a second */
#endif	/* WD_DUMP_NOT_TRUSTED */

		/* update block count */
		nblks -= 1;
		blkno += 1;
		va += lp->d_secsize;
	}

	wddoingadump = 0;
	return 0;
}

/*******************************************************
 * activate & deactivate
 *******************************************************/
static int
wdclink_wddeactivate(idec)
	struct channel_link *idec;
{
	struct wd_softc *wd = (struct wd_softc *) idec->wk_isc;

	if ((wd->sc_flags & WDF_REMOVAL) && wd->sc_dk.dk_openmask)
		printf("WARNING %s still opened\n", wd->sc_dev.dv_xname);

	return 0;
}

static int
wdclink_wdactivate(idec)
	struct channel_link *idec;
{
	struct wd_softc *wd = (struct wd_softc *) idec->wk_isc;
	struct ata_drive_datas *drvp = idec->wk_drvp;

	drvp->state = INIT;
	wd->sc_skip = 0;

	wd->sc_hwcreq = wd->sc_hwc = 0;	/* XXX */
	if (wd->sc_powt > 0)
		wd->sc_hwcreq |= WDFHW_POWSAVE;

	return 0;
}

static int
wdclink_wdtimeout(idec)
	struct channel_link *idec;
{
	struct ata_drive_datas *drvp = idec->wk_drvp;
	struct wd_softc *wd = (struct wd_softc *) idec->wk_isc;

	if (idec->wk_interface != MODE_IDE)
		return EINVAL;

	if (drvp->state > RECAL)
		drvp->state = RECAL;

	if (wd->sc_mode >= WDM_DMA)
	{
		/* XXX:
		 * DMA transfer failed, use PIO transfer.
		 */
		wd->sc_mode = WDM_PIOMULTI;
		drvp->drive_flags &= ~(DRIVE_UDMA | DRIVE_DMA);
		printf("%s: abort DMA transfer mode. use PIO transfer mode\n",
			wd->sc_dev.dv_xname);
	}

	wd->sc_skip = 0;

	return 0;
}

/*******************************************************
 * sub functions
 *******************************************************/
/*
 * Internalize the bad sector table.
 */
static void
bad144intern(wd)
	struct wd_softc *wd;
{
	struct dkbad *bt = &wd->sc_dk.dk_cpulabel->bad;
	struct disklabel *lp = wd->sc_dk.dk_label;
	int i = 0;

	for (; i < 126; i++)
	{
		if (bt->bt_bad[i].bt_cyl == 0xffff)
			break;
		wd->sc_badsect[i] = bt->bt_bad[i].bt_cyl * lp->d_secpercyl +
		    (bt->bt_bad[i].bt_trksec >> 8) * lp->d_nsectors +
		    (bt->bt_bad[i].bt_trksec & 0xff);
	}

	for (; i < 127; i++)
		wd->sc_badsect[i] = -1;
}

static void
wderror(dev, bp, msg)
	void *dev;
	struct buf *bp;
	char *msg;
{
	struct wd_softc *wd = dev;
	struct channel_link *idec = wd->sc_idec;

	if (bp)
	{
		diskerr(bp, "wd", msg, LOG_PRINTF, wd->sc_skip / DEV_BSIZE,
		    wd->sc_dk.dk_label);
		printf("\n");
	}
	wdcerror(idec, msg);
}

int
idedrv_prevent(wd, flags)
	struct wd_softc *wd;
	u_int flags;
{
	struct channel_link *idec = wd->sc_idec;
	int error, cmd;	

	if (flags == IDEDRV_FUNC_PREVENT)
		cmd = WDCC_LOCK;
	else
		cmd = WDCC_UNLOCK;

	error = wdc_exec_cmd(idec, cmd);
	if (error != 0 && cmd == WDCC_UNLOCK)
	{
		(void) wdc_exec_cmd(idec, WDCC_LOCK);
	}

	return error;
}

static int
idedrv_test_unit_ready(wd)
	struct wd_softc *wd;
{
	struct channel_link *idec = wd->sc_idec;
	int i, error = 0;

	for (i = 0; i < 3; i ++)
	{
		error = wdc_exec_cmd(idec, WDCC_RECAL);
		if (error != EBUSY)
			break;
	}
	return error;
}

/*******************************************************
 * sub functions for user requests
 *******************************************************/
static int
idedrv_usrans(wd, cp)
	struct wd_softc *wd;
	struct idedrv_ctrl *cp;
{
	int error = 0;

	switch(cp->idc_cmd)
	{
	case IDEDRV_RCACHE:
		if (wd->sc_hwc & WDFHW_RCACHE)
			cp->idc_feat = IDEDRV_RCACHE_FEAT_ON;
		else
			cp->idc_feat = IDEDRV_RCACHE_FEAT_OFF;
		break;

	case IDEDRV_WCACHE:
		if (wd->sc_hwc & WDFHW_WCACHE)
			cp->idc_feat = IDEDRV_WCACHE_FEAT_ON;
		else
			cp->idc_feat = IDEDRV_WCACHE_FEAT_OFF;
		break;

	case IDEDRV_POWSAVE:
		cp->idc_secc = wd->sc_powt / (60 / 5) ;
		break;

	default:
		error = ENOTTY;
	}

	return error;
}

static int
idedrv_usrreq(wd, cp)
	struct wd_softc *wd;
	struct idedrv_ctrl *cp;
{
	int error = 0;
	u_int powt;

	switch(cp->idc_cmd)
	{
	case IDEDRV_RCACHE:
		if (cp->idc_feat == IDEDRV_RCACHE_FEAT_ON)
			wd->sc_hwcreq |= WDFHW_RCACHE;
		else if (cp->idc_feat == IDEDRV_RCACHE_FEAT_OFF)
			wd->sc_hwcreq &= ~WDFHW_RCACHE;
		else
			error = EINVAL;
		break;

	case IDEDRV_WCACHE:
		if (cp->idc_feat == IDEDRV_WCACHE_FEAT_ON)
			wd->sc_hwcreq |= WDFHW_WCACHE;
		else if (cp->idc_feat == IDEDRV_WCACHE_FEAT_OFF)
			wd->sc_hwcreq &= ~WDFHW_WCACHE;
		else
			error = EINVAL;
		break;

	case IDEDRV_POWSAVE:
		powt = cp->idc_secc;
		powt *= (60 / 5);
		if (powt < 255)
		{
			if (powt != wd->sc_powt)
			{
				wd->sc_hwcreq |= WDFHW_POWSAVE;
				wd->sc_powt = powt;
			}
		}
		else
			error = EINVAL;
		break;

	default:
		error = ENOTTY;
	}

	return error;
}
