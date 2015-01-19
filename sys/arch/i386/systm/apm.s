/*	$NecBSD: apm.s,v 1.9 1999/07/25 16:54:05 honda Exp $	*/
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
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#if	NAPM > 0
#include <machine/apmvar.h>

#define	APM_PUSH_REGISTER						\
	pushl %ebp							;\
	pushl %edi							;\
	pushl %esi							;\
	pushl %ebx

#define	APM_POP_REGISTER						\
	popl %ebx							;\
	popl %esi							;\
	popl %edi							;\
	popl %ebp

#define	APM_32B_FARCALL							\
	pushfl								;\
	lcall _C_LABEL(apm_farcall)

#define	APM_BLOCK_NECSMM						\
	movl	_C_LABEL(apm_NEC_SMMaddr),%edx				;\
	andl	%edx,%edx						;\
	jz	9							;\
	inb	%dx, %al						;\
	andl	_C_LABEL(apm_NEC_SMMmask),%eax				;\
	outb	%al,%dx							;\
9:

/*
 * void apm_cpu_busy __P((void))
 */
.globl	_C_LABEL(apm_NEC_SMMaddr)
.globl	_C_LABEL(apm_NEC_SMMmask)

NENTRY(apm_cpu_busy)
	APM_PUSH_REGISTER

	movl $((APM_BIOS_FNCODE << 8) | APM_CPU_BUSY), %eax

	cli
	APM_32B_FARCALL
	lahf
	APM_BLOCK_NECSMM
	sti

	andl $0x100, %eax
	jnz 2f

1:
	APM_POP_REGISTER
	ret

2:
	pushl	$3f
	call	_C_LABEL(printf)
	addl	$4,%esp
	jmp 1b

3:	.asciz	"cpu busy error"

/*
 * void apm_cpu_idle __P((void))
 */
NENTRY(apm_cpu_idle)
	APM_PUSH_REGISTER

	movl $((APM_BIOS_FNCODE << 8) | APM_CPU_IDLE), %eax

	cli
	APM_32B_FARCALL
	lahf
	APM_BLOCK_NECSMM
	sti

	andl $0x100, %eax
	jnz 2f

1:
	APM_POP_REGISTER
	ret

2:
	pushl	$3f
	call	_C_LABEL(printf)
	addl	$4,%esp
	jmp 1b

3:	.asciz	"cpu idle error"

/*
 * int apm_32b_call __P((int funcno; struct apm_reg_args *regs));
 */
NENTRY(apm_32b_call)
	APM_PUSH_REGISTER

	movl 24(%esp),%esi

	movl 20(%esp),%eax
	orb $APM_BIOS_FNCODE,%ah

	movl 4(%esi),%ebx
	movl 8(%esi),%ecx
	movl 12(%esi),%edx

	cli
	APM_32B_FARCALL
	movl %eax, %edi
	movl %edx, %ebp
	lahf
	APM_BLOCK_NECSMM
	sti

	movl 24(%esp),%esi

	movl %edi,0(%esi)
	movl %ebx,4(%esi)
	movl %ecx,8(%esi)
	movl %ebp,12(%esi)

	andl $0x100, %eax
	APM_POP_REGISTER
	ret

#endif	/* NAPM > 0 */
