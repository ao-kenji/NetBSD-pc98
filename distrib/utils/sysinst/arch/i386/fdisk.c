/*	$NecBSD: fdisk.c,v 1.12 1999/08/07 11:12:38 kmatsuda Exp $	*/
/*	$NetBSD: fdisk.c,v 1.2.2.5 1998/11/16 07:25:36 cgd Exp $	*/

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
 *      This product includes software develooped for the NetBSD Project by
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

/* fdisk.c -- routines to deal with /sbin/fdisk ... */

#include <stdio.h>
#include "defs.h"
#include "md.h"
#include "txtwalk.h"
#include "msg_defs.h"
#include "menu_defs.h"

struct lookfor fdiskbuf[] = {
	{"DLCYL", "DLCYL=%d", "a $0", &dlcyl, NULL},
	{"DLHEAD", "DLHEAD=%d", "a $0", &dlhead, NULL},
	{"DLSEC", "DLSEC=%d", "a $0", &dlsec, NULL},
	{"BCYL", "BCYL=%d", "a $0", &bcyl, NULL},
	{"BHEAD", "BHEAD=%d", "a $0", &bhead, NULL},
	{"BSEC", "BSEC=%d", "a $0", &bsec, NULL},
	{"PART0ID", "PART0ID=%d", "a $0", &part[0][ID], NULL},
	{"PART0SIZE", "PART0SIZE=%d", "a $0", &part[0][SIZE], NULL},
	{"PART0START", "PART0START=%d", "a $0", &part[0][START], NULL},
	{"PART0FLAG", "PART0FLAG=0x%d", "a $0", &part[0][FLAG], NULL},
	{"PART1ID", "PART1ID=%d", "a $0", &part[1][ID], NULL},
	{"PART1SIZE", "PART1SIZE=%d", "a $0", &part[1][SIZE], NULL},
	{"PART1START", "PART1START=%d", "a $0", &part[1][START], NULL},
	{"PART1FLAG", "PART1FLAG=0x%d", "a $0", &part[1][FLAG], NULL},
	{"PART2ID", "PART2ID=%d", "a $0", &part[2][ID], NULL},
	{"PART2SIZE", "PART2SIZE=%d", "a $0", &part[2][SIZE], NULL},
	{"PART2START", "PART2START=%d", "a $0", &part[2][START], NULL},
	{"PART2FLAG", "PART2FLAG=0x%d", "a $0", &part[2][FLAG], NULL},
	{"PART3ID", "PART3ID=%d", "a $0", &part[3][ID], NULL},
	{"PART3SIZE", "PART3SIZE=%d", "a $0", &part[3][SIZE], NULL},
	{"PART3START", "PART3START=%d", "a $0", &part[3][START], NULL},
#ifdef	ORIGINAL_CODE
	{"PART3FLAG", "PART3FLAG=0x%d", "a $0", &part[3][FLAG], NULL}
#else	/* PC-98 */
	{"PART3FLAG", "PART3FLAG=0x%d", "a $0", &part[3][FLAG], NULL},
	{"PART4ID", "PART4ID=%d", "a $0", &part[4][ID], NULL},
	{"PART4SIZE", "PART4SIZE=%d", "a $0", &part[4][SIZE], NULL},
	{"PART4START", "PART4START=%d", "a $0", &part[4][START], NULL},
	{"PART4FLAG", "PART4FLAG=0x%d", "a $0", &part[4][FLAG], NULL},
	{"PART5ID", "PART5ID=%d", "a $0", &part[5][ID], NULL},
	{"PART5SIZE", "PART5SIZE=%d", "a $0", &part[5][SIZE], NULL},
	{"PART5START", "PART5START=%d", "a $0", &part[5][START], NULL},
	{"PART5FLAG", "PART5FLAG=0x%d", "a $0", &part[5][FLAG], NULL},
	{"PART6ID", "PART6ID=%d", "a $0", &part[6][ID], NULL},
	{"PART6SIZE", "PART6SIZE=%d", "a $0", &part[6][SIZE], NULL},
	{"PART6START", "PART6START=%d", "a $0", &part[6][START], NULL},
	{"PART6FLAG", "PART6FLAG=0x%d", "a $0", &part[6][FLAG], NULL},
	{"PART7ID", "PART7ID=%d", "a $0", &part[7][ID], NULL},
	{"PART7SIZE", "PART7SIZE=%d", "a $0", &part[7][SIZE], NULL},
	{"PART7START", "PART7START=%d", "a $0", &part[7][START], NULL},
	{"PART7FLAG", "PART7FLAG=0x%d", "a $0", &part[7][FLAG], NULL}
#endif	/* PC-98 */
};

