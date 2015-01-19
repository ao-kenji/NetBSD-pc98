/*	$NecBSD: cpu.c,v 1.40.4.1 1999/12/14 06:35:31 kmatsuda Exp $	*/
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
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Kouichi Matsuda.  All rights reserved.
 */

#include "opt_cputype.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <i386/include/pmap.h>

#include <machine/isa_machdep.h>
#include <i386/isa/cpuspec.h>

/************************************************
 * Proto
 ************************************************/
void flush_cache __P((void));	/* XXX */
void flush_wbcache __P((void));	/* XXX */

static u_int8_t read_cyrix_reg __P((u_short));
static void write_cyrix_reg __P((u_short, u_int8_t));
static void set_cyrix_crg __P((u_short, u_int));

#ifdef	BLUELIGHTNING
static int ibm_486DLC3_cache __P((cpu_tag_t, u_int));
static void set_ibm486_wrmsr __P((u_int32_t, u_int32_t, u_int32_t));
#endif	/* BLUELIGHTNING */
/************************************************
 * tags
 ************************************************/
static struct cpu_tag cpu_tag_cyrix_486DLC = {
	0,
	NULL,
	
	CPU_FLAGS_PG_TRAP | CPU_FLAGS_CF_POSTR | CPU_FLAGS_CF_POSTW,

	0,
	CPU_control_cyrix_486DLC_enable,

	NULL,
	NULL,
	flush_cache,
	flush_cache,
};

static struct cpu_tag cpu_tag_cyrix_5x86 = {
	0,
	NULL,

	CPU_FLAGS_CF_PRER | CPU_FLAGS_CF_PREW,

	0,
	CPU_control_cyrix_5x86_enable,

	flush_wbcache,		/* write back cache */
	flush_wbcache,		/* write back cache */
	NULL,
	NULL,
};

#ifdef	BLUELIGHTNING
static struct cpu_tag cpu_tag_ibm_486 = {
	0,
	NULL,

	CPU_FLAGS_PG_TRAP | CPU_FLAGS_CF_POSTR | CPU_FLAGS_CF_POSTW,

	0,
	ibm_486DLC3_cache,

	NULL,
	NULL,
	flush_cache,		/* same as Cyrix 486DLC */
	flush_cache,		/* same as Cyrix 486DLC */
};
#endif	/* BLUELIGHTNING */

static struct cpu_tag cpu_tag_WriteBack_enable = {
	0,
	NULL,

	CPU_FLAGS_CF_PRER | CPU_FLAGS_CF_PREW | \
        CPU_FLAGS_CF_POSTR | CPU_FLAGS_CF_POSTW,

	0,
	CPU_control_WriteBack_enable,

	flush_wbcache,		/* pre read case ? */
	flush_wbcache,
	flush_cache,	
	flush_cache,
};

static struct cpu_tag cpu_tag_intel = {
	0,
	NULL,

	0,

	0,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,
};

cpu_tag_t curcpu = &cpu_tag_intel;

/************************************************
 * subr
 ************************************************/
void
CPU_control_cpu_init(void)
{
	/* Currently, the variable `cpu' is stored if he is a processor
	 * that might not have a cpuid instruction,
	 * 	    value of `cpu'
	 * CPU_386SX	0	if Intel 80386SX
	 * CPU_386	1	if Intel 80386DX
	 * CPU_486SX	2	if Intel 80486SX
	 * CPU_486	3	if Intel 80486DX
	 * CPU_486DLC	4	if Cyrix 486DLC
	 * CPU_NX586	5	if NexGen 586
	 * CPU_5x86	6	if Cyrix/IBM 5x86
	 */
	switch (cpu)
	{
#ifdef	BLUELIGHTNING
	case CPU_486:
		/* XXX: how to treat IBM BlueLightning??? */
		curcpu = &cpu_tag_ibm_486;
		break;
#endif	/* BLUELIGHTNING */

	default:
#ifdef	MELCO_EUDF
		{
		extern char cpu_vendor[];

		/* XXX */
		if (!strncmp(cpu_vendor, "GenuineIntel", sizeof(u_int32_t)*3))
			curcpu = &cpu_tag_WriteBack_enable;
		}
#endif	/* MELCO_EUDF */
		break;
	}

	if (curcpu == NULL)
		curcpu = &cpu_tag_intel;

	cpu_cc(curcpu, CPU_CC_ON);
}

