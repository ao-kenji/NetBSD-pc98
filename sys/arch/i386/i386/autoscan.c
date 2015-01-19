/*	$NecBSD: autoscan.c,v 1.48.4.1 1999/08/20 21:29:59 honda Exp $	*/
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

/*
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include "opt_autoscan.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <machine/dvcfg.h>
#include <i386/i386/autoscan.h>

/*******************************************************************
 * AUTOSCAN
 *******************************************************************/
int autoscan_debug;

static int find_pc98_dospart __P((struct dos_check_args *, int));
static int find_at_dospart __P((struct dos_check_args *, int));
static void change_disk_geometry __P((struct dos_check_args *));
static struct part_type *check_fake_entry __P((struct disklabel *, struct dos_partition *, int));
static struct part_type *check_pc98_disktype __P((sysid_t));
static int scan_geo_by_config __P((u_char *, u_int *, u_int *));

/*******************************************************************
 * PARTITION TYPE
 *******************************************************************/
#ifndef	DOSSID_LINUX
#define	DOSSID_LINUX	0xe2
#endif

struct part_type {
	sysid_t	sysid;		/* disk system id */
	sysid_t mask;		/* identification mask */
	int fstype;		/* file system type */
#define	ROK	0x01
#define	FOK	0x02
	u_char flags;		/* flags */
};

#define	MAKEID(mid, sid) sysid_activate(sysid_make_id((mid), (sid)))
struct part_type part_types[] = {
	{MAKEID(0x14, DOSSID_NETBSD), 0xff7f, FS_BSDFFS, ROK},
#if	0
	{MAKEID(0x14, DOSSID_NETBSD), 0xff7f, FS_BSDLFS, ROK},
#endif
	{MAKEID(0x21, 0xb1), 	      0xff7f, FS_HPFS,   ROK | FOK},
	{MAKEID(0x11, DOSSID_MSDOS),  0x8f7f, FS_MSDOS,  ROK | FOK},
	{MAKEID(0x20, DOSSID_MSDOS),  0x8f7f, FS_MSDOS,  ROK | FOK},
	{MAKEID(0x21, DOSSID_MSDOS),  0x8f7f, FS_MSDOS,  ROK | FOK},
	{MAKEID(0x22, DOSSID_MSDOS),  0x8f7f, FS_MSDOS,  ROK | FOK},
	{MAKEID(0x23, DOSSID_MSDOS),  0x8f7f, FS_MSDOS,  ROK | FOK},
	{MAKEID(0x07, DOSSID_MSDOS),  0x8f7f, FS_HPFS,   ROK | FOK},
#if	0
	{MAKEID(0x02, DOSSID_NETBSD), 0x8f7f, FS_SWAP,	 ROK | FOK},
#endif
	{MAKEID(0x75, DOSSID_LINUX),  0xff7f, FS_EX2FS,	 ROK | FOK},
	{MAKEID(0xa0, DOSSID_LINUX),  0xff7f, FS_EX2FS,  ROK | FOK},
	{0},
};

#define	IS_BSDFS(pt) ((pt)->fstype == FS_BSDFFS || (pt)->fstype == FS_BSDLFS)

/*******************************************************************
 * EXPORT FUNCS
 *******************************************************************/
u_int
check_dospart(dosp)
	struct dos_check_args *dosp;
{

	/* initialize the dos check args struct */
	dosp->kind = NODOSPART;
	dosp->fake = 0;
	dosp->cyl = 0;
	dosp->ssec = 0;

	/* I: check a pc98 dos partition table */
	if (find_pc98_dospart(dosp, 0) > 0)
		return dosp->ssec;

	/* II: check a IBM dos partition table */
	if (find_at_dospart(dosp, 0) > 0)
		return dosp->ssec;

	/* Nothing found! */
#ifndef	COMPAT_DISK_GEOMETRY
	/* !STRATEGY CHANGE!
	 * If AT or PC-98 dos partitions not found,
	 * we employ the disk gemetry which bios asserts.
	 */
	find_pc98_dospart(dosp, 1);
#endif	/* COMPAT_DISK_GEOMETRY */

