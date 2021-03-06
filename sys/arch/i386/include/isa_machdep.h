/*	$NecBSD: isa_machdep.h,v 3.1.4.1 1999/08/28 02:24:22 honda Exp $	*/
/*	$NetBSD: isa_machdep.h,v 1.13 1999/03/19 05:02:11 cgd Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)isa.h	5.7 (Berkeley) 5/9/91
 */

/*
 * Various pieces of the i386 port want to include this file without
 * or in spite of using isavar.h, and should be fixed.
 */

#ifndef _I386_ISA_MACHDEP_H_			/* XXX */
#define _I386_ISA_MACHDEP_H_			/* XXX */

#include <machine/bus.h>
#include <dev/isa/isadmavar.h>
#ifndef	ORIGINAL_CODE
#include <machine/vm_interface.h>
#include <machine/sysinfo.h>
#include <machine/systmbusvar.h>
#include <i386/isa/cpuspec.h>			/* XXX */
#endif	/* PC-98 */

/*
 * XXX THIS FILE IS A MESS.  copyright: berkeley's probably.
 * contents from isavar.h and isareg.h, mostly the latter.
 * perhaps charles's?
 *
 * copyright from berkeley's isa.h which is now dev/isa/isareg.h.
 */

/*
 * Types provided to machine-independent ISA code.
 */
struct i386_isa_chipset {
	struct isa_dma_state ic_dmastate;
};

typedef struct i386_isa_chipset *isa_chipset_tag_t;

struct device;			/* XXX */
struct isabus_attach_args;	/* XXX */

/*
 * Functions provided to machine-independent ISA code.
 */
void	isa_attach_hook __P((struct device *, struct device *,
	    struct isabus_attach_args *));
int	isa_intr_alloc __P((isa_chipset_tag_t, int, int, int *));
void	*isa_intr_establish __P((isa_chipset_tag_t ic, int irq, int type,
	    int level, int (*ih_fun)(void *), void *ih_arg));
void	isa_intr_disestablish __P((isa_chipset_tag_t ic, void *handler));
int	isa_mem_alloc __P((bus_space_tag_t, bus_size_t, bus_size_t,
	    bus_addr_t, int, bus_addr_t *, bus_space_handle_t *));
void	isa_mem_free __P((bus_space_tag_t, bus_space_handle_t, bus_size_t));
#ifndef	ORIGINAL_CODE
void	isa_intr_activate __P((isa_chipset_tag_t, void *, u_int, u_char *));
#define	ISA_INTR_INACTIVE	1
#define	ISA_INTR_RECLAIM	2
void	isa_intr_deactivate __P((isa_chipset_tag_t, void *, int));
u_int	isa_get_empty_irq __P((isa_chipset_tag_t ic, u_int));
#endif	/* PC-98 */

#define	isa_dmainit(ic, bst, dmat, d)					\
	_isa_dmainit(&(ic)->ic_dmastate, (bst), (dmat), (d))
#define	isa_dmacascade(ic, c)						\
	_isa_dmacascade(&(ic)->ic_dmastate, (c))
#define	isa_dmamap_create(ic, c, s, f)					\
	_isa_dmamap_create(&(ic)->ic_dmastate, (c), (s), (f))
#define	isa_dmamap_destroy(ic, c)					\
	_isa_dmamap_destroy(&(ic)->ic_dmastate, (c))
#define	isa_dmastart(ic, c, a, n, p, f, bf)				\
	_isa_dmastart(&(ic)->ic_dmastate, (c), (a), (n), (p), (f), (bf))
#define	isa_dmaabort(ic, c)						\
	_isa_dmaabort(&(ic)->ic_dmastate, (c))
#define	isa_dmacount(ic, c)						\
	_isa_dmacount(&(ic)->ic_dmastate, (c))
#define	isa_dmafinished(ic, c)						\
	_isa_dmafinished(&(ic)->ic_dmastate, (c))
#define	isa_dmadone(ic, c)						\
	_isa_dmadone(&(ic)->ic_dmastate, (c))
#define	isa_dmafreeze(ic)						\
	_isa_dmafreeze(&(ic)->ic_dmastate)
#define	isa_dmathaw(ic)							\
	_isa_dmathaw(&(ic)->ic_dmastate)
#define	isa_dmamem_alloc(ic, c, s, ap, f)				\
	_isa_dmamem_alloc(&(ic)->ic_dmastate, (c), (s), (ap), (f))
#define	isa_dmamem_free(ic, c, a, s)					\
	_isa_dmamem_free(&(ic)->ic_dmastate, (c), (a), (s))
#define	isa_dmamem_map(ic, c, a, s, kp, f)				\
	_isa_dmamem_map(&(ic)->ic_dmastate, (c), (a), (s), (kp), (f))
#define	isa_dmamem_unmap(ic, c, k, s)					\
	_isa_dmamem_unmap(&(ic)->ic_dmastate, (c), (k), (s))
#define	isa_dmamem_mmap(ic, c, a, s, o, p, f)				\
	_isa_dmamem_mmap(&(ic)->ic_dmastate, (c), (a), (s), (o), (p), (f))
#define	isa_drq_isfree(ic, c)						\
	_isa_drq_isfree(&(ic)->ic_dmastate, (c))