/************************************************
 * Cyrix family Cpu Identification.
 ************************************************/
#include <i386/isa/cpureg.h>

/************************************************
 * Cyrix 486DLC family Cpu Control.
 ************************************************/
static u_int8_t
read_cyrix_reg(addr)
	u_short addr;
{

	outb(0x22, (u_int8_t) addr);
	return inb(0x23);
}

static void
write_cyrix_reg(addr, data)
	u_short addr;
	u_int8_t data;
{

	outb(0x22, (u_int8_t) addr);
	outb(0x23, data);
}

static void
set_cyrix_crg(addr, data)
	u_short addr;
	u_int data;
{

	write_cyrix_reg(addr, (data >> 16) & 0xff);
	write_cyrix_reg(addr + 1, (data >> 8) & 0xff);
	write_cyrix_reg(addr + 2, data & 0xff);
}

/*
 * check_cyrix_model(): check DIR0 value and Cyrix CPU type
 */
void
CPU_control_check_cyrix_model(void)
{
	extern char cpu_vendor[];
	extern int cpu;
	u_int8_t save_ccr2, save_ccr3;
	int ccr2_rw = 0, ccr3_rw = 0;
	u_int8_t dir0_val;

#define	CCR3_MASK	0x80	/* reserved for almost all Cyrix CPU */

	dir0_val = -1;

	if (!strcmp(cpu_vendor, "Cyrix"))
	{
		/* Step 1:
		 * test CCR2 register read/writable
		 */
		/* get current CCR2 value */
		save_ccr2 = read_cyrix_reg(CCR2);

		/* toggle test bit and write test value to CCR2 */
		write_cyrix_reg(CCR2, (save_ccr2 ^ CCR2_LOCK_NW));
		read_cyrix_reg(CCR2);		/* dummy read to change bus */

		/* did test bit toggle? */
		if (read_cyrix_reg(CCR2) != save_ccr2)
			ccr2_rw = 1;		/* yes, bit changed */

		/* return CCR2 to original value */
		write_cyrix_reg(CCR2, save_ccr2);

		/* Step 2:
		 * test CCR3 register read/writable
		 */
		/* get current CCR3 value */
		save_ccr3 = read_cyrix_reg(CCR3);

		/* toggle test bit and write test value to CCR3 */
		write_cyrix_reg(CCR3, (save_ccr3 ^ CCR3_MASK));
		read_cyrix_reg(CCR3);		/* dummy read to change bus */

		/* did test bit change? */
		if (read_cyrix_reg(CCR3) != save_ccr3)
			ccr3_rw = 1;		/* yes, it did */

		/* return CCR3 to original value */
		write_cyrix_reg(CCR3, save_ccr3);

		if ((ccr2_rw && ccr3_rw) || (!ccr2_rw && ccr3_rw))
		{
			/*
			 * device id register OK
			 * read device id via DIR0
			 */
			dir0_val = read_cyrix_reg(DIR0);
		}
		else if (ccr2_rw && !ccr3_rw)
		{
			/* Cx486S A step */
			dir0_val = 0xfe;
			return;
		}
		else if (!ccr2_rw && !ccr3_rw)
		{
			/* Pre ID registers. Cx486SLC or DLC */
			dir0_val = 0xfd;
			return;
		}
	}

	if (((0x20 <= dir0_val) && (dir0_val <= 0x27))
	    || (0x30 <= dir0_val)) {
		cpu = CPU_6x86;
	}
}

