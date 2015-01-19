/*	$NecBSD: vector.s,v 3.29 1999/08/02 23:49:22 kmatsuda Exp $	*/
/*	$NetBSD: vector.s,v 1.36.8.1 1997/11/18 00:57:02 mellon Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995, 1997 Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *	derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */

#include "opt_ddb.h"

#include <i386/isa/icu.h>
#include <dev/isa/isareg.h>

#define ICU_HARDWARE_MASK

/*
 * These macros are fairly self explanatory.  If ICU_SPECIAL_MASK_MODE is
 * defined, we try to take advantage of the ICU's `special mask mode' by only
 * EOIing the interrupts on return.  This avoids the requirement of masking and
 * unmasking.  We can't do this without special mask mode, because the ICU
 * would also hold interrupts that it thinks are of lower priority.
 *
 * Many machines do not support special mask mode, so by default we don't try
 * to use it.
 */

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) % 8))
#define	IRQ_BYTE(irq_num)	((irq_num) / 8)

#ifdef ICU_SPECIAL_MASK_MODE

#define	ACK1(irq_num)
#define	ACK2(irq_num) \
	movb	$(0x60|IRQ_SLAVE),%al	/* specific EOI for IRQ2 */	;\
	outb	%al,$IO_ICU1
#define	MASK(irq_num, icu)
#define	UNMASK(irq_num, icu) \
	movb	$(0x60|(irq_num%8)),%al	/* specific EOI */		;\
	outb	%al,$icu

#else /* ICU_SPECIAL_MASK_MODE */

#ifndef	AUTO_EOI_1
#define	ACK1(irq_num) \
	movb	$(0x60|(irq_num%8)),%al	/* specific EOI */		;\
	outb	%al,$IO_ICU1
#else
#define	ACK1(irq_num)
#endif

#ifndef AUTO_EOI_2
#define	ACK2(irq_num) \
	movb	$(0x60|(irq_num%8)),%al	/* specific EOI */		;\
	outb	%al,$IO_ICU2		/* do the second ICU first */	;\
	movb	$(0x60|IRQ_SLAVE),%al	/* specific EOI for IRQ2 */	;\
	outb	%al,$IO_ICU1
#else
#define	ACK2(irq_num)
#endif

#ifdef ICU_HARDWARE_MASK

#define	MASK(irq_num, icu) \
	movb	_C_LABEL(imen) + IRQ_BYTE(irq_num),%al			;\
	orb	$IRQ_BIT(irq_num),%al					;\
	movb	%al,_C_LABEL(imen) + IRQ_BYTE(irq_num)			;\
	outb	%al,$(icu+icu_imr)

#define	UNMASK(irq_num, icu) \
	cli								;\
	movb	_C_LABEL(imen) + IRQ_BYTE(irq_num),%al			;\
	andb	$~IRQ_BIT(irq_num),%al					;\
	movb	%al,_C_LABEL(imen) + IRQ_BYTE(irq_num)			;\
	outb	%al,$(icu+icu_imr)						;\
	sti

#else /* ICU_HARDWARE_MASK */

#define	MASK(irq_num, icu)
#define	UNMASK(irq_num, icu)

#endif /* ICU_HARDWARE_MASK */

#endif /* ICU_SPECIAL_MASK_MODE */

/*
 * Macros for interrupt entry, call to handler, and exit.
 *
 * XXX
 * The interrupt frame is set up to look like a trap frame.  This may be a
 * waste.  The only handler which needs a frame is the clock handler, and it
 * only needs a few bits.  Xdoreti() needs a trap frame for handling ASTs, but
 * it could easily convert the frame on demand.
 *
 * The direct costs of setting up a trap frame are two pushl's (error code and
 * trap number), an addl to get rid of these, and pushing and popping the
 * callee-saved registers %esi, %edi, %ebx, and %ebp twice.
 *
 * If the interrupt frame is made more flexible,  INTR can push %eax first and
 * decide the ipending case with less overhead, e.g., by avoiding loading the
 * segment registers.
 *
 * XXX
 * Should we do a cld on every system entry to avoid the requirement for
 * scattered cld's?
 */

	.globl	_C_LABEL(isa_strayintr)
	.globl	_C_LABEL(intrframe_level)

/*
 * Normal vectors.
 *
 * We cdr down the intrhand chain, calling each handler with its appropriate
 * argument (0 meaning a pointer to the frame, for clock interrupts).
 *
 * The handler returns one of three values:
 *   0 - This interrupt wasn't for me.
 *   1 - This interrupt was for me.
 *  -1 - This interrupt might have been for me, but I don't know.
 * If there are no handlers, or they all return 0, we flags it as a `stray'
 * interrupt.  On a system with level-triggered interrupts, we could terminate
 * immediately when one of them returns 1; but this is a PC.
 *
 * On exit, we jump to Xdoreti(), to process soft interrupts and ASTs.
 */

#define MY_COUNT _C_LABEL(uvmexp)

