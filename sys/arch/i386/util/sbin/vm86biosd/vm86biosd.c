/*	$NecBSD: vm86biosd.c,v 1.10 1999/08/01 10:53:45 honda Exp $	*/
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
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <machine/vm86.h>
#include <machine/psl.h>
#include <machine/pio.h>
#include <machine/vm86bios.h>

/* #define	DEBUG */
/**************************************************************
 * VM86 space setup
 **************************************************************/
#define	VM86IOPERM	(0x10000 / NBBY)
#define	VM86STACKSZ	0x1000
#define	I386_IOPL_ASSERT	(0)	/* if any trouble, set (3) ! */

struct stack
{
	u_char dummy[VM86STACKSZ];
	u_char bot;
} stack;

/* vm86 excution code */
u_char Xint[] = { 0xcd, 0xff, 0xcd, 0xff, 0xcf };

/* vm86 io permission */
u_char ioperm[VM86IOPERM];

/**************************************************************
 * variable
 **************************************************************/
struct vm86_call vm86_arg;
struct vm86_struct vm86;

int vmspFd;

#define	_PATH_LOGPID	"/var/run/vm86biosd.pid"
char *PidFile = _PATH_LOGPID;

u_char sig_on_stack[SIGSTKSZ + 4];

/**************************************************************
 * Signal
 **************************************************************/
int exec_vm86_task __P((void));
int gp_fault __P((int, struct sigcontext *));
static inline u_int8_t *get_code_pointer __P((struct sigcontext *));
static inline void set_code_pointer __P((struct sigcontext *, u_int32_t));

static inline u_int8_t *
get_code_pointer(scp)
	struct sigcontext *scp;
{

	return (u_int8_t *)
		(((scp->sc_cs & 0xffff) << 4) + (scp->sc_eip & 0xffff));
}

static inline void
set_code_pointer(scp, step)
	struct sigcontext *scp;
	u_int32_t step;
{

	scp->sc_eip = scp->sc_eip + step;
}

#define	SETBYTE(reg, val) \
	(reg = ((((u_int32_t) val) & 0xff) | (reg & 0xffffff00)))
#define	SETWORD(reg, val) \
	(reg = ((((u_int32_t) val) & 0xffff) | (reg & 0xffff0000)))

int
gp_fault(code, scp)
	int code;
	struct sigcontext *scp;
{
	u_int8_t *eip;
	int error = 0;
	int data32, code32, segPrefix;

	eip = get_code_pointer(scp);

#ifdef	DEBUG
	printf("code %x, eip %x, inst %x\n", code, eip, *eip);
#endif

