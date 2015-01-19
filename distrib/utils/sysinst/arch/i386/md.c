/*	$NecBSD: md.c,v 1.7.2.1 1999/08/25 23:52:26 honda Exp $	*/
/*	$NetBSD: md.c,v 1.21.2.4 1999/07/12 19:37:52 perry Exp $ */

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

/* md.c -- Machine specific code for i386 */

#include <stdio.h>
#include <util.h>
#ifdef	ORIGINAL_CODE
#include <sys/param.h>
#include <machine/cpu.h>
#include <sys/sysctl.h>
#endif	/* !PC-98 */
#include "defs.h"
#include "md.h"
#include "msg_defs.h"
#include "menu_defs.h"


#ifdef	ORIGINAL_CODE
char mbr[512];
int mbr_present, mbr_len;
#else	/* PC-98 */
int mbr_present;
#endif	/* PC-98 */
int c1024_resp;
#ifdef	ORIGINAL_CODE
struct disklist *disklist = NULL;
struct nativedisk_info *nativedisk;
struct biosdisk_info *biosdisk = NULL;
int netbsd_mbr_installed = 0;
int netbsd_bootsel_installed = 0;

static int md_read_bootcode __P((char *, char *, size_t));
static int count_mbr_parts __P((struct mbr_partition *));
static int mbr_part_above_chs __P((struct mbr_partition *));
static int mbr_partstart_above_chs __P((struct mbr_partition *));
static void configure_bootsel __P((void));
static void md_upgrade_mbrtype __P((void));

struct mbr_bootsel *mbs;
int defbootselpart, defbootseldisk;
#endif	/* !PC-98 */


/* prototypes */


int
md_get_info()
{
#ifdef	ORIGINAL_CODE
	read_mbr(diskdev, mbr, sizeof mbr);
	if (!valid_mbr(mbr)) {
		memset(&mbr[MBR_PARTOFF], 0,
		    NMBRPART * sizeof (struct mbr_partition));
		/* XXX check result and give up if < 0 */
		mbr_len = md_read_bootcode(_PATH_MBR, mbr, sizeof mbr);
		netbsd_mbr_installed = 1;
	} else
		mbr_len = MBR_SECSIZE;
	md_bios_info(diskdev);

edit:
	edit_mbr((struct mbr_partition *)&mbr[MBR_PARTOFF]);

	if (mbr_part_above_chs(part) &&
	    (biosdisk == NULL || !(biosdisk->bi_flags & BIFLAG_EXTINT13))) {
		msg_display(MSG_partabovechs);
		process_menu(MENU_noyes);
		if (!yesno)
			goto edit;
	}

	if (count_mbr_parts(part) > 1) {
		msg_display(MSG_installbootsel);
		process_menu(MENU_yesno);
		if (yesno) {
			mbr_len =
			    md_read_bootcode(_PATH_BOOTSEL, mbr, sizeof mbr);
			configure_bootsel();
			netbsd_mbr_installed = netbsd_bootsel_installed = 1;
		}
	}

	if (mbr_partstart_above_chs(part) && !netbsd_mbr_installed) {
		msg_display(MSG_installmbr);
		process_menu(MENU_yesno);
		if (yesno) {
			mbr_len = md_read_bootcode(_PATH_MBR, mbr, sizeof mbr);
			netbsd_mbr_installed = 1;
		}
	}

	return 1;
#else	/* PC-98 */
	get_fdisk_info();

	/* Check fdisk information */
	if (part[0][ID] == 0 && part[1][ID] == 0 && part[2][ID] == 0 &&
	    part[3][ID] == 0 && part[4][ID] == 0 && part[5][ID] == 0 &&
	    part[6][ID] == 0 && part[7][ID] == 0) {
		mbr_present = 0;
		process_menu(MENU_nobiosgeom);
	} else {
		mbr_present = 1;
		msg_display(MSG_confirmbiosgeom);
		process_menu(MENU_confirmbiosgeom);
	}

	/* Ask about disk type ... */
	if (strncmp(disk->dd_name, "wd", 2) == 0) {
		process_menu(MENU_wdtype);
		disktype = "ST506";
		/* Check against disk geometry. */
		if (disk->dd_cyl != dlcyl || disk->dd_head != dlhead
		    || disk->dd_sec != dlsec)
			process_menu(MENU_dlgeom);
	} else {
		disktype = "SCSI";
		if (disk->dd_cyl*disk->dd_head*disk->dd_sec != disk->dd_totsec) {
			if (disk->dd_cyl != dlcyl || disk->dd_head != dlhead
			    || disk->dd_sec != dlsec)
				process_menu(MENU_scsigeom1);
			else
				process_menu(MENU_scsigeom2);
		}
	}

	/* Compute the full sizes ... */
	dlcylsize = dlhead*dlsec;
	dlsize = dlcyl*dlcylsize;
	bcylsize = bhead*bsec;
	bsize = bcyl*bcylsize;

	msg_display(MSG_diagcyl);
	process_menu(MENU_noyes);
	if (yesno) {
		dlcyl--;
		dlsize -= dlcylsize;
		if (dlsize < bsize)
			bcyl = dlsize / bcylsize;
	}
	return edit_mbr();
#endif	/* PC-98 */
}