int
CPU_control_cyrix_486DLC_enable(ct, flag)
	cpu_tag_t ct;
	u_int flag;
{
	int disable_x16cache;

	/* if ram exists, enable cache */
	disable_x16cache = (lookup_sysinfo(SYSINFO_X16HASRAM) == 0);

	asm("pushfl");
	disable_intr();
	curcpu = &cpu_tag_cyrix_486DLC;

	lcr0(rcr0() | CR0_CD | CR0_NW);
	flush_cache();	/* XXX */
	write_cyrix_reg(CCR0, 0);
	write_cyrix_reg(CCR1, 0);
	if (flag)
	{
		ct->ct_ccs = 1;
		/* enable cache region 0x0a0000->0x0c0000 128k = 0x0a06 */
		set_cyrix_crg(NCR1, 0x0a06);
		/* enable cache region 0x0c0000->0x100000 256k = 0x0c07 */
		set_cyrix_crg(NCR2, 0x0c07);
		/* enable cache region 0xf00000->0x1000000 1M = 0xf009 */
		set_cyrix_crg(NCR3, disable_x16cache > 0 ? 0xf009 : 0);
	}
	else
	{
		ct->ct_ccs = 0;
		set_cyrix_crg(NCR1, 0x0f);
		set_cyrix_crg(NCR2, 0x0);
		set_cyrix_crg(NCR3, 0x0);
	}
	set_cyrix_crg(NCR4, 0x0);
	lcr0(rcr0() & (~(CR0_CD | CR0_NW)));
	asm("popfl");
	return 0;
}

#ifndef	CYRIX_5X86_PCR0
#define	PCR0_5X86	(0)	/* nothing */
#else	/* CYRIX_5X86_PCR0 */
#define	PCR0_5X86	((CYRIX_5X86_PCR0) & (PCR0_RSTK_EN | PCR0_LOOP_EN | PCR0_LSSER | PCR0_BTB_EN))
#endif	/* CYRIX_5X86_PCR0 */

int
CPU_control_cyrix_5x86_enable(ct, flag)
	cpu_tag_t ct;
	u_int flag;
{

	asm("pushfl");
	disable_intr();
	curcpu = &cpu_tag_cyrix_5x86;
	lcr0(rcr0() | CR0_CD | CR0_NW);	/* disable cache */
	flush_wbcache();	/* XXX */
	if (flag == CPU_CONTROL_CACHE_ENABLE)
	{
		ct->ct_ccs = 1;
		(void) read_cyrix_reg(CCR3);
		/* CCR1 */
		write_cyrix_reg(CCR1, 0);
		/* CCR2:
		 * CCR2_USE_WBAK
		 *	enable Write-Back Cache
		 * CCR2_SUSP_HALT
		 *	enables entering suspend mode on HLT instructions
		 */
		write_cyrix_reg(CCR2, (CCR2_USE_WBAK | CCR2_SUSP_HALT));
		/* CCR3:
		 * CCR3_MAPEN0
		 *	selects active controle register set for 0xd0..0xfd
		 *	to access CCR4
		 */
		write_cyrix_reg(CCR3, CCR3_MAPEN0);
		/* CCR4:
		 * CCR4_DTE_EN
		 *	enables directory table entry cache
		 * CCR4_MEM_BYP
		 *	enables memory bypassing
		 * CCR4.bit0..bit2 unchanged (i.e., no clock delay)
		 */
		write_cyrix_reg(CCR4, (CCR4_DTE_EN | CCR4_MEM_BYP));
		/* PCR0:
		 * default:
		 * PCR0_BTB_EN
		 *	branch target buffer enable
		 * optional:
		 * PCR0_RSTK_EN
		 *	return stack enable
		 * PCR0_LOOP_EN
		 *	loop enable
		 * PCR0_LSSER
		 *	load store serialize enable (reorder disable)
		 */
		write_cyrix_reg(PCR0, PCR0_5X86);
		/* CCR3 */
		write_cyrix_reg(CCR3, 0x00);
		/* dummy */
		(void) read_cyrix_reg(0x80);
	}
	else
	{
		ct->ct_ccs = 0;
		/* XXX: cache disable */
	}
	/* enable cache: write back mode */
	lcr0(rcr0() & ~CR0_CD);
	asm("popfl");
	return 0;
}

#ifdef	BLUELIGHTNING
/************************************************
 * IBM BLUE LIGHTNING (486DLC3) family Cpu Control.
 ************************************************/
