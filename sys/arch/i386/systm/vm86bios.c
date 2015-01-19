/*	$NecBSD: vm86bios.c,v 1.12.2.1 1999/08/14 14:41:31 honda Exp $	*/
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
 * Copyright (c) 1996, 1997, 1998, 1999
 *	Naofumi HONDA.  All rights reserved.
 */

#include "opt_vm86biosd.h"

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

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/vm86bios.h>

#include <vm/vm.h>
#include <vm/vm_pageout.h>
#include <uvm/uvm.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <i386/isa/pc98spec.h>

struct vm86_task {
	TAILQ_ENTRY(vm86_task) vt_chain;

#define	VM86TASK_DONE		0x01
#define	VM86TASK_INPROGRESS	0x02
#define	VM86TASK_ERROR		0x04
#define	VM86TASK_ASYNC		0x08
	u_int vt_flags;

	int vt_intno;
	struct sigcontext vt_reg;

	void (*vt_callback) __P((void *));
	void *vt_arg;
};


TAILQ_HEAD(vm86_task_tqtab, vm86_task);

struct vm86bios_softc {
	struct device sc_dev;

	struct vm86_task_tqtab sc_vt_waittab;
	struct vm86_task_tqtab sc_vt_freetab;

#define	VM86BIOS_WAIT	0x0001
#define	VM86BIOS_SWAIT	0x0002		/* start wait */
	u_int sc_flags;
	u_int sc_cnt;

	u_int sc_totalvt;

	struct vm86_task *sc_nexus;

	struct pgrp *sc_pgrp;
	pid_t sc_pgid;
};

static struct vm86bios_softc vm86bios_XXX;
struct vm86bios_softc *vm86bios_gsc;	/* for debugging, globall exported */

int vm86biosopen __P((dev_t, int, int, struct proc *));
int vm86biosclose __P((dev_t, int, int, struct proc *));
int vm86biosioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int vm86biosmmap __P((dev_t dev, int, int));

void vm86task_timeout __P((void *));

static void vm86biosinit __P((void));
static void vm86biosfreevt __P((struct vm86bios_softc *, struct vm86_task *));
static struct vm86_task *vm86biosgetvt __P((struct vm86bios_softc *));
static void vm86bioswait __P((struct vm86bios_softc *, struct vm86_task *));
static void vm86biosdone __P((struct vm86bios_softc *, struct vm86_task *));
static void vm86taskwakeup __P((struct vm86bios_softc *));

#define	splvm86bios spltty

/*****************************************************
 * misc functions
 *****************************************************/
static void
vm86biosinit(void)
{
	struct vm86bios_softc *sc = &vm86bios_XXX;
	int s;

	TAILQ_INIT(&sc->sc_vt_waittab);
	TAILQ_INIT(&sc->sc_vt_freetab);
	strcpy(sc->sc_dev.dv_xname, "vm86bios");

	s = splvm86bios();
	vm86bios_gsc = sc;
	splx(s);
}


#define	VM86BIOS_TOTALVT 0x10

static struct vm86_task *
vm86biosgetvt(sc)
	struct vm86bios_softc *sc;
{
	struct vm86_task *vtp;
	int s = splvm86bios();

	if ((vtp = sc->sc_vt_freetab.tqh_first) == NULL)
	{
		if (sc->sc_totalvt ++ > VM86BIOS_TOTALVT)
			return NULL;

		vtp = malloc(sizeof(*vtp), M_DEVBUF, M_NOWAIT);
		if (vtp == NULL)
			panic("%s no mem\n", sc->sc_dev.dv_xname);
	}
	else
		TAILQ_REMOVE(&sc->sc_vt_freetab, vtp, vt_chain);

	splx(s);

	memset(vtp, 0, sizeof(*vtp));
	return vtp;
}

static void
vm86biosfreevt(sc, vtp)
	struct vm86bios_softc *sc;
	struct vm86_task *vtp;
{
	int s = splvm86bios();

	TAILQ_INSERT_TAIL(&sc->sc_vt_freetab, vtp, vt_chain);
	splx(s);
}

