/*	$NecBSD: iopinfo.c,v 1.4.16.1 1999/08/16 01:05:05 honda Exp $	*/
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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/device.h>
#include <vm/vm.h>
#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <limits.h>

#define NEWVM		/* XXX till old has been updated or purged */
struct nlist namelist[] = {
	{ "_i386_iomap_array"}, 
	{ "" }
};

/* Objects defined in dkstats.c */
kvm_t *kd;
char	*nlistf, *memf;

void kread __P((int, void *, size_t));
void show_ports __P((void));

int main __P((int, char **));

int
main(argc, argv)
	register int argc;
	register char **argv;
{
	extern int optind;
	extern char *optarg;
	register int c, todo;
	u_int interval;
	int reps;
        char errbuf[_POSIX2_LINE_MAX];

	memf = nlistf = NULL;
	interval = reps = todo = 0;

	if (nlistf != NULL || memf != NULL)
		setgid(getgid());

        kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
	if (kd == 0) {
		(void)fprintf(stderr,
		    "vmstat: kvm_openfiles: %s\n", errbuf);
		exit(1);
	}

	if ((c = kvm_nlist(kd, namelist)) != 0) {
		if (c > 0) {
			(void)fprintf(stderr,
			    "vmstat: undefined symbols:");
			for (c = 0;
			    c < sizeof(namelist)/sizeof(namelist[0]); c++)
				if (namelist[c].n_type == 0)
					fprintf(stderr, " %s",
					    namelist[c].n_name);
			(void)fputc('\n', stderr);
		} else
			(void)fprintf(stderr, "vmstat: kvm_nlist: %s\n",
			    kvm_geterr(kd));
		exit(1);
	}

	show_ports();

	exit(0);
}

void
kread(nlx, addr, size)
	int nlx;
	void *addr;
	size_t size;
{
	char *sym;

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		(void)fprintf(stderr,
		    "vmstat: symbol %s not defined\n", sym);
		exit(1);
	}
	if (kvm_read(kd, namelist[nlx].n_value, addr, size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		(void)fprintf(stderr, "vmstat: %s: %s\n", sym, kvm_geterr(kd));
		exit(1);
	}
}

#define	MAX_IO_ADDR 0x10000

void
show_ports(void)
{
	u_int32_t idx, mask;
	u_int32_t buf[MAX_IO_ADDR / sizeof(u_int32_t)];
	int i;

	kread(0, buf, sizeof(buf));

	printf("  ADDR: ");
	for (i = 0; i < 16; i ++)
		printf("%x", i);
		
	for (i = 0; i < MAX_IO_ADDR; i ++)
	{
		idx = i / (sizeof(u_int32_t) * NBBY);
		mask = 1 << (i % (sizeof(u_int32_t) * NBBY));
		if ((i % 16) == 0)
		{
			if (buf[idx] == 0)
			{
				i += sizeof(u_int32_t) * NBBY - 1;
				continue;
			}
			printf("\n0x%04x: ", i);
		}
		
		if (buf[idx] & mask)
			printf("X");
		else
			printf(" ");
	}
	printf("\n");
}
