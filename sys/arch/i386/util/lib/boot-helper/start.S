/*	$NecBSD: start.S,v 1.2 1999/04/15 01:36:53 kmatsuda Exp $	*/
/*	$NetBSD$	*/
#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <machine/asm.h>
#define	addr32	.byte 0x67
#define	data32	.byte 0x66

BOOTSEG		=	0x9000	# boot will be loaded here (below 640K)
BOOTSTACK	=	0xfffc	# boot stack

	.text	
	.globl	start

ENTRY(boot1)
start:
	jmp 1f

	/* parameter headers */
	.word	0		/* saved boot cylinder */
	.ascii "IPL1   "
	.asciz "NetBSD/pc98 boot-helper"
1:
	cli
	data32				/* setup ds */
	movl	$0,%eax
	mov     %ax,%ds
	movl	%eax,%ebp

	data32				/* setup es */
	movl	$BOOTSEG,%eax
	mov	%ax,%es

	mov	%ax,%ss			/* setup stack */
	data32
	movl	$BOOTSTACK, %esp

	addr32
	movb	0x584,%al
	movl	%eax,%ebp 		/* DAUA saved in ebp */

media_check:
	andb    $0xf0,%al
	cmpb    $0x90,%al		/* FD */
	jz	bootfd
	cmpb    $0x30,%al		/* 1.44 FD */
	jz	bootfd
	jmp	boothd			/* HD */

bootfd:
	jmp	bootfd			/* terminate */

boothd:
	/* step I: loading the dospartition table */
	movl	%ebp,%eax
	movb	$0x06,%ah
	data32
	movl	$2048,%ebx		/* blocksize minimum */		
	xorl	%ecx,%ecx
	xorl	%edx,%edx
	xorl	%ebp,%ebp
	int	$0x1b

	/* step II: search our NetBSD entry (first hit) */
	data32
	movl	$512,%ebp
	movb	$0x08,%cl		/* ndospart */
	jmp 	9f

1:
	data32
	addl	$32,%ebp
	decb	%cl
	jnz	9f

2:	/* no NetBSD partition found: terminate */
	jmp	2b		

9:
	addr32
	movb	%es:0(%ebp),%al
	andb	$0x7f,%al
	cmpb	$0x14,%al		/* check mid */
	jnz	1b
	addr32
	cmpb	$0xc4,%es:1(%ebp)	/* check sid with active */
	jnz	1b

	addr32
	movb	%es:8(%ebp),%dl		/* sector */
	addr32
	movb	%es:9(%ebp),%dh		/* head */
	addr32
	data32
	movl	%es:10(%ebp),%ecx	/* cyl */

	/* check a recursive loader call */
	xorl	%eax,%eax
	cmpl	%eax,%edx
	jnz	hello_NecBSD_world
	cmpl	%eax,%ecx
3:
	jz	3b			/* terminate */

hello_NecBSD_world:
	addr32
	movb	0x584,%al
	movb	$0x06,%ah
	data32
	movl	$512*16, %ebx
	xorl	%ebp,%ebp
	int	$0x1b

	data32
	ljmp	$BOOTSEG, $0
