/*	$NecBSD: vm_machdep.c,v 3.24 1999/07/23 05:15:52 kmatsuda Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.75.2.1 1999/05/05 17:07:06 perry Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * Copyright (c) 1989, 1990 William Jolitz
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 */

/*
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 */

#include "opt_user_ldt.h"
#include "opt_pmap_new.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/ptrace.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/gdt.h>
#include <machine/reg.h>
#include <machine/specialreg.h>

#include "npx.h"
#if NNPX > 0
extern struct proc *npxproc;
#endif

void	setredzone __P((u_short *, caddr_t));

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */
void
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct pcb *pcb = &p2->p_addr->u_pcb;
	register struct trapframe *tf;
	register struct switchframe *sf;

#if NNPX > 0
	/*
	 * If npxproc != p1, then the npx h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps.
	 *
	 * If npxproc == p1, then we have to save the npx h/w state to
	 * p1's pcb so that we can copy it.
	 */
	if (npxproc == p1)
		npxsave();
#endif

	p2->p_md.md_flags = p1->p_md.md_flags;

	/* Copy pcb from proc p1 to p2. */
	if (p1 == curproc) {
		/* Sync the PCB before we copy it. */
		savectx(curpcb);
	}
#ifdef DIAGNOSTIC
	else if (p1 != &proc0)
		panic("cpu_fork: curproc");
#endif
#ifdef	ORIGINAL_CODE
	*pcb = p1->p_addr->u_pcb;
#else	/* PC-98 */
	if (p1->p_addr->u_pcb.pcb_flags & PCB_IOMAP_ON)
	{
		*pcb = p1->p_addr->u_pcb;
	}
	else
	{
		bcopy(&p1->p_addr->u_pcb, pcb,
		      (caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss);
		pcb->pcb_iomap[0] = -1;
	}
#endif	/* PC-98 */
	pmap_activate(p2);

	/*
	 * Preset these so that gdt_compact() doesn't get confused if called
	 * during the allocations below.
	 */
	pcb->pcb_tss_sel = GSEL(GNULL_SEL, SEL_KPL);
	pcb->pcb_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);

	/* Fix up the TSS. */
	pcb->pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	pcb->pcb_tss.tss_esp0 = (int)p2->p_addr + USPACE - 16;
	tss_alloc(pcb);

#ifdef USER_LDT
	/* Copy the LDT, if necessary. */
	if (pcb->pcb_flags & PCB_USER_LDT) {
		size_t len;
		union descriptor *new_ldt;

		len = pcb->pcb_ldt_len * sizeof(union descriptor);
		new_ldt = (union descriptor *)uvm_km_alloc(kernel_map, len);
		memcpy(new_ldt, pcb->pcb_ldt, len);
		pcb->pcb_ldt = new_ldt;
		ldt_alloc(pcb, new_ldt, len);
	}
#endif

	/*
	 * Copy the trapframe, and arrange for the child to return directly
	 * through rei().
	 *
	 * Note the inlined version of cpu_set_kpc().
	 */
	p2->p_md.md_regs = tf = (struct trapframe *)pcb->pcb_tss.tss_esp0 - 1;
	*tf = *p1->p_md.md_regs;
	sf = (struct switchframe *)tf - 1;
	sf->sf_ppl = 0;
	sf->sf_esi = (int)child_return;
	sf->sf_ebx = (int)p2;
	sf->sf_eip = (int)proc_trampoline;
	pcb->pcb_esp = (int)sf;
}

void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{
	struct switchframe *sf = (struct switchframe *)p->p_addr->u_pcb.pcb_esp;

	sf->sf_esi = (int) pc;
	sf->sf_ebx = (int) arg;
	sf->sf_eip = (int) proc_trampoline;
}

void
cpu_swapout(p)
	struct proc *p;
{

#if NNPX > 0
	/*
	 * Make sure we save the FP state before the user area vanishes.
	 */
	if (npxproc == p)
		npxsave();
#endif
}

/*
 * cpu_exit is called as the last action during exit.
 *
 * We clean up a little and then call switch_exit() with the old proc as an
 * argument.  switch_exit() first switches to proc0's context, and finally
 * jumps into switch() to wait for another process to wake up.
 */
void
cpu_exit(p)
	register struct proc *p;
{
#ifdef USER_LDT
	struct pcb *pcb;
#endif
	struct vmspace *vm;

#if NNPX > 0
	/* If we were using the FPU, forget about it. */
	if (npxproc == p)
		npxproc = 0;
#endif

#ifdef USER_LDT
	pcb = &p->p_addr->u_pcb;
	if (pcb->pcb_flags & PCB_USER_LDT)
		i386_user_cleanup(pcb);
#endif

	vm = p->p_vmspace;

	uvmexp.swtch++;
	switch_exit(p);
}

/*
 * Dump the machine specific segment at the start of a core dump.
 */     