	return dosp->ssec;
}

/*******************************************************************
 * Disk type functions
 *******************************************************************/
static struct part_type *
check_pc98_disktype(id)
	sysid_t id;
{
	struct part_type *pt;

	for (pt = &part_types[0]; pt->sysid != 0; pt ++)
	{
		if ((id & pt->mask) == (pt->sysid & pt->mask) &&
		    (pt->flags & ROK) != 0)
			return pt;
	}
	return NULL;
}

/*******************************************************************
 * Make fake entries
 *******************************************************************/
static struct part_type *
check_fake_entry(lp, dp, force)
	struct disklabel *lp;
	struct dos_partition *dp;
	int force;
{
	int part, dospoff;
	u_int maxpart;
	struct part_type *ptype;

	if ((ptype = check_pc98_disktype(dos_partition_read_sysid(dp))) == NULL)
		return NULL;

	if (force == 0 && (ptype->flags & FOK) == 0)
		return NULL;

	/* already exists ? */
	maxpart = lp->d_npartitions;
	if (maxpart > MAXPARTITIONS)
		maxpart = MAXPARTITIONS;

	dospoff = disk_start_sector(lp, dp);
	for (part = 0; part < maxpart; part++)
	{
		if (lp->d_partitions[part].p_fstype != ptype->fstype)
			continue;
		if (lp->d_partitions[part].p_offset == dospoff)
			return NULL;
	}

	/* ok our target */
	return ptype;
}

u_char *
make_fake_disklabel(olp, dp, msg, force)
	struct disklabel *olp;
	struct dos_partition *dp;
	u_char *msg;
	int force;
{
	int dospoff, i, part, add = 0;
	struct disklabel disktab = *olp;
	struct disklabel *lp = &disktab;
	struct part_type *ptype;

	if (dp == NULL)
		return msg;

	for (part = lp->d_npartitions; part < MAXPARTITIONS; part++)
	{
		lp->d_partitions[part].p_fstype = FS_UNUSED;
		lp->d_partitions[part].p_size = 0;
		lp->d_partitions[part].p_offset = 0;
	}

	for (part = i = 0; i < NDOSPART; i++, dp++)
	{
		if (dos_partition_read_sysid(dp) == 0)
			break;

		/* check a fake entry */
		if ((ptype = check_fake_entry(olp, dp, force)) == NULL)
			continue;

		/* find an empty slot */
		for ( ; part < MAXPARTITIONS; part ++)
			if (lp->d_partitions[part].p_fstype == FS_UNUSED &&
			    part != RAW_PART - 1 && part != RAW_PART)
				break;
		if (part == MAXPARTITIONS)
			break;

		/* register the target */
		dospoff = disk_start_sector(lp, dp);
		lp->d_partitions[part].p_size = disk_size_sector(lp, dp);
		lp->d_partitions[part].p_offset = dospoff;
		lp->d_partitions[part].p_fstype = ptype->fstype;
		lp->d_partitions[part].p_frag = 0;
		lp->d_partitions[part].p_cpg = 0;
		add ++;
	}

	if (add == 0)
		return msg;

	lp->d_npartitions = MAXPARTITIONS;
	lp->d_magic = lp->d_magic2 = DISKMAGIC;
	if (msg)
	{
		lp->d_flags |= D_MISCFAKE;
		strncpy(lp->d_packname, "fake label", 16);
	}
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);

	*olp = *lp;
	return NULL;
}

/*******************************************************************
 * CHECK PC-98 & IBM DOS PARTITIONS
 *******************************************************************/
