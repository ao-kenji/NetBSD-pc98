/*	$NecBSD: tuple.h,v 1.8 1998/09/10 02:08:39 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	 NetBSD/pc98 porting staff. All rights reserved.
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

#ifndef	_TUPLE_H_
#define	_TUPLE_H_

typedef u_int32_t cis_addr_t;
typedef u_int32_t cis_size_t;

#define	CIS_ADDRUNK	  ((cis_addr_t) -1)
#define	CIS_ATTR_FLAG	  ((cis_addr_t) 0x80000000)
#define	CIS_ADDR_MASK	  (~CIS_ATTR_FLAG)
#define	CIS_IS_ATTR(va)   (((cis_addr_t)(va)) & CIS_ATTR_FLAG)
#define	CIS_ATTR_ADDR(va) (((cis_addr_t)(va)) | CIS_ATTR_FLAG)
#define	CIS_COMM_ADDR(va) (((cis_addr_t)(va)) & CIS_ADDR_MASK)

struct tuple_request {
	u_int8_t tr_code;		/* target code */
	int tr_mfcid;			/* target mfc id */

#define	NOT_FOLLOW_LINK	0x01		/* do not follow link */
	u_int tr_flags;			/* control flags */
	int tr_count;			/* count: detect infinte loop */
};

struct tuple_data {
	cis_addr_t	tp_offs;	/* current entry offset */
	cis_size_t	tp_size;	/* current entry size */
	u_int8_t	tp_code;	/* current entry code */

#define LINK_C		0x01		/* link to common mem */
#define LINK_A		0x02		/* link to attr mem */
#define LINK_MFC	0x04		/* link to MFC cis */
	u_int tp_stat;			/* link status */

	cis_addr_t tp_Aoffs;		/* attribute offset */
	cis_addr_t tp_Coffs;		/* common offset */
	cis_addr_t tp_Moffs;		/* MFC offset */
	int tp_lnkcnt;			/* MFC total entry */
	int tp_lnkid;			/* MFC current id */
	int tp_plnkid;			/* virtual MFC id */
};

/* cis definitions */
struct cis_header {		/* common tuple header structure */
	u_int8_t eh_code;	/* type code */
	u_int8_t eh_size;	/* size of this entry */
} __attribute__((packed));

#define	CIS_CODE(bp)	(((struct cis_header *)(bp))->eh_code)
#define	CIS_SIZE(bp)	(((struct cis_header *)(bp))->eh_size)
#define	CIS_DATAP(bp)	(((cis_addr_t) (bp)) + sizeof(struct cis_header))
#define	CISTD_END	255	/* tuple data end mark */

/**************************************************************
 * Layer I: Basic Compatiblity Tuple
 **************************************************************/
#define CIS_NULL		0x00
#define CIS_DEVICE		0x01
struct cis_device {
	u_int8_t dev_code;	/* tuple code */
	u_int8_t dev_size;	/* tuple size */

#define	DEVID_DTYPE(id)		(((id) & 0xf0) >> 4)	/* device type */
#define	DEVID_WPS		0x08			/* write protect */
#define	DEVID_SPEED(id)		((id) & 0x07)		/* speed desc */
#define	DEVID_SP250		1			/* 250 ns */
#define	DEVID_SP200		2			/* 200 ns */
#define	DEVID_SP150		3			/* 150 ns */
#define	DEVID_SP100		4			/* 100 ns */
#define	DEVID_SPEXT		7			/* has ext speed byte */
	u_int8_t dev_id;	/* device id */

#define	DEVID_EXTSB		0x80			/* has ext speed byte */
	/* extra speed bytes might exist */

#define	DEVSZ_ADDR(sz)		(((sz) >> 3) & 0x1f)	/* address unit */
#define	DEVSZ_U512		0			/* 512 bytes unit */
#define	DEVSZ_U2K		1			/* ... */
#define	DEVSZ_U8K		2
#define	DEVSZ_U32K		3
#define	DEVSZ_U128K		4
#define	DEVSZ_U512K		5
#define	DEVSZ_U2M		6			/* 2M bytes unit */
#define	DEVSZ_SIZE(sz)		((sz) & 0x07)		/* unit size */
	u_int8_t dev_sz;	/* device size */
};

#define CIS_LONGLINK_CB		0x02
#define CIS_CONFIG_CB		0x04
#define CIS_CFTABLE_ENTRY_CB	0x05
#define CIS_LONGLINK_MFC	0x06
struct cis_longlink_mfc_header {
	u_int8_t mh_code;	/* type code */
	u_int8_t mh_size;	/* size of this tuple */
	u_int8_t mh_cnt;	/* num of multi function entires */
				/* struct cis_longlink_mfc_entry continues */
} __attribute__((packed));

