/*	$NecBSD: pcb.h,v 3.12 1999/07/23 05:15:58 kmatsuda Exp $	*/
/*	$NetBSD: pcb.h,v 1.24 1998/08/15 05:10:25 mycroft Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *	@(#)pcb.h	5.10 (Berkeley) 5/12/91
 */

/*
 * Intel 386 process control block
 */

#ifndef _I386_PCB_H_
#define _I386_PCB_H_

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_pmap_new.h"
#endif

#include <sys/signal.h>

#include <machine/segments.h>
#include <machine/tss.h>
#include <machine/npx.h>
#include <machine/sysarch.h>

#ifdef	ORIGINAL_CODE
#define	NIOPORTS	1024		/* # of ports we allow to be mapped */
#else	/* PC-98 */
#define	NIOPORTS	(0x10000)	/* # of ports we allow to be mapped */
#endif	/* PC-98 */

struct pcb {
	struct	i386tss pcb_tss;
#define	pcb_cr3	pcb_tss.tss_cr3
#define	pcb_esp	pcb_tss.tss_esp
#define	pcb_ebp	pcb_tss.tss_ebp
#define	pcb_fs	pcb_tss.tss_fs
#define	pcb_gs	pcb_tss.tss_gs
#define	pcb_ldt_sel	pcb_tss.tss_ldt
	int	pcb_tss_sel;
        union	descriptor *pcb_ldt;	/* per process (user) LDT */
        int	pcb_ldt_len;		/*      number of LDT entries */
	int	pcb_cr0;		/* saved image of CR0 */
	struct	save87 pcb_savefpu;	/* floating point state for 287/387 */
	struct	emcsts pcb_saveemc;	/* Cyrix EMC state */
/*
 * Software pcb (extension)
 */
	int	pcb_flags;
#define	PCB_USER_LDT	0x01		/* has user-set LDT */
#ifndef	ORIGINAL_CODE
#define	PCB_NO_KVAFAULT 0x4000
#define	PCB_IOMAP_ON	0x8000
#endif	/* PC-98 */
	caddr_t	pcb_onfault;		/* copyin/out fault recovery */
	int	vm86_eflags;		/* virtual eflags for vm86 mode */
	int	vm86_flagmask;		/* flag mask for vm86 mode */
	void	*vm86_userp;		/* XXX performance hack */
#ifdef	ORIGINAL_CODE
	u_long	pcb_iomap[NIOPORTS/32];	/* I/O bitmap */
#if defined(PMAP_NEW)
	struct pmap *pcb_pmap;		/* back pointer to our pmap */
#endif
#else	/* PC-98 */
	/* XXX:
	 * What a stupid to locate the pcb_pmap after the iomap
	 * considering the EVM intel extensions.
	 * However, this modification breaks the compatiblity of kmem
	 * user programs between i386/AT and i386/pc98.
	 */
#if defined(PMAP_NEW)
	struct pmap *pcb_pmap;		/* back pointer to our pmap */
#endif
	u_long	pcb_iomap[NIOPORTS/32];	/* I/O bitmap */
#endif	/* PC-98 */
};

/*    
 * The pcb is augmented with machine-dependent additional data for 
 * core dumps. For the i386, there is nothing to add.
 */     
struct md_coredump {
	long	md_pad[8];
};    

#ifdef _KERNEL
struct pcb *curpcb;		/* our current running pcb */
#ifndef	ORIGINAL_CODE
void tss_iomap_on __P((struct pcb *));
void tss_iomap_off __P((struct pcb *));
#endif	/* PC-98 */
#endif

#endif /* _I386_PCB_H_ */
