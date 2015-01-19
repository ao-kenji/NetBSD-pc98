/*	$NecBSD: idectrl.c,v 1.10 1998/03/14 07:01:27 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
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
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <i386/Cbus/dev/atapi/whdio.h>

void usage __P(());
int show_status __P((int));

struct idecmd {
	u_char *cmdname;
	u_int cmd;
	u_int narg;
	u_int8_t feat;
};

struct idecmd idecmd[] = {
	{"ron", IDEDRV_RCACHE, 0, IDEDRV_RCACHE_FEAT_ON},
	{"roff", IDEDRV_RCACHE, 0, IDEDRV_RCACHE_FEAT_OFF},
	{"won", IDEDRV_WCACHE, 0, IDEDRV_WCACHE_FEAT_ON},
	{"woff", IDEDRV_WCACHE, 0, IDEDRV_WCACHE_FEAT_OFF},
	{"pow", IDEDRV_POWSAVE, 1, 0},
	{0,},
};

struct idecmd status_idecmd[] = {
	{"read cache", IDEDRV_RCACHE, 0, IDEDRV_RCACHE_FEAT_ON},
	{"write cache", IDEDRV_WCACHE, 0, IDEDRV_RCACHE_FEAT_OFF},
	{"power down", IDEDRV_POWSAVE, 1, 0},
	{0,},
};

void
usage()
{
	printf("usage: idectrl wd? cmd [argument]\n");
	printf("status  -- show current status.\n");
	printf("ron     -- read cache on.\n");
	printf("roff    -- read cache off.\n");
	printf("won     -- write cache on.\n");
	printf("woff    -- write cache off.\n");
	printf("pow n   -- auto power down after n mins.\n");
}

int 
show_status(fd)
	int fd;
{
	struct idedrv_ctrl dinfo;
	struct idecmd *cp = &status_idecmd[0];

	for ( ; cp->cmdname != NULL; cp ++)
	{
		bzero(&dinfo, sizeof(dinfo));
		dinfo.idc_cmd = cp->cmd;
		if (ioctl(fd, IDEIOGMODE, &dinfo) < 0)
		{
			perror("ioctl");
			exit(1);
		}

		if (cp->narg)
			printf("%s(%d) ", cp->cmdname, dinfo.idc_secc);
		else if (dinfo.idc_feat == 0)
			printf ("%s(on) ", cp->cmdname);
		else
			printf ("%s(off) ", cp->cmdname);
	}

	printf("\n");
	return 0;
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int fd;
	u_char pathname[FILENAME_MAX];
	u_char *s;
	struct idedrv_ctrl dinfo;
	struct idecmd *cp;

	if (argc < 2)
	{
		usage();
		exit(1);
	}

	s = argv[1];
	if (s[0] != 'w' || s[1] != 'd')
	{
		usage();
		exit(1);
	}

	sprintf(pathname, "/dev/r%sd", s);
	fd = open(pathname, O_RDONLY, 0);
	if (fd < 0)
	{
		perror(pathname);
		exit(1);
	}

	if (argc == 2 || strcasecmp(argv[2], "status") == 0)
	{
		show_status(fd);
		exit(0);
	}

	for (cp = &idecmd[0]; cp->cmdname != NULL; cp ++)
	{
		if (strcasecmp(cp->cmdname, argv[2]) != 0)
			continue;

		dinfo.idc_cmd = cp->cmd;
		dinfo.idc_feat = cp->feat;
		if (cp->narg)
		{
			if (argc < 4)
			{
				printf("%s: too few argument\n", argv[0]);
				exit(1);
			}
			dinfo.idc_secc = atoi(argv[3]);
		}
		else
			dinfo.idc_secc = 0;
		break;
	}

	if (cp->cmdname == NULL)
	{
		printf("%s: cmd invalid\n", argv[0]);
		usage();
		exit(1);
	}

	if (ioctl(fd, IDEIOSMODE, &dinfo) < 0)
	{
		perror("ioctl");
		exit(1);
	}

	return 0;
}