static void
set_ibm486_wrmsr(redx, reax, recx)
	u_int32_t redx, reax, recx;
{

	asm(".byte 0x0f, 0x30" :
			       : "a" (reax), "c" (recx), "d" (redx)
			       : "%eax", "%ecx", "%edx");
}

static int
ibm_486DLC3_cache(ct, flag)
	cpu_tag_t ct;
	u_int flag;
{

	asm("pushfl");
	disable_intr();
	lcr0(rcr0() | CR0_CD | CR0_NW);
	flush_cache();	/* XXX */
	if (flag == CPU_CONTROL_CACHE_ENABLE)
	{
		ct->ct_ccs = 1;
		/* If using Intel-FPU, set 0x1c92 instead of 0x9c92 */
		set_ibm486_wrmsr(0x0, 0x9c92, 0x1000);
		/* 13MB(0x0d) cache. (over 1MB-14MB region
		 * 0-640KB(0x03ff) cache. (64KB block * 10)
		 */
		set_ibm486_wrmsr(0xd0, 0x03ff, 0x1001);
		/* 0x03000000 means double-clock-mode. or set 0x0 */
		set_ibm486_wrmsr(0x0, 0x03000000, 0x1002);
	}
	else
	{
		ct->ct_ccs = 0;
		/* XXX: cache disable */
	}
	lcr0(rcr0() & (~(CR0_CD | CR0_NW)));
	asm("popfl");
	return 0;
}
#endif	/* BLUELIGHTNING */

/******************************************************************
 * GENERIC WB cache control
 *****************************************************************/
int
CPU_control_WriteBack_enable(ct, flag)
	cpu_tag_t ct;
	u_int flag;
{

	asm("pushfl");
	disable_intr();
	curcpu = &cpu_tag_WriteBack_enable;	/* XXX */

	if (flag == CPU_CONTROL_CACHE_ENABLE)
	{
		ct->ct_ccs = 1;
		lcr0(rcr0() | CR0_CD | CR0_NW);
		flush_cache();	/* XXX */
		lcr0(rcr0() & (~(CR0_CD | CR0_NW)));
	}
	else
	{
		ct->ct_ccs = 0;
		lcr0(rcr0() | CR0_CD | CR0_NW);
		flush_cache();	/* XXX */
		/* lcr0(rcr0() & (~(CR0_CD | CR0_NW))); */
	}
	asm("popfl");
	return 0;
}

/******************************************************************
 * AMD K5/K6 setup
 *****************************************************************/
#define	CPU_AMD_NOT_OPTIMIZE		0x00
#define	CPU_AMD_WRITE_ALLOCATE_ENABLE	0x01
#define	CPU_AMD_CACHE_DISABLE		0x02

int cpu_k5_ctrl = CPU_AMD_NOT_OPTIMIZE;			/* XXX */
int cpu_k6_ctrl = CPU_AMD_WRITE_ALLOCATE_ENABLE;

