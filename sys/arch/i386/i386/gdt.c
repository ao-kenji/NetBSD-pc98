/*	$NecBSD: gdt.c,v 3.14 1999/07/26 06:23:55 honda Exp $	*/
/*	$NetBSD: gdt.c,v 1.16 1999/03/24 05:51:00 mrg Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John T. Kohl and Charles M. Hannum.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <uvm/uvm_extern.h>

#include <machine/gdt.h>

#define	MINGDTSIZ	512
#define	MAXGDTSIZ	8192

int gdt_size;		/* total number of GDT entries */
int gdt_count;		/* number of GDT entries in use */
int gdt_next;		/* next available slot for sweeping */
int gdt_free;		/* next free slot; terminated with GNULL_SEL */

int gdt_flags;
#define	GDT_LOCKED	0x1
#define	GDT_WANTED	0x2

static __inline void gdt_lock __P((void));
static __inline void gdt_unlock __P((void));
void gdt_compact __P((void));
void gdt_init __P((void));
void gdt_grow __P((void));
void gdt_shrink __P((void));
int gdt_get_slot __P((void));
void gdt_put_slot __P((int));

/*
 * Lock and unlock the GDT, to avoid races in case gdt_{ge,pu}t_slot() sleep
 * waiting for memory.
 *
 * Note that the locking done here is not sufficient for multiprocessor
 * systems.  A freshly allocated slot will still be of type SDT_SYSNULL for
 * some time after the GDT is unlocked, so gdt_compact() could attempt to
 * reclaim it.
 */
static __inline void
gdt_lock()
{

	while ((gdt_flags & GDT_LOCKED) != 0) {
		gdt_flags |= GDT_WANTED;
		tsleep(&gdt_flags, PZERO, "gdtlck", 0);
	}
	gdt_flags |= GDT_LOCKED;
}

static __inline void
gdt_unlock()
{

	gdt_flags &= ~GDT_LOCKED;
	if ((gdt_flags & GDT_WANTED) != 0) {
		gdt_flags &= ~GDT_WANTED;
		wakeup(&gdt_flags);
	}
}

/*
 * Compact the GDT as follows:
 * 0) We partition the GDT into two areas, one of the slots before gdt_count,
 *    and one of the slots after.  After compaction, the former part should be
 *    completely filled, and the latter part should be completely empty.
 * 1) Step through the process list, looking for TSS and LDT descriptors in
 *    the second section, and swap them with empty slots in the first section.
 * 2) Arrange for new allocations to sweep through the empty section.  Since
 *    we're sweeping through all of the empty entries, and we'll create a free
 *    list as things are deallocated, we do not need to create a new free list
 *    here.
 */
void
gdt_compact()
{
	struct proc *p;
	struct pcb *pcb;
	int slot = NGDT, oslot;

	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		pcb = &p->p_addr->u_pcb;
		oslot = IDXSEL(pcb->pcb_tss_sel);
		if (oslot >= gdt_count) {
			while (gdt[slot].sd.sd_type != SDT_SYSNULL) {
				if (++slot >= gdt_count)
					panic("gdt_compact botch 1");
			}
			gdt[slot] = gdt[oslot];
			gdt[oslot].gd.gd_type = SDT_SYSNULL;
			pcb->pcb_tss_sel = GSEL(slot, SEL_KPL);
		}
		oslot = IDXSEL(pcb->pcb_ldt_sel);
		if (oslot >= gdt_count) {
			while (gdt[slot].sd.sd_type != SDT_SYSNULL) {
				if (++slot >= gdt_count)
					panic("gdt_compact botch 2");
			}
			gdt[slot] = gdt[oslot];
			gdt[oslot].gd.gd_type = SDT_SYSNULL;
			pcb->pcb_ldt_sel = GSEL(slot, SEL_KPL);
		}
	}
	for (; slot < gdt_count; slot++)
		if (gdt[slot].gd.gd_type == SDT_SYSNULL)
			panic("gdt_compact botch 3");
	for (slot = gdt_count; slot < gdt_size; slot++)
		if (gdt[slot].gd.gd_type != SDT_SYSNULL)
			panic("gdt_compact botch 4");
	gdt_next = gdt_count;
	gdt_free = GNULL_SEL;
}

/*
 * Grow or shrink the GDT.
 */
void
gdt_init()
{
	size_t max_len, min_len;
	struct region_descriptor region;
	union descriptor *old_gdt;

	max_len = MAXGDTSIZ * sizeof(gdt[0]);
	min_len = MINGDTSIZ * sizeof(gdt[0]);

	gdt_size = MINGDTSIZ;
	gdt_count = NGDT;
	gdt_next = NGDT;
	gdt_free = GNULL_SEL;

	old_gdt = gdt;
	gdt = (union descriptor *)uvm_km_valloc(kernel_map, max_len);
	uvm_map_pageable(kernel_map, (vaddr_t)gdt,
	    (vaddr_t)gdt + min_len, FALSE);
	memcpy(gdt, old_gdt, NGDT * sizeof(gdt[0]));

	setregion(&region, gdt, max_len - 1);
	lgdt(&region);
}

void
gdt_grow()
{
	size_t old_len, new_len;

	old_len = gdt_size * sizeof(gdt[0]);
	gdt_size <<= 1;
	new_len = old_len << 1;

	uvm_map_pageable(kernel_map, (vaddr_t)gdt + old_len,
	    (vaddr_t)gdt + new_len, FALSE);
}

