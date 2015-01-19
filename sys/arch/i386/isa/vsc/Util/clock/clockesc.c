/*	$NecBSD: clockesc.c,v 1.14 1998/03/14 07:09:25 kmatsuda Exp $	*/
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

/* N. Honda.
 * Sample prog for hardware status line.
 * You must invoke this from root.( This is demon prog.)
 * You must enable the status line. (i.e. vsc -SON).
 */
/* K. Matsuda. May 09, 1995.
 * If define NEW_VSC_DEV, use /dev/ttyv0 instead of /dev/vga,
 * /dev/ttyv1 instead of /dev/vsc0, and so on.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

#define	TIMEINTERVAL	5
#define	MAX	5

char buf[0x100];
char fname[0x20];
int fd[MAX];
int timeint;

void
main(argc, argv)
	int argc;
	char **argv;
{
	int i;

	if (argc == 2)
	{
		timeint = atoi(argv[1]);
		if (timeint <= 0 || timeint > 60)
			timeint = TIMEINTERVAL;
	}
	else
		timeint = TIMEINTERVAL;

	for (i = 0; i < 20; i++)
		close(i);
	if (fork() == 0)
	{
		tclock();
	}
}

int
openD(num)
	int num;
{

	sprintf(fname, "/dev/ttyv%d", num);
	fd[num] = open(fname, O_RDWR, 0700);
}

void
tclock(void)
{
	int i;
	char *tz;
	time_t clock;

	setsid();	/* release session */
	for (i = 0; i < MAX; i++)
		openD(i);

	buf[0] = 0x1b;	/* ansi esc seq. put strings into status line */
	buf[1] = '_';

	buf[2] = '<';	/* win no service */
	buf[4] = '>';
	buf[5] = ' ';
	while (1)
	{
		time(&clock);
		tz = ctime(&clock);
		strcpy(&buf[6], tz);
		buf[30] = 0x1b;	/* ansi esc seq. terminate strings */
		buf[31] = '\\';
		for (i = 0; i < MAX; i++)
		{
			if (fd[i] >= 0)
			{
				buf[3] = i + '0';
				if (write(fd[i], buf, 32) < 0)
				{
					/* protect against getty's
					 * revoke call
					 */
					close(fd[i]);
					openD(i);
				}
			}
		}
		sleep(timeint);
	}
}
