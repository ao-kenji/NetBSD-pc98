/*	$NecBSD: sysinfo.c,v 1.23.4.2 1999/12/01 17:41:36 honda Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/sysinfo.h>
#include <machine/vm_interface.h>
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <i386/pio.h>

static void init_sysclk __P((void));
static u_int total_memsz __P((void));
static void product_memory_hook __P((psize_t *));

/************************************************
 * BIOS table refs
 ************************************************/
u_int8_t system_parameter[BIOS_TABLE_SIZE];

u_int32_t
lookup_bios_param(addr, size)
	vaddr_t addr;
	u_int size;
{
	u_int res;

	if (addr < BIOS_TABLE_OFFSET)
		goto bad;

	addr -= BIOS_TABLE_OFFSET;
	if (addr < BIOS_TABLE_SIZE)
	{
		switch (size)
		{
		case sizeof(u_int8_t):
			res = *(u_int8_t *) &system_parameter[addr];
			break;
		case sizeof(u_int16_t):
			res = *(u_int16_t *) &system_parameter[addr];
			break;
		case sizeof(u_int32_t):
			res = *(u_int32_t *) &system_parameter[addr];
			break;
		default:
			goto bad;
		}
		return res;
	}
bad:
	printf("illegal bios_table ref\n");
	return 0;
}

/************************************************
 * system clock info
 ************************************************/
#define	FREQ5M			2457600
#define	FREQ8M			1996800

u_int system_clock;
u_int system_clock_freq;
u_int scf_div_100;

static void
init_sysclk(void)
{

	/* 0x501 system clock */
	if (lookup_bios_param(BIOS_SYSTEM_CLOCK, sizeof(u_int8_t)) & 0x80)
	{
		system_clock = SYSTEM_CLOCK_8M;
		system_clock_freq = FREQ8M;
	}
	else
	{
		system_clock = SYSTEM_CLOCK_5M;
		system_clock_freq = FREQ5M;
	}
	/* this is used in microtime.s */
	scf_div_100 = system_clock_freq / 100;
}

/************************************************
 * phys mem info
 ************************************************/
static vaddr_t x16hole_start;
static psize_t x16hole_sz;
static int x16hole_hasram;
static sysinfo_info_t mem_level;
static sysinfo_info_t mem_high;

static u_int
total_memsz(void)
{
	psize_t mem, ext_mem;

	/*
	 * Check real mem size between 1M <=> 16M
	 */
#ifndef FEXTMEM_SIZE
	/* first memory a unit per 128K at 0x401 */
	mem = lookup_bios_param(BIOS_EXT_MEM, sizeof(u_int8_t)) * 128;
#else	/* FEXTMEM_SIZE */
	mem = ((FEXTMEM_SIZE) / 128) * 128;
#endif	/* FEXTMEM_SIZE */
	if (mem > 1024 * 15)
		mem = 1024 * 15;

	product_memory_hook(&mem);	/* hook for epson machines */

	x16hole_start = 0x100000 + mem * 1024;

	/*
	 * Check real mem size over 16M
	 */
#ifndef	SEXTMEM_SIZE
	ext_mem = lookup_bios_param(BIOS_EXT_MEM16, sizeof(u_int16_t));
#else	/* SEXTMEM_SIZE */
	ext_mem = (SEXTMEM_SIZE) / 1024;
#endif	/* SEXTMEM_SIZE */

	ext_mem = ext_mem * 1024;	/* kb unit */
	if (ext_mem != 0)
	{
		mem += ext_mem;
		mem_level = MEM_LEVEL1;
		mem_high = X16HOLE_END + ext_mem * 1024;
		x16hole_sz = X16HOLE_END - x16hole_start;
	}
	else if (mem != 0)
	{
		mem_high = x16hole_start;
		x16hole_sz = 0;
	}
	else
	{
		mem_high = 0xa0000;
		x16hole_sz = 0;
	}

	/*
	 * Check if 15-16M hole has really RAM (size 1M).
	 */
	if (mem_high >= X16HOLE_END && x16hole_sz == 0)
		x16hole_hasram = 1;

	return mem;
}

/**************************************
 * system info init
 **************************************/
static u_int com_clock_mode;
static u_int com_level;
static u_int pmemsz;
static sysinfo_info_t info_crt;
static sysinfo_info_t info_machine;
static sysinfo_info_t info_keyboard;
static sysinfo_info_t info_consmode;
static sysinfo_info_t info_product;
static void info_machine_product __P((void));

static void
info_machine_product(void)
{
	int count;
	u_int32_t id = 0;

	for (count = 0x10000; count > 0; count -- )
	{
		if ((inb(IO_GDC1) & 0x20) == 0)
			break;	
	}
	for (count = 0x10000; count > 0; count -- )
	{
		if ((inb(IO_GDC1) & 0x20) != 0)
			break;	
	}
	outb(IO_CGROM, 0);
	(void) inb(0x5f);
	outb(IO_CGROM + 2, 'A');
	(void) inb(0x5f);

	for (count = 0; count < sizeof(id); count ++)
		id += *(((u_int32_t *) ISA_HOLE_VADDR(0xa4000)) + count);
	if (id == 0x6efc58fc)
		info_product |= PRODUCT_VENDOR_NEC;
	else
		info_product |= PRODUCT_VENDOR_EPSON;

	if (lookup_bios_param(BIOS_H98_FLAGS, sizeof(u_int8_t)) & 0x80)
		info_product |= PRODUCT_NEC_H98;

	if ((info_product & PRODUCT_VENDOR_MASK) == PRODUCT_VENDOR_EPSON)
	{
		id = *((u_int8_t *) ISA_HOLE_VADDR(0xfd804));
		info_product  |= (id & PRODUCT_MINOR_MASK);
	}
}

