/*	$NecBSD: disklabel.h,v 3.28 1999/07/23 20:47:03 honda Exp $	*/
/*	$NetBSD: disklabel.h,v 1.4.14.3 1998/11/23 07:49:45 cgd Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _MACHINE_DISKLABEL_H_
#define _MACHINE_DISKLABEL_H_
#define	LABELSECTOR	1		/* sector containing label */
#define	LABELOFFSET	0		/* offset of label in sector */
#define	MAXPARTITIONS	8		/* number of partitions */
#define	RAW_PART	3		/* raw partition: XX?d (XXX) */

/* DOS partition table -- located in boot block */
#define	DOSBBSECTOR	0		/* DOS boot block relative sector # */

#ifdef	ORIGINAL_CODE
#define	DOSPARTOFF	446
#define	NDOSPART	4
#include <sys/disklabel_mbr.h>

#else	/* PC-98 */
#define	AT_DOSPARTOFF	446
#define	AT_PARTOFF	512
#define	AT_DOSLSSZ	(DEV_BSIZE)
#define	AT_NDOSPART	4

#define	PC98_DOSPARTOFF	512
#define	PC98_DOSLSSZ	(DEV_BSIZE * 2)
#define	PC98_NDOSPART	8

#define	DOSPARTOFF	PC98_DOSPARTOFF
#define DOSLSSZ		PC98_DOSLSSZ
#define	NDOSPART	PC98_NDOSPART
#endif	/* PC-98 */

#ifndef __ASSEMBLER__
#ifdef	ORIGINAL_CODE
struct dos_partition {
	unsigned char	dp_flag;	/* bootstrap flags */
	unsigned char	dp_shd;		/* starting head */
	unsigned char	dp_ssect;	/* starting sector */
	unsigned char	dp_scyl;	/* starting cylinder */
	unsigned char	dp_typ;		/* partition type (see below) */
	unsigned char	dp_ehd;		/* end head */
	unsigned char	dp_esect;	/* end sector */
	unsigned char	dp_ecyl;	/* end cylinder */
	unsigned long	dp_start;	/* absolute starting sector number */
	unsigned long	dp_size;	/* partition size in sectors */
};
#else	/* PC-98 */
/* AT dos partition */
struct at_dos_partition {
	u_int8_t	dp_flag;	/* bootstrap flags */
	u_int8_t	dp_shd;		/* starting head */
	u_int8_t	dp_ssect;	/* starting sector */
	u_int8_t	dp_scyl;	/* starting cylinder */
	u_int8_t	dp_typ;		/* partition type (see below) */
	u_int8_t	dp_ehd;		/* end head */
	u_int8_t	dp_esect;	/* end sector */
	u_int8_t	dp_ecyl;	/* end cylinder */
	u_int32_t	dp_start;	/* absolute starting sector number */
	u_int32_t	dp_size;	/* partition size in sectors */
} __attribute__((packed));

/* PC-98 dos partition */
struct pc98_dos_partition {
	union {
		u_int16_t did_sysid;
		struct dos_partition_diskid {
			u_int8_t	diskid_mid;
			u_int8_t	diskid_sid;
		} __attribute__((packed)) did_diskid;
	} __attribute__((packed)) dp_did;
	u_int8_t	dp_dum1;
	u_int8_t	dp_dum2;
	u_int8_t	dp_ipl_sct;
	u_int8_t	dp_ipl_head;
	u_int16_t	dp_ipl_cyl;
	u_int8_t	dp_ssect;	/* starting sector */
	u_int8_t	dp_shd;		/* starting head */
	u_int16_t	dp_scyl;	/* starting cylinder */
	u_int8_t	dp_esect;	/* end sector */
	u_int8_t	dp_ehd;		/* end head */
	u_int16_t	dp_ecyl;	/* end cylinder */
	u_int8_t	dp_name[16];
} __attribute__((packed));

/* abstract sysid_t type */
typedef	u_long	sysid_t;

#define	dp_mid	dp_did.did_diskid.diskid_mid
#define dp_sid	dp_did.did_diskid.diskid_sid
#define	dp_sysid  dp_did.did_sysid

/* XXX */
#define	MBR_BBSECTOR	DOSBBSECTOR	/* MBR relative sector # */
#define	MBR_PARTOFF	DOSPARTOFF	/* offset of MBR partition table */
#define	NMBRPART	NDOSPART
#define	mbr_partition 	pc98_dos_partition
#define	dos_partition 	pc98_dos_partition
#endif	/* PC-98 */
#endif