#ifdef	ORIGINAL_CODE
/*
 * Read MBR code from a file. It may be a maximum of "len" bytes
 * long. This function skips the partition table. Space for this
 * is assumed to be in the file, but a table already in the buffer
 * is not overwritten.
 */
static int
md_read_bootcode(path, buf, len)
	char *path, *buf;
	size_t len;
{
	int fd, cc;
	struct stat st;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	if (fstat(fd, &st) < 0 || st.st_size > len || st.st_size < MBR_SECSIZE){
		close(fd);
		return -1;
	}
	if (read(fd, buf, MBR_PARTOFF) != MBR_PARTOFF) {
		close(fd);
		return -1;
	}
	if (lseek(fd, MBR_MAGICOFF, SEEK_SET) < 0) {
		close(fd);
		return -1;
	}
	cc = read(fd, &buf[MBR_MAGICOFF], st.st_size - MBR_MAGICOFF);

	close(fd);

	return (cc + MBR_MAGICOFF);
}
#endif	/* !PC-98 */

int
md_pre_disklabel()
{
#ifdef	ORIGINAL_CODE
	msg_display(MSG_dofdisk);

	/* write edited MBR onto disk. */
	if (write_mbr(diskdev, mbr, sizeof mbr, 1) != 0) {
		msg_display(MSG_wmbrfail);
		process_menu(MENU_ok);
		return 1;
	}
	md_upgrade_mbrtype();
	return 0;
#else	/* PC-98 */
	/* Fdisk the disk! */
	msg_display(MSG_dofdisk);

	/* write edited MBR onto disk. */
	set_fdisk_info();

	return 0;
#endif	/* PC-98 */
}

int
md_post_disklabel(void)
{
	/* Sector forwarding / badblocks ... */
	if (*doessf) {
		msg_display(MSG_dobad144);
		return run_prog(0, 1, NULL, "/usr/sbin/bad144 %s 0", diskdev);
	}
	return 0;
}

int
md_post_newfs(void)
{
	/* boot blocks ... */
	msg_display(MSG_dobootblks, diskdev);
	return run_prog(0, 1, NULL,
	    "/usr/mdec/installboot -v /usr/mdec/biosboot.sym /dev/r%sa",
	    diskdev);
}

int
md_copy_filesystem(void)
{
	return 0;
}