int
CPU_control_AMD_K5_or_K6_enable(ct, flags)
	cpu_tag_t ct;
	u_int flags;
{
	extern int cpu_id;
	u_long maxmem = lookup_sysinfo(SYSINFO_MEMHIGH);

	switch (cpu_id & 0xff0)
	{
	case 0x560:
	case 0x570:
	case 0x580:
		if ((cpu_k6_ctrl & CPU_AMD_WRITE_ALLOCATE_ENABLE) != 0)
		{
			u_long val;

			val = ((maxmem / K6_WHCR_PAGESZ) << 1);
			val &= K6_WHCR_WAELIM;
			if (lookup_sysinfo(SYSINFO_X16HASRAM) != 0)
				val |= K6_WHCR_WAE15M;

			wrmsr(CPUMSR_K6_WHCR, (u_int64_t) val); 
			printf("cpu0: enable K6 write allocate(0x%lx)\n", val);
		}

		if ((cpu_k6_ctrl & CPU_AMD_CACHE_DISABLE) != 0)
		{
			wrmsr(CPUMSR_K6_TR12, (u_int64_t) K6_TR12_CI); 
			printf("cpu0: disable K6 1st cache\n");
		}
		break;

	case 0x590:
		if ((cpu_k6_ctrl & CPU_AMD_WRITE_ALLOCATE_ENABLE) != 0)
		{
			u_long val;

			val = ((maxmem / K6_3_WHCR_PAGESZ) << K6_3_WHCR_SHIFT);
			val &= K6_3_WHCR_WAELIM;
			if (lookup_sysinfo(SYSINFO_X16HASRAM) != 0)
				val |= K6_3_WHCR_WAE15M;

			wrmsr(CPUMSR_K6_3_WHCR, (u_int64_t) val); 
			printf("cpu0: enable K6III write allocate(0x%lx)\n", val);
		}
		break;

	case	0x510:
	case 	0x520:
	case	0x530:
		if ((cpu_id & 0x0f) <= 4)
			break;

		if ((cpu_k5_ctrl & CPU_AMD_WRITE_ALLOCATE_ENABLE) != 0)
		{
			u_long msr, eval;

			/*
			 * Written by KATO Takenori.
		 	 * what are the register names of 0x83, 0x85, 0x86 ?
			 */
			msr = rdmsr(0x83);
			wrmsr(0x83, (u_int64_t) (msr & ~0x10));
 
			wrmsr(0x86, (u_int64_t) 0x0ff00f0);

			eval = (maxmem / NBPG) << 16;
			eval |= (K5_WT_ALLOC_TME | K5_WT_ALLOC_PRE |
			         K5_WT_ALLOC_FRE);
			wrmsr(0x85, (u_int64_t) eval);
			(void) rdmsr(0x85);

			msr = rdmsr(0x83);
			wrmsr(0x83, (u_int64_t) (msr | 0x10));

			printf("cpu0: enable K5 write allocate\n");
		}
	
	default:
		break;
	}	
	return 0;
}

/******************************************************************
 * IDT C6
 *****************************************************************/
static void IDT_C6_write_combine_region __P((register_t, bus_addr_t, bus_size_t)); 

static void
IDT_C6_write_combine_region(reg, base, mask)
	register_t reg;
	bus_addr_t base;
	bus_size_t mask;
{

	mask &= ~C2_MCR_PAGE_MASK;
	mask |= (C2_MCR_ATTR_WC | C2_MCR_ATTR_WRO);
	base &= ~C2_MCR_PAGE_MASK;
	printf("cpu0: IDT C2 write combine: register 0x%x base 0x%lx mask 0x%lx\n",
		reg, base, mask);
	__asm __volatile("wrmsr" : :"a" (mask), "c" (reg), "d" (base));
}

int
CPU_control_IDT_C6_enable(ct, flags)
	cpu_tag_t ct;
	u_int flags;
{
	u_long maxmem = lookup_sysinfo(SYSINFO_MEMHIGH);
	int x16hon = lookup_sysinfo(SYSINFO_X16HASRAM);
	bus_addr_t addr;
	register_t reg;
	u_int64_t regv;

	regv = rdmsr(C2_MCR_CTRL);
	if ((((u_long) regv) & C2_MCR_CTRL_IMASK) != C2_MCR_CTRL_ITRT)
		return EINVAL;

	regv |= (u_int64_t) (C2_MCR_CTRL_WC | C2_MCR_CTRL_TRT);
	wrmsr(C2_MCR_CTRL, regv);
	
	IDT_C6_write_combine_region(C2_MCR_0, 0, (~0x7ffff));

	addr = 0x100000;
	for (reg = C2_MCR_1; reg <= C2_MCR_7; reg ++, addr += addr)
	{
		if (x16hon == 0 && addr == 0x800000)
			continue;
		if (addr >= maxmem)	/* XXX: I know the overflow !! */
			continue;
 
		regv = rdmsr(C2_MCR_CTRL);
		regv |= (u_int64_t) (C2_MCR_CTRL_WC | C2_MCR_CTRL_TRT);
		wrmsr(C2_MCR_CTRL, regv);
		IDT_C6_write_combine_region(reg, addr, ~(addr - 1));
	}

	regv = rdmsr(C2_MCR_CTRL);
	printf("cpu0: IDT C2 mcr ctrl: 0x%lx\n", (u_long) regv);
	return 0;
}