int numfdiskbuf = sizeof(fdiskbuf) / sizeof(struct lookfor);



/*
 * Fetch current MBR from disk into core by parsing /sbin/fdisk output.
 */
void get_fdisk_info(void)
{
	char *textbuf;
	int   textsize;
#ifdef	ORIGINAL_CODE
	int   t1, t2;
#endif	/* !PC-98 */

	/* Get Fdisk information */
	textsize = collect(T_OUTPUT, &textbuf,
			    "/sbin/fdisk -S /dev/r%sd 2>/dev/null", diskdev);
	if (textsize < 0) {
		endwin();
		(void) fprintf(stderr, "Could not run fdisk.");
		exit(1);
	}
	walk(textbuf, textsize, fdiskbuf, numfdiskbuf);
	free(textbuf);

	/* A common failure of fdisk is to get the number of cylinders
	   wrong and the number of sectors and heads right.  This makes
	   a disk look very big.  In this case, we can just recompute
	   the number of cylinders and things should work just fine.
	   Also, fdisk may correctly indentify the settings to include
	   a cylinder total > 1024, because translation mode is not used.
	   Check for it. */

#ifdef	ORIGINAL_CODE
	if (bcyl > 1024 && disk->dd_head == bhead && disk->dd_sec == bsec)
		bcyl = 1024;
	else if (bcyl > 1024 && bsec < 64) {
		t1 = disk->dd_cyl * disk->dd_head * disk->dd_sec;
		t2 = bhead * bsec;
		if (bcyl * t2 > t1) {
			t2 = t1 / t2;
			if (t2 < 1024)
				bcyl = t2;
		}
	}
#endif	/* !PC-98 */
}

/*
 * Write incore MBR geometry and partition info to disk
 * using  /sbin/fdisk.
 */
void set_fdisk_info(void)
{
	int i;

#ifdef	ORIGINAL_CODE
	for (i=0; i<4; i++)
#else	/* PC-98 */
	for (i=0; i<NDOSPART; i++)
#endif	/* PC-98 */
		if (part[i][SET])
                        run_prog(0, 0, NULL, "/sbin/fdisk -u -f -%d -b %d/%d/%d "
                                 "-s %d/%d/%d /dev/r%sd",
                                 i, bcyl, bhead, bsec,
                                 part[i][ID], part[i][START],
                                 part[i][SIZE],  diskdev);

#ifdef	ORIGINAL_CODE
	if (activepart >= 0)
		run_prog(0, 0, NULL, "/sbin/fdisk -a -%d -f /dev/r%s",
			  activepart, diskdev);
#endif	/* !PC-98 */

}

struct part_id {
	int id;
	char *name;
} part_ids[] = {
	{0, "unused"},
	{1, "Primary DOS, 12 bit FAT"},
#ifdef	ORIGINAL_CODE
	{4, "Primary DOS, 16 bit FAT <32M"},
#else	/* PC-98 */
	{4, "PC-UX"},
#endif	/* PC-98 */
	{5, "Extended DOS"},
	{6, "Primary DOS, 16-bit FAT >32MB"},
	{7, "NTFS"},
#ifndef	ORIGINAL_CODE
	{17, "MSDOS"},
	{20, "NetBSD/FreeBSD/386bsd"},
	{32, "MSDOS"},
	{33, "MSDOS"},
	{34, "MSDOS"},
	{35, "MSDOS"},
#endif	/* PC-98 */
#ifdef	ORIGINAL_CODE
	{131, "Linux native"},
	{131, "Linux swap"},
#endif	/* !PC-98 */
	{165, "NetBSD/FreeBSD/386bsd"},
	{169, "new NetBSD"},
	{-1, "Unknown"},
};