int
md_make_bsd_partitions(void)
{
	FILE *f;
	int i;
	int part;
	int maxpart = getmaxpartitions();
	int remain;

#ifdef	ORIGINAL_CODE
editlab:
#endif	/* PC-98 */
	/* Ask for layout type -- standard or special */
	msg_display(MSG_layout,
			(1.0*fsptsize*sectorsize)/MEG,
			(1.0*minfsdmb*sectorsize)/MEG,
			(1.0*minfsdmb*sectorsize)/MEG+rammb+XNEEDMB);
	process_menu(MENU_layout);

	if (layoutkind == 3) {
		ask_sizemult();
	} else {
		sizemult = MEG / sectorsize;
		multname = msg_string(MSG_megname);
	}


	/* Build standard partitions */
	emptylabel(bsdlabel);

	/* Partitions C and D are predefined. */
	bsdlabel[C].pi_fstype = FS_UNUSED;
	bsdlabel[C].pi_offset = ptstart;
	bsdlabel[C].pi_size = fsptsize;
	
	bsdlabel[D].pi_fstype = FS_UNUSED;
	bsdlabel[D].pi_offset = 0;
	bsdlabel[D].pi_size = fsdsize;

	/* Standard fstypes */
	bsdlabel[A].pi_fstype = FS_BSDFFS;
	bsdlabel[B].pi_fstype = FS_SWAP;
	bsdlabel[E].pi_fstype = FS_UNUSED;
	bsdlabel[F].pi_fstype = FS_UNUSED;
	bsdlabel[G].pi_fstype = FS_UNUSED;
	bsdlabel[H].pi_fstype = FS_UNUSED;

	switch (layoutkind) {
	case 1: /* standard: a root, b swap, c/d "unused", e /usr */
	case 2: /* standard X: a root, b swap (big), c/d "unused", e /usr */
		partstart = ptstart;

		/* check that we have enouth space */
		i = NUMSEC(20+2*rammb, MEG/sectorsize, dlcylsize);
		i += NUMSEC(layoutkind * 2 * (rammb < 16 ? 16 : rammb),
			   MEG/sectorsize, dlcylsize);
		if ( i > fsptsize) {
			msg_display(MSG_disktoosmall);
			process_menu(MENU_ok);
			goto custom;
		}
		/* Root */
		i = NUMSEC(20+2*rammb, MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
				   dlcylsize) - partstart;
		bsdlabel[A].pi_offset = partstart;
		bsdlabel[A].pi_size = partsize;
		bsdlabel[A].pi_bsize = 8192;
		bsdlabel[A].pi_fsize = 1024;
		strcpy (fsmount[A], "/");
		partstart += partsize;

		/* swap */
		i = NUMSEC(layoutkind * 2 * (rammb < 16 ? 16 : rammb),
			   MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
			   dlcylsize) - partstart;
		bsdlabel[B].pi_offset = partstart;
		bsdlabel[B].pi_size = partsize;
		partstart += partsize;

		/* /usr */
		partsize = fsptsize - (partstart - ptstart);
		bsdlabel[E].pi_fstype = FS_BSDFFS;
		bsdlabel[E].pi_offset = partstart;
		bsdlabel[E].pi_size = partsize;
		bsdlabel[E].pi_bsize = 8192;
		bsdlabel[E].pi_fsize = 1024;
		strcpy (fsmount[E], "/usr");
		break;

	case 3: /* custom: ask user for all sizes */
custom:		ask_sizemult();
		msg_display(MSG_defaultunit, multname);
		partstart = ptstart;
		remain = fsptsize;

		/* root */
		i = NUMSEC(20+2*rammb, MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
				   dlcylsize) - partstart;
		if (partsize > remain)
			partsize = remain;
		msg_display_add(MSG_askfsroot1, remain/sizemult, multname);
		partsize = getpartsize(MSG_askfsroot2, partstart, partsize);
		bsdlabel[A].pi_offset = partstart;
		bsdlabel[A].pi_size = partsize;
		bsdlabel[A].pi_bsize = 8192;
		bsdlabel[A].pi_fsize = 1024;
		strcpy (fsmount[A], "/");
		partstart += partsize;
		remain -= partsize;
		
		/* swap */
		i = NUMSEC( 2 * (rammb < 16 ? 16 : rammb),
			   MEG/sectorsize, dlcylsize) + partstart;
		partsize = NUMSEC (i/(MEG/sectorsize)+1, MEG/sectorsize,
			   dlcylsize) - partstart;
		if (partsize > remain)
			partsize = remain;
		msg_display(MSG_askfsswap1, remain/sizemult, multname);
		partsize = getpartsize(MSG_askfsswap2, partstart, partsize);
		bsdlabel[B].pi_offset = partstart;
		bsdlabel[B].pi_size = partsize;
		partstart += partsize;
		remain -= partsize;
		
		/* Others E, F, G, H */
		part = E;
		if (remain > 0)
			msg_display (MSG_otherparts);
		while (remain > 0 && part <= H) {
			msg_display_add(MSG_askfspart1, diskdev,
			    partname[part], remain/sizemult, multname);
			partsize = getpartsize(MSG_askfspart2, partstart,
			    remain);
			if (partsize > 0) {
				if (remain - partsize < sizemult)
					partsize = remain;
				bsdlabel[part].pi_fstype = FS_BSDFFS;
				bsdlabel[part].pi_offset = partstart;
				bsdlabel[part].pi_size = partsize;
				bsdlabel[part].pi_bsize = 8192;
				bsdlabel[part].pi_fsize = 1024;
				if (part == E)
					strcpy (fsmount[E], "/usr");
				msg_prompt_add (MSG_mountpoint, fsmount[part],
						fsmount[part], 20);
				partstart += partsize;
				remain -= partsize;
			}
			part++;
		}
		
		break;
	}

	/*
	 * OK, we have a partition table. Give the user the chance to
	 * edit it and verify it's OK, or abort altogether.
	 */
	if (edit_and_check_label(bsdlabel, maxpart, RAW_PART, RAW_PART) == 0) {
		msg_display(MSG_abort);
		return 0;
	}

#ifdef	ORIGINAL_CODE
	/*
	 * XXX check for int13 extensions.
	 */
	if ((bsdlabel[A].pi_offset + bsdlabel[A].pi_size) / bcylsize > 1024 &&
	    (biosdisk == NULL || !(biosdisk->bi_flags & BIFLAG_EXTINT13))) {
		process_menu(MENU_cyl1024);
		/* XXX UGH! need arguments to process_menu */
		switch (c1024_resp) {
		case 1:
			edit_mbr((struct mbr_partition *)&mbr[MBR_PARTOFF]);
			/*FALLTHROUGH*/
		case 2:
			goto editlab;
		default:
			break;
		}
	}
#endif	/* !PC-98 */

	/* Disk name */
	msg_prompt (MSG_packname, "mydisk", bsddiskname, DISKNAME_SIZE);

	/* Create the disktab.preinstall */
	run_prog (0, 0, NULL, "cp /etc/disktab.preinstall /etc/disktab");
#ifdef DEBUG
	f = fopen ("/tmp/disktab", "a");
#else
	f = fopen ("/etc/disktab", "a");
#endif
	if (f == NULL) {
		endwin();
		(void) fprintf (stderr, "Could not open /etc/disktab");
		exit (1);
	}
	(void)fprintf (f, "%s|NetBSD installation generated:\\\n", bsddiskname);
	(void)fprintf (f, "\t:dt=%s:ty=winchester:\\\n", disktype);
	(void)fprintf (f, "\t:nc#%d:nt#%d:ns#%d:\\\n", dlcyl, dlhead, dlsec);
	(void)fprintf (f, "\t:sc#%d:su#%d:\\\n", dlhead*dlsec, dlsize);
	(void)fprintf (f, "\t:se#%d:%s\\\n", sectorsize, doessf);
	for (i=0; i<8; i++) {
		(void)fprintf (f, "\t:p%c#%d:o%c#%d:t%c=%s:",
			       'a'+i, bsdlabel[i].pi_size,
			       'a'+i, bsdlabel[i].pi_offset,
			       'a'+i, fstypenames[bsdlabel[i].pi_fstype]);
		if (bsdlabel[i].pi_fstype == FS_BSDFFS)
			(void)fprintf (f, "b%c#%d:f%c#%d",
				       'a'+i, bsdlabel[i].pi_bsize,
				       'a'+i, bsdlabel[i].pi_fsize);
		if (i < 7)
			(void)fprintf (f, "\\\n");
		else
			(void)fprintf (f, "\n");
	}
	fclose (f);

	/* Everything looks OK. */
	return (1);
}


