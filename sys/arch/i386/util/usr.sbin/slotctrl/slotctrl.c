/*	$NecBSD: slotctrl.c,v 1.16 1999/07/12 16:49:45 honda Exp $	*/
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
 * Pcmcia card information service program.
 * Written by N. Honda.
 * Version 0.0 ALPHA(0)
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/proc.h>

#include <i386/pccs/pccsio.h>

struct ctrl_cmd {
	u_char *name;
	u_int cmd;
	u_int statbit;
} ctrl_cmd[] = {
	{ "up", PCCS_SLOT_DOWN, 0 },
	{ "down", PCCS_SLOT_DOWN, 0 },
	{ "slot.pow", PCCS_SLOT_DOWN, PCCS_SS_POW_UP },
	{ "slot.spkr", PCCS_SPKR_DOWN, PCCS_SS_SPKR_UP },
	{ "slot.autores", PCCS_AUTO_RESUME_DOWN, PCCS_SS_AUTORES_UP },
	{ "slot.autoins", PCCS_AUTO_INSERT_DOWN, PCCS_SS_AUTOINS_UP },
	{ NULL, 0 },
};

char *statstr[SLOT_HASDEV + 1] = {
	"empty", "cardin (power off)", "null", "null", "cardin (power on)",
	"cardin (mapped)", "cardin (device allocated)"
};

int main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	u_char pathname[FILENAME_MAX];
	u_char *path;
	struct ctrl_cmd *cc;
	int no, ctrl, DevFd;
	struct slot_info si;

	path = (argc <= 1) ? "slot0" : argv[1];
	if (strncmp(path, "slot", 4))
	{
		printf("slotctrl slot? [up|down]\n");
		exit(1);
	}

	no = path[4] - '0';
	sprintf(pathname, "/dev/ata%d", no);
	DevFd = open(pathname, O_RDONLY, 777);

	if (DevFd < 0)
	{
		perror("slotctrl");
		exit(1);
	}

	argc -= 2;
	argv += 2;
	for ( ; argc > 0; argc --, argv ++)
	{ 
		for (cc = &ctrl_cmd[0]; cc->name != NULL; cc ++)
		{
			if (strncmp(*argv, cc->name, strlen(cc->name)) == 0)
			{
				ctrl = cc->cmd;
				if (strstr(*argv, "up") != NULL)
					ctrl |= 0x1;
				if (ioctl(DevFd, PCCS_IOC_CTRL, &ctrl))
				{
					perror("slotctrl");
					exit(1);
				}
				break;
			}
		}

		if (strncmp(*argv, "slot.timing", 11) == 0)
		{

			ctrl = PCCS_HW_TIMING;
			if (strstr(*argv, "high") != NULL)
				ctrl |= 3;
			else if (strstr(*argv, "middle") != NULL)
				ctrl |= 2;
			else if (strstr(*argv, "low") != NULL)
				ctrl |= 1;
			else if (strstr(*argv, "default") != NULL)
				ctrl |= 0;
			if (ioctl(DevFd, PCCS_IOC_CTRL, &ctrl))
			{
				perror("slotctrl");
				exit(1);
			}
			break;
		}
	}

	si.si_mfcid = PUNKMFCID;
	if (ioctl(DevFd, PCCS_IOG_SSTAT, &si))
	{
		perror("slotctrl");
		exit(1);
	}

	printf("%s(%s):\n", path, pathname);
	printf("slot.state=%s\n",  statstr[si.si_st]);

	for (cc = &ctrl_cmd[0]; cc->name != NULL; cc ++)
	{
		u_char *s;

		if (cc->statbit == 0)
			continue;

		if (si.si_cst & cc->statbit)
			s = "up";
		else
			s = "down";

		printf("%s=%s\n", cc->name, s);
	}

	return 0;
}