static int
find_at_dospart(dosp, force)
	struct dos_check_args *dosp;
	int force;
{
	struct disklabel *lp = dosp->lp;
	struct at_dos_partition *dp;
	struct partition *pp;
	int i, nfound = 0;

 	dp = (struct at_dos_partition *) dosp->dp;
	bcopy(dosp->bp->b_data + AT_DOSPARTOFF, dp, AT_NDOSPART * sizeof(*dp));

	/* (I)
	 * Are there NetBSD or AT MSDOS entries ?
	 */
	for (i = 0; i < AT_NDOSPART; i++, dp++)
	{
		if (dp->dp_size == 0)
			continue;
		if (is_dos_partition_at_msdos(dp) != 0 ||
		    is_dos_partition_at_bsd(dp) != 0)
			nfound ++;
	}

	if (nfound == 0)
		return nfound;
	dosp->kind = AT_DOSPART;

	/* (II)
	 * Load info
	 */
 	dp = (struct at_dos_partition *) dosp->dp;
	for (i = 0; i < AT_NDOSPART; i++, dp++)
	{
		if (dp->dp_size == 0)
			continue;

		/* Install in partition e, f, g, or h. */
		pp = &lp->d_partitions[RAW_PART + 1 + i];
		if (is_dos_partition_at_msdos(dp) != 0)
		{
			pp->p_fstype = FS_MSDOS;
			pp->p_offset = dp->dp_start;
			pp->p_size = dp->dp_size;
			lp->d_npartitions = RAW_PART + 2 + i;
		}

		if (is_dos_partition_at_bsd(dp) != 0 && dosp->ssec == 0)
		{
			dosp->kind = AT_FFSPART;
			dosp->nsecs = dp->dp_size;
			dosp->ssec = dp->dp_start;
			dosp->cyl = at_dpcyl(dp->dp_scyl, dp->dp_ssect);
		}
	}

	return nfound;
}

static int
find_pc98_dospart(dosp, force)
	struct dos_check_args *dosp;
	int force;
{
	struct disklabel *lp = dosp->lp;
	struct dos_partition *dp;
	int i, size, nfound = 0;
	sysid_t id;

	/* (I)
	 * Are there NetBSD or PC98 msdos entries ?
	 */
	dp = dosp->dp;
	size = PC98_NDOSPART * sizeof(*dp);
	bcopy(dosp->bp->b_data + PC98_DOSPARTOFF, dp, size);
	for (i = 0; i < NDOSPART; i++, dp++)
	{
		id = dos_partition_pc98_read_sysid(dp);
		if (id == (sysid_t) 0)
			break;
		if (check_pc98_disktype(id) != NULL)
			nfound ++;
	}

	if (nfound == 0 && force == 0)
		return nfound;

	/* (II)
	 * Yes. Update the geometry.
	 */
	if (dosp->flag & GEO_CHG_EN)
		change_disk_geometry(dosp);

	if (nfound == 0)
		return nfound;

	dosp->kind = PC98_DOSPART;

	/* (III)
	 * Load info
	 */
	dp = dosp->dp;
	for (i = 0; i < PC98_NDOSPART; i++, dp++)
	{
		struct part_type *ptype;
		sysid_t id;

		if ((id = dos_partition_pc98_read_sysid(dp)) == 0)
			break;
		if ((ptype = check_pc98_disktype(id)) == NULL)
			continue;

		if (ptype->flags & FOK)
			dosp->fake ++;

		if (IS_BSDFS(ptype) != 0 && dosp->ssec == 0)
		{
			dosp->kind = PC98_FFSPART;
			dosp->ssec= disk_start_sector(lp, dp);
			dosp->nsecs = disk_size_sector(lp, dp);
			dosp->cyl = pc98_dpcyl(dp->dp_scyl, dp->dp_ssect);
		}
	}

	return nfound;
}

/*******************************************************************
 * GEOMETRY SCANNING (MAGIC)
 *******************************************************************/
#include <i386/Cbus/dev/magicvar.h>

int cfd_idx;
struct config_device_geometry cfd_data[CFD_NGEOMETRIES + 1];

