/*	$NecBSD: vm_interface.h,v 1.1.2.1 1999/08/28 02:24:23 honda Exp $	*/
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

#ifndef	_I386_VM_INTERFACE_H_
#define	_I386_VM_INTERFACE_H_

/* physical memory holes definitions */
#define	X16HOLE_START		0x00f00000
#define	X16HOLE_END		0x01000000
#define	X16HOLE_SIZE		0x00100000

#define	CBUSHOLE_START		0x000a0000
#define	CBUSHOLE_USPACE_START	0x000c0000
#define	CBUSHOLE_USPACE_END	0x000e0000
#define	CBUSHOLE_BIOS_START	0x000f0000
#define	CBUSHOLE_BIOS_END	0x00100000
#define	CBUSHOLE_END		0x00100000
#define	CBUSHOLE_SIZE		0x00060000

#define	CBUSHOLE_TRAM_SIZE	0x00001000
#define	CBUSHOLE_TRAM_OFFSET	0x00000000
#define	CBUSHOLE_TARAM_OFFSET	0x00001000

#define	CBUSHOLE_GRAM_SIZE	0x00008000
#define	CBUSHOLE_GRAM0_OFFSET	0x00008000
#define	CBUSHOLE_GRAM1_OFFSET	0x00010000
#define	CBUSHOLE_GRAM2_OFFSET	0x00018000
#define	CBUSHOLE_GRAM3_OFFSET	0x00040000

#define	CBUSHOLE_PEGC_WINDOW_OFFSET	0x00008000
#define	CBUSHOLE_PEGC_WINDOW_SIZE	0x00010000
#define	CBUSHOLE_PEGC_REGISTER_OFFSET	0x00040000

#define	ISAHOLE_END		CBUSHOLE_END	/* XXX */

#if	defined(_KERNEL) && !defined(_LOCORE)
extern	paddr_t hole_start, hole2_start, hole2_end, physmem_end;
int is_fake_page __P((int));
#endif	/* _KERNEL && !_LOCORE */
#endif	/* !_I386_VM_INTERFACE_H_ */
