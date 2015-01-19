/*	$NecBSD: devopen.c,v 1.2 1999/08/02 05:41:46 kmatsuda Exp $	*/
/*	$NetBSD: devopen.c,v 1.7 1998/05/15 16:38:53 drochner Exp $	 */

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 * Copyright (c) 1996, 1997
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
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


#include <sys/types.h>
#ifdef COMPAT_OLDBOOT
#include <sys/disklabel.h>
#endif

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <libi386.h>
#include <biosdisk.h>
#ifdef _STANDALONE
#include <bootinfo.h>
#endif

extern int parsebootfile __P((const char *, char**, char**, unsigned int*,
			      unsigned int*, const char**));
#ifndef	ORIGINAL_CODE
extern int get_biosparam __P((unsigned short));
#endif	/* PC-98 */

#ifdef	ORIGINAL_CODE
static struct {
	char           *name;
	int             biosdev;
}               biosdevtab[] = {
	{
		"fd", 0
	},
	{
		"hd", 0x80
	},
#ifdef COMPAT_OLDBOOT
	{
		"wd", 0x80
	},
	{
		"sd", 0x80
	}
#endif
};
#else	/* PC-98 */
static struct {
	char           *name;
	int             biosdev;
	int             major;
	unsigned int	maxunit;
}               biosdevtab[] = {
	{
		"fd", 0x90, 2, 3	/* XXX */
	},
#ifdef COMPAT_OLDBOOT
	{
		"wd", 0x80, 0, 3
	},
	{
		"sd", 0xa0, 4, 7
	}
#endif
};
#endif	/* PC-98 */
#define NUMBIOSDEVS (sizeof(biosdevtab) / sizeof(biosdevtab[0]))

#ifdef	ORIGINAL_CODE
static int
dev2bios(devname, unit, biosdev)
	char           *devname;
	unsigned int    unit;
	int            *biosdev;
{
	int             i;

	for (i = 0; i < NUMBIOSDEVS; i++)
		if (!strcmp(devname, biosdevtab[i].name)) {
			*biosdev = biosdevtab[i].biosdev + unit;
			break;
		}
	if (i == NUMBIOSDEVS)
		return (ENXIO);

	if (unit >= 4)		/* ??? */
		return (EUNIT);

	return (0);
}
#else	/* PC-98 */
static int
dev2bios(devname, unit, biosdev)
	char           *devname;
	unsigned int    unit;
	int            *biosdev;
{
	int		i;
	int		daua;

	for (i = 0; i < NUMBIOSDEVS; i++)
		if (!strcmp(devname, biosdevtab[i].name)) {
			*biosdev = biosdevtab[i].biosdev + unit;
			break;
		}
	if (i == NUMBIOSDEVS) {
		printf("BIOSDEV not found\n");
		return (ENXIO);
	}

	if (unit > biosdevtab[i].maxunit) {
		printf("BIOSDEV unit too big\n");
		return (EUNIT);
	}

	if (biosdevtab[i].major == 2) {
		/* XXX: get DA/UA of boot perheral */
		daua = get_biosparam(0x584) & 0xf0;
		if (daua == 0x30 || daua == 0x90)
			*biosdev = daua + unit;
	}

#ifdef	DISK_DEBUG
	printf("maj %x unit %x daua %x biosdev %x\n", maj, unit, daua, biosdev);
#endif	/* DISK_DEBUG */
	return (0);
}
#endif	/* PC-98 */

#ifdef	ORIGINAL_CODE
int
bios2dev(biosdev, devname, unit)
	int             biosdev;
	char          **devname;
	unsigned int   *unit;
{
	if (biosdev & 0x80) {
#if defined(COMPAT_OLDBOOT) && defined(_STANDALONE)
		extern struct disklabel disklabel;

		if(disklabel.d_magic == DISKMAGIC) {
			if(disklabel.d_type == DTYPE_SCSI)
				*devname = biosdevtab[3].name;
			else
				*devname = biosdevtab[2].name;
		} else
#endif
			/* call it "hd", we don't know better */
			*devname = biosdevtab[1].name;
	} else
		*devname = biosdevtab[0].name;

	*unit = biosdev & 0x7f;

	return (0);
}
#else	/* PC-98 */
int
bios2dev(biosdev, devname, unit)
	int             biosdev;	/* in */
	char          **devname;	/* out */
	unsigned int   *unit;		/* out */
{
	
	/* XXX: DON'T USE `hd' disk selection */
	switch (biosdev & 0xf0) {
		case 0x30:
		case 0xb0:
		case 0x90:
			*devname = "fd";	/* major = 2 */
			break;
		case 0x80:
			*devname = "wd";	/* major = 0 */
			break;
		default:
			*devname = "sd";	/* major = 4 */
			break;
	}

	*unit = biosdev & 0x0f;

	return (0);
}
#endif	/* PC-98 */

#ifdef _STANDALONE
struct btinfo_bootpath bibp;
#endif

/*
 * Open the BIOS disk device
 */
int
devopen(f, fname, file)
	struct open_file *f;
	const char     *fname;
	char          **file;
{
	char           *fsname, *devname;
	unsigned int    unit, partition;
	int             biosdev;
	int             error;
	struct devsw   *dp;

	if ((error = parsebootfile(fname, &fsname, &devname,
				   &unit, &partition, (const char **) file))
	    || (error = dev2bios(devname, unit, &biosdev)))
		return (error);

#ifdef	DISK_DEBUG	/* debug debug */
	printf("dev %s biosdev %x unit %x\n", devname, biosdev, unit);
#endif	/* DISK_DEBUG */
	dp = &devsw[0];		/* must be biosdisk */
	f->f_dev = dp;

#ifdef _STANDALONE
	strncpy(bibp.bootpath, *file, sizeof(bibp.bootpath));
	BI_ADD(&bibp, BTINFO_BOOTPATH, sizeof(bibp));
#endif

	return (biosdiskopen(f, biosdev, partition));
}