/*
 * Define symbolic name for MBR IDs if undefined, for backwards
 * source compatibility.
 */
#ifdef	ORIGINAL_CODE
#ifndef DOSPTYP_NETBSD
#define DOSPTYP_NETBSD	0xa9
#endif
#ifndef DOSPTYP_386BSD
#define DOSPTYP_386BSD	0xa5
#endif
#endif	/* !PC-98 */

/*
 * Which MBR partition ID to use for a newly-created NetBSD partition.
 * Left patchable as we're in the middle of changing partition IDs.
 */
#ifdef notyet
#define DOSPTYP_OURS	DOSPTYP_NETBSD	/* NetBSD >= 1.3D */
#else
#define DOSPTYP_OURS	DOSPTYP_386BSD	/* 386bsd, FreeBSD, old NetBSD */
#endif

#ifdef	ORIGINAL_CODE
int dosptyp_nbsd = DOSPTYP_OURS;	
#else	/* PC-98 */
sysid_t dosptyp_nbsd = DOSPTYP_OURS;	
#endif	/* PC-98 */

/* prototypes */
int otherpart(int id);
int ourpart(int id);
int edit_mbr __P((void));



/*
 * First, geometry  stuff...
 */
int check_geom(void)
{
#ifdef	ORIGINAL_CODE
	return bcyl <= 1024 && bsec < 64 && bcyl > 0 && bhead > 0 && bsec > 0;
#else	/* PC-98 */
	return bcyl < 0x10000 && bsec < 0x100 && bcyl > 0 && bhead > 0 && bsec > 0;
#endif	/* PC-98 */
}


/*
 * get C/H/S geometry from user via menu interface and
 * store in globals.
 */
void set_fdisk_geom(void)
{
	char res[80];

	msg_display_add(MSG_setbiosgeom);
	disp_cur_geom();
	msg_printf_add("\n");
	snprintf(res, 80, "%d", bcyl);
	msg_prompt_add(MSG_cylinders, res, res, 80);
	bcyl = atoi(res);

	snprintf(res, 80, "%d", bhead);
	msg_prompt_add(MSG_heads, res, res, 80);
	bhead = atoi(res);

	snprintf(res, 80, "%d", bsec);
	msg_prompt_add(MSG_sectors, res, res, 80);
	bsec = atoi(res);
}


void disp_cur_geom(void)
{
	msg_display_add(MSG_realgeom, dlcyl, dlhead, dlsec);
	msg_display_add(MSG_biosgeom, bcyl, bhead, bsec);
}


/*
 * Then,  the partition stuff...
 */
int
otherpart(int id)
{
#ifdef	ORIGINAL_CODE
	return (id != 0 && id != DOSPTYP_386BSD && id != DOSPTYP_NETBSD);
#else	/* PC-98 */
	return (id != 0 && is_sysid_bsd((sysid_t) id) == 0);
#endif	/* PC-98 */
}

int
ourpart(int id)
{
#ifdef	ORIGINAL_CODE
	return (id != 0 && (id == DOSPTYP_386BSD || id == DOSPTYP_NETBSD));
#else	/* PC-98 */
	return (id != 0 && is_sysid_bsd((sysid_t) id) != 0);
#endif	/* PC-98 */
}

/*
 * Let user change incore Master Boot Record partitions via menu.
 */
