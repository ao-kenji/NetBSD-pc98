/*	$NecBSD: microtime.s,v 3.28 1999/08/05 08:52:39 kmatsuda Exp $	*/
/*	$NetBSD: microtime.s,v 1.18 1998/12/01 04:31:01 thorpej Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	Naofumi HONDA.  All rights reserved.
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	Kouichi Matsuda.  All rights reserved.
 */
#endif	/* PC-98 */
/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 */

#ifndef	ORIGINAL_CODE
#include "opt_cputype.h"
#include "opt_ddb.h"
#endif	/* PC-98 */

#include <machine/asm.h>
#include <dev/isa/isareg.h>
#include <i386/isa/timerreg.h>

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) % 8))
#define	IRQ_BYTE(irq_num)	((irq_num) / 8)

ENTRY(microtime)
#ifndef	ORIGINAL_CODE
#if	defined(I586_CPU) || defined(I686_CPU)
	.globl	_C_LABEL(Pcpu_synch_done)

	cmpl	$0, _C_LABEL(Pcpu_synch_done)
	jnz	_C_LABEL(Pcpu_microtime)
#endif	/* PETINUM_CLOCK */
#endif	/* PC-98 */

	# clear registers and do whatever we can up front
	pushl	%edi
	pushl	%ebx
	xorl	%edx,%edx
	movl	$(TIMER_SEL0|TIMER_LATCH),%eax

	cli				# disable interrupts

	# select timer 0 and latch its counter
	outb	%al,$TIMER_MODE
	inb	$IO_ICU1,%al		# as close to timer latch as possible
	movb	%al,%ch			# %ch is current ICU mask

	# Read counter value into [%al %dl], LSB first
	inb	$TIMER_CNTR0,%al
	movb	%al,%dl			# %dl has LSB
	inb	$TIMER_CNTR0,%al	# %al has MSB

	# save state of IIR in ICU, and of ipending, for later perusal
	movb	_C_LABEL(ipending) + IRQ_BYTE(0),%cl # %cl is interrupt pending
	
	# save the current value of _time
	movl	_C_LABEL(time),%edi	# get time.tv_sec
	movl	_C_LABEL(time)+4,%ebx	#  and time.tv_usec

	sti				# enable interrupts, we're done

	# At this point we've collected all the state we need to
	# compute the time.  First figure out if we've got a pending
	# interrupt.  If the IRQ0 bit is set in ipending we've taken
	# a clock interrupt without incrementing time, so we bump
	# time.tv_usec by a tick.  Otherwise if the ICU shows a pending
	# interrupt for IRQ0 we (or the caller) may have blocked an interrupt
	# with the cli.  If the counter is not a very small value (3 as
	# a heuristic), i.e. in pre-interrupt state, we add a tick to
	# time.tv_usec

	testb	$IRQ_BIT(0),%cl		# pending interrupt?
	jnz	1f			# yes, increment count

	testb	$IRQ_BIT(0),%ch		# hardware interrupt pending?
	jz	2f			# no, continue
	testb	%al,%al			# MSB zero?
	jnz	1f			# no, add a tick
	cmpb	$3,%dl			# is this small number?
	jbe	2f			# yes, continue
1:	addl	_C_LABEL(isa_timer_tick),%ebx	# add a tick

	# We've corrected for pending interrupts.  Now do a table lookup
	# based on each of the high and low order counter bytes to increment
	# time.tv_usec
2:	movw	_C_LABEL(isa_timer_msb_table)(,%eax,2),%ax
	subw	_C_LABEL(isa_timer_lsb_table)(,%edx,2),%ax
	addl	%eax,%ebx		# add msb increment

	# Normalize the struct timeval.  We know the previous increments
	# will be less than a second, so we'll only need to adjust accordingly
	cmpl	$1000000,%ebx	# carry in timeval?
	jb	3f
	subl	$1000000,%ebx	# adjust usec
	incl	%edi		# bump sec
	
3:	movl	12(%esp),%ecx	# load timeval pointer arg
	movl	%edi,(%ecx)	# tvp->tv_sec = sec
	movl	%ebx,4(%ecx)	# tvp->tv_usec = usec

	popl	%ebx
	popl	%edi
	ret

