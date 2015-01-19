/*	$NecBSD: screenmode.c,v 1.10 1999/08/08 01:31:03 kmatsuda Exp $	*/
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
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <machine/vm86bios.h>

#include <machine/vscio.h>

struct screenmode {
	u_char *what;

	u_int32_t reg_eax;
	u_int32_t reg_ebx;
	u_int32_t reg_ecx;

	int lines;
};

struct screenmode screenmodes[] = {
	{"#0 31.5k 30 lines 480 PEGC", 0x300c, 0x3200, 0x0100, 30 },
	{"#1 31.5k 25 lines 480 PEGC", 0x300c, 0x3100, 0x0100, 25 },
	{"#2 25k   25 lines 400 PEGC", 0x3008, 0x2100, 0x0100, 25 },
	{"#3 25k   25 lines 400 EGC",  0x3008, 0x2100, 0x0000, 25 },
};

struct screenmode *smp;
int vmspFd;
int ttyFd;

void
usage(void)
{
	int no;

	printf("usage: screenmode [mode no]\n");
	for (no = 0; no < sizeof(screenmodes) / sizeof(struct screenmode); no ++)
		printf("%s\n", screenmodes[no].what);
	exit(0);
}

int
main(argc, argv)
	int argc;
	u_char **argv;
{
	u_char *fname = "/dev/vm86bios";
	int no, error;

	vmspFd = open(fname, O_RDWR, 0);
	if (vmspFd < 0)
	{
		perror(fname);
		exit(1);
	}

	ttyFd = open("/dev/ttyv0", O_RDWR, 0);
	if (ttyFd < 0)
	{
		perror("/dev/ttyv0");
		exit(1);
	}

	if (argc < 2)
		usage();

	no = *(argv[1]) - '0';
	if (no >= sizeof(screenmodes) / sizeof(struct screenmode))
		usage();

	smp = &screenmodes[no];

	error = screen_exec();

	return error;
}

int
screen_exec(void)
{
	struct vm86_call vm86_arg;
	struct sigcontext *scp = &vm86_arg.vc_reg;
	int error;

	vm86_arg.vc_intno = 0x18;
	vm86_arg.vc_flags = 0;
	scp->sc_eax = smp->reg_eax;
	scp->sc_ebx = smp->reg_ebx;
	error = ioctl(vmspFd, VM86BIOS_IOC_USRREQ, &vm86_arg);
	if (error)
		return error;

	scp->sc_eax = 0x4d00;
	scp->sc_ecx = smp->reg_ecx;
	error = ioctl(vmspFd, VM86BIOS_IOC_USRREQ, &vm86_arg);
	if (error)
		return error;

	scp->sc_eax = 0x0c00;
	error = ioctl(vmspFd, VM86BIOS_IOC_USRREQ, &vm86_arg);
	if (error)
		return error;

	scp->sc_eax = 0x1100;
	error = ioctl(vmspFd, VM86BIOS_IOC_USRREQ, &vm86_arg);
	if (error)
		return error;

	return 0;
}
