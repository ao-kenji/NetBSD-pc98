/*	$NecBSD: comctrl.c,v 1.13 1998/12/31 02:30:19 honda Exp $	*/
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
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <i386/Cbus/dev/serial/serio.h>

struct modes {
	const char *name;
	long set;
	long unset;
};

const struct modes cmodes[] = {
	{ "cs5",	CS5, CSIZE },
	{ "cs6",	CS6, CSIZE },
	{ "cs7",	CS7, CSIZE },
	{ "cs8",	CS8, CSIZE },
	{ "cstopb",	CSTOPB, 0 },
	{ "-cstopb",	0, CSTOPB },
	{ "cread",	CREAD, 0 },
	{ "-cread",	0, CREAD },
	{ "parenb",	PARENB, 0 },
	{ "-parenb",	0, PARENB },
	{ "parodd",	PARODD, 0 },
	{ "-parodd",	0, PARODD },
	{ "parity",	PARENB | CS7, PARODD | CSIZE },
	{ "-parity",	CS8, PARODD | PARENB | CSIZE },
	{ "evenp",	PARENB | CS7, PARODD | CSIZE },
	{ "-evenp",	CS8, PARODD | PARENB | CSIZE },
	{ "oddp",	PARENB | CS7 | PARODD, CSIZE },
	{ "-oddp",	CS8, PARODD | PARENB | CSIZE },
	{ "pass8",	CS8, PARODD | PARENB | CSIZE },
	{ "-pass8",	PARENB | CS7, PARODD | CSIZE },
	{ "hupcl",	HUPCL, 0 },
	{ "-hupcl",	0, HUPCL },
	{ "hup",	HUPCL, 0 },
	{ "-hup",	0, HUPCL },
	{ "clocal",	CLOCAL, 0 },
	{ "-clocal",	0, CLOCAL },
	{ "crtscts",	CRTSCTS, 0 },
	{ "-crtscts",	0, CRTSCTS },
	{ "mdmbuf",	MDMBUF, 0 },
	{ "-mdmbuf",	0, MDMBUF },
	{ NULL },
};

/******************************************************
 * Util
 ******************************************************/
char *
get_dev_name(s)
	char *s;
{
	static u_char buff[FILENAME_MAX + 1];
	u_int c;
	int n;

	if (*s =='/' || *s == '.')
	{
		strncpy(buff, s, FILENAME_MAX);
		return buff;
	}
	else if (strncmp(s, "cua", 3) == 0)
		sprintf(buff, "/dev/%s", s);
	else if (strlen(s) == 1 && isdigit((u_int)s[0]))
		sprintf(buff, "/dev/cua%s", s);
	else
		sprintf(buff, "%s", s);

	return buff;
}

/******************************************************
 * info print
 ******************************************************/
#define	on(f)	((tmp&f) != 0)
#if	0
#define put(n, f, d) { if (on(f) != d) printf("%s ", (n + on(f))); }
#else
#define put(n, f, d) { printf("%s ", (n + on(f))); }
#endif

void
show(fn, cctrl, swflags)
	u_char *fn;
	struct serhw_ctrl *cctrl;
	u_int swflags;
{
	struct termios *tios = &cctrl->ser_tios;
	u_int tmp = tios->c_cflag;
	u_char *s;

	printf("target: %s ", fn);

	if (cctrl->ser_flags & SER_CTRL_FIXED)
		s = "hwfix";
	else
		s = "-hwfix";

	printf("ctrl mode: %s\n", s);
	printf("ispeed: %d ospeed: %d\n", tios->c_ispeed, tios->c_ospeed);

	if (swflags & TIOCFLAG_SOFTCAR)
		s = "softcar";
	else
		s = "-softcar";
	printf("carrier: %s\n", s);

	printf("c_cflags: ");

	put("-cread", CREAD, 1);
	switch (tmp & CSIZE)
	{
	case CS5:
		s = "cs5";
		break;

	case CS6:
		s = "cs6";
		break;

	case CS7:
		s = "cs7";
		break;

	case CS8:
		s = "cs8";
		break;
	}

	printf("%s ", s);
	put("-parenb", PARENB, 1);
	put("-parodd", PARODD, 0);
	put("-hupcl", HUPCL, 1);
	put("-clocal", CLOCAL, 0);
	put("-cstopb", CSTOPB, 0);
	put("-crtscts", CRTSCTS, 0);
	put("-mdmbuf", MDMBUF, 0);

	printf("\n");
}

