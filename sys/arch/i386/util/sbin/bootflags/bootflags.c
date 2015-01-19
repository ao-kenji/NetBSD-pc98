/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999 NetBSD/pc98 porting staff.
 *  All rights reserved.
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
#include <time.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#define	_KERNEL
#include <machine/bootinfo.h>
#include <machine/bootflags.h>

#include "keycap.h"
static int prase_files __P((struct bootflags_device *));
static u_char * getstring __P((u_char *, u_char *));

/*******************************************************
 * MACRO
 *******************************************************/
#define	MAX_CONFFILE_SIZE 0x10000
#define CAPBUFSZ 1024
#define	MAX_SLOTS 4

/*******************************************************
 * MISC
 *******************************************************/
#define	FLAGSET(name, ref, val)				\
	{						\
		if (kgetflag((name))) 			\
			(ref) |= (val);			\
	 }

#define	VALSET(name, ref) 				\
	{ 						\
		register int tmp = kgetnum((name)); 	\
							\
		if (tmp != -1) 				\
			(ref) = tmp;			\
	}

static u_char *
getstring(name, ref)
	u_char *name, *ref;
{
	char *pos, **str;

	*ref = 0;
	pos = ref;
	str = &pos;
	return kgetstr(name, str);
}

static int
prase_files(bdp)
	struct bootflags_device *bdp;
{
	u_char tbuf[CAPBUFSZ];
	u_char buf[CAPBUFSZ];
	int i, rv;

	bzero(buf, sizeof(buf));
	rv = kgetentnext(buf);
	if (rv <= 0)
		return ENOENT;

	ksetup(buf);
	buf[CAPBUFSZ - 1] = 0;

	bzero(bdp->bd_name, sizeof(bdp->bd_name));
	if (getstring("dv", tbuf) == NULL)
		return EINVAL;

	strncpy(bdp->bd_name, tbuf, sizeof(bdp->bd_name) - 1);
	for (i = 0; i < 8; i ++)
		bdp->bd_loc[i] = -1;
	VALSET("sel", bdp->bd_loc[0]);
	VALSET("io", bdp->bd_loc[2]);
	VALSET("iomem", bdp->bd_loc[4]);
	VALSET("iosiz", bdp->bd_loc[5]);
	VALSET("irq", bdp->bd_loc[6]);
	VALSET("drq", bdp->bd_loc[7]);
	bdp->bd_flags = -1;
	VALSET("flags", bdp->bd_flags);
	return 0;
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int fd, ofd;
	struct stat file_stat;
	u_char *buf;
	u_int filesize, bufsize;
	u_int8_t *bp;
	struct bootflags_info_header *bip;
	struct bootflags_device *bdp;
	int n;

	fd = open(BOOTFLAGS_PATH, O_RDONLY, 0);
	if (fd < 0)
	{
		perror(BOOTFLAGS_PATH);
		goto bad;
	}

	if (fstat(fd, &file_stat) != 0)
		goto bad;

	filesize = (u_int) file_stat.st_size;
	if (filesize > MAX_CONFFILE_SIZE)
		goto bad;

	bufsize = (filesize / 1024 + 2) * 1024;
	buf = malloc(bufsize);
	if (buf == NULL)
		goto bad;

	bzero(buf, bufsize);
	lseek(fd, (off_t) 0, 0);
	if (read(fd, buf, filesize) != filesize)
	{
		free(buf);
		goto bad;
	}

	ConfBuf = buf;
	ConfBufSize = filesize;
	close(fd);

	bp = malloc(sizeof(*bip) + sizeof(*bdp) * BOOTFLAGS_BTINFO_MAXENTRY);
	bip = (void *) bp;
	bdp = (void *) (bp + sizeof(*bip));
	bip->bi_signature = BOOTFLAGS_SIGNATURE;
	bip->bi_nentry = 0;

	kinit();
	for (n = 0; n < BOOTFLAGS_BTINFO_MAXENTRY; n ++, bdp ++)
	{
		if (prase_files(bdp) == 0)
			bip->bi_nentry ++;
		else
			break;
	}

	fd = open("/etc/bootflags.db", O_RDWR | O_CREAT | O_TRUNC, 0);
	if (fd < 0)
	{
		perror("/etc/bootflags.db");
		goto bad;
	}
	write(fd, bp, sizeof(*bip) + sizeof(*bdp) * n);
	close(fd);

bad:
	exit(1);
}