struct md_core {
	struct reg intreg;
	struct fpreg freg;
};
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	struct md_core md_core;
	struct coreseg cseg;
	int error;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	/* Save integer registers. */
	error = process_read_regs(p, &md_core.intreg);
	if (error)
		return error;

	/* Save floating point registers. */
	error = process_read_fpregs(p, &md_core.freg);
	if (error)
		return error;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred,
	    NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return 0;
}

#if 0
/*
 * Set a red zone in the kernel stack after the u. area.
 */
void
setredzone(pte, vaddr)
	u_short *pte;
	caddr_t vaddr;
{
/* eventually do this by setting up an expand-down stack segment
   for ss0: selector, allowing stack access down to top of u.
   this means though that protection violations need to be handled
   thru a double fault exception that must do an integral task
   switch to a known good context, within which a dump can be
   taken. a sensible scheme might be to save the initial context
   used by sched (that has physical memory mapped 1:1 at bottom)
   and take the dump while still in mapped mode */
}
#endif

#if defined(PMAP_NEW)
/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	register pt_entry_t *fpte, *tpte, ofpte, otpte;

	if (size % CLBYTES)
		panic("pagemove");
	fpte = kvtopte(from);
	tpte = kvtopte(to);
	while (size > 0) {
		otpte = *tpte;
		ofpte = *fpte;
		*tpte++ = *fpte;
		*fpte++ = 0;
#if defined(I386_CPU)
		if (cpu_class != CPUCLASS_386)
#endif
		{
			if (otpte & PG_V)
				pmap_update_pg((vaddr_t) to);
			if (ofpte & PG_V)
				pmap_update_pg((vaddr_t) from);
		}
		from += NBPG;
		to += NBPG;
		size -= NBPG;
	}
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		pmap_update();
#endif
}
#else /* PMAP_NEW */
/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	register pt_entry_t *fpte, *tpte;

	if (size % CLBYTES)
		panic("pagemove");
	fpte = kvtopte(from);
	tpte = kvtopte(to);
	while (size > 0) {
		*tpte++ = *fpte;
		*fpte++ = 0;
		from += NBPG;
		to += NBPG;
		size -= NBPG;
	}
	pmap_update();
}
#endif /* PMAP_NEW */

/*
 * Convert kernel VA to physical address
 */
int
kvtop(addr)
	register caddr_t addr;
{
	vaddr_t va;

	va = pmap_extract(pmap_kernel(), (vaddr_t)addr);
	if (va == 0)
		panic("kvtop: zero page frame");
	return((int)va);
}

extern vm_map_t phys_map;

/*
 * Map an IO request into kernel virtual address space.  Requests fall into
 * one of five catagories:
 *
 *	B_PHYS|B_UAREA:	User u-area swap.
 *			Address is relative to start of u-area (p_addr).
 *	B_PHYS|B_PAGET:	User page table swap.
 *			Address is a kernel VA in usrpt (Usrptmap).
 *	B_PHYS|B_DIRTY:	Dirty page push.
 *			Address is a VA in proc2's address space.
 *	B_PHYS|B_PGIN:	Kernel pagein of user pages.
 *			Address is VA in user's address space.
 *	B_PHYS:		User "raw" IO request.
 *			Address is VA in user's address space.
 *
 * All requests are (re)mapped into kernel VA space via the phys_map
 * (a name with only slightly more meaning than "kernel_map")
 */

#if defined(PMAP_NEW)

void
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t faddr, taddr, off;
	paddr_t fpa;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	faddr = trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr= uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	/*
	 * The region is locked, so we expect that pmap_pte() will return
	 * non-NULL.
	 * XXX: unwise to expect this in a multithreaded environment.
	 * anything can happen to a pmap between the time we lock a 
	 * region, release the pmap lock, and then relock it for
	 * the pmap_extract().
	 *
	 * no need to flush TLB since we expect nothing to be mapped
	 * where we we just allocated (TLB will be flushed when our
	 * mapping is removed).
	 */
	while (len) {
		fpa = pmap_extract(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map),
				   faddr);
		pmap_enter(vm_map_pmap(phys_map), taddr, fpa,
			   VM_PROT_READ|VM_PROT_WRITE, TRUE, 0);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
		len -= PAGE_SIZE;
	}
}

#else /* PMAP_NEW */

void
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t faddr, taddr, off;
	pt_entry_t *fpte, *tpte;
	pt_entry_t *pmap_pte __P((pmap_t, vaddr_t));

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	faddr = trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	/*
	 * The region is locked, so we expect that pmap_pte() will return
	 * non-NULL.
	 */
	fpte = pmap_pte(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map), faddr);
	tpte = pmap_pte(vm_map_pmap(phys_map), taddr);
	do {
		*tpte++ = *fpte++;
		len -= PAGE_SIZE;
	} while (len);
}

#endif

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page(bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}
