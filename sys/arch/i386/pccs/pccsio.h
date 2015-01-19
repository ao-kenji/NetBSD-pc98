/*	$NecBSD: pccsio.h,v 1.14.6.1 1999/08/24 20:32:09 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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
 * Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef	_PCCS_IOCTL_H_
#define	_PCCS_IOCTL_H_

#include <machine/slot_device.h>

#define	PCCSIO_VERSION	0x01320000

/**************************************************
 * slot status
 **************************************************/
#define	SLOT_NULL	0		/* initial state */
#define	SLOT_CARDIN	1		/* card found */
#define	SLOT_READY	4		/* power on */
#define	SLOT_MAPPED	5		/* resources allocated */
#define	SLOT_HASDEV	6		/* device connected */

typedef	int slot_status_t;

/**************************************************
 * macros
 **************************************************/
#define PUNKADDR	SLOT_DEVICE_UNKVAL
#define PUNKSZ		SLOT_DEVICE_UNKVAL
#define	PUNKIRQ		SLOT_DEVICE_UNKVAL
#define	PUNKDRQ		SLOT_DEVICE_UNKVAL
#define	PUNKMADDR	SLOT_DEVICE_UNKVAL
#define	PUNKMSIZE	SLOT_DEVICE_UNKVAL
#define	PUNKMFCID	(-1)
#define	PUNKINDEX	((u_int8_t) -1)
#define	PAUTOIRQ	0x80000000

/**************************************************
 * memory windows
 **************************************************/
#define	MAX_MEMWF	SLOT_DEVICE_NMEM /* max mem windows */

#define	WFMEM_ATTR	0x00000001		/* attribute memory */
#define	WFMEM_FLAGSBITS \
	"\020\026wp\025wa\024ds64\023ds32\022ds16\021cs16\001attr"

/**************************************************
 * io windows
 **************************************************/
#define	MAX_IOWF	SLOT_DEVICE_NIO	/* max io windows */
#define	DEFAULT_IOBITS	12		/* default address lines */
#define	MIN_IOBITS	12		/* min address lines */
#define	MAX_IOBITS	16		/* max address lines */

#define	WFIO_ZONE	0x000000001	/* (Psudo) zone address */
#define	WFIO_FLAGSBITS \
	"\020\026wp\026wa\024ds64\023ds32\022ds16\021cs16\001zone"

/**************************************************
 * card registers (attribute area)
 **************************************************/
/* card registers mask */
#define	BCISR_CCOR	0x00000001
#define	BCISR_CCSR	0x00000002
#define	BCISR_PRP	0x00000004
#define	BCISR_SCR	0x00000008
#define	BCISR_ESR	0x00000010
#define	BCISR_IOB0	0x00000020
#define	BCISR_IOB0MASK	0x000001e0
#define	BCISR_IOL0	0x00000200