/* Known DOS partition types. */
#ifdef	ORIGINAL_CODE
#define DOSPTYP_NETBSD	0xa9		/* NetBSD partition type */
#define	DOSPTYP_386BSD	0xa5		/* 386BSD partition type */
#define DOSPTYP_FAT12	0x01		/* 12-bit FAT */
#define DOSPTYP_FAT16S	0x04		/* 16-bit FAT, less than 32M */
#define DOSPTYP_FAT16B	0x06		/* 16-bit FAT, more than 32M */
#define DOSPTYP_FAT32	0x0b		/* 32-bit FAT */
#define DOSPTYP_FAT32L	0x0c		/* 32-bit FAT, LBA-mapped */
#define DOSPTYP_FAT16L	0x0e		/* 16-bit FAT, LBA-mapped */
#define DOSPTYP_LNXEXT2	0x83		/* Linux native */
#endif	/* !PC-98 */

#ifndef __ASSEMBLER__
#include <sys/dkbad.h>
struct cpu_disklabel {
	struct dos_partition dosparts[NDOSPART];
	struct dkbad bad;
};

/* Isolate the relevant bits to get sector and cylinder. */
#ifdef	ORIGINAL_CODE
#define	DPSECT(s)	((s) & 0x3f)
#define	DPCYL(c, s)	((c) + (((s) & 0xc0) << 2))
#else	/* PC-98 */

/* sysid <=> diskid conversion (pc98 only) */
#define	_DISKID_IDMASK	0x7f
#define	_DISKID_ACTIVE	0x80
#define	_DISKID_MASK	0xff
#define	_DISKID_IDSHIFT NBBY
#define	_SYSID_ACTIVE	(_DISKID_ACTIVE << _DISKID_IDSHIFT)

#define	sysid_make_id(mid, sid) \
	(((((u_long)(sid)) | _DISKID_ACTIVE) << _DISKID_IDSHIFT) | (mid))
#define	sysid_to_sid(id) (((id) >> _DISKID_IDSHIFT) & _DISKID_MASK)
#define	sysid_to_mid(id) ((id) & _DISKID_MASK)

/* read/write sysid */
#define	dos_partition_pc98_read_sysid(dp)      ((sysid_t) ((dp)->dp_sysid))
#define	dos_partition_pc98_write_sysid(dp, id) ((dp)->dp_sysid = (u_int16_t) (id))
#define	dos_partition_at_read_sysid(dp)		((sysid_t) ((dp)->dp_typ))
#define	dos_partition_at_write_sysid(dp, id)	((dp)->dp_typ = (u_int8_t) (id))

/* active or non active (pc98 only) */
#define	is_dos_partition_active(dp)	((dp)->dp_sysid & _SYSID_ACTIVE)
#define	dos_partition_activate(dp)	((dp)->dp_sysid |= _SYSID_ACTIVE)
#define	dos_partition_deactivate(dp)	((dp)->dp_sysid &= ~_SYSID_ACTIVE)

#define	is_sysid_active(id)		((id) & _SYSID_ACTIVE)
#define	sysid_activate(id)		((id) | _SYSID_ACTIVE)
#define	sysid_deactivate(id)		((id) & ~_SYSID_ACTIVE)

/* sysid definition */
/* PC-98 */
#define	DOSMID_386BSD	(0x14|0x80)	/* 386bsd|bootable */
#define	DOSSID_386BSD	(0x44|0x80)	/* 386bsd|active */
#define	DOSMID_NETBSD	(0x14|0x80)	/* NetBSD|bootable */
#define	DOSSID_NETBSD	(0x44|0x80)	/* NetBSD|active */
#define	DOSMID_MSDOS	(0x20|0x80)	/* MSDOS|bootable (0) */
#define	DOSSID_MSDOS	(0x21|0x80)	/* MSDOS|active (0) */

#define	SYSID_PC98_MSDOS	sysid_make_id(DOSMID_MSDOS, DOSSID_MSDOS)
#define	SYSID_PC98_386BSD	sysid_make_id(DOSMID_NETBSD, DOSSID_NETBSD)
#define	SYSID_PC98_NETBSD	sysid_make_id(DOSMID_NETBSD, DOSSID_NETBSD)
#define	DOSPTYP_PC98_386BSD	SYSID_PC98_386BSD
#define DOSPTYP_PC98_NETBSD	SYSID_PC98_NETBSD