int
scan_geo_by_config(name, tracks, sectors)
	u_char *name;
	u_int *tracks, *sectors;
{
	struct config_device_geometry *cfdp;
	int no;

	for (cfdp = cfd_data, no = 0; no < CFD_NGEOMETRIES; cfdp ++, no ++)
	{
		if (cfdp->cfd_name[0] == 0)
			continue;

		if(strncmp(name, cfdp->cfd_name, CFD_NAMELEN) == 0 &&
		   cfdp->cfd_tracks != 0 && cfdp->cfd_sectors != 0)
		{
			*tracks = cfdp->cfd_tracks;
			*sectors = cfdp->cfd_sectors;
			return 0;
		}
	}

	return ENOENT;
}
			
/*******************************************************************
 * SCSI GEOMETRY SCANNING
 *******************************************************************/
#include "sd.h"
#if	NSD_ATAPIBUS > 0 || NSD_SCSIBUS > 0
#include <sys/disk.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsiconf.h>
#include <i386/conf/sdgsets.h>
#include <dev/scsipi/sdvar.h>

#define	b_cylin	b_resid
#define	OS_ANY	0

#if	NSD_SCSIBUS > 0
#ifdef	SCSIBIOS_SCSIBUS_ID
int scsibios_scsibus_id = SCSIBIOS_SCSIBUS_ID;
#else	/* !SCSIBIOS_SCSIBUS_ID */
int scsibios_scsibus_id = 0;
#endif	/* !SCSIBIOS_SCSIBUS_ID */

static struct sd_softc *snoop_sd_softc __P((int));
static int snoop_sd_dvcfg __P((int, u_int *));
static int snoop_scsibus_info __P((int, u_int *, u_int *, u_int *));

static int scsi_geo_match __P((struct dos_check_args *, u_int, u_int, int));
static int scan_geo_by_ur __P((struct dos_check_args *, u_int *, u_int *));
static int scan_geo_by_tl __P((struct dos_check_args *, u_int, u_int *, u_int *));
static u_int round_up __P((u_int));
void sdstrategy __P((struct buf *));

static u_char sdsymbol[4] = {'s', 'd', '0', 0};

static struct sd_softc *
snoop_sd_softc(unit)
	int unit;
{
	extern struct cfdriver sd_cd;

	if (unit >= sd_cd.cd_ndevs)
		return NULL;

	return ((struct sd_softc *) (sd_cd.cd_devs[unit]));
}

static int
snoop_scsibus_info(unit, busid, id, lun) 
	int unit;
	u_int *busid, *id, *lun;
{
	struct sd_softc *sc;
	struct scsipi_link *sl;

	if ((sc = snoop_sd_softc(unit)) == NULL)
		return ENOENT;

	if ((sl = sc->sc_link) == NULL)
		return ENOENT;	

	*busid = (u_int) sl->scsipi_scsi.scsibus;
	*id = (u_int) sl->scsipi_scsi.target;
	*lun = (u_int) sl->scsipi_scsi.lun;
	return 0;
}

static int
snoop_sd_dvcfg(unit, dvcfg)
	int unit;
	u_int *dvcfg;
{
	struct sd_softc *sc;

	*dvcfg = 0;
	if ((sc = snoop_sd_softc(unit)) == NULL)
		return ENOENT;

	if (sc->sc_dev.dv_cfdata == NULL)
		return ENOENT;

	*dvcfg = sc->sc_dev.dv_cfdata->cf_flags;
	return 0;
}

static u_int
round_up(val)
	u_int val;
{
	u_int eval;

	for(eval = 1; val < eval || val >= eval * 2; eval <<= 1)
		;

	return eval;
}

static int
scsi_geo_match(dosp, tracks, sectors, ostype)
	struct dos_check_args *dosp;
	u_int tracks, sectors;
	int ostype;
{
	struct disklabel *lp = dosp->lp;
	struct disklabel slp;
	struct buf *tbp;
	u_char *buf;
	u_int total;
	int error = EINVAL;

	slp = *lp;

	total = lp->d_secperunit;
	lp->d_ntracks = tracks;
	lp->d_nsectors = sectors;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	lp->d_ncylinders = total / lp->d_secpercyl;
	lp->d_secperunit = lp->d_ncylinders * lp->d_secpercyl;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);

	tbp = geteblk((int)lp->d_secsize);
	tbp->b_dev = dosp->bp->b_dev;
	tbp->b_bcount = lp->d_secsize;
	tbp->b_blkno = lp->d_secpercyl;
	tbp->b_flags = B_BUSY | B_READ;
	tbp->b_cylin = 1;
	(*dosp->start)(tbp);
	if (biowait(tbp))
		goto done;

	buf = tbp->b_data;

	/****** XXX ******/
	if (buf[0] == 0xeb)
		error = 0;
	/****** XXX ******/