/* card registers offsets */
#define	cisr_ccor	0		/* Configuration Option Register */
#define	CCOR_IDXMASK	0x3f		/* ccor index mask (RW) */
#define	CCOR_INTR	0x40		/* ccor level intr (RW) */
#define	CCOR_RST	0x80		/* ccor reset request (RW) */
#define	CCOR_MFEN	0x01		/* ccor function enable (RW) */
#define	CCOR_MIOBEN	0x02		/* ccor io base register enable (RW) */
#define	CCOR_MIEN	0x04		/* ccor interrupt enable */
#define	CCOR_MMASK	0x07		/* ccor multi function bits mask */
#define	cisr_ccsr	2		/* Configuration and Status Register */
#define	CCSR_INTR	0x02		/* int asserted (R) */
#define	CCSR_POWDOWN	0x04		/* power down mode (RW) */
#define	CCSR_SPKR	0x08		/* sound enable (RW) */
#define	CCSR_BUS8	0x20		/* 8 bit access (RW) */
#define	CCSR_SIGCHG	0x40		/* enable stschg singnal (RW) */
#define	CCSR_CHANGED	0x80		/* status changed (R) */
#define	cisr_prp	4		/* Pin Replacement Register */
#define	PRP_CBVD1	0x80		/* change of bvd1 bit (RW) */
#define	PRP_CBVD2	0x40		/* change of bvd2 bit (RW) */
#define	PRP_CRDYBSY	0x20		/* change of rdybsy bit (RW) */
#define	PRP_CWP		0x10		/* change of wp bit (RW) */
#define	PRP_RBVD1	0x08		/* bvd1 status (RW) */
#define	PRP_RBVD2	0x04		/* bvd2 status (RW) */
#define	PRP_RRDYBSY	0x02		/* rdy/bsy state (RW) */
#define	PRP_RWP		0x01		/* write protect (RW) */
#define	cisr_scr	6		/* Socket and Copy Register */
#define	SCR_CNM		0x70		/* Copy Number Mask (RW) */
#define	SCR_SNM		0x0f		/* Slot Number Mask (RW) */
#define	cisr_esr	8		/* Extended Status Register */
#define	cisr_iob0	10		/* I/O Base address register 0 */
#define	cisr_iob1	12		/* I/O Base address register 1 */
#define	cisr_iob2	14		/* I/O Base address register 2 */
#define	cisr_iob3	16		/* I/O Base address register 3 */
#define	cisr_iol	18		/* I/O Limit register */

#define	INDEX_MASK	CCOR_IDXMASK	/* max index value */
#define	INTR_EDGE	CCOR_INTR	/* assert edge intr */

#define	IRQBIT(PIN)	(1 << (PIN))
#define	PCCS_MAXMFC	4		/* XXX */

/**************************************************
 * card information structures 
 **************************************************/
#define	PRODUCT_NAMELEN	256
#define	PCCS_NAMELEN	16
#define	PCCS_GET_MFCID(ci) ((ci)->ci_mfcid)
#define	PCCS_SET_MFCID(ci, mfcid) (ci)->ci_mfcid = (mfcid);
#define	PCCS_GET_INDEX(ci) ((ci)->ci_cr.cr_ccor)
#define	PCCS_SET_INDEX(ci, idx) (ci)->ci_cr.cr_ccor = (idx);
#define	PCCS_GET_SCR(ci)   ((ci)->ci_cr.cr_scr)
#define	PCCS_SET_SCR(ci, scr)   (ci)->ci_cr.cr_scr = (scr);
 		
struct card_info {
	u_int32_t ci_manfid;			/* product id */
	u_int8_t ci_product[PRODUCT_NAMELEN];	/* raw product data */

	int ci_nmfc;				/* num multi functions */
	int ci_mfcid;				/* multi function id */

	u_int ci_iobits;			/* io address lines */
	u_int ci_delay;				/* special weight */

	struct slot_device_resources ci_res;	/* iomem irq data */

	struct card_registers {
		u_long		cr_offset;	/* card register offset */
		u_int32_t	cr_mask; 	/* register mask */
		u_int 		cr_lastidx;	/* last idx of ccor */
		u_int		cr_iouse;	/* io wins in use for iob reg */
		u_int		cr_memuse;	/* mem win in use */

		u_int8_t	cr_ccmor;	/* psuedo ccor control */
		u_int8_t	cr_ccor; 	/* registers ... */
		u_int8_t 	cr_ccsr;
		u_int8_t 	cr_prp;
		u_int8_t 	cr_scr;
		u_int8_t 	cr_esr;
		u_int32_t 	cr_iob;
		u_int32_t 	cr_iol;
		/* XXX: add miscs */
	} ci_cr;				/* card registers */

	struct power {
		int pow_vcc0;
		int pow_vcc1;
		int pow_vpp0;
		int pow_vpp1;
#define	PCCS_POWER_DEFAULT	0
#define	PCCS_POWER_NONE		(-1)
	} ci_pow;				/* power informations */