	i386_iopl(3);
	switch (VM86_TYPE(code))
	{
	case VM86_UNKNOWN:
		for (data32 = code32 = segPrefix = 0; ; )
		{
			switch(*eip)
			{
			case 0x66:
				data32 = 1;
				eip ++;
				set_code_pointer(scp, 1);
				continue;

			case 0x67:
				code32 = 1;
				eip ++;
				set_code_pointer(scp, 1);
				continue;

			default:
				break;
			}
			break;
		}

		switch (*eip)
		{
		case 0xe4: /* inb */
			SETBYTE(scp->sc_eax, inb(eip[1]));
			set_code_pointer(scp, 2);
			break;

		case 0xe5: /* inw */
			if (data32)
				scp->sc_eax = inl(eip[1]);
			else
				SETWORD(scp->sc_eax, inw(eip[1]));
			set_code_pointer(scp, 2);
			break;

		case 0xe6: /* outb */
			outb(eip[1], scp->sc_eax);
			set_code_pointer(scp, 2);
			break;

		case 0xe7: /* outw */
			if (data32)
				outl(eip[1], scp->sc_eax);
			else
				outw(eip[1], scp->sc_eax);
			set_code_pointer(scp, 2);
			break;

		case 0xec: /* inb */
			SETBYTE(scp->sc_eax, inb(scp->sc_edx));
			set_code_pointer(scp, 1);
			break;

		case 0xed: /* inw */
			if (data32)
				scp->sc_eax = inl(scp->sc_edx);
			else
				SETWORD(scp->sc_eax, inw(scp->sc_edx));
			set_code_pointer(scp, 1);
			break;

		case 0xee:
			outb(scp->sc_edx, scp->sc_eax);
			set_code_pointer(scp, 1);
			break;

		case 0xef:
			if (data32)
				outl(scp->sc_edx, scp->sc_eax);
			else
				outw(scp->sc_edx, scp->sc_eax);
			set_code_pointer(scp, 1);
			break;

		case 0xf4:/* hlt */
			set_code_pointer(scp, 1);
			break;

		default:
			syslog(LOG_NOTICE, "invalid op 0x%x", (u_int) *eip);
			error = EINVAL;
			break;
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	i386_iopl(I386_IOPL_ASSERT);
	return error;
}

/**************************************************************
 * Signal
 **************************************************************/
void
sig_vm86(sig, code, scp)
	int sig, code;
	struct sigcontext *scp;
{
	int error;

	if (VM86_TYPE(code) != VM86_INTx || VM86_ARG(code) != 0xff)
	{
		error = gp_fault(code, scp);
		if (error)
		{
			syslog(LOG_NOTICE, "unsupported gp fault");
			exit(1);
		}
		return;
	}

#if	0
	printf("eax %x, ebx %x ecx %x edx %x flags %x\n",
		scp->sc_eax, scp->sc_ebx, scp->sc_ecx, scp->sc_edx,
		scp->sc_eflags);
#endif

	vm86_arg.vc_error = 0;
	vm86_arg.vc_reg = *scp;
	error = ioctl(vmspFd, VM86BIOS_IOC_TASKDONE, &vm86_arg);
	if (error)
		exit(1);
	exit(0);
}

/**************************************************************
 * Main
 **************************************************************/
int
main(argc, argv)
	int argc;
	u_char **argv;
{
	u_char *fname = "/dev/vm86bios";
	pid_t pid;
	int error, stat;
	FILE *fp;

	vmspFd = open(fname, O_RDWR, 0);
	if (vmspFd < 0)
	{
		perror(fname);
		exit(1);
	}

	if (fork() == 0)
	{
		int tmpfd;

		chdir("/");
		setproctitle("vm86thread");

		for (tmpfd = 0; tmpfd < FOPEN_MAX; tmpfd++)
			close(tmpfd);

		setsid();

		vmspFd = open(fname, O_RDWR, 0);
		if (vmspFd < 0)
		{
			syslog(LOG_WARNING, "open error %s\n", fname);
			exit(1);
		}

		if (error = ioctl(vmspFd, VM86BIOS_IOC_CONNECT, 0))
		{
			syslog(LOG_WARNING, "connect busy\n");
			exit(1);
		}

		fp = fopen(PidFile, "w");
		if (fp != NULL)
		{
			fprintf(fp, "%d\n", getpid());
			(void) fclose(fp);
		}

		while (1)
		{
			error = ioctl(vmspFd, VM86BIOS_IOC_WAIT);
			if (error < 0)
				exit(1);

			error = ioctl(vmspFd, VM86BIOS_IOG_TASK, &vm86_arg);
			if (error || vm86_arg.vc_id == NULL)
				continue;

			if ((pid = fork()) == 0)
			{
				error = exec_vm86_task();
				exit(error);
			}

			if (pid > 0)
			{
				wait(&stat);
				if (WIFEXITED(stat) && WEXITSTATUS(stat) == 0)
					continue;
			}

			vm86_arg.vc_error = EIO;
			ioctl(vmspFd, VM86BIOS_IOC_TASKDONE, &vm86_arg);
		}
	}

	exit(0);
}

int
exec_vm86_task(void)
{
	struct sigcontext *vmsc;
	struct sigaltstack nss;
	struct sigaction sa;
	int error, stat;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = sig_vm86;
	sa.sa_flags = SA_ONSTACK;
	sigaction(SIGURG, &sa, NULL);

	nss.ss_sp = sig_on_stack;
	nss.ss_size = SIGSTKSZ;
	nss.ss_flags = 0;
	sigaltstack(&nss, NULL);

	error = (int) mmap((caddr_t) VM86BIOS_IMAPBASE, VM86BIOS_IMAPSIZE,
			   PROT_READ | PROT_WRITE, MAP_FILE | MAP_FIXED,
			   vmspFd, (off_t) VM86BIOS_IMAPBASE);

	if (error < 0)
	{
		syslog(LOG_NOTICE, "mmap(vector) failed");
		exit(1);
	}

	error = (int) mmap((caddr_t) VM86BIOS_BMAPBASE, VM86BIOS_BMAPSIZE,
			PROT_WRITE | PROT_READ | PROT_EXEC,
			MAP_FILE | MAP_FIXED,
			vmspFd, (off_t) VM86BIOS_BMAPBASE);

	if (error < 0)
	{
		syslog(LOG_NOTICE, "mmap(bios) failed");
		exit(1);
	}

	error = i386_iopl(I386_IOPL_ASSERT);
	if (error)
	{
		syslog(LOG_NOTICE, "i386_iopl failed");
		exit(1);
	}

	error = i386_set_ioperm(ioperm);
	if (error)
	{
		syslog(LOG_NOTICE, "i386_set_ioperm failed");
		exit(1);
	}

	/* setup int no */
	Xint[1] = vm86_arg.vc_intno;

	/* dummy write and allocate pages*/
	bzero(stack.dummy, VM86STACKSZ);

	vmsc = &vm86.substr.regs.vmsc;

	bzero(&vm86, sizeof(vm86));
	vm86.int_byuser[31] = 0xff;
	vm86.substr.ss_cpu_type = VCPU_386;

	/* setup stack */
	vmsc->sc_ss = ((u_int32_t) &stack) / 0x10;
	vmsc->sc_ds = ((u_int32_t) &stack.bot) /0x10;
	vmsc->sc_es = ((u_int32_t) &stack.bot) /0x10;
	vmsc->sc_esp = VM86STACKSZ - 4;
	vmsc->sc_eflags = 0x23200;

	/* setup code */
	vmsc->sc_cs = ((u_int32_t) Xint) / 0x10;
	vmsc->sc_eip = ((u_int32_t) Xint) % 0x10;
	vmsc->sc_eax = vm86_arg.vc_reg.sc_eax;
	vmsc->sc_ebx = vm86_arg.vc_reg.sc_ebx;
	vmsc->sc_ecx = vm86_arg.vc_reg.sc_ecx;
	vmsc->sc_edx = vm86_arg.vc_reg.sc_edx;
	vmsc->sc_edi = vm86_arg.vc_reg.sc_edi;
	vmsc->sc_esi = vm86_arg.vc_reg.sc_esi;
	vmsc->sc_ebp = vm86_arg.vc_reg.sc_ebp;

	error = i386_vm86(&vm86);
	if (error)
	{
		syslog(LOG_NOTICE, "check VM86 kernel config options");
		return 1;
	}

	return 1;
}