struct cis_longlink_mfc_entry {
#define	MFC_ENTRY_ATTRIBUTE	0	/* jump to attirbute area */
#define	MFC_ENTRY_COMMON	1	/* jump to common area */
	u_int8_t mt_flags;		/* flags */
	u_int32_t mt_offset;		/* entry offset */
} __attribute__((packed));

#define CIS_BAR			0x07
#define CIS_CHECKSUM		0x10
#define CIS_LONGLINK_A		0x11
#define CIS_LONGLINK_C		0x12
struct cis_longlink {
	u_int8_t ll_code;	/* type code */
	u_int8_t ll_size;	/* size of this tuple */
	u_int32_t ll_offset;	/* jump offset */
} __attribute__((packed));

#define CIS_LINKTARGET		0x13
struct cis_linktarget {
	u_int8_t lt_code;	/* type code */
	u_int8_t lt_size;	/* size of this tuple */
	u_int8_t lt_str[3];	/* CIS ident string */
} __attribute__((packed));

#define CIS_NO_LINK		0x14
#define CIS_VERS_1		0x15
struct cis_vers_1 {
	u_int8_t v1_code;	/* type code */
	u_int8_t v1_size;	/* size of this tuple */

	struct v1_version {
		u_int8_t v1_major;	/* major version */
		u_int8_t v1_minor;	/* minor version */
	} __attribute__((packed)) v1_version;

	u_int8_t v1_str[1];	/* information strings */
				/* manufacture str */
				/* version str */
				/* add info (1) str */
				/* add info (2) str */
} __attribute__((packed));

#define CIS_ALTSTR		0x16
#define CIS_DEVICE_A		0x17
#define CIS_JEDEC_C		0x18
#define CIS_JEDEC_A		0x19
#define CIS_CONFIG		0x1a
struct cis_config {
	u_int8_t tpcc_code;	/* type code */
	u_int8_t tpcc_size;	/* size of this tuple */

#define	TPCC_SZ_RSVD(sz)	(((sz) >> 6) & 0x03)
#define	TPCC_SZ_RMASK(sz)	((((sz) >> 2) & 0x0f) + 1)
#define	TPCC_SZ_RADR(sz)	(((sz) & 0x03) + 1) 
	u_int8_t tpcc_sz;	/* field size byte */
	u_int8_t tpcc_last;	/* last idx */

	/* tpcc_radr */
	/* tpcc_rmsk */
} __attribute__((packed));
	
#define CIS_CFTABLE_ENTRY	0x1b
struct cis_cftable_entry {
	u_int8_t tpce_code;	/* type code */
	u_int8_t tpce_size;	/* size of this tuple */

#define	TPCE_INDEX_HASIF	0x80	/* has interface byte */
#define	TPCE_INDEX_DEFAULT	0x40	/* default conf */
	u_int8_t tpce_index;

#define	TPCE_IF_FLAGS(idx)	((idx >> 4) & 0x0f)
#define	TPCE_IF_MWAIT	0x80	/* memory wait */
#define	TPCE_IF_RDYBSY	0x40	/* rdy/bsy */
#define	TPCE_IF_WP	0x20	/* write protect */
#define	TPCE_IF__BVD	0x10	/* battery notification */
#define	TPCE_IF_INTERFACE(idx) ((idx & 0x0f) + 1)
#define	TPCE_IF_MEMCARD	0	/* memory card */
#define	TPCE_IF_IOCARD	1	/* io card */
	/* interface desc byte */

#define	TPCE_FS_POWER	0x03	/* power function desc exists */
#define	TPCE_FS_TIMING	0x04	/* timing function desc exists */
#define	TPCE_FS_IO	0x08	/* io function desc exists */
#define	TPCE_FS_IRQ	0x10	/* irq function desc exists */
#define TPCE_FS_MEM	0x60	/* mem function desc exists */
#define	TPCE_FS_EXT	0x80	/* misc function desc exists */
	u_int8_t tpce_fs;	/* function select byte */
} __attribute__((packed));

/* ext function field byte */
#define	TPCE_EXT_HASEXT	0x80	/* has ext field byte */
#define	TPCE_EXT_WAKEUP	0x40	/* has event wakeup function */
#define	TPCE_EXT_POWDOWN 0x20	/* has power down mode */
#define	TPCE_EXT_RONLY	0x10	/* rdonly config */
#define	TPCE_EXT_AUDIO	0x08	/* has spkr output */