#ifndef	ORIGINAL_CODE
#if	defined(I586_CPU) || defined(I686_CPU)
ENTRY(Pcpu_microtime)
	.globl	_C_LABEL(Pcpu_clock_factor), _C_LABEL(Pcpu_base_sec), _C_LABEL(Pcpu_base_usec)

	cli
	rdtsc				# pentium clock
	mull	_C_LABEL(Pcpu_clock_factor)
	movl	_C_LABEL(Pcpu_base_sec),%eax	# get time.tv_sec
	addl	_C_LABEL(Pcpu_base_usec),%edx	# add time.tv_usec
	sti

	movl	$1000000,%ecx
	cmpl	%ecx,%edx	# carry in timeval?
	jb	8f
	subl	%ecx,%edx	# adjust usec
	incl	%eax		# bump sec
	cmpl	%ecx,%edx	# carry in timeval?
	jae	9f
	
8:	movl	4(%esp),%ecx	# load timeval pointer arg
	movl	%eax,(%ecx)	# tvp->tv_sec = sec
	movl	%edx,4(%ecx)	# tvp->tv_usec = usec
	ret

9: 	incl	%eax		
	xorl	%edx, %edx
	jmp	8b

ENTRY(Pcpu_clock_synch)
	.globl	_C_LABEL(clock_factor), _C_LABEL(clock_divisor)

	xorl	%edx,%edx
	xorl	%eax,%eax
	movl	$16,%ecx

	cli
	wrmsr 
	movb	$(TIMER_SEL0|TIMER_LATCH),%al
	outb	%al,$TIMER_MODE		# latch timer count

	xorl	%ecx,%ecx
	inb	$TIMER_CNTR0,%al
	movb	%al,%cl
	inb	$TIMER_CNTR0,%al
	movb	%al,%ch
	sti

	movl	_C_LABEL(clock_divisor),%eax
	subl	%ecx,%eax
	mull	_C_LABEL(clock_factor)

	movl	_C_LABEL(time),%eax	# sec
	addl	_C_LABEL(time)+4,%edx	# usec
	addl	$10000, %edx	# add 10ms to usec
	movl	$1000000,%ecx
	cmpl	%ecx,%edx	# carry in timeval?
	jb	9f
	subl	%ecx,%edx	# adjust usec
	incl	%eax		# bump sec
	
9:	movl %edx,_C_LABEL(Pcpu_base_usec)
	movl %eax,_C_LABEL(Pcpu_base_sec)
	movl %eax,_C_LABEL(Pcpu_synch_done)
	ret

ENTRY(Pcpu_get_cycle)
	.globl	_C_LABEL(system_clock_freq)

	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp

	movl	_C_LABEL(system_clock_freq), %ebx
	shll	$1, %ebx
	movl	_C_LABEL(scf_div_100), %edi

	xorl	%edx,%edx
	xorl	%eax,%eax
	movl	$16,%ecx

	cli
	wrmsr

	movb	$(TIMER_SEL0|TIMER_LATCH),%al
	outb	%al,$TIMER_MODE		# latch timer 0's counter

	rdtsc				# pentium clock

	movl	%eax, %ebp

	inb	$TIMER_CNTR0,%al
	movb	%al,%cl
	inb	$TIMER_CNTR0,%al
	movb	%al,%ch
	movl	%ecx, %esi


6: 	movb	$(TIMER_SEL0|TIMER_LATCH),%al
	outb	%al,$TIMER_MODE		# latch timer 0's counter

	inb	$TIMER_CNTR0,%al
	movb	%al,%cl
	inb	$TIMER_CNTR0,%al
	movb	%al,%ch

	subl	%ecx, %esi
	subl	%esi, %ebx
	cmpl	%edx, %esi
	jge	7f
	subl	%edi, %ebx

7:	movl	%ecx, %esi
	cmpl	%edx, %ebx
	jg	6b

	rdtsc			# pentium clock (6 clocks)
	sti

	subl	%ebp, %eax
	shrl	$1, %eax

	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	ret
#endif	/* PENTIUM_CLOCK */
#endif	/* PC-98 */