	struct tpce_if {
#define	IF_MEMORY	1
#define	IF_IO		2
		u_int8_t if_ifm;

#define	IF_BVD		1
#define	IF_WP		2
#define	IF_RDYBSY	4
#define	IF_MWAIT	8
#define	IFFLAGSBITS "\020\04mwait\03rdybsy\02wp\01bvd"
		u_int8_t if_flags;
	} ci_tpce;				/* interface byte */

#define	DV_FUNCID_MULTI	0
#define	DV_FUNCID_MEMORY 1
#define	DV_FUNCID_SERIAL 2
#define	DV_FUNCID_PARA	3
#define	DV_FUNCID_FIXED	4
#define	DV_FUNCID_VIDEO	5
#define	DV_FUNCID_NET	6
#define	DV_FUNCID_AIMS	7
#define	DV_FUNCID_SCSI	8
	u_int8_t ci_function;			/* function */
};

/**************************************************
 * ioctl structures
 **************************************************/
struct pcdv_attach_args {
	struct card_info ca_ci;			/* IN: card resources */
	u_char ca_name[PCCS_NAMELEN];		/* IN/OUT: device name */
	int ca_unit;				/* OUT: multiple attach */

	u_int ca_version;			/* version */
};

struct card_prefer_config {
	struct card_info cp_ci;			/* OUT: prefer config */
	int cp_pri;				/* OUT: priority */

	u_int cp_disable;			/* IN: disable bit maps */
	u_char cp_name[PCCS_NAMELEN];		/* IN: device name */
};	

struct slot_info {
	int si_mfcid;				/* IN: function id */

	/* common card status */
	int si_cst;				/* OUT: ctrl status */
#define	PCCS_SS_POW_UP     0x0001
#define	PCCS_SS_SPKR_UP    0x0002
#define	PCCS_SS_AUTORES_UP 0x0004
#define	PCCS_SS_AUTOINS_UP 0x0008
	slot_status_t si_st;			/* OUT: slot status */
	int si_buswidth;			/* OUT: bus width */
#define	PCCS_BUSWIDTH_NONE	(-1)
#define	PCCS_BUSWIDTH_16	0
#define	PCCS_BUSWIDTH_32	1
	struct card_info si_ci;			/* OUT: shared resources */

	/* function info */
	int si_fst;
	struct card_info si_fci;
	u_char si_xname[PCCS_NAMELEN];
};

struct slot_event {
	int sev_code;
#define	PCCS_SEV_NULL			0x0000	
#define	PCCS_SEV_HWREMOVE		0x0001
#define	PCCS_SEV_HWINSERT		0x0002
#define	PCCS_SEV_MSUSPEND		0x0003
#define	PCCS_SEV_MRESUME		0x0004
	time_t sev_time;
};
/**************************************************
 * ioctl cmds
 **************************************************/
#define	CARD_ATTR_SZ	4096
#define	PCCS_IOG_ATTR	_IOR('P', 60, u_char[CARD_ATTR_SZ])

#define PCCS_IOC_INIT _IO('P', 61)
#define PCCS_IOG_SSTAT _IOWR('P', 62, struct slot_info)
#define	PCCS_IOG_EVENT _IOR('P', 63, struct slot_event)

#define	PCCS_IOC_CONNECT _IO('P', 65)
#define PCCS_IOC_CONNECT_BUS _IOWR('P', 66, struct pcdv_attach_args)

#define	PCCS_CTRLCMD_MASK       0xffff0000
#define	PCCS_SLOT_DOWN	        0x00000000
#define	PCCS_SLOT_UP		0x00000001
#define	PCCS_SPKR_DOWN		0x00010000
#define	PCCS_SPKR_UP		0x00010001
#define	PCCS_AUTO_RESUME_DOWN	0x00020000
#define	PCCS_AUTO_RESUME_UP	0x00020001
#define	PCCS_AUTO_INSERT_DOWN	0x00030000
#define	PCCS_AUTO_INSERT_UP	0x00030001
#define	PCCS_HW_TIMING		0x00040000
#define	PCCS_HW_TIMING_MASK	0x0000ffff
#define	PCCS_IOC_CTRL _IOW('P', 67, int)

#define	PCCS_IOC_PREFER _IOWR('P', 68, struct card_prefer_config)
#endif	/* !_PCCS_IOCTL_H_ */