done:
	tbp->b_flags |= B_INVAL;
	brelse(tbp);
	*lp = slp;
	return error;
}

static int
scan_geo_by_tl(dosp, dvcfg, track, sector)
	struct dos_check_args *dosp;
	u_int dvcfg;
	u_int *track, *sector;
{
	struct disklabel *lp = dosp->lp;
	struct sdg_conv_element *scep;
	struct sdg_conv_table *sctp;
	int no;

	dvcfg = (DVCFG_MINOR(dvcfg) >> 12) & 0x0f;
	if (dvcfg >= sizeof(sdg_conv_table_array) / sizeof(struct sdg_conv_table *))
		return ENOENT;

	sctp = sdg_conv_table_array[dvcfg];
	if (sctp == NULL || sctp->sct_sz == 0)
		return ENOENT;

	scep = sctp->sct_pt;
	for (no = 0; no < sctp->sct_sz; no ++, scep ++)
	{
		if (autoscan_debug)
			printf("scan %d %d\n",
				(u_int32_t) scep->sce_org.sdg_tracks,
				(u_int32_t) scep->sce_org.sdg_sectors);

		if ((u_int32_t) scep->sce_org.sdg_tracks == lp->d_ntracks &&
		    (u_int32_t) scep->sce_org.sdg_sectors == lp->d_nsectors)
		{
			if (autoscan_debug)
				printf("match %d %d\n",
					(u_int32_t) scep->sce_mod.sdg_tracks,
					(u_int32_t) scep->sce_mod.sdg_sectors);
			break;
		}
	}

	if (no >= sctp->sct_sz)
		return ENOENT;

	*track = (u_int32_t) scep->sce_mod.sdg_tracks;
	*sector = (u_int32_t) scep->sce_mod.sdg_sectors;

	return 0;
}

int
scan_geo_by_ur(dosp, track, sector)
	struct dos_check_args *dosp;
	u_int *track, *sector;
{
	u_int etrack, esector;
	struct disklabel *lp = dosp->lp;
	int error;

	/* (A) original geo (55 NEC type) */
	etrack = lp->d_ntracks;
	esector = lp->d_nsectors;
	if ((error = scsi_geo_match(dosp, etrack, esector, OS_ANY)) == 0)
		goto match;

	/* (B) 92 fixed */
	etrack = 8;
	esector = 32;
	if ((error = scsi_geo_match(dosp, etrack, esector, OS_ANY)) == 0)
		goto match;

	esector = 128;
	if ((error = scsi_geo_match(dosp, etrack, esector, OS_ANY)) == 0)
		goto match;

	/* (C) round up */
	etrack = lp->d_ntracks;
	esector = round_up(lp->d_nsectors);
	if ((error = scsi_geo_match(dosp, etrack, esector, OS_ANY)) == 0)
		goto match;

match:
	if (error == 0)
	{
		*track = etrack;
		*sector = esector;
	}
	else
	{
		*track = lp->d_ntracks;
		*sector = lp->d_nsectors;
	}

	return error;
}
#endif	/* NSD_SCSIBUS > 0 */
#endif	/* NSD > 0 */

/*******************************************************************
 * IDE DISK GEOMETRY SCANNING
 *******************************************************************/
#include "wd.h"
#if	NWD > 0
static u_char wdsymbol[4] = {'w', 'd', '0', 0};
void wdstrategy __P((struct buf *));
#endif	/* NWD > 0 */

/*******************************************************************
 * GEOMETRY CHANGE
 *******************************************************************/