int edit_mbr()
{
	int i, j;

	/* Ask full/part */
	msg_display(MSG_fullpart, diskdev);
	process_menu(MENU_fullpart);

	/* DOS fdisk label checking and value setting. */
	if (usefull) {
		int otherparts = 0;
		int ourparts = 0;

		int i;
		/* Count nonempty, non-BSD partitions. */
#ifdef	ORIGINAL_CODE
		for (i = 0; i < 4; i++) {
			otherparts += otherpart(part[0][ID]);
			/* check for dualboot *bsd too */
			ourparts   += ourpart(part[0][ID]);
		}					  
#else	/* PC-98 */
		for (i = 0; i < NDOSPART; i++) {
			/* XXX: why? */
			otherparts += otherpart(part[i][ID]);
			/* check for dualboot *bsd too */
			ourparts   += ourpart(part[i][ID]);
		}					  
#endif	/* PC-98 */

		/* Ask if we really want to blow away non-NetBSD stuff */
		if (otherparts != 0 || ourparts > 1) {
			msg_display(MSG_ovrwrite);
			process_menu(MENU_noyes);
			if (!yesno) {
				if (logging)
					(void)fprintf(log, "User answered no to destroy other data, aborting.\n");
				return 0;
			}
		}

		/* Set the partition information for full disk usage. */
#ifdef	ORIGINAL_CODE
		part[0][ID] = part[0][SIZE] = 0;
		part[0][SET] = 1;
		part[1][ID] = part[1][SIZE] = 0;
		part[1][SET] = 1;
		part[2][ID] = part[2][SIZE] = 0;
		part[2][SET] = 1;
		part[3][ID] = dosptyp_nbsd;
		part[3][SIZE] = bsize - bsec;
		part[3][START] = bsec;
		part[3][FLAG] = 0x80;
		part[3][SET] = 1;

		ptstart = bsec;
		ptsize = bsize - bsec;
		fsdsize = dlsize;
		fsptsize = dlsize - bsec;
		fsdmb = fsdsize / MEG;
		activepart = 3;
#else	/* PC-98 */
		/* XXX:
		 * PC98 partitions NEVER has gaps (non used partitions)!
		 */
		part[7][ID] = part[0][SIZE] = 0;
		part[7][SET] = 1;
		part[1][ID] = part[1][SIZE] = 0;
		part[1][SET] = 1;
		part[2][ID] = part[2][SIZE] = 0;
		part[2][SET] = 1;
		part[3][ID] = part[3][SIZE] = 0;
		part[3][SET] = 1;
		part[4][ID] = part[4][SIZE] = 0;
		part[4][SET] = 1;
		part[5][ID] = part[5][SIZE] = 0;
		part[5][SET] = 1;
		part[6][ID] = part[6][SIZE] = 0;
		part[6][SET] = 1;
		/* XXX:
		 * A PC-98 partition should start at cyl 1.
		 */ 
		part[0][ID] = dosptyp_nbsd;
		part[0][SIZE] = bsize - bcylsize;
		part[0][START] = bcylsize;
		part[0][FLAG] = 0;
		part[0][SET] = 1;

		ptstart = bcylsize;
		ptsize = bsize - bcylsize;
		fsdsize = dlsize;
		fsptsize = dlsize - dlcylsize;
		fsdmb = fsdsize / MEG;
		activepart = 0;
#endif	/* PC-98 */
	} else {
		int numbsd, overlap;
		int numfreebsd, freebsdpart;	/* dual-boot */

		/* Ask for sizes, which partitions, ... */
		ask_sizemult();
		bsdpart = freebsdpart = -1;
		activepart = -1;
#ifdef	ORIGINAL_CODE
		for (i=0; i<4; i++)
			if (part[i][FLAG] != 0)
				activepart = i;
#else	/* PC-98 */
		/* XXX (FIXME) */
		activepart = -1;
#endif	/* PC-98 */
		do {
			process_menu(MENU_editparttable);
			numbsd = 0;
			bsdpart = -1;
			freebsdpart = -1;
			numfreebsd = 0;
			overlap = 0;
			yesno = 0;
#ifdef	ORIGINAL_CODE
			for (i=0; i<4; i++) {
#else	/* PC-98 */
			for (i=0; i<NDOSPART; i++) {
#endif	/* PC-98 */
				/* Count 386bsd/FreeBSD/NetBSD partitions */
#ifdef	ORIGINAL_CODE
				if (part[i][ID] == DOSPTYP_386BSD) {
#else	/* PC-98 */
				if (is_sysid_bsd((sysid_t) part[i][ID]) != 0) {
#endif	/* PC-98 */
					freebsdpart = i;
					numfreebsd++;
				}
				/* Count NetBSD-only partitions */
				if (part[i][ID] == DOSPTYP_NETBSD) {
					bsdpart = i;
					numbsd++;
				}
#ifdef	ORIGINAL_CODE
				for (j = i+1; j<4; j++)
#else	/* PC-98 */
				for (j = i+1; j<NDOSPART; j++)
#endif	/* PC-98 */
				       if (partsoverlap(i,j))
					       overlap = 1;
			}

			/* If no new-NetBSD partition, use 386bsd instead */
			if (numbsd == 0 && numfreebsd > 0) {
				numbsd = numfreebsd;
				bsdpart = freebsdpart;
				/* XXX check partition type? */
			}

			/* Check for overlap or multiple native partitions */
			if (overlap || numbsd != 1) {
				msg_display(MSG_reeditpart);
				process_menu(MENU_yesno);
			}
		} while (yesno && (numbsd != 1 || overlap));

		if (numbsd == 0) {
			msg_display(MSG_nobsdpart);
			process_menu(MENU_ok);
			return 0;
		}
			
		if (numbsd > 1) {
			msg_display(MSG_multbsdpart, bsdpart);
			process_menu(MENU_ok);
		}
			
		ptstart = part[bsdpart][START];
		ptsize = part[bsdpart][SIZE];
		fsdsize = dlsize;
		if (ptstart + ptsize < bsize)
			fsptsize = ptsize;
		else
			fsptsize = dlsize - ptstart;
		fsdmb = fsdsize / MEG;

		/* Ask if a boot selector is wanted. XXXX */
	}

	/* Compute minimum NetBSD partition sizes (in sectors). */
	minfsdmb = (80 + 4*rammb) * (MEG / sectorsize);

	return 1;
}

int partsoverlap(int i, int j)
{
	if (part[i][SIZE] == 0 || part[j][SIZE] == 0)
		return 0;

	return 
		(part[i][START] < part[j][START] &&
		 part[i][START] + part[i][SIZE] > part[j][START])
		||
		(part[i][START] > part[j][START] &&
		 part[i][START] < part[j][START] + part[j][SIZE])
		||
		(part[i][START] == part[j][START]);

}

void disp_cur_part(int sel, int disp)
{
	int i, j, start, stop, rsize, rend;

	if (disp < 0)
#ifdef	ORIGINAL_CODE
		start = 0, stop = 4;
#else	/* PC-98 */
		start = 0, stop = NDOSPART;
#endif	/* PC-98 */
	else
		start = disp, stop = disp+1;
	msg_display_add(MSG_part_head, multname, multname, multname);
	for (i=start; i<stop; i++) {
		if (sel == i) msg_standout();
		if (part[i][SIZE] == 0 && part[i][START] == 0)
			msg_printf_add("%d %36s  ", i, "");
		else {
			rsize = part[i][SIZE] / sizemult;
			if (part[i][SIZE] % sizemult)
				rsize++;
			rend = (part[i][START] + part[i][SIZE]) / sizemult;
			if ((part[i][SIZE] + part[i][SIZE]) % sizemult)
				rend++;
			msg_printf_add("%d %12d%12d%12d  ", i,
					part[i][START] / sizemult,
					rsize, rend);
		}
		for (j = 0; part_ids[j].id != -1 &&
#ifdef	ORIGINAL_CODE
			    part_ids[j].id != part[i][ID]; j++);
#else	/* PC-98 */
	    (part_ids[j].id | 0x80) != sysid_to_mid(part[i][ID]); j++);
#endif	/* PC-98 */
		msg_printf_add("%s\n", part_ids[j].name);
		if (sel == i) msg_standend();
	}
}