#define	isa_malloc(ic, c, s, p, f)					\
	_isa_malloc(&(ic)->ic_dmastate, (c), (s), (p), (f))
#define	isa_free(a, p)							\
	_isa_free((a), (p))
#define	isa_mappage(m, o, p)						\
	_isa_mappage((m), (o), (p))

/*
 * ALL OF THE FOLLOWING ARE MACHINE-DEPENDENT, AND SHOULD NOT BE USED
 * BY PORTABLE CODE.
 */

extern struct i386_bus_dma_tag isa_bus_dma_tag;

/*
 * Cookie used by ISA dma.  A pointer to one of these it stashed in
 * the DMA map.
 */
struct i386_isa_dma_cookie {
	int	id_flags;		/* flags; see below */

	/*
	 * Information about the original buffer used during
	 * DMA map syncs.  Note that origibuflen is only used
	 * for ID_BUFTYPE_LINEAR.
	 */
	void	*id_origbuf;		/* pointer to orig buffer if
					   bouncing */
	bus_size_t id_origbuflen;	/* ...and size */
	int	id_buftype;		/* type of buffer */

	void	*id_bouncebuf;		/* pointer to the bounce buffer */
	bus_size_t id_bouncebuflen;	/* ...and size */
	int	id_nbouncesegs;		/* number of valid bounce segs */
	bus_dma_segment_t id_bouncesegs[0]; /* array of bounce buffer
					       physical memory segments */
};

/* id_flags */
#define	ID_MIGHT_NEED_BOUNCE	0x01	/* map could need bounce buffers */
#define	ID_HAS_BOUNCE		0x02	/* map currently has bounce buffers */
#define	ID_IS_BOUNCING		0x04	/* map is bouncing current xfer */

/* id_buftype */
#define	ID_BUFTYPE_INVALID	0
#define	ID_BUFTYPE_LINEAR	1
#define	ID_BUFTYPE_MBUF		2
#define	ID_BUFTYPE_UIO		3
#define	ID_BUFTYPE_RAW		4

/*
 * XXX Various seemingly PC-specific constants, some of which may be
 * unnecessary anyway.
 */

/*
 * RAM Physical Address Space (ignoring the above mentioned "hole")
 */
#define	RAM_BEGIN	0x0000000	/* Start of RAM Memory */
#define	RAM_END		0x1000000	/* End of RAM Memory */
#define	RAM_SIZE	(RAM_END - RAM_BEGIN)

#ifdef	ORIGINAL_CODE
/*
 * Oddball Physical Memory Addresses
 */
#define	COMPAQ_RAMRELOC	0x80c00000	/* Compaq RAM relocation/diag */
#define	COMPAQ_RAMSETUP	0x80c00002	/* Compaq RAM setup */
#define	WEITEK_FPU	0xC0000000	/* WTL 2167 */
#define	CYRIX_EMC	0xC0000000	/* Cyrix EMC */
#endif	/* !PC-98 */

/*
 * stuff that used to be in pccons.c
 */
#ifdef	ORIGINAL_CODE
#define	MONO_BASE	0x3B4
#define	MONO_BUF	0xB0000
#define	CGA_BASE	0x3D4
#define	CGA_BUF		0xB8000
#define	IOPHYSMEM	0xA0000
#endif	/* !PC-98 */


/*
 * Interrupt handler chains.  isa_intr_establish() inserts a handler into
 * the list.  The handler is called with its (single) argument.
 */

struct intrhand {
	int	(*ih_fun) __P((void *));
	void	*ih_arg;
	u_long	ih_count;
	struct	intrhand *ih_next;
	int	ih_level;
	int	ih_irq;
#ifndef	ORIGINAL_CODE
	int	(*ih_rfun) __P((void *));
#define	IH_NULL		0
#define	IH_SLEEP	1
#define	IH_INACTIVE	2
#define	IH_ACTIVE	3
	int	ih_state;
#endif	/* PC-98 */
};

 
/*
 * ISA DMA bounce buffers.
 * XXX should be made partially machine- and bus-mapping-independent.
 *
 * DMA_BOUNCE is the number of pages of low-addressed physical memory
 * to acquire for ISA bounce buffers.
 *
 * isaphysmem is the location of those bounce buffers.  (They are currently
 * assumed to be contiguous.
 */

#ifndef DMA_BOUNCE
#define	DMA_BOUNCE      8		/* one buffer per channel */
#endif

extern vaddr_t isaphysmem;


/*
 * Variables and macros to deal with the ISA I/O hole.
 * XXX These should be converted to machine- and bus-mapping-independent
 * function definitions, invoked through the softc.
 */

extern u_long atdevbase;           /* kernel virtual address of "hole" */

/*
 * Given a kernel virtual address for some location
 * in the "hole" I/O space, return a physical address.
 */
#define ISA_PHYSADDR(v) ((void *) ((u_long)(v) - atdevbase + IOM_BEGIN))

/*
 * Given a physical address in the "hole",
 * return a kernel virtual address.
 */
#define ISA_HOLE_VADDR(p)  ((void *) ((u_long)(p) - IOM_BEGIN + atdevbase))


/*
 * Miscellanous functions.
 */
void sysbeep __P((int, int));		/* beep with the system speaker */

#endif /* _I386_ISA_MACHDEP_H_ XXX */
