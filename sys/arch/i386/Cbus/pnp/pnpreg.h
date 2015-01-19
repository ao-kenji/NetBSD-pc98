/*	$NecBSD: pnpreg.h,v 1.8 1998/09/11 13:00:35 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * Copyright (c) 1996, Sujal M. Patel
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
 *      This product includes software developed by Sujal M. Patel
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: pnpcfg.h,v 1.2 1996/01/12 21:35:10 smpatel Exp smpatel $
 */

#ifndef _PNPREG_H_
#define _PNPREG_H_

#define IPPI_DEFAULT_RDPORT	0x259	/* 0x279 for AT */

#define	ippi_rdar		0x00	/* Resource Data Address Register */
#define	ippi_sir		0x01	/* Serial Isolation Register */
#define	ippi_ccr		0x02	/* Config Control Resiger */
#define	CCR_INIT		0x01	/* initialize */
#define	CCR_WFK			0x02	/* assert Wake For Key state */
#define	CCR_CSNR		0x04	/* reset CSN */
#define	ippi_wur		0x03	/* Wake Up Resiger */
#define	WUR_CONFIG		0x00	/* 0 means Config State */
#define	ippi_rdr		0x04	/* Resource Data Register */
#define	ippi_sr			0x05	/* Status Register */
#define	SR_READY		0x01	/* data ready */
#define	ippi_csnr		0x06	/* Card Select Number Register */
#define	ippi_ldnr		0x07	/* Logical Device Number Register */

#define	ippi_ar			0x30	/* Activate Register */
#define	AR_ACTIVE		0x01	/* logical device active */
#define	ippi_iorcr		0x31	/* I/O Range Check register */
#define	IORCR_ENABLE		0x02	/* check enable */
#define	IORCR_ODDSET		0x01	/* set odd bits */

#define	IPPI_MEMR_SKIP		0x08
#define	ippi_mar		0x40	/* Memory base Address Register */
#define	ippi_mcr		0x42	/* Memory Control Register */
#define	MCR_BUS16		0x02	/* 16bits memory */
#define	MCR_ULA			0x01	/* upper limit address (R) */
#define	ippi_mular		0x43	/* Mem Upper Limit Address Register */

#define	IPPI_IOR_SKIP		0x02
#define	ippi_ioar		0x60	/* IO base Address Register */

#define	IPPI_ICR_SKIP		0x02
#define	ippi_irlr		0x70	/* Interrupt Request Level Register */
#define	ippi_irtr		0x71	/* Interrupt Request Type select */
#define	IRTR_HIGH		0x02	/* High level trigger */
#define	IRTR_LEVEL		0x01	/* Level trigger */

#define	IPPI_DMAR_SKIP		0x01
#define	ippi_dcsr		0x74	/* DMA Channel Select Register */

/* Resource Definitions */
struct serial_id {
	u_int32_t si_product;		/* Vendor/Product (network byte order) */
	u_int32_t si_un;		/* unique number (network byte order) */	
	u_int8_t  si_cksum;
} __attribute__((packed));

#define	SRDT_LEN(code)	((code) & 0x07)
#define	SRDT_NAME(code) (((code) & 0x78) >> 3)
#define	SRDT_LTAG(code) ((code) & 0x80)

/* Small Resource Item Definitions */
#define	SRDT_VER	1		/* PnP VERsion */
struct srdt_ver {
	u_int8_t sv_type;		/* type code */
	u_int8_t sv_version;		/* pnp version */
	u_int8_t sv_vu;			/* vendor specific */
} __attribute__((packed));

#define	SRDT_LDID	2		/* Logical Device ID */
struct srdt_ldid {
	u_int8_t  sl_type;		/* type code */
	u_int32_t sl_id;		/* logical device id */
#define	RM0_BOOTABLE	1
	u_int8_t  sl_rm0;		/* register mask 0x31 - 0x37 */
	u_int8_t  sl_rm1;		/* register mask 0x38 - 0x3f */
} __attribute__((packed));

#define	SRDT_CDID	3		/* Compatible Device ID */
struct srdt_cdid {
	u_int8_t  sc_type;		/* type code */
	u_int32_t sc_id;		/* compatible device id */
} __attribute__((packed));

#define SRDT_IRQF	4		/* IRQ Format */
struct srdt_irqf {
	u_int8_t  si_type;		/* type code */
	u_int16_t si_im;		/* irq mask */
#define	IFG_LOW		8		/* low trig */
#define	IFG_HIGH	4		/* high trig */
#define	IFG_LOEDGE	2		/* low edge */
#define	IFG_HIEDGE	1		/* high edge */
	u_int16_t si_ifg;		/* irq flags */
} __attribute__((packed));

#define	SRDT_DMAF	5		/* DMA Format */
struct srdt_dmaf {
	u_int8_t sd_type;		/* type code */
	u_int8_t sd_dm;			/* dma mask */
#define	DFG_8BITS	1		/* 8 bits */
#define	DFG_ANYBITS	2		/* 8 or 16 bits */
#define	DFG_16BITS	3		/* 16 bits */
#define	DFG_BUSMASTER	0x04		/* bus master */
#define	DFG_BYTEMODE	0x08		/* byte mode */
#define	DFG_WORDMODE	0x10		/* word mode */
#define	DFG_SPEED	0x60		/* speed */
	u_int8_t sd_dfg;
} __attribute__((packed));

