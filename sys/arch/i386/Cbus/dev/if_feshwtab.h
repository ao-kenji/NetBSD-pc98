/*	$NecBSD: if_feshwtab.h,v 1.2 1998/09/08 03:23:47 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 *  Copyright (c) 1998
 *	Kouichi Matsuda. All rights reserved.
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
 *      This product includes software developed by Kouichi Matsuda.
 * 4. The names of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

bus_addr_t fes_hw_iat_al98[] = {
	/* EtherStar NIC core */
	0x0000, 0x0100, 0x0200, 0x0300,	/* DLCR0-3 */
	0x0400, 0x0500, 0x0600, 0x0700,	/* DLCR4-7 */
	0x0800, 0x0900, 0x0A00, 0x0B00,	/* DLCR8-11 */
	0x0C00, 0x0D00, 0x0E00, 0x0F00,	/* DLCR12-15 */
	0x1000, 0x1100, 0x1200, 0x1300,	/* BMPR0-3 */
	0x1400, 0x1500, 0x1600, 0x1700,
	0x1800, 0x1900, 0x1A00, 0x1B00,
	0x1C00, 0x1D00, 0x1E00, 0x1F00,
};
bus_addr_t fes_hw_asic_iat_al98[] = {
	/* ASIC, hardware depend */
	0xC000, 0xE000	
};
int fes_hw_iomap_al98[] = {
	0x00D0, 0x00D6, 0x00E0, 0x00E4
};
struct fes_hw fes_hw_al98 = {
	FES_HW_BUSISA,
	fes_hw_iat_al98,
	32,
	fes_hw_asic_iat_al98,
	2,
	fes_hw_iomap_al98,
	4,
	0x1068,		/* irq 3, 5, 6, 12 */
	fes_find_al98,
	fes_detect_al98,
	fes_attach_al98
};

bus_addr_t fes_hw_iat_pc85151[] = {
	/* EtherStar NIC core */
	0x0000, 0x0001, 0x0002, 0x0003,	/* DLCR0-3 */
	0x0004, 0x0005, 0x0006, 0x0007,	/* DLCR4-7 */
	0x0008, 0x0009, 0x000A, 0x000B,	/* DLCR8-11 */
	0x000C, 0x000D, 0x000E, 0x000F,	/* DLCR12-15 */
	0x0200, 0x0201, 0x0202, 0x0203,	/* BMPR0-3 */
	0x0204, 0x0205, 0x0206, 0x0207,
};
bus_addr_t fes_hw_asic_iat_pc85151[] = {
	/* ASIC, hardware depend */
	0x0208, 0x0209, 0x020A, 0x020B,	/* SA0-5 */
	0x020C, 0x020D, 0x020E, 0x020F,
};
int fes_hw_iomap_pc85151[] = {
	0x00D0, 0x04D0, 0x08D0, 0x0CD0, 0x10D0, 0x14D0, 0x18D0, 0x1CD0
};
struct fes_hw fes_hw_pc85151 = {
	FES_HW_BUSISA,
	fes_hw_iat_pc85151,
	24,
	fes_hw_asic_iat_pc85151,
	8,
	fes_hw_iomap_pc85151,
	8,
	0x1068,		/* irq 3, 5, 6, 12 */
	fes_find_pc85151,
	fes_detect_pc85151,
	fes_attach_pc85151
};

int fes_hw_iomap_pc86131[] = {
	0x00D0
};
struct fes_hw fes_hw_pc86131 = {
	FES_HW_BUSISA,
	fes_hw_iat_pc85151,
	24,
	fes_hw_asic_iat_pc85151,
	8,
	fes_hw_iomap_pc86131,
	8,
	0x8,		/* XXX: irq 3 (irq 5 does not work well) */
	fes_find_pc86131,
	fes_detect_pc86131,
	fes_attach_pc85151
};

static dvcfg_hw_t fes_hwsel_array[] = {
/* 0x00 */	NULL,
/* 0x01 */	&fes_hw_al98,
/* 0x02 */	&fes_hw_pc85151,
/* 0x03 */	&fes_hw_pc86131,
};

struct dvcfg_hwsel fes_hwsel = {
	DVCFG_HWSEL_SZ(fes_hwsel_array),
	fes_hwsel_array
};