/* XXX See comment in locore.s */
#ifdef __ELF__
#define	XINTR(irq_num)		Xintr/**/irq_num
#define	XHOLD(irq_num)		Xhold/**/irq_num
#define	XSTRAY(irq_num)		Xstray/**/irq_num
#else
#define	XINTR(irq_num)		_Xintr/**/irq_num
#define	XHOLD(irq_num)		_Xhold/**/irq_num
#define	XSTRAY(irq_num)		_Xstray/**/irq_num
#endif

#define	INTR_NESTCOUNT_INC(irq_num) \
	incl	_C_LABEL(intrframe_level)
#define	INTR_NESTCOUNT_DEC(irq_num) \
	decl	_C_LABEL(intrframe_level)

#define	INTR(irq_num, icu, ack) \
IDTVEC(recurse/**/irq_num)						;\
	pushfl								;\
	pushl	%cs							;\
	pushl	%esi							;\
	cli								;\
IDTVEC(intr/**/irq_num)							;\
	pushl	$0			/* dummy error code */		;\
	pushl	$T_ASTFLT		/* trap # for doing ASTs */	;\
	INTRENTRY							;\
	MAKE_FRAME							;\
	MASK(irq_num, icu)		/* mask it in hardware */	;\
	ack(irq_num)			/* and allow other intrs */	;\
	testb	$IRQ_BIT(irq_num),_C_LABEL(cpl) + IRQ_BYTE(irq_num)	;\
	jnz	XHOLD(irq_num)		/* currently masked; hold it */	;\
9:	INTR_NESTCOUNT_INC(irq_num)	/* inc intrframe level */	;\
	movl	_C_LABEL(cpl),%eax	/* cpl to restore on exit */	;\
	pushl	%eax							;\
	orl	_C_LABEL(intrmask) + (irq_num) * 4,%eax			;\
	movl	%eax,_C_LABEL(cpl)	/* add in this intr's mask */	;\
	sti				/* safe to take intrs now */	;\
	movl	_C_LABEL(intrhand) + (irq_num) * 4,%ebx	/* head of chain */ ;\
	testl	%ebx,%ebx						;\
	jz	XSTRAY(irq_num)		/* no handlears; we're stray */	;\
	STRAY_INITIALIZE		/* nobody claimed it yet */	;\
7:	movl	IH_ARG(%ebx),%eax	/* get handler arg */		;\
	testl	%eax,%eax						;\
	jnz	4f							;\
	movl	%esp,%eax		/* 0 means frame pointer */	;\
4:	pushl	%eax							;\
	call	IH_FUN(%ebx)		/* call it */			;\
	addl	$4,%esp			/* toss the arg */		;\
	incl	MY_COUNT+V_INTR		/* statistical info */		;\
	incl	_C_LABEL(intrcnt) + (4*(irq_num))	/* XXX */	;\
	STRAY_INTEGRATE			/* maybe he claimed it */	;\
	incl	IH_COUNT(%ebx)		/* count the intrs */		;\
	movl	IH_NEXT(%ebx),%ebx	/* next handler in chain */	;\
	testl	%ebx,%ebx						;\
	jnz	7b							;\
	STRAY_TEST			/* see if it's a stray */	;\
5:	INTR_NESTCOUNT_DEC(irq)		/* dec intrframe level */	;\
	UNMASK(irq_num, icu)		/* unmask it in hardware */	;\
	jmp	_C_LABEL(Xdoreti)	/* lower spl and do ASTs */	;\
IDTVEC(hold/**/irq_num)							;\
	orb	$IRQ_BIT(irq_num),_C_LABEL(ipending) + IRQ_BYTE(irq_num);\
	INTRFASTEXIT							;\
IDTVEC(resume/**/irq_num)						;\
	cli								;\
	testb	$IRQ_BIT(irq_num),_C_LABEL(imen) + IRQ_BYTE(irq_num)	;\
	jnz	9b							;\
	MASK(irq_num, icu)		/* mask it in hardware */	;\
	ack(irq_num)			/* and allow other intrs */	;\
	jmp	9b							;\
IDTVEC(stray/**/irq_num)						;\
	pushl	$irq_num						;\
	call	_C_LABEL(isa_strayintr)					;\
	addl	$4,%esp							;\
	incl	MY_COUNT+V_INTR		/* statistical info */		;\
	incl	_C_LABEL(strayintrcnt) + (4*(irq_num))			;\
	jmp	5b

#if defined(DEBUG) && defined(notdef)
#define	STRAY_INITIALIZE \
	xorl	%esi,%esi
#define	STRAY_INTEGRATE \
	orl	%eax,%esi
#define	STRAY_TEST \
	testl	%esi,%esi						;\
	jz	XSTRAY(irq_num)
#else /* !DEBUG */
#define	STRAY_INITIALIZE
#define	STRAY_INTEGRATE
#define	STRAY_TEST
#endif /* DEBUG */

#ifdef DDB
#define	MAKE_FRAME \
	leal	-8(%esp),%ebp
#else /* !DDB */
#define	MAKE_FRAME
#endif /* DDB */

