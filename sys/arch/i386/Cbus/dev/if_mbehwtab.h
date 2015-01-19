/*	$NecBSD: if_mbehwtab.h,v 1.4.6.2 1999/08/30 05:36:45 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 *  Copyright (c) 1996, 1997, 1998, 1999
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

bus_addr_t mbe_hw_iat_re1000[] = {
	0x0000, 0x0001, 0x0200, 0x0201,	/* DLCR0-3 */
	0x0400, 0x0401, 0x0600, 0x0601,	/* DLCR4-7 */
	0x0800, 0x0801, 0x0A00, 0x0A01,	/* DLCR,MAR,BMPR8-11 */
	0x0C00, 0x0C01, 0x0E00, 0x0E01,	/* DLCR,MAR,BMPR12-15 */
	0x1000, 0x1001, 0x1200, 0x1201,	/* BMPR16-19 */
	0x1400, 0x1401, 0x1600, 0x1601,
	0x1800, 0x1801, 0x1A00, 0x1A01,
	0x1C00, 0x1C01, 0x1E00, 0x1E01
};
int mbe_hw_iomap_re1000[] = {
	0x00D0, 0x00D2, 0x00D4, 0x00D6, 0x00D8, 0x00DA, 0x00DC, 0x00DE,
	0x01D0, 0x01D2, 0x01D4, 0x01D6, 0x01D8, 0x01DA, 0x01DC, 0x01DE
};
struct mbe_hw mbe_hw_re1000 = {
	MBE_HW_BUSISA,
	mbe_hw_iat_re1000,
	32,
	mbe_hw_iomap_re1000,
	16,
	0x1068,		/* irq 3, 5, 6, 12 */
	mbe_find_re1000,
	mbe_detect_re1000,
	mbe_attach_re1000
};

bus_addr_t mbe_hw_iat_re1000p[] = {
	0x0000, 0x0001, 0x0200, 0x0201,
	0x0400, 0x0401, 0x0600, 0x0601,	/* DLCR0-7 */
	0x0800, 0x0801, 0x0A00, 0x0A01,	/* DLCR,MAR,BMPR8-11 */
	0x0C00, 0x0C01, 0x0E00, 0x0E01,	/* DLCR,MAR,BMPR12-15 */
	0x1000, 0x1200, 0x1400, 0x1600,	/* BMPR16-19 */
	0x1800, 0x1A00, 0x1C00, 0x1E00,
	0x1001, 0x1201, 0x1401, 0x1601,
	0x1801, 0x1A01, 0x1C01, 0x1E01
};
int mbe_hw_iomap_re1000p[] = {
	0x00D0, 0x00D2, 0x00D4, 0x00D8, 0x01D4, 0x01D6, 0x01D8, 0x01DA
};
struct mbe_hw mbe_hw_re1000p = {
	MBE_HW_BUSISA | MBE_HW_CONFIRQ /* | MBE_HW_CONFIO */,
	mbe_hw_iat_re1000p,
	32,
	mbe_hw_iomap_re1000p,
	8,
	0x1068,		/* irq 3, 5, 6, 12 */
	mbe_find_re1000p,
	mbe_detect_re1000p,
	mbe_attach_re1000p
};

bus_addr_t mbe_hw_iat_lac98[] = {
	0x0000, 0x0002, 0x0004, 0x0006,
	0x0008, 0x000A, 0x000C, 0x000E,	/* DLCR0-7 */
	0x0100, 0x0102, 0x0104, 0x0106,	/* DLCR,MAR,BMPR8-11 */
	0x0108, 0x010A, 0x010C, 0x010E,	/* DLCR,MAR,BMPR12-15 */
	0x0200, 0x0202, 0x0204, 0x0206,	/* BMPR16-19 */
	0x0208, 0x020A, 0x020C, 0x020E,
	0x0300, 0x0302, 0x0304, 0x0306,
	0x0308, 0x030A, 0x030C, 0x030E
};
int mbe_hw_iomap_lac98[] = {
	0x00D0, 0x04D0, 0x08D0, 0x0CD0
};
struct mbe_hw mbe_hw_lac98 = {
	MBE_HW_BUSISA,
	mbe_hw_iat_lac98,
	32,
	mbe_hw_iomap_lac98,
	4,
	0x1068,		/* irq 3, 5, 6, 12 */
	mbe_find_lac98,
	mbe_detect_lac98,
	mbe_attach_lac98
};

