/*	$NecBSD: devid.c,v 1.10 1998/11/19 16:48:59 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <i386/pccs/pccsio.h>
#include <i386/pccs/tuple.h>

static void showbuf __P((u_char *));
int main __P((int, char **));

int DevFd;

static void
showbuf(buf)
	u_char *buf;
{
	int i, j;

	for (i = 0; i < CARD_ATTR_SZ; i += 0x10)
	{
		printf("\n%4x:", i);
		for (j = 0; j < 0x10; j++)
			printf(" %02x", buf[i + j]);
		printf("\t\t");
		for (j = 0; j < 0x10; j++)
		{
			if (buf[i + j] >= 0x20 && buf[i + j] < 0x7f)
				printf("%c", buf[i + j]);
			else
				printf(" ");
		}
	}
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	u_char buf[CARD_ATTR_SZ];
	u_char pathname[FILENAME_MAX];
	u_char *path;
	int no;
	struct slot_info si;
	off_t offs;

	path = (argc <= 1) ? "slot0" : argv[1];
	if (strncmp(path, "slot", 4))
	{
		printf("cardinfo slot?\n");
		exit(1);
	}
	no = path[4] - '0';
	sprintf(pathname, "/dev/ata%d", no);

	DevFd = open(pathname, O_RDONLY, 777);
	if (DevFd < 0)
	{
		perror("devid");
		exit(1);
	}

	si.si_mfcid = PUNKMFCID;
	if (ioctl(DevFd, PCCS_IOG_SSTAT, &si))
	{
		perror("devid.stat");
		exit(1);
	}

	if (si.si_st == SLOT_NULL)
	{
		printf("no card in slot\n");
		exit(0);
	}

	if (si.si_st < SLOT_READY && ioctl(DevFd, PCCS_IOC_INIT, 0))
	{
		perror("devid.init");
		exit(1);
	}

	if (ioctl(DevFd, PCCS_IOG_ATTR, buf))
	{
		perror("devid");
		exit(1);
	}

	printf("<KERNEL CIS ATTR>");
	showbuf(buf);

	printf("\n\n<DIRECT READ CIS ATTR>");
	offs = (off_t) (u_int) CIS_ATTR_ADDR(0);
	lseek(DevFd, offs, 0);
	read(DevFd, buf, 4096);
	showbuf(buf);

	printf("\n\n<DIRECT READ COMM MEM>");
	offs = (off_t) (u_int) CIS_COMM_ADDR(0x0);
	lseek(DevFd, offs, 0);
	read(DevFd, buf, 4096);
	showbuf(buf);

	exit(0);
}
