/*	$NecBSD: loadascii.c,v 1.8 1999/08/08 01:30:44 kmatsuda Exp $	*/
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
 * N. Honda
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/device.h>

#include <machine/vscio.h>

#include "fontascii.h"

#define	DEVTTY	"/dev/ttyv0"

void
main(void)
{
	int i;
	int fd, error;

	fd= open(DEVTTY, O_RDWR, 0777);
	if (fd < 0)
	{
		perror("open");
	}

#ifdef	ALMOSTFULL
	/* unload JIS83 font */
	if (ioctl(fd, VSC_RESET_FONT))
	{
		perror("ioctl");
	}
#endif	/* ALMOSTFULL */

	for (i = 0; fontasciilist[i].code != 0; i++)
	{
		error= ioctl(fd, VSC_LOAD_FONT, &fontasciilist[i]);
		if (error < 0)
		{
			perror("ioctl");
			printf("fontcode %x\n", i);
		}
	}
}
