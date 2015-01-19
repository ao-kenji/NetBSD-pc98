/*	$NecBSD: diskinfo.c,v 1.6 1999/02/12 05:57:07 kmatsuda Exp $	*/
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <errno.h>
#include <machine/vscio.h>	/* XXX */

#define	NBPG 4096
struct diskinfo {
	u_char *name;
	u_int maxunit;
} diskinfo[] = { {"wd", 4}, {"sd", 7}, {NULL, 0} };

u_char buf[NBPG];
struct disklabel dl;

int check_rootdev __P((u_char *));
int check_target __P((u_char *, u_char *));
int main __P((int, u_char **));

int
main(argc, argv)
	int argc;
	u_char **argv;
{
	int unit;
	struct diskinfo *di;
	u_char devname[FILENAME_MAX + 1];

	if (argc == 2)
	{
		check_rootdev(argv[1]);
		exit(0);
	}
	else if (argc == 3)
	{
		if (check_target(argv[1], argv[2]) > 0)
		{
			printf("0");
		}
		exit(0);
	}
		
	di = &diskinfo[0];

	printf("%6s %8s %8s %8s %8s %8s %10s\n",
		"disk", "bsize", "sectors", "heads", "cyls", "total", "size  ");

	while (di->name != NULL)
	{
		for (unit = 0; unit < di->maxunit; unit++)
		{
			sprintf(devname, "%s%d", di->name, unit);
			check_target(devname, NULL);
		}
		di ++;
	}

	return 0;
}

int
check_rootdev(req)
	u_char *req;
{
	dev_t rootdev;
	int fd, error;
	u_char *s = NULL;

	if ((fd = open("/dev/console", O_RDONLY, 0)) < 0)
		goto bad;

	error = ioctl(fd, VSC_GET_ROOTDEV, &rootdev);
	close(fd);
	if (error < 0)
		goto bad;

	switch (major(rootdev))
	{
	case 0: s = "wd"; break;
	case 2: s = "fd"; break;
	case 4: s = "sd"; break;
	case 6: s = "cd"; break;
	}

	printf("%s%d", s, DISKUNIT(minor(rootdev)));
	return 0;

bad:
	printf("unknown");
	return 1;
}

int
check_target(dv, req)
	u_char *dv;
	u_char *req;
{
	struct dos_partition *dp;
	struct disklabel *lp = &dl;
	int part, count, fd;
	u_char devname[FILENAME_MAX + 1];

	sprintf(devname, "/dev/r%sd", dv);
	if ((fd = open(devname, O_RDONLY, 0777)) < 0)
		return ENODEV;

	if (ioctl(fd, DIOCGDINFO, lp) < 0)
		return EIO;

	lseek(fd, (off_t) 0, 0);
	count = read(fd, buf, NBPG);	
	if (count != NBPG)
		return EIO;

	dp = (struct dos_partition *) (buf + DOSPARTOFF);
	for (part = 0; part < MAXPARTITIONS; part ++, dp ++)
		if (is_dos_partition_bsd(dp) != 0)
			break;

	if (req == NULL)
	{
		double size;

		size = lp->d_secperunit / (1024 * 1024 / lp->d_secsize);
		printf("%6s %8d %8d %8d %8d %8d %8.1fMB\n",
			dv,
			lp->d_secsize,
			lp->d_nsectors,
			lp->d_ntracks,
			lp->d_ncylinders,
			lp->d_secperunit,
			(float) size);
	}
	else
	{
		u_int val = 0;


		if (strcmp(req, "secsize") == 0)
			val = lp->d_secsize;
		else if (strcmp(req, "nsectors") == 0)
			val = lp->d_nsectors;
		else if (strcmp(req, "ntracks") == 0)
			val = lp->d_ntracks;
		else if (strcmp(req, "ncylinders") == 0)
			val = lp->d_ncylinders;
		else if (strcmp(req, "dosscyl") == 0)
		{
			if (part == MAXPARTITIONS)
				val = 0;
			else
				val = dp->dp_scyl;
		}
		else if (strcmp(req, "dosecyl") == 0)
		{
			if (part == MAXPARTITIONS)
				val = lp->d_ncylinders - 1;
			else if (dp->dp_ecyl >= lp->d_ncylinders)
				val = lp->d_ncylinders - 1;
			else
				val = dp->dp_ecyl;
		}
		printf("%u", val);
	}
	return 0;
}