/* Upgrade support */
int
md_update(void)
{
	endwin();
	md_copy_filesystem();
	md_post_newfs();
#ifdef	ORIGINAL_CODE
	md_upgrade_mbrtype();
#endif	/* !PC-98 */
	puts(CL);		/* XXX */
	wclear(stdscr);
	wrefresh(stdscr);
	return 1;
}

#ifdef	ORIGINAL_CODE
void
md_upgrade_mbrtype()
{
	struct mbr_partition *mbrp;
	int i, netbsdpart = -1, oldbsdpart = -1, oldbsdcount = 0;

	if (read_mbr(diskdev, mbr, sizeof mbr) < 0)
		return;

	mbrp = (struct mbr_partition *)&mbr[MBR_PARTOFF];

	for (i = 0; i < NMBRPART; i++) {
		if (mbrp[i].mbrp_typ == MBR_PTYPE_386BSD) {
			oldbsdpart = i;
			oldbsdcount++;
		} else if (mbrp[i].mbrp_typ == MBR_PTYPE_NETBSD)
			netbsdpart = i;
	}

	if (netbsdpart == -1 && oldbsdcount == 1) {
		mbrp[oldbsdpart].mbrp_typ = MBR_PTYPE_NETBSD;
		write_mbr(diskdev, mbr, sizeof mbr, 0);
	}
}
#endif	/* !PC-98 */



