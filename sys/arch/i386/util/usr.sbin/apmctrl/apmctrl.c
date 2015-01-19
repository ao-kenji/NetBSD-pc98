/*	$NecBSD: apmctrl.c,v 1.8 1998/03/14 07:11:39 kmatsuda Exp $	*/
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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <machine/apmvar.h>

#include "apmprint.h"

#define	PINFOSZ	1024

/******************************************************
 * main
 ******************************************************/
struct apm_action {
	u_char *key;
	u_long request;
	int mode;
	int narg;
};

struct apm_action apm_action_table[] =
{
	{ "susp", APM_IOC_APM, APM_CTRL_REQ_SUSPEND, 0},
	{ "st", APM_IOC_APM, APM_CTRL_REQ_STANDBY, 0},
	{ "ti", APM_IOC_APM, APM_CTRL_SET_TOUTCNT, 1},
	{ "low", APM_IOC_CPU, CPU_CTRL_ON, 0},
	{ "high", APM_IOC_CPU, CPU_CTRL_OFF, 0},
	{ NULL, NULL, NULL },
};

int main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct apm_power_info info;
	u_char buf[PINFOSZ];
	struct apm_action *ac;
	char *dev_file = "/dev/apm";
	int fd, mode, ch;
	extern int opterr, optind;
	extern char *optarg;

	opterr = 0;
	while ((ch = getopt(argc, argv, "f:")) != EOF)
	{

		switch (ch)
		{
		case 'f':
			dev_file = optarg;
			break;
		}

		opterr = 0;
	}

	argc -= optind;
	argv += optind;

	if ((fd = open(dev_file, O_RDWR)) == -1)
	{
		perror(dev_file);
		exit(1);
	}

	if (argc == 0)
	{
		if (apm_get_power(fd, &info) == 0)
		{
			apm_sprint_power(&info, buf);
			printf("apmctrl: %s\n", buf);
		}
		exit(0);
	}

	for (ac = &apm_action_table[0]; ac->key; ac ++)
	{
		if (strstr(argv[0], ac->key) == NULL)
			continue;

		mode = ac->mode;
		if (ac->narg > 0)
		{
			if (argc < 1 + ac->narg)
			{
				printf("apmctrl: arg required\n");
				exit(1);
			}
			mode |= (atoi(argv[1]) & APM_CTRL_ARG_MASK);
		}

		if (ioctl(fd, ac->request, &mode))
		{
			perror("ioctl");
			exit(1);
		}
		exit(0);
	}

	printf("apmctrl: %s -- unknown cmd\n", argv[0]);
	exit(1);
}
