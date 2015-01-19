/*	$NecBSD: fdc.c,v 1.8 1998/03/14 07:12:02 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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
 * NetBSD 1.0 pc98 	fdc.c by N. Honda.
 * Ver 0.0
 * 1) all facility integrated.
 *    Do format. Specify interleave values. Change default density.
 *    Toggle automatic density detect facility.
 *
 * Import to NetBSD 1.0A (Mar 04, 1995 tarballs) by K. Matsuda,
 * Mar 10, 1995.
 *
 * N. Honda, Mar 20, 1995.
 * display status like sytax "fdc 1" ( show status of /dev/rfd1a ).
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <machine/fdio.h>

/*************************************************
 * DATA STRUCTURE
 *************************************************/
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
#ifndef	ORIGINAL_CODE
	#define	MODE_HD		0x0
	#define	MODE_DD		0x1
	#define	MODE_144	0x2
	int	imode;		/* interface mode. 1.44,HD or DD */
	int	interleave;	/* interleave factor. default=1 */
#endif	/* PC-98 */
};

#define	FDC_500KBPS	0x00	/* 500KBPS MFM drive transfer rate */
#define	FDC_300KBPS	0x01	/* 300KBPS MFM drive transfer rate */

struct fd_type fd_types[] = {
{18,2,36,2,0xff,0xbf,0x1b,0x6c,80,2880,1,FDC_500KBPS, "1.44MB",   MODE_144,1},
{15,2,30,2,0xff,0xbf,0x1b,0x54,80,2400,1,FDC_500KBPS, "1.2MB 2HC",MODE_HD,1},
{8, 2,16,3,0xff,0xbf,0x35,0x74,77,1232,1,FDC_500KBPS, "1.2MB 2HD",MODE_HD,1},
{9, 2,18,2,0xff,0xbf,0x2a,0x50,80,1440,1,FDC_300KBPS, "720KB/x",  MODE_DD,1},
#ifdef	WE_NEEDS_640K
{8, 2,16,2,0xff,0xbf,0x2a,0x50,80,1280,1,FDC_300KBPS, "640KB/x",  MODE_DD,1},
#endif	/* WE_NEEDS_640K */
};

#define	FD_TABLE_SIZE	(sizeof(fd_types) / sizeof(struct fd_type))
#define	FD_TABLE_LAST	(&fd_types[FD_TABLE_SIZE])

/* flags */
#define	INTERLEAVE	1
#define	DENSITY		2
#define	MODE		4
#define	FORMAT		8
#define	SHOW		16
#define	HELP		32
#define	FORCE144M	64

/* static data */
int flags;
char buff[FILENAME_MAX + 20];
char *fname;

static void usage __P((int));
static void help __P((void));
static char *get_dev_name __P((char *));
static int density_s2n __P((u_char *));
static int set_density __P((int, u_char *));
static int get_density __P((int));
static int get_mode __P((int, int *));
static void show __P((int));
static int set_interleave __P((int, char *));
static int do_format __P((int));
static int force_144M __P((int, char *));
static int set_mode __P((int, char *));
int main __P((int, char **));

/*************************************************
 * Util Func
 *************************************************/
static void
usage(fdfcmd)
	int fdfcmd;
{

	if (fdfcmd)
		(void)fprintf(stderr,
		    "usage: fdformat [-d density] device\n");
	else
		(void)fprintf(stderr,
		    "usage: fdc [-vhf][-ae ON|OFF][-i interleave][-d density]"
		    " device\n");
}

static void
help(void)
{

	printf("Device  name format: /dev/rfd0a or rfd0a or rfd0 or fd0 or 0\n");
	printf("Density name       : 1.44(1) 2HD(hd) 2HC(hc) 720(7) 640(6)\n");
}

static char *
get_dev_name(s)
	char *s;
{
	u_int c;
	int n;

	if (*s =='/' || *s == '.')
	{
		strncpy(buff, s, FILENAME_MAX);
		return buff;
	}
	else if (strncmp(s, "fd", 2) == 0)
		sprintf(buff, "/dev/r%s", s);
	else if (strncmp(s, "rfd", 3) == 0)
		sprintf(buff, "/dev/%s", s);
	else if (strlen(s) == 1 && isdigit((u_int)buff[0]))
		sprintf(buff, "/dev/rfd%sa", buff);
	else
		sprintf(buff, "/dev/rfd%s", s);

	n = strlen(buff);
	c = buff[n - 1];
	if (c == 'a')
		return buff;
	if (isdigit(c))
		strcat(buff, "a");
	else
		strcat(buff, "0a");
	return buff;
}

static int
density_s2n(s)
	u_char *s;
{
	int i, n = strlen(s);

	for (i = 0; i < n; i++)
		*(s + i) = (u_char)toupper((u_int)*(s + i));
	for (i = 0; i < FD_TABLE_SIZE; i++)
		if (strstr(fd_types[i].name, s))
			return i;
	return -1;
}

static int
set_density(fd, s)
	int fd;
	u_char *s;
{
	int num, error = 0;

	num = density_s2n(s);
	if (num < 0)
	{
		printf("fdc: density mistaken\n");
		help();
	}
	else if (error = ioctl(fd, FDIOCSDENSITY, &num))
		perror("fdc");
	return error;
}

