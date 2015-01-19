/*	$NecBSD: clock.c,v 1.22 1998/03/14 07:09:24 kmatsuda Exp $	*/
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
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include <machine/vscio.h>
#include <machine/apmvar.h>

#define	TIMEINTERVAL	5
#define	VSCMAX	10
#define	VSCCOL	80

#define	URGENT_COLOR	redblink
#define	NORMAL_COLOR	sky

u_char *red = "[0;31m";
u_char *redblink = "[0;31;5m";
u_char *green = "[0;32m";
u_char *sky = "[0;36m";
u_char *def = "[0m";
u_char *ccolor;

struct status_line st;
struct status_line st_attr;
struct status_line st_def_attr;

int vscfd[VSCMAX];
int apmFd;

int timeint;

u_char fname[FILENAME_MAX];

/****************************************************
 * INFO
 ****************************************************/
int write_status_line __P((int, struct status_line *, struct status_line *));
int mkstring __P((u_char *, int));
int tclock __P((void));

int
write_status_line(id, attr, msg)
	int id;
	struct status_line *attr;
	struct status_line *msg;
{

	st_attr.length = strlen(ccolor);
	strcpy(st_attr.msg, ccolor);
	if (ioctl(vscfd[id], VSC_WRITE_ST, attr))
		return errno;

	if (ioctl(vscfd[id], VSC_WRITE_ST, msg))
		return errno;

	if (ioctl(vscfd[id], VSC_WRITE_ST, &st_def_attr))
		return errno;

	return 0;
}

int
mkstring(buf, slen)
	u_char *buf;
	int slen;
{
	struct apm_power_info ainfo;
	int len, error;
	time_t clock;
	u_char *tz;
	u_char apmstring[256];
	u_char timestring[256];

	memset(buf, ' ', slen);

	/* date string */
	time(&clock);
	tz = ctime(&clock);
	len = strlen(tz);
	strncpy(timestring, tz, len - 1);
	timestring[len] = 0;

	/* apm status string */
	if (apmFd >= 0)
	{
		error = ioctl(apmFd, APM_IOG_POWER, &ainfo);
		if (error == 0)
		{
			if (apm_crit_exec(&ainfo))
				ccolor = URGENT_COLOR;
			else
				ccolor = NORMAL_COLOR;

			apm_sprint_power(&ainfo, apmstring);
		}
		else
			goto out;

		sprintf(buf, "vt[ ]  %s  %s", timestring, apmstring);
		return 0;
	}

out:
	sprintf(buf, "vt[ ]  %s", timestring);
	return 0;
}

int
tclock(void)
{
	struct apm_power_info ainfo;
	int id, error;

	apmFd = open("/dev/apm", O_RDONLY, 0);
	if (apmFd >=0)
	{
		if (ioctl(apmFd, APM_IOG_POWER, &ainfo))
		{
			close(apmFd);
			apmFd = -1;
		}
	}

	for (id = 0; id < VSCMAX; id++)
	{
		sprintf(fname, "/dev/ttyv%d", id);
		vscfd[id] = open(fname, O_RDWR, 0);
	}

	st.length = VSCCOL;

	st_def_attr.length = strlen(def);
	strcpy(st_def_attr.msg, def);

	while (1)
	{
		mkstring(st.msg, VSCCOL);

		for (id = 0; id < VSCMAX; id ++)
		{
			if (vscfd[id] < 0)
				continue;

			st.msg[3] = id + '0';

			error = write_status_line(id, &st_attr, &st);
			if (error)
			{
 				if (error == ENOTTY)
				{
					sprintf(fname, "/dev/ttyv%d", id);
					close(vscfd[id]);
					vscfd[id] = open(fname, O_RDWR, 0);
				}

			}
		}

		sleep(timeint);
	}

	exit(0);
}

/****************************************************
 * MAIN
 ****************************************************/
int
main(argc, argv)
	int argc;
	char **argv;
{
	int i, ch;
	extern int opterr, optind;
	extern char *optarg;

	timeint = TIMEINTERVAL;

	opterr = 0;
	while ((ch = getopt(argc, argv, "t:")) != EOF)
	{

		switch (ch)
		{
		case 't':
			timeint = atoi(optarg);
			if (timeint <= 0 || timeint > 60)
				timeint = TIMEINTERVAL;
			break;

		}

		opterr = 0;
	}

	argc -= optind;
	argv += optind;

	if (fork() == 0)
	{
		for (i = 0; i < FOPEN_MAX; i++)
			close(i);

		setsid();

		ccolor = NORMAL_COLOR;

		tclock();
	}

	exit(0);
}