void
gdt_shrink()
{
	size_t old_len, new_len;

	old_len = gdt_size * sizeof(gdt[0]);
	gdt_size >>= 1;
	new_len = old_len >> 1;

	uvm_map_pageable(kernel_map, (vaddr_t)gdt + new_len,
	    (vaddr_t)gdt + old_len, TRUE);
}

/*
 * Allocate a GDT slot as follows:
 * 1) If there are entries on the free list, use those.
 * 2) If there are fewer than gdt_size entries in use, there are free slots
 *    near the end that we can sweep through.
 * 3) As a last resort, we increase the size of the GDT, and sweep through
 *    the new slots.
 */
int
gdt_get_slot()
{
	int slot;

	gdt_lock();

	if (gdt_free != GNULL_SEL) {
		slot = gdt_free;
		gdt_free = gdt[slot].gd.gd_selector;
	} else {
		if (gdt_next != gdt_count)
			panic("gdt_get_slot botch 1");
		if (gdt_next >= gdt_size) {
			if (gdt_size >= MAXGDTSIZ)
				panic("gdt_get_slot botch 2");
			gdt_grow();
		}
		slot = gdt_next++;
	}

	gdt_count++;
	gdt_unlock();
	return (slot);
}

/*
 * Deallocate a GDT slot, putting it on the free list.
 */
void
gdt_put_slot(slot)
	int slot;
{

	gdt_lock();
	gdt_count--;

	gdt[slot].gd.gd_type = SDT_SYSNULL;
	/* 
	 * shrink the GDT if we're using less than 1/4 of it.
	 * Shrinking at that point means we'll still have room for
	 * almost 2x as many processes as are now running without
	 * having to grow the GDT.
	 */
#ifdef	ORIGINAL_CODE
	if (gdt_size > MINGDTSIZ && gdt_count <= gdt_size / 4) {
		gdt_compact();
		gdt_shrink();
	} else {
		gdt[slot].gd.gd_selector = gdt_free;
		gdt_free = slot;
	}
#else	/* PC-98 */
	gdt[slot].gd.gd_selector = gdt_free;
	gdt_free = slot;
#endif	/* PC-98 */

	gdt_unlock();
}

void
tss_alloc(pcb)
	struct pcb *pcb;
{
	int slot;
#ifndef	ORIGINAL_CODE
	int size;
#endif	/* PC-98 */

	slot = gdt_get_slot();
#ifdef	ORIGINAL_CODE
	setsegment(&gdt[slot].sd, &pcb->pcb_tss, sizeof(struct pcb) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
#else	/* PC-98 */
	if (pcb->pcb_flags & PCB_IOMAP_ON)
		size = sizeof(struct pcb);
	else
		size = (caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss;

	setsegment(&gdt[slot].sd, &pcb->pcb_tss, size - 1,
	   SDT_SYS386TSS, SEL_KPL, 0, 0);
#endif	/* PC-98 */
	pcb->pcb_tss_sel = GSEL(slot, SEL_KPL);
}

#ifndef	ORIGINAL_CODE
void
tss_iomap_off(pcb)
	struct pcb *pcb;
{
	int slot;

	pcb->pcb_iomap[0] = -1;

	pcb->pcb_flags &= ~PCB_IOMAP_ON;

	slot = IDXSEL(pcb->pcb_tss_sel);
#ifdef	DIAGNOSTIC
	if (slot == 0)
		panic("tss_iomap_off: slot 0\n");
#endif	/* DIAGNOSTIC */

	setsegment(&gdt[slot].sd, &pcb->pcb_tss,
		   (caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss - 1,
		   SDT_SYS386TSS, SEL_KPL, 0, 0);

	if (&curproc->p_addr->u_pcb == pcb)
		ltr(pcb->pcb_tss_sel);
}

void
tss_iomap_on(pcb)
	struct pcb *pcb;
{
	int slot, x;

	for (x = 0; x < sizeof(pcb->pcb_iomap) / 4; x++)
		pcb->pcb_iomap[x] = 0xffffffff;

	pcb->pcb_flags |= PCB_IOMAP_ON;

	slot = IDXSEL(pcb->pcb_tss_sel);
#ifdef	DIAGNOSTIC
	if (slot == 0)
		panic("tss_iomap_on: slot 0\n");
#endif	/* DIAGNOSTIC */

	setsegment(&gdt[slot].sd, &pcb->pcb_tss, sizeof(struct pcb) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);

	if (&curproc->p_addr->u_pcb == pcb)
		ltr(pcb->pcb_tss_sel);
}
#endif	/* PC-98 */

void
tss_free(pcb)
	struct pcb *pcb;
{

	gdt_put_slot(IDXSEL(pcb->pcb_tss_sel));
}

void
ldt_alloc(pcb, ldt, len)
	struct pcb *pcb;
	union descriptor *ldt;
	size_t len;
{
	int slot;

	slot = gdt_get_slot();
	setsegment(&gdt[slot].sd, ldt, len - 1, SDT_SYSLDT, SEL_KPL, 0, 0);
	pcb->pcb_ldt_sel = GSEL(slot, SEL_KPL);
}

void
ldt_free(pcb)
	struct pcb *pcb;
{

	gdt_put_slot(IDXSEL(pcb->pcb_ldt_sel));
}