static int
get_density(fd)
	int fd;
{
	int res;

	if (ioctl(fd, FDIOCGDENSITY, &res))
		return -1;
	return res;
}

static int
get_mode(fd, mode)
	int fd;
	int *mode;
{
	int error;

	*mode = 0;
	if (error = ioctl(fd, FDIOCGMODE, mode))
	{
		perror("fdc");
		return error;
	}
	return error;
}

static void
show(fd)
	int fd;
{
	int num;
	char *sa, *sf;

	num = get_density(fd);
	if (num >= 0)
		printf("fdc: %s density = %s", fname,fd_types[num].name);
	get_mode(fd,&num);
	sa = (num & FDIOM_ADCHK) ? "auto" : "fixed";
	sf = (num & FDIOM_144M) ? "on" : "off";
	printf("(%s), 1.44M = %s, ", sa, sf);
	printf("interleave = %d\n", num & FDIOM_IRMASK);
}

static int
set_interleave(fd, is)
	int fd;
	char *is;
{
	int error, mode, interleave;

	interleave = atoi(is);
	if (interleave < 0 || interleave > 0x10)
	{
		printf("fdc: interleave too large\n");
		return -1;
	}
	if (error = get_mode(fd, &mode))
		return error;
	mode &= ~FDIOM_IRMASK;
	mode |= interleave;
	if (error = ioctl(fd, FDIOCSMODE, &mode))
		perror("fdc");
	return error;
}

static int
do_format(fd)
	int fd;
{
	char c;
	int num, i, error;

	show(fd);
	fflush(stdin);
	printf("Format? [y/n] ");
	c = getchar();
	if (c != 'y' && c != '\n')
		return 0;
	num = get_density(fd);
	if (num < 0)
		return 1;
	for (i = 0; i < fd_types[num].tracks * fd_types[num].heads; i++)
	{
		if (error = ioctl(fd, FDIOCFORMAT, &i))
		{
			perror("fdc");
			return 1;
		}
		printf("F");
		fflush(stdout);
	}
	printf("fdc: Format completed. Total steps = %d.\n", i);
	return 0;
}

static int
force_144M(fd, on)
	int fd;
	char *on;
{
	int error, mode;

	if (error = ioctl(fd, FDIOCGMODE, &mode))
		perror("fdc");
	if (on)
		mode |= FDIOM_144M;
	else
		mode &= ~FDIOM_144M;
	if (error = ioctl(fd, FDIOCSMODE, &mode))
		perror("fdc");
	return error;
}

static int
set_mode(fd, on)
	int fd;
	char *on;
{
	int error, mode;

	if (error = ioctl(fd, FDIOCGMODE, &mode))
		perror("fdc");
	if (on)
		mode |= FDIOM_ADCHK;
	else
		mode &= ~FDIOM_ADCHK;
	if (error = ioctl(fd, FDIOCSMODE, &mode))
		perror("fdc");
	return error;
}

/**********************************************
 * Main
 *********************************************/
int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *__progname;
	int ch;
	char *is, *ds, *f144, *ms, *p;
	int fd, fdfcmd;

	if (!strcmp(__progname, "fdformat")) {
		fdfcmd = 1;
		p = "fd:";
		flags |= FORMAT;	/* force flag f */
	} else {
		fdfcmd = 0;
		p = "a:e:vhfi:d:";
	}

	opterr = 0;
	while ((ch = getopt(argc, argv, p)) != EOF)
	{

		switch (ch) {
		case 'a':
			ms = optarg;
			flags |=MODE;
			break;
		case 'e':
			f144 = optarg;
			flags |=FORCE144M;
			break;
		case 'i':
			is = optarg;
			flags |=INTERLEAVE;
			break;
		case 'd':
			ds = optarg;
			flags |=DENSITY;
			break;
		case 'f':
			flags |=FORMAT;
			break;
		case 'v':
			flags |=SHOW;
			break;
		case 'h':
			flags |=HELP;
			break;
		default:
			usage(fdfcmd);
			exit(1);
			break;
		}
		opterr = 0;
	}
	argc -= optind;
	argv += optind;

	switch (argc)
	{
	case 0:
		fname = "/dev/rfd0a";
		flags |= SHOW;
		break;
	case 1:
		fname = get_dev_name(argv[0]);
		if (!flags)
			flags |= SHOW;
		break;
	default:
		fname = get_dev_name(argv[0]);
		ds = argv[1];
		flags |= DENSITY;
		break;
	}

	fd = open(fname, O_RDONLY, 0777);
	if (fd < 0)
	{
		perror("fdc");
		help();
		exit(1);
	}

	if (flags & FORCE144M)
	{
		char *on = strstr(f144, "ON");

		if (force_144M(fd, on))
			exit(1);
		if (on && (flags & DENSITY) == 0)
		{
			u_char dum[2] = { '1', 0 };

			if (set_density(fd, dum))
				exit(1);
		}
	}
	if (flags & INTERLEAVE)
		if (set_interleave(fd, is))
			exit(1);
	if (flags & DENSITY)
		if (set_density(fd, ds))
			exit(1);
	if (flags & FORMAT)
		if (do_format(fd))
			exit(1);
	if (flags & MODE)
		if (set_mode(fd, strstr(ms, "ON")))
			exit(1);
	if (flags & SHOW)
		show(fd);
	if (flags & HELP)
		help();
	close(fd);
	exit(0);
}