void
md_cleanup_install(void)
{
	char realfrom[STRSIZE];
	char realto[STRSIZE];
	char sedcmd[STRSIZE];

	strncpy(realfrom, target_expand("/etc/rc.conf"), STRSIZE);
	strncpy(realto, target_expand("/etc/rc.conf.install"), STRSIZE);

	sprintf(sedcmd, "sed 's/rc_configured=NO/rc_configured=YES/' < %s > %s",
	    realfrom, realto);
	if (logging)
		(void)fprintf(log, "%s\n", sedcmd);
	if (scripting)
		(void)fprintf(script, "%s\n", sedcmd);
	do_system(sedcmd);

	run_prog(1, 0, NULL, "mv -f %s %s", realto, realfrom);
	run_prog(0, 0, NULL, "rm -f %s", target_expand("/sysinst"));
	run_prog(0, 0, NULL, "rm -f %s", target_expand("/.termcap"));
	run_prog(0, 0, NULL, "rm -f %s", target_expand("/.profile"));
}

#ifdef	ORIGINAL_CODE
int
md_bios_info(dev)
	char *dev;
{
	int mib[2], i, len;
	struct biosdisk_info *bip;
	struct nativedisk_info *nip = NULL, *nat;
	int cyl, head, sec;

	if (disklist == NULL) {
		mib[0] = CTL_MACHDEP;
		mib[1] = CPU_DISKINFO;
		if (sysctl(mib, 2, NULL, &len, NULL, 0) < 0)
			goto nogeom;
		disklist = (struct disklist *)malloc(len);
		sysctl(mib, 2, disklist, &len, NULL, 0);
	}

	nativedisk = NULL;

	for (i = 0; i < disklist->dl_nnativedisks; i++) {
		nat = &disklist->dl_nativedisks[i];
		if (!strcmp(dev, nat->ni_devname)) {
			nativedisk = nip = nat;
			break;
		}
	}
	if (nip == NULL || nip->ni_nmatches == 0) {
nogeom:
		msg_display(MSG_nobiosgeom, dlcyl, dlhead, dlsec);
		if (guess_biosgeom_from_mbr(mbr, &cyl, &head, &sec) >= 0) {
			msg_display_add(MSG_biosguess, cyl, head, sec);
			set_bios_geom(cyl, head, sec);
		} else
			set_bios_geom(dlcyl, dlhead, dlsec);
		biosdisk = NULL;
	} else if (nip->ni_nmatches == 1) {
		bip = &disklist->dl_biosdisks[nip->ni_biosmatches[0]];
		msg_display(MSG_onebiosmatch);
		msg_printf_add("%6x%10d%7d%10d\n", bip->bi_dev - 0x80,
		    bip->bi_cyl, bip->bi_head, bip->bi_sec);
		process_menu(MENU_biosonematch);
	} else {
		msg_display(MSG_biosmultmatch);
		for (i = 0; i < nip->ni_nmatches; i++) {
			bip = &disklist->dl_biosdisks[nip->ni_biosmatches[i]];
			msg_printf_add("%d: %6x%10d%7d%10d\n", i,
			    bip->bi_dev - 0x80, bip->bi_cyl, bip->bi_head,
			    bip->bi_sec);
		}
		process_menu(MENU_biosmultmatch);
	}
	if (biosdisk != NULL && (biosdisk->bi_flags & BIFLAG_EXTINT13))
		bsize = dlsize;
	else
		bsize = bcyl * bhead * bsec;
	bcylsize = bhead * bsec;
	return 0;
}