static void
vm86bioswait(sc, vtp)
	struct vm86bios_softc *sc;
	struct vm86_task *vtp;
{

	while ((vtp->vt_flags & VM86TASK_DONE) == 0)
		(void) tsleep((caddr_t) vtp, PRIBIO + 1, "vm86bios", 0);
}

static void
vm86biosdone(sc, vtp)
	struct vm86bios_softc *sc;
	struct vm86_task *vtp;
{

	vtp->vt_flags &= ~VM86TASK_INPROGRESS;
	vtp->vt_flags |= VM86TASK_DONE;

	if (vtp->vt_callback != NULL)
		(*vtp->vt_callback)(vtp->vt_arg);

	if (vtp->vt_flags & VM86TASK_ASYNC)
		vm86biosfreevt(sc, vtp);
	else
		wakeup(vtp);
}

static void
vm86taskwakeup(sc)
	struct vm86bios_softc *sc;
{

	if (sc->sc_flags & VM86BIOS_WAIT)
	{
		wakeup((caddr_t) &sc->sc_cnt);
		sc->sc_flags &= ~VM86BIOS_WAIT;
	}
}

/*****************************************************
 * open close ioctl
 *****************************************************/
int
vm86biosopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	if (vm86bios_gsc == NULL)
		vm86biosinit();

	return 0;
}

int
vm86biosclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return 0;
}

void
vm86task_timeout(arg)
	void *arg;
{
	struct vm86bios_softc *sc = vm86bios_gsc;
	struct vm86_task *vtp = arg;
	int s = splvm86bios();

	if ((vtp->vt_flags & VM86TASK_INPROGRESS) == 0)
	{
		TAILQ_REMOVE(&sc->sc_vt_waittab, vtp, vt_chain);
	}
	else if (sc->sc_nexus == vtp)
	{
		sc->sc_nexus = NULL;
	}
	else
	{
		splx(s);
		return;
	}

	vtp->vt_flags |= VM86TASK_ERROR;
	vm86biosdone(sc, vtp);

	printf("%s: timeout vm86task (check vm86biosd)\n", sc->sc_dev.dv_xname);
	splx(s);
}

int
vm86biosioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct vm86bios_softc *sc = vm86bios_gsc;
	struct vm86_task *vtp;
	struct vm86_call *vcp;
	int error, s;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return error;

	switch (cmd)
	{
	case VM86BIOS_IOC_USRREQ:
		vcp = (struct vm86_call *) data;
		vcp->vc_flags &= ~VM86BIOS_ASYNC;
		vm86biosstart(vcp, NULL, NULL, 0);
		return 0;

	case VM86BIOS_IOC_CONNECT:
		if (sc->sc_pgrp && sc->sc_pgrp != pgfind(sc->sc_pgid))
			sc->sc_pgrp = NULL;

		if (sc->sc_pgrp && sc->sc_pgrp != p->p_pgrp)
			return EBUSY;

		sc->sc_pgrp = p->p_pgrp;
		sc->sc_pgid = p->p_pgid;
		return 0;
	}

	if (sc->sc_pgrp == NULL || sc->sc_pgrp != pgfind(sc->sc_pgid))
	{
		sc->sc_pgrp = NULL;
		return EPERM;
	}

	if (p->p_pgid != sc->sc_pgid)
		return EPERM;

	switch (cmd)
	{
	case VM86BIOS_IOC_WAIT:
		if ((sc->sc_flags & VM86BIOS_SWAIT) != 0)
		{
			wakeup(&vm86bios_gsc);
			sc->sc_flags &= ~VM86BIOS_SWAIT;
		}

		s = splvm86bios();
		if (sc->sc_vt_waittab.tqh_first == NULL || sc->sc_nexus)
		{
			sc->sc_flags |= VM86BIOS_WAIT;
			error = tsleep((caddr_t) &sc->sc_cnt, PZERO | PCATCH,
					"vm86bios", 0);
			if (error)
			{
				splx(s);
				return error;
			}
		}

		splx(s);
		return 0;

	case VM86BIOS_IOG_TASK:
		vcp = (struct vm86_call *) data;
		vcp->vc_id = NULL;

		if (sc->sc_nexus)
			return EBUSY;

		s = splvm86bios();
		if ((vtp = sc->sc_vt_waittab.tqh_first) == NULL)
		{
			splx(s);
			return 0;
		}

		TAILQ_REMOVE(&sc->sc_vt_waittab, vtp, vt_chain);
		vtp->vt_flags |= VM86TASK_INPROGRESS;
		sc->sc_nexus = vtp;
		splx(s);

		vcp->vc_reg = vtp->vt_reg;
		vcp->vc_intno = vtp->vt_intno;
		vcp->vc_error = EIO;
		vcp->vc_id = vtp;
		return 0;

	case VM86BIOS_IOC_TASKDONE:
		vcp = (struct vm86_call *) data;

		if (vcp->vc_id != sc->sc_nexus)
			return EIO;

		vtp = sc->sc_nexus;
		if ((vtp->vt_flags & (VM86TASK_DONE | VM86TASK_INPROGRESS)) !=
		    VM86TASK_INPROGRESS)
		{
			printf("%s: already done or non started vm86task\n",
				sc->sc_dev.dv_xname);
			return EIO;
		}

		untimeout(vm86task_timeout, vtp);

		vtp->vt_reg = vcp->vc_reg;
		if (vcp->vc_error)
			vtp->vt_flags |= VM86TASK_ERROR;

		vm86biosdone(sc, vtp);
		sc->sc_nexus = NULL;
		return 0;

	default:
		return ENOTTY;
	}
}