INTR(0, IO_ICU1, ACK1)
INTR(1, IO_ICU1, ACK1)
INTR(2, IO_ICU1, ACK1)
INTR(3, IO_ICU1, ACK1)
INTR(4, IO_ICU1, ACK1)
INTR(5, IO_ICU1, ACK1)
INTR(6, IO_ICU1, ACK1)
INTR(7, IO_ICU1, ACK1)
INTR(8, IO_ICU2, ACK2)
INTR(9, IO_ICU2, ACK2)
INTR(10, IO_ICU2, ACK2)
INTR(11, IO_ICU2, ACK2)
INTR(12, IO_ICU2, ACK2)
INTR(13, IO_ICU2, ACK2)
INTR(14, IO_ICU2, ACK2)
INTR(15, IO_ICU2, ACK2)

/*
 * These tables are used by the ISA configuration code.
 */
/* interrupt service routine entry points */
IDTVEC(intr)
	.long	_C_LABEL(Xintr0), _C_LABEL(Xintr1)
	.long	_C_LABEL(Xintr2), _C_LABEL(Xintr3)
	.long	_C_LABEL(Xintr4), _C_LABEL(Xintr5)
	.long	_C_LABEL(Xintr6), _C_LABEL(Xintr7)
	.long	_C_LABEL(Xintr8), _C_LABEL(Xintr9)
	.long	_C_LABEL(Xintr10), _C_LABEL(Xintr11)
	.long	_C_LABEL(Xintr12), _C_LABEL(Xintr13)
	.long	_C_LABEL(Xintr14), _C_LABEL(Xintr15)

/*
 * These tables are used by Xdoreti() and Xspllower().
 */
/* resume points for suspended interrupts */
IDTVEC(resume)
	.long	_C_LABEL(Xresume0), _C_LABEL(Xresume1)
	.long	_C_LABEL(Xresume2), _C_LABEL(Xresume3)
	.long	_C_LABEL(Xresume4), _C_LABEL(Xresume5)
	.long	_C_LABEL(Xresume6), _C_LABEL(Xresume7)
	.long	_C_LABEL(Xresume8), _C_LABEL(Xresume9)
	.long	_C_LABEL(Xresume10), _C_LABEL(Xresume11)
	.long	_C_LABEL(Xresume12), _C_LABEL(Xresume13)
	.long	_C_LABEL(Xresume14), _C_LABEL(Xresume15)
	/* for soft interrupts */
	.long	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	.long	_C_LABEL(Xsoftserial), _C_LABEL(Xsoftnet), _C_LABEL(Xsoftclock)
/* fake interrupts to resume from splx() */
IDTVEC(recurse)
	.long	_C_LABEL(Xrecurse0), _C_LABEL(Xrecurse1)
	.long	_C_LABEL(Xrecurse2), _C_LABEL(Xrecurse3)
	.long	_C_LABEL(Xrecurse4), _C_LABEL(Xrecurse5)
	.long	_C_LABEL(Xrecurse6), _C_LABEL(Xrecurse7)
	.long	_C_LABEL(Xrecurse8), _C_LABEL(Xrecurse9)
	.long	_C_LABEL(Xrecurse10), _C_LABEL(Xrecurse11)
	.long	_C_LABEL(Xrecurse12), _C_LABEL(Xrecurse13)
	.long	_C_LABEL(Xrecurse14), _C_LABEL(Xrecurse15)
	/* for soft interrupts */
	.long	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	.long	_C_LABEL(Xsoftserial), _C_LABEL(Xsoftnet), _C_LABEL(Xsoftclock)


/* Old-style vmstat -i interrupt counters.  Should be replaced with evcnts. */
	.data
	.globl	_C_LABEL(intrnames), _C_LABEL(eintrnames)
	.globl	_C_LABEL(intrcnt), _C_LABEL(eintrcnt)

	ALIGN_DATA
_C_LABEL(intrnames):
	       /*0123456789a*/
	.asciz	"00:timer   "
	.asciz	"01:kbd     "
	.asciz	"02:crtv    "
	.asciz	"03:int0    "
	.asciz	"04:rs232c  "
	.asciz	"05:int1    "
	.asciz	"06:int2    "
	.asciz	"07:slave   "
	.asciz	"08:printer "
	.asciz	"09:ide     "
	.asciz	"10:int41   "
	.asciz	"11:fd      "
	.asciz	"12:int5    "
	.asciz	"13:mouse   "
	.asciz	"14:printer "
	.asciz	"15:systm   "
_C_LABEL(strayintrnames):
	.asciz	"stray0", "stray1", "stray2", "stray3"
	.asciz	"stray4", "stray5", "stray6", "stray7"
	.asciz	"stray8", "stray9", "stray10", "stray11"
	.asciz	"stray12", "stray13", "stray14", "stray15"
_C_LABEL(eintrnames):

	/* And counters */
	.data
	.align 4
_C_LABEL(intrcnt):
	.long	0, 0, 0, 0, 0, 0, 0, 0
	.long	0, 0, 0, 0, 0, 0, 0, 0
_C_LABEL(strayintrcnt):
	.long	0, 0, 0, 0, 0, 0, 0, 0
	.long	0, 0, 0, 0, 0, 0, 0, 0
_C_LABEL(eintrcnt):

	.text