static int
count_mbr_parts(pt)
	struct mbr_partition *pt;
{
	int i, count = 0;;

	for (i = 0; i < NMBRPART; i++)
		if (pt[i].mbrp_typ != 0)
			count++;

	return count;
}

static int
mbr_part_above_chs(pt)
	struct mbr_partition *pt;
{
	return ((pt[bsdpart].mbrp_start + pt[bsdpart].mbrp_size) >=
		bcyl * bhead * bsec);
}

static int
mbr_partstart_above_chs(pt)
	struct mbr_partition *pt;
{
	return (pt[bsdpart].mbrp_start >= bcyl * bhead * bsec);
}

static void
configure_bootsel()
{
	struct mbr_partition *parts =
	    (struct mbr_partition *)&mbr[MBR_PARTOFF];
	int i;


	mbs = (struct mbr_bootsel *)&mbr[MBR_BOOTSELOFF];
	mbs->flags = BFL_SELACTIVE;

	process_menu(MENU_configbootsel);

	for (i = 0; i < NMBRPART; i++) {
		if (parts[i].mbrp_typ != 0 &&
		   parts[i].mbrp_start >= (bcyl * bhead * bsec)) {
			mbs->flags |= BFL_EXTINT13;
			break;
		}
	}
}

void
disp_bootsel(part, mbsp)
	struct mbr_partition *part;
	struct mbr_bootsel *mbsp;
{
	int i;

	msg_display_add(MSG_bootselheader);
	for (i = 0; i < 4; i++) {
		msg_printf_add("%6d      %-32s     %s\n",
		    i, get_partname(i), mbs->nametab[i]);
	}
}
#endif	/* !PC-98 */

#ifndef	ORIGINAL_CODE
#include <sys/stat.h>
int extract_dist __P((void));		/* XXX */

