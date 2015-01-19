/*	$NecBSD: systmsubr.c,v 1.12.4.3 1999/08/24 23:36:12 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
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
 * Copyright (c) 1996, 1997, 1998, 1999
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/kthread.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include <sys/mount.h>
#include <sys/filedesc.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <vm/vm_pageout.h>
#include <uvm/uvm.h>

#include <machine/systmbusvar.h>

struct systmtag {
	struct device *smt_dv;

	int (*smt_notify) __P((struct device *, systm_event_t));

	TAILQ_ENTRY(systmtag) smt_chain;
};

TAILQ_HEAD(systmtaghead, systmtag) systmtaghead;

void systmmsg_print_all __P((void));

void
systmmsg_init()
{

	TAILQ_INIT(&systmtaghead);
}

void
systmmsg_bind(dv, fn)
	struct device *dv;
	int (*fn) __P((struct device *, systm_event_t));
{
	struct systmtag *smt;

	smt = malloc(sizeof(*smt), M_DEVBUF, M_NOWAIT);
	if (smt == NULL)
		panic("systmtag: no memory\n");

	memset(smt, 0, sizeof(*smt));
	smt->smt_dv = dv;
	smt->smt_notify = fn;
	TAILQ_INSERT_TAIL(&systmtaghead, smt, smt_chain);
}

int
systmmsg_notify(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct systmtag *smt;
	int error = 0;

	for (smt = systmtaghead.tqh_first; smt; smt = smt->smt_chain.tqe_next)
	{
		if (smt->smt_notify && smt->smt_dv != dv)
			error |= ((*smt->smt_notify) (smt->smt_dv, ev));
	}

	return error;
}

#define	SYSTMSBUR_DEBUG
#ifdef	SYSTMSBUR_DEBUG
void
systmmsg_print_all(void)
{
	struct systmtag *smt;
	struct device *dv;

	for (smt = systmtaghead.tqh_first; smt; smt = smt->smt_chain.tqe_next)
	{
		dv = smt->smt_dv;
		printf("%s device 0x%lx, notify fn 0x%lx\n",
			dv->dv_xname, (u_long) dv, (u_long) smt->smt_notify);
	}
}
#endif	/* SYSTMSBUR_DEBUG */

void systm_kthread_trampoline __P((void *));

struct systm_kthread_dispatch_args {
	struct proc *ka_proc;
	char *ka_ename;
	char *ka_eargs;
};

void
systm_kthread_trampoline(arg)
	void *arg;
{
	struct systm_kthread_dispatch_args *ka = arg;
	struct proc *p;
	vaddr_t addr;
	struct sys_execve_args {
		syscallarg(const char *) path;
		syscallarg(char * const *) argp;
		syscallarg(char * const *) envp;
	} args;
	int i, error;
	register_t retval[2];
	char *slash, *ucp, **uap, *arg0, *arg1, *prog, *opts;

	prog = ka->ka_ename;
	opts = ka->ka_eargs;
	p = ka->ka_proc;
	free(ka, M_TEMP);

	/*
	 * Need just enough stack to hold the faked-up "execve()" arguments.
	 */
	addr = USRSTACK - PAGE_SIZE;
	if (uvm_map(&p->p_vmspace->vm_map, &addr, PAGE_SIZE, 
                    NULL, UVM_UNKNOWN_OFFSET, 
                    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_COPY,
		    UVM_ADV_NORMAL,
                    UVM_FLAG_FIXED|UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW))
		!= KERN_SUCCESS)
	{
		printf("exec %s: uvm allocate failed\n", prog);
		kthread_exit(ENOMEM);
	}
	p->p_vmspace->vm_maxsaddr = (caddr_t) addr;

	ucp = (char *)(addr + PAGE_SIZE);
	if (opts != NULL)
	{
		i = strlen(opts) + 1;
		(void)copyout((caddr_t) opts, (caddr_t)(ucp -= i), i);
		arg1 = ucp;
	}

	i = strlen(prog) + 1;
	(void)copyout((caddr_t)prog, (caddr_t)(ucp -= i), i);
	arg0 = ucp;

	/*
	 * Move out the arg pointers.
	 */
	uap = (char **)((long)ucp & ~ALIGNBYTES);
	(void)suword((caddr_t)--uap, 0);	/* terminator */

	if (opts != NULL)
		(void)suword((caddr_t)--uap, (long)arg1);

	slash = strrchr(prog, '/');
	if (slash)
		(void)suword((caddr_t)--uap,
		    (long)arg0 + (slash + 1 - prog));
	else
		(void)suword((caddr_t)--uap, (long)arg0);

	/*
	 * Point at the arguments.
	 */
	SCARG(&args, path) = arg0;
	SCARG(&args, argp) = uap;
	SCARG(&args, envp) = NULL;

	/*
	 * Now try to exec the program.  If can't for any reason
	 * other than it doesn't exist, complain.
	 */
	error = sys_execve(p, &args, retval);
	if (error == 0 || error == EJUSTRETURN)
		return;
	printf("exec %s: error %d\n", prog, error);
	kthread_exit(error);
}

void
systm_kthread_dispatch(arg)
	void *arg;
{
	struct systm_kthread_dispatch_args *ka;
	struct systm_kthread_dispatch_table *kt = arg;
	struct proc *p2;
	int error;

	for ( ; kt->kt_ename != NULL; kt ++)
	{
		if ((kt->kt_flags & SYSTM_KTHREAD_DISPATCH_OK) == 0)
			continue;

		ka = malloc(sizeof(*ka), M_TEMP, M_WAITOK);
		if (ka == NULL)
			continue; 

		error = fork1(&proc0, 0, NULL, &p2);
		if (error != 0)
		{
			free(ka, M_TEMP);
			continue;
		}

		ka->ka_proc = p2;
		ka->ka_ename = kt->kt_ename;
		ka->ka_eargs = kt->kt_eargs;

		p2->p_flag |= P_INMEM | P_SYSTEM /* | P_NOCLDWAIT */;
		strcpy(p2->p_comm, kt->kt_xname);
		cpu_set_kpc(p2, systm_kthread_trampoline, ka);

		printf("systm_kthread_dispatch: starting %s args=%s\n",
			ka->ka_ename,
			(ka->ka_eargs == NULL) ? "none" : ka->ka_eargs);
	}
}