static void
change_disk_geometry(dosp)
	struct  dos_check_args *dosp;
{
	struct disklabel *lp = dosp->lp;

	switch (lp->d_type)
	{
	default:
		return;

#if	NSD_ATAIBUS > 0
	case DTYPE_ATAPI:
		/* XXX */
		return;
#endif	/* NSD_ATAPIBUS > 0 */

#if	NSD_SCSIBUS > 0
	case DTYPE_SCSI:
		{
		u_int total, cyls, heads, dvcfg, track, sector, busid, id, lun;
		vaddr_t addr;
		int error, unit;
		u_int8_t ebits;

		addr = 0;
 		unit = DISKUNIT(dosp->bp->b_dev);
		if (dosp->start != sdstrategy)
			return;

		snoop_sd_dvcfg(unit, &dvcfg);
		if (autoscan_debug)
			printf("unit %x dvcfg 0x%x track %d sector %d\n",
				unit, dvcfg, lp->d_ntracks, lp->d_nsectors);

		sdsymbol[2] = unit + '0';
		if (scan_geo_by_config(sdsymbol, &track, &sector) == 0 ||
		    scan_geo_by_tl(dosp, dvcfg, &track, &sector) == 0)
		{
			total = lp->d_secperunit;
			lp->d_ntracks = track;
			lp->d_nsectors = sector;
			lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
			lp->d_ncylinders = total / lp->d_secpercyl;
			break;
		}

		error = snoop_scsibus_info(unit, &busid, &id, &lun);
		if (error == 0 && busid == scsibios_scsibus_id &&
		    id < 7 && lun == 0)
		{
			addr = (vaddr_t)(BIOS_SCSI_PARAM + id * 4);
			ebits = lookup_bios_param(BIOS_SCSI_EQ, sizeof(char));
			ebits &= (u_int8_t) (1 << id);
		}
		else
			ebits = 0;

		if (ebits == 0)
		{
			total = lp->d_secperunit;
			scan_geo_by_ur(dosp, &track, &sector);
			lp->d_ntracks = track;
			lp->d_nsectors = sector;
			lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
			lp->d_ncylinders = total / lp->d_secpercyl;
		}
		else
		{
			cyls = lookup_bios_param(addr + 2, sizeof(u_short));
			heads = lookup_bios_param(addr + 1, sizeof(char));

			if (cyls & 0x4000)
			{
				lp->d_ntracks = heads & 0x0f;
				lp->d_ncylinders = (cyls & 0xfff);
				lp->d_ncylinders |= ((heads & 0xf0) << 8);
			}
			else
			{
				lp->d_ntracks = heads;
				lp->d_ncylinders = (cyls & 0xfff);
			}

			lp->d_nsectors = lookup_bios_param(addr, sizeof(char));
			lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
		}

		if (lp->d_secpercyl == 0)
			lp->d_secpercyl = 100;
		break;
		}
#endif	/* NSD > 0 */

#if	NWD > 0
	case DTYPE_ESDI:
	case DTYPE_ST506:
		{
		int unit;
		u_int total, track, sector;

 		unit = DISKUNIT(dosp->bp->b_dev);
		if (dosp->start != wdstrategy)
			return;

		total = lp->d_ncylinders * lp->d_secpercyl;

		wdsymbol[2] = unit + '0';
		if (scan_geo_by_config(wdsymbol, &track, &sector) == 0)
		{
			lp->d_ntracks = track;
			lp->d_nsectors = sector;
		}
		else if (howmany(total, 8 * 17) < (1 << 16))
		{
			lp->d_ntracks =  8;
			lp->d_nsectors = 17;
		}
		else if (lp->d_subtype > 0)
		{
			lp->d_ntracks =  8 * 2;
			lp->d_nsectors = 17;
		}

		lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
		lp->d_ncylinders = total / lp->d_secpercyl;
		break;
		}
#endif	/* NWD > 0 */
	}

	lp->d_secperunit = lp->d_secpercyl * lp->d_ncylinders;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;

	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
}
