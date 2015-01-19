/*	$NecBSD: cputypes.h,v 3.3 1999/07/23 05:15:56 kmatsuda Exp $	*/
/*	$NetBSD: cputypes.h,v 1.11 1998/10/15 13:40:33 bad Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	 NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 * Copyright (c) 1993 Christopher G. Demetriou
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Classes of Processor. CPU identification code depends on
 * this starting at 0, and having an increment of one.
 */

#define	CPUCLASS_386	0
#define	CPUCLASS_486	1
#define	CPUCLASS_586	2
#define	CPUCLASS_686	3

#ifdef	ORIGINAL_CODE
/*
 * Kinds of Processor. Only the first 7 are used, as they are processors
 * that might not have a cpuid instruction.
 */
#else	/* PC-98 */
/*
 * Kinds of Processor. Only the first 8 are used, as they are processors
 * that might not have a cpuid instruction.
 */
#endif	/* PC-98 */

#define	CPU_386SX	0	/* Intel 80386SX */
#define	CPU_386		1	/* Intel 80386DX */
#define	CPU_486SX	2	/* Intel 80486SX */
#define	CPU_486		3	/* Intel 80486DX */
#define	CPU_486DLC	4	/* Cyrix 486DLC */
#ifdef	ORIGINAL_CODE
#define CPU_6x86	5	/* Cyrix/IBM 6x86 */
#define CPU_NX586	6	/* NexGen 586 */
#define	CPU_586		7	/* Intel P.....m (I hate lawyers; it's TM) */
#define CPU_AM586	8	/* AMD Am486 and Am5x86 */
#define CPU_K5		9	/* AMD K5 */
#define CPU_K6		10	/* NexGen 686 aka AMD K6 */
#define CPU_686		11	/* Intel Pentium Pro */
#define CPU_C6		12	/* IDT WinChip C6 */
#else	/* PC-98 */
#define CPU_5x86	5	/* Cyrix/IBM 5x86 */
#define CPU_6x86	6	/* Cyrix/IBM 6x86 */
#define CPU_NX586	7	/* NexGen 586 */
#define	CPU_586		8	/* Intel P.....m (I hate lawyers; it's TM) */
#define CPU_AM586	9	/* AMD Am486 and Am5x86 */
#define CPU_K5		10	/* AMD K5 */
#define CPU_K6		11	/* NexGen 686 aka AMD K6 */
#define CPU_686		12	/* Intel Pentium Pro */
#define CPU_C6		13	/* IDT WinChip C6 */
#endif	/* PC-98 */

/*
 * CPU vendors
 */

#define CPUVENDOR_UNKNOWN	-1
#define CPUVENDOR_INTEL		0
#define CPUVENDOR_CYRIX		1
#define CPUVENDOR_NEXGEN	2
#define CPUVENDOR_AMD		3
#define CPUVENDOR_IDT		4

/*
 * Some other defines, dealing with values returned by cpuid.
 */

#define CPU_MAXMODEL	15	/* Models within family range 0-15 */
#define CPU_DEFMODEL	16	/* Value for unknown model -> default  */
#define CPU_MINFAMILY	 4	/* Lowest that cpuid can return (486) */
#define CPU_MAXFAMILY	 6	/* Highest we know (686) */