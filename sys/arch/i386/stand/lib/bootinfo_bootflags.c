/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <machine/cpu.h>

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include "libi386.h"
#include "bootinfo.h"

#define	_KERNEL
#include <machine/bootflags.h>
extern int get_biosparam __P((unsigned short));

void
bi_getbootflags()
{
	int fd;
	struct bootflags_info_header bih;
	struct bootflags_btinfo_header *bbhp;
	struct bootflags_device *bdp;
	u_int8_t *bp;
	u_int32_t bootflags;
	int nent = 0, size;
	u_char *path = "/etc/bootflags.db";

	fd = open(path,  0);
	if (fd < 0)
	{
		printf("bi_getbootflags: %s not found\n", path);
		fd = -1;
	}
	else if (read(fd, &bih, sizeof(bih)) != sizeof(bih))
	{
		printf("bi_getbootflags: header data read failed\n");
		close(fd);
		fd = -1;
	}

	if (fd >= 0)
	{
		if (bih.bi_signature == BOOTFLAGS_SIGNATURE)
		{
			nent = bih.bi_nentry;
			if (nent >= BOOTFLAGS_BTINFO_MAXENTRY)
				nent = BOOTFLAGS_BTINFO_MAXENTRY;
		}
		else
		{
			close(fd);
			fd = -1;
			printf("bi_getbootflags: signature mismatch\n");
		}
	}

	size = sizeof(*bbhp) + (nent * sizeof(*bdp));
	bp = alloc(size);
	if (bp == NULL)
		goto out;

	bzero(bp, size);
	bbhp = (void *) bp;
	bdp = (void *) (bp + sizeof(*bbhp));
	if (fd >= 0 && nent > 0)
	{
		if (read(fd, bp + sizeof(*bbhp), nent * sizeof(*bdp)) 
		    != nent * sizeof(*bdp))
		{
			close(fd);
			fd = -1;
			nent = 0;
			printf("bi_getbootflags: read failed\n");
		}
	}

	bbhp->bb_bi.bi_signature = BOOTFLAGS_SIGNATURE;
	bbhp->bb_bi.bi_nentry = nent;
	bootflags = get_biosparam(BOOTFLAGS_BIOS_OFFSET);
	bbhp->bb_bootflags = bootflags;
	BI_ADD(&bbhp->bb_common, BTINFO_BOOTFLAGS, size);
	printf("bi_getbootflags: bootflags succeeded(%d)\n", nent);

out:
	if (fd >= 0)
		close(fd);
}
