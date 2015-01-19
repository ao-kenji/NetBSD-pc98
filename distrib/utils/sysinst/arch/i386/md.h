/*	$NecBSD: md.h,v 1.6.2.1 1999/08/25 23:52:27 honda Exp $	*/
/*	$NetBSD: md.h,v 1.12.2.3 1999/06/24 22:46:23 cgd Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* md.h -- Machine specific definitions for the i386 */


#ifdef	ORIGINAL_CODE
#include <machine/cpu.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif	/* !PC-98 */

#ifdef	ORIGINAL_CODE
/* i386 uses the mbr code. */
#include "mbr.h"
#else	/* PC-98 */
/* PC-98 SHOULD/DOES NOT use the mbr code. */

/* incore fdisk (mbr, bios) geometry */
EXTERN int bcyl, bhead, bsec, bsize, bcylsize;
EXTERN int bstuffset INIT(0);

/* incore copy of  MBR partitions */
enum info {ID,SIZE,START,FLAG,SET};
EXTERN int part[8][5] INIT({{0}});
EXTERN int activepart;
EXTERN int bsdpart;			/* partition in use by NetBSD */
EXTERN int usefull;			/* on install, clobber entire disk */


/* from mbr.c */
void	set_fdisk_geom (void);		/* edit incore BIOS geometry */
void	disp_cur_geom (void);
int	check_geom (void);		/* primitive geometry sanity-check */

int	edit_mbr __P((void));		
int 	partsoverlap (int, int);	/* primive partition sanity-check */

/* from fdisk.c */
void	get_fdisk_info (void);		/* read from disk into core */
void	set_fdisk_info (void);		/* write incore info into disk */
void	disp_cur_part (int, int);

/* scsi_fake communication */
EXTERN int fake_sel;
void	scsi_fake __P((void));
#endif	/* PC-98 */

/* constants and defines */


/* Megs required for a full X installation. */
#define XNEEDMB 50


/*
 *  Default filesets to fetch and install during installation
 *  or upgrade. The standard sets are:
 *      base, etc, comp, games, man, misc, text,
 *      xbase, xfont, xserver, xcontrib, xcomp.
 *
 * i386 has the  MD set kern first, because generic kernels are  too
 * big to fit on install floppies. i386 does not yet include the x sets. 
 *
 * Third entry is the last extension name in the split sets for loading
 * from floppy.
 */
#ifdef	ORIGINAL_CODE
EXTERN distinfo dist_list[]
#ifdef MAIN
= {
    {"kern",	1, "ag", "Kernel       : "},
    {"base",	1, "bw", "Base         : "},
    {"etc",	1, "aa", "System (/etc): "},
    {"comp",	1, "bl", "Compiler     : "},
    {"games",	1, "am", "Games        : "},
    {"man",	1, "ar", "Manuals      : "},
    {"misc",	1, "aj", "Miscellaneous: "},
    {"text",	1, "af", "Text tools   : "},

    /* XXX no X11 on floppies, what sets are they?*/
    {"xbase",	1, "al", "X11 clients  : "},
    {"xfont",	1, "az", "X11 fonts    : "},
    {"xserver",	1, "cr", "X11 servers  : "},
    {"xcontrib",1, "aa", "X11 contrib  : "},
    {"xcomp",	1, "ah", "X programming: "},
    {NULL, 0, NULL, NULL }
}
#endif
;
#else	/* PC-98 */
/* XXX: not yet ajusted "xx" */
EXTERN distinfo dist_list_main[]
#ifdef MAIN
= {
    {"kern",	1, "ag", "Kernel       : "},
    {"base",	1, "bw", "Base         : "},
    {"etc",	1, "aa", "System (/etc): "},
    {"comp",	1, "bl", "Compiler     : "},
    {"games",	1, "am", "Games        : "},
    {"man",	1, "ar", "Manuals      : "},
    {"misc",	1, "aj", "Miscellaneous: "},
    {"text",	1, "af", "Text tools   : "},

    /* XXX no X11 on floppies, what sets are they?*/
#ifdef	X98_INTEGRATED
    {"xbase",	1, "al", "X11 clients  : "},
    {"xfont",	1, "az", "X11 fonts    : "},
    {"xserver",	1, "cn", "X11 servers  : "},
    {"xcontrib",1, "aa", "X11 contrib  : "},
    {"xcomp",	1, "ah", "X programming: "},
#else	/* X98_INTEGRATED */
    {"xbase",	0, "al", "X11 clients  : "},
    {"xfont",	0, "az", "X11 fonts    : "},
    {"xserver",	0, "cr", "X11 servers  : "},
    {"xcontrib",0, "aa", "X11 contrib  : "},
    {"xcomp",	0, "ah", "X programming: "},
#endif	/* X98_INTEGRATED */
    {NULL, 0, NULL, NULL }
}
#endif
;

