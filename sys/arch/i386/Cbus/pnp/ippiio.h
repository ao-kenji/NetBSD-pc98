/*	$NecBSD: ippiio.h,v 1.12 1998/03/14 07:06:25 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
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

#ifndef	_IPPI_IOCTL_H_
#define	_IPPI_IOCTL_H_

#include <machine/slot_device.h>

typedef struct slot_device_resources *ippi_res_t;
typedef	int ippi_status_t;

/*********************************************************
 * parameters
 *********************************************************/
/* global parameters */
#define	IPPI_DVNAMELEN	16		/* device name length */
#define MAX_CARDS	8		/* max targets for ippi */
#define	MAX_LDNS	3		/* max logical devices per a board */
#define	MAX_RDSZ	2048		/* pnp data buffer size */
#define	PA_DATALEN 	9		/* pn data resource head size */

/* device status */
#define	IPP_FOUND	1		/* pnp board found (serialize) */
#define	IPP_SLEEP	2		/* sleep */
#define	IPP_MAPPED	3		/* resource allocated */
#define	IPP_DEVALLOC	4		/* device connected */

/* Configuration Information */
#define	MAX_IRQSET	2
#define	MAX_DRQSET	2
#define	MAX_IOWIN	8
#define	MAX_MEMWIN	4

/*********************************************************
 * pnp ioctl information structure
 *********************************************************/
struct ippi_ipid {
	u_int ipd_csn;			/* card selection number */
	u_int ipd_ldn;			/* card logical device number */
};

struct ippi_bid {
	u_int bid_id;			/* vendor/product id */
	u_int bid_serial;		/* serial id */
};

struct ippi_ipp_info {
	struct ippi_ipid ip_ipd;	/* ippi id */
	struct ippi_bid ip_bid;		/* board id */

	ippi_status_t ip_state;		/* current status of device */
	struct slot_device_resources ip_dr;	/* pnp resources */

	u_char ip_dvname[IPPI_DVNAMELEN];/* device name */
	u_int8_t ip_cis[MAX_RDSZ];	/* cis data buffer */
};

struct ippidev_connect_args {
	struct ippi_ipid ica_ipd;	/* ippi id */
	struct ippi_bid ica_bid;	/* board id */

	struct slot_device_resources ica_dr;	/* pnp resource data */
	u_char ica_name[IPPI_DVNAMELEN + 1];	/* IN/OUT */
};

struct ippi_ctrl {
	struct ippi_ipid ic_ipd;	/* ippi id */

#define	IPPI_DEV_ACTIVATE	0
#define	IPPI_DEV_DEACTIVATE	1
	int ic_ctrl;			/* control */
};

struct ippres_request {
	u_int ir_ldn;			/* logical device number */
	u_int ir_dfn;			/* dependent function number */
	u_int32_t ir_cid;		/* search compatible dev id */
	u_int ir_ndfn;			/* num dependent function */
};

struct ippres_resources {
	struct slot_device_resources irr_dr;	/* resources */

	/* 
	 * the followings are additional informations which are only
	 * needed in ipp resource allocation process.
	 */
	int irr_pri;			/* priority */

#define	IPPRES_LDEV	0x0001
#define	IPPRES_CDEV	0x0002		/* compatible log dev */
#define	IPPRES_BDEV	0x0004		/* bootable device */
	u_int irr_flags;		/* flags */

	struct ippres_io {		/* additional info of io */
		u_long io_hi;		/* io high addr */
		u_long io_al;		/* io align */
	} irr_io[SLOT_DEVICE_NIO];

	struct ippres_mem {		/* additional info of mem */
		u_long mem_hi;		/* mem high addr */
		u_long mem_al;		/* mem align */
		int mem_bussz;		/* bus size */
	} irr_mem[SLOT_DEVICE_NMEM];
};

typedef struct ippres_resources *ippres_res_t;

#define	IPPI_IOG_RD		_IOWR('I', 60, struct ippi_ipp_info)
#define	IPPI_IOC_DEVICE		_IOW('I', 61, struct ippi_ctrl)
#define	IPPI_IOC_MAP		_IOWR('I', 62, struct ippidev_connect_args)
#define	IPPI_IOC_CONNECT_PISA	_IOWR('I', 63, struct ippidev_connect_args)
#define	IPPI_IOC_PREFER		_IOWR('I', 64, struct ippi_ipp_info)

#endif	/* !_IPPI_IOCTL_H_ */