struct mbe_hw mbe_hw_tdk = {
	MBE_HW_BUSPISA,
	BUS_SPACE_IAT_1,	/* default io table (skip 1) */
	16,
	NULL,
	0,
	-1,
	mbe_find_tdk,
	mbe_detect_tdk,
	mbe_attach_tdk
};

int mbe_hw_iomap_cnet98p2[] = {
	0x03D0, 0x13D0, 0x23D0, 0x33D0, 0x43D0, 0x53D0, 0x63D0,
	0x73D0, 0x83D0, 0x93D0, 0xA3D0, 0xB3D0, 0xC3D0, 0xD3D0
};
int mbe_hw_irqmap_cnet98p2[] = {
	3, 5, 6, 9, 10, 12, 13
};
struct mbe_hw mbe_hw_cnet98p2 = {
	MBE_HW_BUSISA | MBE_HW_BUSPISA | MBE_HW_CONFIO | MBE_HW_CONFIRQ,
	BUS_SPACE_IAT_1,	/* default io table (skip 1) */
	16,
	mbe_hw_iomap_cnet98p2,
	14,
	0x3668,		/* irq 3, 5, 6, 9, 10, 12, 13 */
	mbe_find_cnet98p2,
	mbe_detect_cnet98p2,
	mbe_attach_cnet98p2,
};
	
struct mbe_hw mbe_hw_mbh1040x = {
	MBE_HW_BUSPISA,
	BUS_SPACE_IAT_1,	/* default io table (skip 1) */
	32,
	NULL,
	0,
	-1,
	mbe_find_mbh1040x,
	mbe_detect_mbh1040x,
	mbe_attach_mbh1040x,
};

bus_addr_t mbe_hw_iat_pc85152[] = {
	0x0000, 0x0001, 0x0002, 0x0003,
	0x0004, 0x0005, 0x0006, 0x0007, /* DLCR0-7 */
	0x0008, 0x0009, 0x000A, 0x000B,
	0x000C, 0x000D, 0x000E, 0x000F, /* DLCR8-15 */
	0x0200, 0x0201, 0x0202, 0x0203,
	0x0204, 0x0205, 0x0206, 0x0207,
	0x0208, 0x0209, 0x020A, 0x020B,
	0x020C, 0x020D, 0x020E, 0x020F
};
int mbe_hw_iomap_pc85152[] = {
	0x00D0, 0x04D0, 0x08D0, 0x0cD0, 0x10D0, 0x14D0, 0x18D0, 0x1CD0
};
struct mbe_hw mbe_hw_pc85152 = {
	MBE_HW_BUSISA,
	mbe_hw_iat_pc85152,
	32,
	mbe_hw_iomap_pc85152,
	8,
	0x1068,		/* irq 3, 5, 6, 12 */
	mbe_find_pc85152,
	mbe_detect_pc85152,
	mbe_attach_pc85152
};

int mbe_hw_iomap_pc86132[] = {
	0x00D0
};
struct mbe_hw mbe_hw_pc86132 = {
	MBE_HW_BUSISA,
	mbe_hw_iat_pc85152,
	32,
	mbe_hw_iomap_pc86132,
	8,
	0x8,		/* XXX: irq 3 (irq 5 does not work well) */
	mbe_find_pc86132,
	mbe_detect_pc86132,
	mbe_attach_pc86132
};

bus_addr_t mbe_hw_iat_rex9886[] = {
	0x0000, 0x0001, 0x0002, 0x0003,	/* DLCR0-3 */
	0x0004, 0x0005, 0x0006, 0x0007,	/* DLCR4-7 */
	0x0008, 0x0009, 0x000A, 0x000B,	/* DLCR8-11 */
	0x000C, 0x000D, 0x000E, 0x000F,	/* DLCR12-15 */
	0x0100, 0x0102, 0x0101, 0x0103,
	0x0104, 0x0105, 0x0106, 0x0107,
	0x0108, 0x0109, 0x010A, 0x010B,
	0x010C, 0x010D, 0x010E, 0x010F
};
int mbe_hw_iomap_rex9886[] = {
	0x54D0
};
struct mbe_hw mbe_hw_rex9886 = {
	MBE_HW_BUSISA,
	mbe_hw_iat_rex9886,
	32,
	mbe_hw_iomap_rex9886,
	1,
	0x1068,		/* irq 3, 5, 6, 12 */
	mbe_find_rex9886,
	mbe_detect_rex9886,
	mbe_attach_rex9886
};