int
vm86biosmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	struct proc *p = curproc;	/* XXX */

	if (suser(p->p_ucred, &p->p_acflag) != 0)
		return -1;

	if (((u_int) off > 0 && (u_int) off < VM86BIOS_BMAPBASE) ||
	    (u_int) off >= ISAHOLE_END)
		return -1;

	return i386_btop(off);
}

/*****************************************************
 * starti (exported)
 *****************************************************/
void
vm86biosstart(vcp, func, arg, flags)
	struct vm86_call *vcp;
	void (*func) __P((void *));
	void *arg;
	int flags;
{
	struct vm86_task *vtp;
	struct vm86bios_softc *sc;
	int s, error;

	if (vm86bios_gsc == NULL)
	{
		vm86biosinit();
	}

restart:
	sc = vm86bios_gsc;
	if (sc->sc_pgrp == NULL)
		goto bad;

	if (sc->sc_pgrp != pgfind(sc->sc_pgid))
	{
		sc->sc_pgrp = NULL;
		goto bad;
	}

	s = splvm86bios();
	vm86taskwakeup(sc);
	splx(s);

	vtp = vm86biosgetvt(sc);
	if (vtp == NULL)
		goto bad;

	vtp->vt_intno = vcp->vc_intno;
	vtp->vt_reg = vcp->vc_reg;
	vtp->vt_callback = func;
	vtp->vt_arg = arg;

	s = splvm86bios();
	TAILQ_INSERT_TAIL(&sc->sc_vt_waittab, vtp, vt_chain);
	timeout(vm86task_timeout, vtp, 2 * hz);
	splx(s);

	if ((vcp->vc_flags & VM86BIOS_ASYNC) == 0)
	{
		vm86bioswait(sc, vtp);
		if ((vtp->vt_flags & VM86TASK_ERROR) == 0)
		{
			vcp->vc_error = 0;
			vcp->vc_reg = vtp->vt_reg;
		}
		else
			vcp->vc_error = EIO;

		vm86biosfreevt(sc, vtp);
	}
	else
		vtp->vt_flags |= VM86TASK_ASYNC;

	return;

bad:
	if ((flags & VM86BIOS_KIO_SWAITOK) != 0)
	{
		sc->sc_flags |= VM86BIOS_SWAIT;
		error = tsleep((caddr_t) &vm86bios_gsc, PRIBIO + 1, 
			      "vm86bios_kio", 0);
		if (error == 0)
			goto restart;
	}

	vcp->vc_error = EIO;
	if (func != NULL)
		(*func) (arg);
}