EXTERN distinfo dist_list_sub[]
#ifdef MAIN
= {
    {"kern98",	1, "ag", "(PC-98)Kernel   : "},
    {"base98",	1, "bw", "(PC-98)Base     : "},
    {"etc98",	1, "aa", "(PC-98)System   : "},
    {"comp98",	1, "bl", "(PC-98)Compiler : "},
    {"games98",	1, "am", "(PC-98)Games    : "},
    {"man98",	1, "ar", "(PC-98)Manuals  : "},
    {"misc98",	1, "aj", "(PC-98)Misc     : "},
    {"text98",	1, "af", "(PC-98)Text     : "},

    /* XXX no X11 on floppies, what sets are they?*/
#ifdef	X98_INTEGRATED
    {"xbase98",	1, "al",   "(PC-98)X11 clients: "},
    {"xfont98",	1, "az",   "(PC-98)X11 fonts  : "},
    {"xserv98", 1, "cr",   "(PC-98)X11 servers: "},
    {"xctrb98", 1, "aa",   "(PC-98)X11 contrib: "},
    {"xcomp98",	1, "ah",   "(PC-98)X11 program: "},
#else	/* X98_INTEGRATED */
    {"xbase98",	0, "al",   "(PC-98)X11 clients: "},
    {"xfont98",	0, "az",   "(PC-98)X11 fonts  : "},
    {"xserv98", 0, "cr",   "(PC-98)X11 servers: "},
    {"xctrb98", 0, "aa",   "(PC-98)X11 contrib: "},
    {"xcomp98",	0, "ah",   "(PC-98)X11 program: "},
#endif	/* X98_INTEGRATED */
    {NULL, 0, NULL, NULL }
}
#endif
;

#ifdef MAIN
distinfo *dist_list = dist_list_main;
#endif
EXTERN distinfo *dist_list;
#endif	/* PC-98 */

/*
 * Disk names accepted as valid targets for a from-scratch installation.
 *
 * On  i386, we allow "wd"  ST-506/IDE disks and  "sd" scsi disks.
 */
EXTERN	char *disk_names[]
#ifdef MAIN
= {"wd", "sd", NULL}
#endif
;


/*
 * Legal start character for a disk for checking input. 
 * this must return 1 for a character that matches the first
 * characters of each member of disk_names.
 *
 * On  i386, that means matching 'w' for st-506/ide and 's' for sd.
 */
#define ISDISKSTART(dn)	(dn == 'w' || dn == 's')

/*
 * Machine-specific command to write a new label to a disk.
 * For example, i386  uses "/sbin/disklabel -w -r", just like i386
 * miniroot scripts, though this may leave a bogus incore label.
 * Sun ports should probably use  DISKLABEL_CMD "/sbin/disklabel -w"
 * to get incore  to ondisk inode translation for the Sun proms.
 * If not defined, we assume the port does not support disklabels and
 * hand-edited disklabel will NOT be written by MI code.
 *
 * On i386, do what the 1.2 install scripts did. 
 */
#define DISKLABEL_CMD "disklabel -w -r"


/*
 * Default fileystem type for floppy disks.
 * On i386, that is  msdos.
 */
EXTERN	char *fdtype INIT("msdos");

#ifdef	ORIGINAL_CODE
extern struct disklist *disklist;
extern struct nativedisk_info *nativedisk;
extern struct biosdisk_info *biosdisk;

#define _PATH_MBR	"/usr/mdec/mbr"
#define _PATH_BOOTSEL	"/usr/mdec/mbr_bootsel"

struct mbr_bootsel {
	u_int8_t defkey;
	u_int8_t flags;
	u_int16_t timeo;
	char nametab[4][9];
	u_int16_t magic;
} __attribute__((packed));
 
extern struct mbr_bootsel *mbs;
 
#define BFL_SELACTIVE   0x01
#define BFL_EXTINT13    0x02
 
#define SCAN_ENTER      0x1c
#define SCAN_F1         0x3b
 
#define MBR_BOOTSELOFF  (MBR_PARTOFF - sizeof (struct mbr_bootsel))

extern int defbootselpart, defbootseldisk;

void disp_bootsel __P((struct mbr_partition *, struct mbr_bootsel *));
#endif	/* !PC-98 */

/*
 *  prototypes for MD code.
 */

#ifndef	ORIGINAL_CODE
void md_overwrite_sets __P((void));
void md_update_boot __P((void));
#endif	/* PC-98 */