void
md_overwrite_sets(void)
{

	doingwhat = msg_string(MSG_doing_overwrite);

	dist_list = dist_list_sub;

	unwind_mounts();
	msg_display(MSG_overwrite);
	process_menu(MENU_yesno);
	if (yesno == 0)
		goto out;

	if (find_disks() < 0)
		return;

	/* if we need the user to mount root, ask them to. */
	if (must_mount_root()) {
		msg_display(MSG_pleasemountroot, diskdev, diskdev, diskdev);
		process_menu(MENU_ok);
		goto out;
	}

	if (!fsck_disks())
		goto out;

	fflush(stdout); puts(CL); clear(); wrefresh(stdscr);

#if	0
	/* Unpack the distribution. */
	get_and_unpack_sets(MSG_overwrite_complete, MSG_abortunpack);
#endif

	/* Ensure mountpoint for distribution files exists in current root. */
	(void) mkdir("/mnt2", S_IRWXU| S_IRGRP|S_IXGRP | S_IXOTH|S_IXOTH);

	/* Find out which files to "get" if we get files. */
	process_menu(MENU_distset);

	/* Get the distribution files */
	process_menu(MENU_distmedium);

	if (nodist)
		goto out;

	if (got_dist) {

		/* ask user  whether to do normal or verbose extraction */
		ask_verbose_dist();

		/* Extract the distribution, abort on errors. */
		if (extract_dist()) {
			goto bad;
		}

		/* Configure the system */
		run_makedev();

		/* Clean up dist dir (use absolute path name) */
		if (clean_dist_dir)
			run_prog(0, 0, NULL, "/bin/rm -rf %s", ext_dir);

		/* Mounted dist dir? */
		if (mnt2_mounted)
			run_prog(0, 0, NULL, "/sbin/umount /mnt2");

		/* Install/Upgrade  complete ... reboot or exit to script */
		msg_display(MSG_overwrite_complete);
		process_menu(MENU_ok);

		sanity_check();
		goto out;
	}

bad:
	msg_display(MSG_abortupgr);
	process_menu(MENU_ok);

out:
	dist_list = dist_list_main;
}

void
md_update_boot(void)
{
	doingwhat = msg_string(MSG_doing_update_boot);

	unwind_mounts();
	msg_display(MSG_update_boot);
	process_menu(MENU_yesno);
	if (yesno == 0)
		return;

	if (find_disks() < 0)
		return;

	fflush(stdout); puts(CL); clear(); wrefresh(stdscr);
	if (md_post_newfs() == 0)
		msg_display(MSG_update_boot_complete);
	else
		msg_display(MSG_update_boot_abort);
	process_menu(MENU_ok);
}

/* choose a fake geometry. */
void
scsi_fake(void)
{
	long fact[20];
	int  numf;

	int geom[5][4] = {{0}};
	int i, j;
	int sects = disk->dd_totsec;
	int head, sec;

	int stop = disk->dd_cyl*disk->dd_head*disk->dd_sec;

	i=0;
	while (i < 4 && sects > stop) {
	       factor(sects, fact, 20, &numf);
	       if (numf >= 3) {
		      head =  fact[0];
		      j = 1;
		      while (j < numf-2 && head*fact[j] < 50)
			      head *= fact[j++];
		      sec = fact[j++];
		      while (j < numf-1 && sec*fact[j] < 500)
			      sec *= fact[j++];
		      if (head >= 5  &&
			  sec >= 50) {
			      geom[i][0] = sects / (head*sec);
			      geom[i][1] = head;
			      geom[i][2] = sec;
			      geom[i][3] = head * sec * geom[i][0];
			      i++;
		      }
	       }
	       sects--;
	}
	while (i < 5) {
		geom[i][0] = disk->dd_cyl;
		geom[i][1] = disk->dd_head;
		geom[i][2] = disk->dd_sec;
		geom[i][3] = stop;
		i++;
	}
	
	msg_display(MSG_scsi_fake, disk->dd_totsec,
		     geom[0][0], geom[0][1], geom[0][2], geom[0][3],
		     geom[1][0], geom[1][1], geom[1][2], geom[1][3],
		     geom[2][0], geom[2][1], geom[2][2], geom[2][3],
		     geom[3][0], geom[3][1], geom[3][2], geom[3][3],
		     geom[4][0], geom[4][1], geom[4][2], geom[4][3]);
	process_menu(MENU_scsi_fake);
	if (fake_sel >= 0) {
		dlcyl  = disk->dd_cyl = geom[fake_sel][0];
		dlhead = disk->dd_head = geom[fake_sel][1];
		dlsec  = disk->dd_sec = geom[fake_sel][2];
		dlsize = disk->dd_totsec = geom[fake_sel][3];
	}
}
#endif	/* PC-98 */