#define	SRDT_SDF	6		/* Start Dependent Function */
struct srdt_sdf {
#define	SDF_HASPRIO	0x01		/* has priority */
	u_int8_t ss_type;		/* type code */
	u_int8_t ss_pri;		/* priority */
} __attribute__((packed));

#define	SRDT_EDF	7		/* End Dependent Function */

#define	SRDT_IOPD 	8		/* IO Port Descriptor */
struct srdt_iopd {
	u_int8_t  si_type;		/* type code */
#define IOINF_EVEN	0x80
	u_int8_t  sio_ioinf;
	u_int16_t sio_loaddr;		/* low port offset */
	u_int16_t sio_hiaddr;		/* high port offset */
	u_int8_t  sio_align;		/* alignment */
	u_int8_t  sio_len;		/* required nports */
} __attribute__((packed));

#define	SRDT_FIOPD	9		/* Fixed IO Port Descriptor */

struct srdt_fiopd {
	u_int8_t  sfio_type;		/* type code */
	u_int16_t sfio_offset;		/* port offset */
	u_int8_t  sfio_len;		/* nports */
} __attribute__((packed));

#define SRDT_R0		11		/* 11 - 13: Reserverd */
#define SRDT_R1		12		/* 11 - 13: Reserverd */
#define SRDT_R2		13		/* 11 - 13: Reserverd */
#define	SRDT_VD		14		/* Vendor Defined */
#define	SRDT_ET		15		/* End Tag */	

/* Large Resource Item Definitions */
#define	LRDT_NAME(code) ((code) & 0x7f)

struct lrdt_entry {			/* Large Resource Data Type */
	u_int8_t  le_type;		/* type code */
	u_int16_t le_size;		/* size of this resource */
} __attribute__((packed));

#define	LRDT_MRD	1		/* Memory Range Descriptor */
struct lrdt_mrd {
	u_int8_t  lm_type;		/* type code */
	u_int16_t lm_size;		/* size of this resource */
#define	MEMINF_EROM	0x40		/* extension rom */
#define	MEMINF_SHADOW	0x20		/* shadowable */
#define	MEMINF_8BITS	0
#define	MEMINF_16BITS	0x08
#define	MEMINF_ANYBITS	0x10
#define	MEMINF_HIA	0x04		/* high address desc */
#define MEMINF_WC	0x02		/* cachable */
#define	MEMINF_WB	0x01		/* writable */
	u_int8_t  lm_meminf;
	u_int16_t lm_loaddr;		/* low offset */
	u_int16_t lm_hiaddr;		/* high offset */
	u_int16_t lm_align;		/* alignment: 0 means 64k */
	u_int16_t lm_blen;		/* 256 blocks */
} __attribute__((packed));

#define	LRDT_AIS	2		/* ANSI Identifier String */
#define	LRDT_UIS	3		/* Unicode Identifier String */
#define	LRDT_VD		4		/* Vendor Defined */

#define	LRDT_32MRD	5		/* 32 bits Memory Range Desc */
struct lrdt_32mrd {
	u_int8_t  l32m_type;		/* type code */
	u_int16_t l32m_size;		/* size of this resource */
	u_int8_t  l32m_meminf;		/* info */
	u_int32_t l32m_loaddr;		/* low offset */
	u_int32_t l32m_hiaddr;		/* high offset */
	u_int32_t l32m_align;		/* alignment */
	u_int32_t l32m_len;		/* length */
} __attribute__((packed));

#define	LRDT_32FMRD	6		/* 32 bits Fixed Memory Range Desc */
struct lrdt_32fmrd {
	u_int8_t  l32fm_type;		/* type code */
	u_int16_t l32fm_size;		/* size of this resource */
	u_int8_t  l32fm_meminf;		/* info */
	u_int32_t l32fm_offset;		/* offset */
	u_int32_t l32fm_len;		/* length */
} __attribute__((packed));

#ifndef	_KERNEL
/* 
 * For FreeBSD compat
 */
/* Small Resource Item names */
#define PNP_VERSION		0x1
#define LOG_DEVICE_ID		0x2
#define COMP_DEVICE_ID		0x3
#define IRQ_FORMAT		0x4
#define DMA_FORMAT		0x5
#define START_DEPEND_FUNC	0x6
#define END_DEPEND_FUNC		0x7
#define IO_PORT_DESC		0x8
#define FIXED_IO_PORT_DESC	0x9
#define SM_RES_RESERVED		0xa-0xd
#define SM_VENDOR_DEFINED	0xe
#define END_TAG			0xf

/* Large Resource Item names */
#define MEMORY_RANGE_DESC	0x1
#define ID_STRING_ANSI		0x2
#define ID_STRING_UNICODE	0x3
#define LG_VENDOR_DEFINED	0x4
#define _32BIT_MEM_RANGE_DESC	0x5
#define _32BIT_FIXED_LOC_DESC	0x6
#define LG_RES_RESERVED		0x7-0x7f
#endif
#endif /* !_PNPREG_H_ */