void
init_sysinfo(void)
{

	/* machine check */
	info_machine_product();

	/* mem info */
	pmemsz = total_memsz();

	/* system clock info */
	init_sysclk();

	/* com info */
	com_level = COM_LEVEL2;

	/* crt info */
	if (lookup_bios_param(BIOS_XXX001, sizeof(u_int8_t)) & 0x40)
		info_crt |= CRT_HAS_PEGC;
	if (lookup_bios_param(BIOS_XXX002, sizeof(u_int8_t)) & 0x20)
		info_crt |= CRT_HZ_315;

	/* machine type */
	/*
	 * detect NOTE
	 * if it supports RAM disk or
	 * if it supports PEGC and has a keyborad of NOTE type
	 */
	if ((lookup_bios_param(BIOS_FLAGS2, sizeof(u_int8_t)) & 0x80) ||
	    ((lookup_bios_param(BIOS_XXX001, sizeof(u_int8_t)) & 0x40) &&
	     (lookup_bios_param(BIOS_KBD_TYPE, sizeof(u_int8_t)) & 0x8)))
		info_machine |= MACHINE_NOTE;

	/* keyboard type */
	if (lookup_bios_param(BIOS_KBD_TYPE, sizeof(u_int8_t)) & 0x8)
		info_keyboard |= KBD_NOTE;

	/* PEGC && ( 31.5 k hz || NOTE ) */
	if ((info_crt & CRT_HAS_PEGC) &&
	    ((info_crt & CRT_HZ_315) || (info_machine & MACHINE_NOTE)))
		info_crt |= CRT_PEGC_ENABLED;

	/* !PEGC && NOTE (because of LCD size) */
	if ((info_crt & CRT_HAS_PEGC) == 0 &&
	    (info_machine & MACHINE_NOTE) != 0)
		info_crt |= CRT_FORCE_25L;
}

/**************************************
 * system info interface
 **************************************/
sysinfo_info_t
lookup_sysinfo(key)
	sysinfo_code_t key;
{

	switch (key)
	{
	case SYSINFO_CLOCK:
		return system_clock;
	case SYSINFO_PMEMSZ:
		return pmemsz;
	case SYSINFO_BMEMSZ:
		return 640;
	case SYSINFO_MEMHIGH:
		return mem_high;
	case SYSINFO_X16HOLESZ:
		return x16hole_sz;
	case SYSINFO_X16HASRAM:
		return x16hole_hasram;
	case SYSINFO_COMCLOCK:
		return com_clock_mode;
	case SYSINFO_COMLEVEL:
		return com_level;
	case SYSINFO_MEMLEVEL:
		return mem_level;
	case SYSINFO_MACHINE:
		return info_machine;
	case SYSINFO_CRT:
		return info_crt;
	case SYSINFO_KBD:
		return info_keyboard;
	case SYSINFO_CONSMODE:
		return info_consmode;
	case SYSINFO_PRODUCT:
		return info_product;
	}
	return 0;
}

int
set_sysinfo(key, val)
	sysinfo_code_t key;
	sysinfo_info_t val;
{

	switch (key)
	{
	case SYSINFO_COMCLOCK:
		com_clock_mode = val;
		return 0;

	case SYSINFO_CONSMODE:
		info_consmode = val;
		return 0;
	}
	return EINVAL;
}

/**************************************
 * Product specific memory hook
 **************************************/
static void
product_memory_hook(pmem)
     psize_t *pmem;
{
	sysinfo_info_t id;

	id = lookup_sysinfo(SYSINFO_PRODUCT);
	if ((id & PRODUCT_VENDOR_MASK) != PRODUCT_VENDOR_EPSON)
		return;

	switch(id & PRODUCT_MINOR_MASK)
	{
	case 0x34:
	case 0x35:
     	case 0x3b:
       		outb(0x43f, 0x42);
       		outw(0xc40, 0x0033);
 
       		outb(0xc48, 0x49); 
       		outb(0xc4c, 0x00);
       		outb(0xc48, 0x48); 
       		outb(0xc4c, 0xf0);
       		outb(0xc48, 0x4d); 
       		outb(0xc4c, 0x00);
       		outb(0xc48, 0x4c); 
       		outb(0xc4c, 0xff);
       		outb(0xc48, 0x4f); 
       		outb(0xc4c, 0x00); 
 
       		outb(0x43f, 0x40);
       		break;
 
	case 0x2b:
	case 0x30:
     	case 0x31:
     	case 0x32:
     	case 0x37:
     	case 0x38:
		outb(0x43f, 0x42);
       		outb(0x467, 0xe0);
       		outb(0x567, 0xd8);
   
       		outb(0x43f, 0x40);
       		outb(0x467, 0xe0);
       		outb(0x567, 0xe0);
       		break;

	default:
		break;
	}
 
	/* Disable 15MB-16MB RAM and enable memory window. */
	outb(0x43b, inb(0x43b) & 0xfd);	/* Clear bit1. */

	/* Modify memory size under 16M */
	if (*pmem > 14 * 1024) 
     		*pmem = 14 * 1024;
}