/* io function desc */
struct tpce_io {
#define	TPCE_IO_DESC_EXIST	0x80			/* range byte exists */
#define	TPCE_IO_DESC_BUS16	0x40
#define	TPCE_IO_DESC_BUS8	0x20
#define	TPCE_IO_DESC_BUS(desc)	((desc) >> 5 & 0x03)	/* bus flags */
#define TPCE_IO_DESC_DECODE	0x1f			/* addr decode line */
	u_int8_t io_desc;	/* desc byte */

#define	TPCE_IO_RANGE_NENT(byte) (((byte) & 0x0f) + 1)	/* num of entries */
#define	TPCE_IO_RANGE_AFSZ(byte) (((byte) & 0x30) >> 4)	/* address field size */
#define	TPCE_IO_RANGE_RFSZ(byte) (((byte) & 0xc0) >> 6)	/* range field size */
	u_int8_t io_range;	/* range byte */
} __attribute__((packed));

/* irq function desc */
struct tpce_irq {
#define	TPCE_IR_SHARE	0x80	/* interrupt share */			
#define	TPCE_IR_EDGE	0x40	/* edge triger */
#define	TPCE_IR_LEVEL	0x20	/* level trigger */
#define	TPCE_IR_HASMASK	0x10	/* has irq mask field */
#define	TPCE_IR_VEND	0x08	/* has vendor specific int */
#define	TPCE_IR_BERR	0x04	/* has bus err int */
#define	TPCE_IR_IOCHK	0x02	/* has io chk int */
#define	TPCE_IR_NMI	0x01	/* has nmi */
	u_int8_t irq_desc;	/* desc byte */
	u_int16_t irq_mask;	/* mask word */
} __attribute__((packed));

/* mem function desc */
struct tpce_mem {
	u_int8_t mem_desc;	/* desc byte */

#define	TPCE_MEM_RANGE_NENT(byte) (((byte) & 0x7) + 1)	  /* num of entries */
#define	TPCE_MEM_RANGE_RFSZ(byte) (((byte) >> 3) & 0x03)  /* address field */
#define	TPCE_MEM_RANGE_AFSZ(byte) (((byte) >> 5) & 0x03)  /* range size field */
#define	TPCE_MEM_RANGE_HAF	  0x80			  /* host addr field */
	u_int8_t mem_range;	/* range byte */
} __attribute__((packed));

#define CIS_DEVICE_OC		0x1c
#define CIS_DEVICE_OA		0x1d
#define CIS_DEVICE_GEO		0x1e
#define CIS_DEVICE_GEO_A	0x1f

/**************************************************************
 * Layer II: Data Recording Format Tuple
 **************************************************************/
#define CIS_MANFID		0x20
struct cis_manfid {
	u_int8_t mid_code;	/* type code */
	u_int8_t mid_size;	/* size of this tuple */

	u_int16_t mid_manf;	/* manifucture id */
	u_int16_t mid_card;	/* number / rev */
};
	
#define CIS_FUNCID		0x21
#define CIS_FUNCE		0x22
#define CIS_SWIL		0x23
#define CIS_END			0xff
#define CIS_VERS_2		0x40
#define CIS_FORMAT		0x41
#define CIS_GEOMETRY		0x42
#define CIS_BYTEORDER		0x43
#define CIS_DATE		0x44
#define CIS_BATTERY		0x45

/**************************************************************
 * Layer III: Data Organization Tuple
 **************************************************************/
#define CIS_ORG			0x46

#ifdef	_KERNEL
/**************************************************************
 * Kernel Internal Only
 **************************************************************/
#define	CIS_REQINIT		0
#define	CIS_PAGECODE(addr)	((addr) / NBPG)
#define	CIS_PAGEOFFS(addr)	((addr) & (NBPG - 1))
#define	CIS_IDSTRING		255
#define	CIS_REQUNK		((u_int) -1)
#define	CIS_REQMASK		((u_int) 0xffff)
#define CIS_REQCODE(maj, min)	(((maj) << 16) | ((min) & CIS_REQMASK))
#define	CIS_MAJOR_REQCODE(code)	(((code) >> 16) & CIS_REQMASK)
#define	CIS_MINOR_REQCODE(code)	((code) & CIS_REQMASK)
#endif	/* _KERNEL */
#endif	/* !_TUPLE_H_ */