/* AT */
#define DOSPTYP_FAT12	0x01		/* 12-bit FAT */
#define DOSPTYP_FAT16S	0x04		/* 16-bit FAT, less than 32M */
#define DOSPTYP_FAT16B	0x06		/* 16-bit FAT, more than 32M */
#define DOSPTYP_FAT32	0x0b		/* 32-bit FAT */
#define DOSPTYP_FAT32L	0x0c		/* 32-bit FAT, LBA-mapped */
#define DOSPTYP_FAT16L	0x0e		/* 16-bit FAT, LBA-mapped */
#define DOSPTYP_LNXEXT2	0x83		/* Linux native */

#define	SYSID_AT_386BSD		0xa5
#define	SYSID_AT_NETBSD		0xa9
#define	DOSPTYP_AT_386BSD	SYSID_AT_386BSD
#define DOSPTYP_AT_NETBSD	SYSID_AT_NETBSD

/* check sysid inline funcs */
static __inline int is_sysid_at_bsd __P((sysid_t));
static __inline int is_sysid_at_msdos __P((sysid_t));
static __inline int is_sysid_pc98_bsd __P((sysid_t));
static __inline int is_sysid_pc98_msdos __P((sysid_t));

static __inline int
is_sysid_at_bsd(id)
	sysid_t id;
{

	return (id == SYSID_AT_NETBSD || id == SYSID_AT_386BSD);
}

static __inline int
is_sysid_at_msdos(id)
	sysid_t id;
{
	return (id == DOSPTYP_FAT12 || id == DOSPTYP_FAT16S ||
	        id == DOSPTYP_FAT16B || id == DOSPTYP_FAT16L ||
		id == DOSPTYP_FAT32 || id == DOSPTYP_FAT32L);
}

static __inline int
is_sysid_pc98_bsd(id)
	sysid_t id;
{
	u_long mid = sysid_to_mid(id);
	u_long sid = sysid_to_sid(id);

	return ((mid | 0x80) == DOSMID_NETBSD && sid == DOSSID_NETBSD);
}

static __inline int
is_sysid_pc98_msdos(id)
	sysid_t id;
{
	u_long mid = sysid_to_mid(id);
	u_long sid = sysid_to_sid(id);

	return ((((mid & 0xf0) | 0x80) == DOSMID_MSDOS) &&
		((sid & 0x80) && (sid & 0x0f) == (DOSSID_MSDOS & 0x0f)));
}

#define	is_dos_partition_at_bsd(dp) 	\
	is_sysid_at_bsd(dos_partition_at_read_sysid((dp)))
#define	is_dos_partition_at_msdos(dp)	\
	is_sysid_at_msdos(dos_partition_at_read_sysid((dp)))
#define	is_dos_partition_pc98_bsd(dp) 	\
	is_sysid_pc98_bsd(dos_partition_pc98_read_sysid((dp)))
#define	is_dos_partition_pc98_msdos(dp)	\
	is_sysid_pc98_msdos(dos_partition_pc98_read_sysid((dp)))

/* cylinder/header/sector operation */
#define	at_dpsect(s)	((s) & 0x3f)
#define	at_dpcyl(c, s)	((c) + (((s) & 0xc0) << 2))
#define	pc98_dpsect(s)	 (s)
#define	pc98_dpcyl(c, s) (c)

/* 
 * SUB-MACHINE default setup 
 * In the future, these macros below are automatically controlled (maybe!).
 */
/* sysid */
#define	SYSID_386BSD		SYSID_PC98_386BSD
#define	SYSID_NETBSD		SYSID_PC98_NETBSD
#define	SYSID_MSDOS		SYSID_PC98_MSDOS
#define	DOSPTYP_386BSD		SYSID_PC98_386BSD
#define DOSPTYP_NETBSD		SYSID_PC98_NETBSD

/* sysid read/write */
#define	dos_partition_read_sysid(dp)      dos_partition_pc98_read_sysid((dp))
#define	dos_partition_write_sysid(dp, id) dos_partition_pc98_write_sysid((dp), (id))

/* sysid check */
#define	is_sysid_bsd(id) 	is_sysid_pc98_bsd((id))
#define	is_sysid_msdos(id)	is_sysid_pc98_msdos((id))

/* disk type check */
#define	is_dos_partition_bsd(dp) is_dos_partition_pc98_bsd((dp))
#define	is_dos_partition_msdos(dp) is_dos_partition_pc98_msdos((dp))

/* disk cyl/sec macro */
#define	DPSECT(s)		pc98_dpsect((s))
#define	DPCYL(c, s)		pc98_dpcyl((c), (s))
#endif	/* PC-98 */
#endif

#ifdef _KERNEL
struct disklabel;
int	bounds_check_with_label __P((struct buf *, struct disklabel *, int));
#endif

#endif /* _MACHINE_DISKLABEL_H_ */
