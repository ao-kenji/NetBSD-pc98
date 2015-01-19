/*	$NecBSD: sysinfo.h,v 1.1.2.2 1999/12/01 17:41:29 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
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

#ifndef	_I386_SYSINFO_H_
#define	_I386_SYSINFO_H_

/*
 * bios info parameters
 */
#define	BIOS_TABLE_OFFSET	0x400
#define	BIOS_TABLE_SIZE		0x600

#define	BIOS_FLAGS2		0x400
#define	BIOS_EXT_MEM		0x401
#define	BIOS_H98_FLAGS		0x458
#define	BIOS_XXX001		0x45c	/* correct name ?? PEGC */
#define	BIOS_SCSI_PARAM		0x460
#define	BIOS_KBD_TYPE		0x481
#define	BIOS_SCSI_EQ		0x482
#define	BIOS_SYSTEM_CLOCK	0x501
#define	BIOS_XXX002		0x54c	/* correct name ?? CRT_HZ */
#define	BIOS_EXT_MEM16		0x594
#define	BIOS_FD_EXT		0x5ae
#define	BIOS_WD_PARAM		0x600

/*
 * system info parameters
 */
#define	SYSINFO_CLOCK		0x00000
#define	SYSTEM_CLOCK_8M		0
#define	SYSTEM_CLOCK_5M		1
#define	SYSTEM_CLOCK_10M	2

#define	SYSINFO_PMEMSZ		0x10000
#define	SYSINFO_BMEMSZ		0x10001
#define	SYSINFO_MEMHIGH		0x10002
#define	SYSINFO_X16HOLESZ	0x10003
#define	SYSINFO_X16HASRAM	0x10004

#define	SYSINFO_COMCLOCK	0x20000
#define	COM_NORMAL_CLOCK	0
#define	COM_EXT_CLOCK		1

#define	SYSINFO_COMLEVEL	0x30000
#define	COM_LEVEL0		0	/* trad 98 */
#define	COM_LEVEL1		1	/* com with 10 bytes fifo */
#define	COM_LEVEL2		2	/* first and second coms */

#define	SYSINFO_MEMLEVEL	0x40000
#define	MEM_LEVEL0		0	/* max below 16M, no second hole */
#define	MEM_LEVEL1		1

#define	SYSINFO_MACHINE		0x50000
#define	MACHINE_NOTE		1

#define	SYSINFO_CRT		0x60000
#define	CRT_HAS_PEGC		1
#define	CRT_HZ_315		2
#define	CRT_PEGC_ENABLED	4
#define	CRT_FORCE_25L		8

#define	SYSINFO_KBD		0x70000
#define	KBD_NOTE		1

#define	SYSINFO_CONSMODE	0x80000
#define	CONSM_NDELAY		1

#define	SYSINFO_PRODUCT		0x90000
#define	PRODUCT_MINOR_MASK	0x0000ffff
#define	PRODUCT_MAJOR_MASK	0x00ff0000
#define	PRODUCT_VENDOR_MASK	0xff000000
#define	PRODUCT_VENDOR_NEC	0x00000000
#define	PRODUCT_VENDOR_EPSON	0x01000000
#define	PRODUCT_NEC_9801	0x00000000
#define	PRODUCT_NEC_9821	0x00010000
#define	PRODUCT_NEC_H98		0x00020000

#if	defined(_KERNEL) && !defined(_LOCORE)
/* system clock */
extern u_int system_clock_freq;
extern u_int scf_div_100;

/* typedefs */
typedef	int sysinfo_code_t;
typedef	u_long sysinfo_info_t;

/* interface funcs */
u_int32_t lookup_bios_param __P((vaddr_t, u_int));
sysinfo_info_t lookup_sysinfo __P((sysinfo_code_t));
int set_sysinfo __P((sysinfo_code_t, sysinfo_info_t));
void init_sysinfo __P((void));
#endif	/* _KERNEL && !_LOCORE */
#endif	/* !_I386_SYSINFO_H_ */