bus_addr_t mbe_hw_iat_rex9883[] = {
	0x0000, 0x0001, 0x0002, 0x0003,	/* DLCR0-3 */
	0x0004, 0x0005, 0x0006, 0x0007,	/* DLCR4-7 */
	0x0008, 0x0009, 0x000A, 0x000B,	/* DLCR8-11 */
	0x000C, 0x000D, 0x000E, 0x000F,	/* DLCR12-15 */
	0x0100, 0x0101, 0x0102, 0x0103,
	0x0104, 0x0105, 0x0106, 0x0107,
	0x0108, 0x0109, 0x010A, 0x010B,
	0x010C, 0x010D, 0x010E, 0x010F
};
int mbe_hw_iomap_rex9883[] = {
	0x64D0, 0x66D0, 0x6CD0, 0x6ED0
};
struct mbe_hw mbe_hw_rex9883 = {
	MBE_HW_BUSISA,
	mbe_hw_iat_rex9883,
	32,
	mbe_hw_iomap_rex9883,
	4,
	0x1068,		/* irq 3, 5, 6, 12 */
	mbe_find_rex9883,
	mbe_detect_rex9883,
	mbe_attach_rex9883
};

struct mbe_hw mbe_hw_jc89532a = {
	MBE_HW_BUSPISA,
	BUS_SPACE_IAT_1,	/* default io table (skip 1) */
	32,
	NULL,
	0,
	-1,
	mbe_find_jc89532a,
	mbe_detect_jc89532a,
	mbe_attach_jc89532a
};

bus_addr_t mbe_hw_iat_cnet9ne[] = {
	0x0000, 0x0001, 0x0002, 0x0003,	/* DLCR0-3 */
	0x0004, 0x0005, 0x0006, 0x0007,	/* DLCR4-7 */
	0x0008, 0x0009, 0x000A, 0x000B,	/* DLCR,MAR,BMPR8-11 */
	0x000C, 0x000D, 0x000E, 0x000F,	/* DLCR,MAR,BMPR12-15 */
	0x0400, 0x0401, 0x0402, 0x0403,	/* BMPR16-19 */
	0x0404, 0x0405, 0x0406, 0x0407,
	0x0408, 0x0409, 0x040A, 0x040B,
	0x040C, 0x040D, 0x040E, 0x040F
};
int mbe_hw_iomap_cnet9ne[] = {
	0x73D0
};
struct mbe_hw mbe_hw_cnet9ne = {
	MBE_HW_BUSISA,
	mbe_hw_iat_cnet9ne,
	32,
	mbe_hw_iomap_cnet9ne,
	1,
	0x0020,		/* irq 5 */
	mbe_find_cnet9ne,
	mbe_detect_cnet9ne,
	mbe_attach_cnet9ne
};

struct mbe_hw mbe_hw_cnet9nc = {
	MBE_HW_BUSISA,		/* XXX: IC card */
	mbe_hw_iat_cnet9ne,	/* same as cnet98ne */
	32,
	mbe_hw_iomap_cnet9ne,	/* same as cnet98ne */
	1,
	0x3668,		/* irq 3, 5, 6, 9, 10, 12, 13 */
	mbe_find_cnet9nc,	/* */
	mbe_detect_cnet9nc,	/* */
	mbe_attach_cnet9nc	/* */
};

static dvcfg_hw_t mbe_hwsel_array[] = {
/* 0x00 */	NULL,			/* ate, flags 0       */
/* 0x01 */	&mbe_hw_re1000,		/* ate, flags 0x10000 */
/* 0x02 */	&mbe_hw_re1000p,	/* ate, flags 0x20000 */
/* 0x03 */	&mbe_hw_tdk,		/* sse, flags 0       */
/* 0x04 */	&mbe_hw_cnet98p2,	/* sse, flags 0x10000 */
/* 0x05 */	&mbe_hw_mbh1040x,	/* mbh, flags 0       */
/* 0x06 */	&mbe_hw_lac98,		/* sse, flags 0x20000 */
/* 0x07 */	&mbe_hw_pc85152,	/* mbu, flags 0       */
/* 0x08 */	&mbe_hw_rex9886,	/* mbr, flags 0x20000 */
/* 0x09 */	&mbe_hw_rex9883,	/* mbr, flags 0x10000 */
/* 0x0A */	&mbe_hw_cnet9ne,	/* */
/* 0x0B */	&mbe_hw_pc86132,	/* mbu, flags 0x10000 */
/* 0x0C */	&mbe_hw_jc89532a,	/* */
/* 0x0D */	&mbe_hw_cnet9nc,	/* */
};

struct dvcfg_hwsel mbe_hwsel = {
	DVCFG_HWSEL_SZ(mbe_hwsel_array),
	mbe_hwsel_array
};