/******************************************************
 * main
 ******************************************************/
int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	extern int opterr, optind;
	extern char *optarg;
	struct serhw_ctrl cctrl;
	struct termios *tios = &cctrl.ser_tios;
	u_char *fn = NULL;
	int fd, error, shoot, verbose;
	u_int swflags;

	verbose = shoot = 0;
	opterr = 0;
	while (optind < argc &&
		strspn(argv[optind], "-vf") == strlen(argv[optind]) &&
		(ch = getopt(argc, argv, "vf")) != -1)
	{

		switch (ch)
		{
		case 'f':
			fn = argv[optind];
			optind ++;
			break;

		case 'v':
			verbose = 1;
			break;

		default:
			break;
		}
		opterr = 0;
	}

	argc -= optind;
	argv += optind;

	if (fn == NULL)
		fn = "/dev/cua0";
	else
		fn = get_dev_name(fn);

	fd = open(fn, O_RDWR | O_NONBLOCK, 0);
	if (fd < 0)
	{
		perror(fn);
		exit(1);
	}

	error = ioctl(fd, SERHWIOCGTERMIOS, &cctrl);
	if (error)
	{
		perror("ioctl(CHWIOCGTERMIOS)");
		exit(1);
	}

	error = ioctl(fd, TIOCGFLAGS, &swflags);
	if (error)
	{
		perror("ioctl(TIOCGFLAGS)");
		exit(1);
	}

	for ( ; *argv; ++argv)
	{
		const struct modes *mp;
		int hit;

		hit = 0;

		if (strcmp("hwfix", *argv) == 0)
		{
			cctrl.ser_flags |= SER_CTRL_FIXED;
			hit = 1;
		}

		if (strcmp("-hwfix", *argv) == 0)
		{
			cctrl.ser_flags &= ~SER_CTRL_FIXED;
			hit = 1;
		}

		if (strcmp("softcar", *argv) == 0)
		{
			swflags |= TIOCFLAG_SOFTCAR;
			hit = 1;
		}

		if (strcmp("-softcar", *argv) == 0)
		{
			swflags &= ~TIOCFLAG_SOFTCAR;
			hit = 1;
		}

		for (mp = cmodes; mp->name; ++mp)
		{
			if (strcmp(mp->name, *argv) == 0)
			{
				tios->c_cflag &= ~mp->unset;
				tios->c_cflag |= mp->set;
				hit = 1;
				break;
			}
		}


		if (hit == 0 && isdigit(*argv[0]))
		{
			tios->c_ispeed = tios->c_ospeed = atoi(*argv);
			hit = 1;
		}

		shoot |= hit;
		if (hit == 0)
			printf("<WARNING> unknown option: %s\n", *argv);
	}

	if (shoot)
	{
		error = ioctl(fd, SERHWIOCSTERMIOS, &cctrl);
		if (error)
		{
			perror("ioctl(CHWIOCSTERMIOS)");
			exit(1);
		}

		error = ioctl(fd, TIOCSFLAGS, &swflags);
		if (error)
		{
			perror("ioctl(TIOCSFLAGS)");
			exit(1);
		}
	}
	else
		verbose = 1;

	close(fd);

	fd = open(fn, O_RDWR | O_NONBLOCK, 0);
	if (fd < 0)
	{
		perror(fn);
		exit(1);
	}

	error = ioctl(fd, SERHWIOCGTERMIOS, &cctrl);
	if (error)
	{
		perror("ioctl(CHWIOCGTERMIOS)");
		exit(1);
	}

	error = ioctl(fd, TIOCGFLAGS, &swflags);
	if (error)
	{
		perror("ioctl(TIOCGFLAGS)");
		exit(1);
	}

	if (verbose)
		show(fn, &cctrl, swflags);

	exit(0);
}
