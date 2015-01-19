/*	$NecBSD: vpdconfig.c,v 1.8 1999/07/31 22:53:08 honda Exp $	*/
/*	$NetBSD: vnconfig.c,v 1.8 1995/12/30 18:14:03 thorpej Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * Copyright (c) 1993 University of Utah.
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
 * from: Utah $Hdr: vnconfig.c 1.1 93/12/15$
 *
 *	@(#)vnconfig.c	8.1 (Berkeley) 12/15/93
 */

#include <sys/types.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <machine/vpdio.h>

#define VPD_CONFIG	1
#define VPD_UNCONFIG	2

int verbose = 0;

static int config __P((char *, char *, int, int));
static char *rawdevice __P((char *));
static char *blkdevice __P((char *));
static void usage __P((void));
int main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, rv, action = VPD_CONFIG, flags = 0;

	while ((ch = getopt(argc, argv, "cuva")) != -1) {
		switch (ch) {
		case 'a':
			flags |= VPD_ALLPART;
		case 'c':
			action = VPD_CONFIG;
			break;
		case 'u':
			action = VPD_UNCONFIG;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
		case '?':
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (action == VPD_CONFIG && argc == 2)
		rv = config(argv[0], argv[1], action, flags);
	else if (action == VPD_UNCONFIG && argc == 1)
		rv = config(argv[0], NULL, action, flags);
	else
		usage();
	exit(rv);
}

static int
config(dev, file, action, flags)
	char *dev;
	char *file;
	int action;
	int flags;
{
	struct vpd_ioctl vpdio;
	FILE *f;
	char *rdev;
	int rv;

	rdev = rawdevice(dev);
	f = fopen(rdev, "rw");
	if (f == NULL) {
		warn(rdev);
		return (1);
	}
	vpdio.vpd_file = NULL;
	vpdio.vpd_flags = flags;

	/*
	 * Clear (un-configure) the device
	 */
	if (action == VPD_UNCONFIG) {
		rv = ioctl(fileno(f), VPDIOCCLR, &vpdio);
		if (rv)
			warn("VPDIOCCLR");
		else if (verbose)
			printf("%s: cleared\n", dev);
	}
	/*
	 * Configure the device
	 */
	if (action == VPD_CONFIG) {
		vpdio.vpd_file = blkdevice(file);
		rv = ioctl(fileno(f), VPDIOCSET, &vpdio);
		if (rv)
			warn("VPDIOCSET");
	}

	fclose(f);
	fflush(stdout);
	return (rv < 0);
}

static char *
blkdevice(dev)
	char *dev;
{
	register char *rawbuf, *dp;
	struct stat sb;

	rawbuf = malloc(FILENAME_MAX);
	strcpy(rawbuf, dev);
	if (stat(rawbuf, &sb) != 0 || !S_ISBLK(sb.st_mode))
	{
		dp = rindex(rawbuf, '/');
		if (dp == NULL)
		{
			sprintf(rawbuf, "/dev/%sd", dev);
		}
	}
	return (rawbuf);
}

static char *
rawdevice(dev)
	char *dev;
{
	register char *rawbuf, *dp, *ep;
	struct stat sb;
	int len;

	if (strncmp(dev, "vpd", 3) == 0)
	{
		rawbuf = malloc(FILENAME_MAX);
		sprintf(rawbuf, "/dev/r%sd", dev);
	}
	else	
	{
		len = strlen(dev);
		rawbuf = malloc(len + 2);
		strcpy(rawbuf, dev);
		if (stat(rawbuf, &sb) != 0 || !S_ISCHR(sb.st_mode)) {
			dp = rindex(rawbuf, '/');
			if (dp) {
				for (ep = &rawbuf[len]; ep > dp; --ep)
					*(ep+1) = *ep;
				*++ep = 'r';
			}
		}
	}
	return (rawbuf);
}

static void
usage(void)
{

	(void)fprintf(stderr, "%s%s%s",
	    "usage: vpdconfig -c [-v] special-file special-file\n",
	    "       vpdconfig -u [-v] special-file\n",
	    "   ex: vpdconfig -c vpd0 wd0\n");
	exit(1);
}
