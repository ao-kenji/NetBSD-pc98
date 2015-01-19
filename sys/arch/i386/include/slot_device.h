/*	$NecBSD: slot_device.h,v 3.22 1998/10/28 06:10:04 honda Exp $	*/
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
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef _I386_SLOT_DEVICE_H_
#define _I386_SLOT_DEVICE_H_

#include <sys/device.h>
#include <sys/queue.h>

#define	SLOT_DEVICE_UNKVAL	((u_long) -1)
#define	SLOT_DEVICE_NIO		5
#define	SLOT_DEVICE_NMEM	5
#define	SLOT_DEVICE_NIM		(SLOT_DEVICE_NIO + SLOT_DEVICE_NMEM)
#define	SLOT_DEVICE_NPIN	4
#define	SLOT_DEVICE_NDRQ	2

struct slot_device_handle;
typedef struct slot_device_handle *slot_device_handle_t;

struct slot_device_slot_tag;
typedef struct slot_device_slot_tag *slot_device_slot_tag_t;

/*************************************************************
 * slot device bus tag structure
 *************************************************************/
/* upper level notification (lower 16 bits are free) */
typedef	int slot_device_event_t;
#define	SD_EVENT_NOTIFY_BUSMASK		 0xffff
#define	SD_EVENT_NOTIFY_SUSPEND		0x10000
#define	SD_EVENT_QUERY_SUSPEND		0x20000
#define	SD_EVENT_NOTIFY_RESUME		0x30000
#define	SD_EVENT_QUERY_RESUME		0x40000
#define	SD_EVENT_NOTIFY_ABORT		(-1)

#define	SD_EVENT_STATUS_BUSMASK		 0xffff
#define	SD_EVENT_STATUS_BUSY		0x10000	/* busy status */
#define	SD_EVENT_STATUS_REJECT		0x20000	/* request reject */
#define	SD_EVENT_STATUS_FERR		0x40000	/* fatal error */

#define	_SLOT_DEVICE_BUS_FUNCS_PROTO(NAME) \
	int NAME##_slot_device_attach __P((slot_device_slot_tag_t,	\
			      slot_device_handle_t, void *));		\
	int NAME##_slot_device_dettach __P((slot_device_slot_tag_t,	\
			      slot_device_handle_t, void *));		\
	int NAME##_slot_device_activate __P((slot_device_slot_tag_t,	\
			      slot_device_handle_t, void *));		\
	int NAME##_slot_device_deactivate __P((slot_device_slot_tag_t,	\
			      slot_device_handle_t, void *));		\
	int NAME##_slot_device_vanish __P((slot_device_slot_tag_t,	\
			      slot_device_handle_t, void *));		\
	int NAME##_slot_device_notify __P((slot_device_slot_tag_t,	\
			      slot_device_handle_t, slot_device_event_t));

#define	_SLOT_DEVICE_BUS_FUNCS_TAB(NAME) 	\
	NAME##_slot_device_attach,		\
	NAME##_slot_device_dettach,		\
	NAME##_slot_device_activate,		\
	NAME##_slot_device_deactivate,		\
	NAME##_slot_device_vanish,		\
	NAME##_slot_device_notify
	
/* bus service request functions (slot controller -> slot bus) */
#define	slot_device_attach(stag, handle, aux) \
	((*((stag)->st_sbtag->sb_attach)) ((stag), (handle), (aux)))
#define	slot_device_detach(stag, handle, aux) \
	((*((stag)->st_sbtag->sb_detach)) ((stag), (handle), (aux)))
#define	slot_device_activate(stag, handle, aux) \
	((*((stag)->st_sbtag->sb_activate)) ((stag), (handle), (aux)))
#define	slot_device_deactivate(stag, handle, aux) \
	((*((stag)->st_sbtag->sb_deactivate)) ((stag), (handle), (aux)))
#define	slot_device_vanish(stag, handle, aux) \
	((*((stag)->st_sbtag->sb_vanish)) ((stag), (handle), (aux)))
#define	slot_device_notify(stag, handle, ev) \
	((*((stag)->st_sbtag->sb_notify)) ((stag), (handle), (ev)))

struct slot_device_bus_tag {
	int sb_id;			/* bus id */
	u_char *sb_name;		/* bus name */
	void *sb_data;			/* bus specific data */

	int (*sb_attach) __P((slot_device_slot_tag_t,
			      slot_device_handle_t, void *));
	int (*sb_dettach) __P((slot_device_slot_tag_t,
			      slot_device_handle_t, void *));
	int (*sb_activate) __P((slot_device_slot_tag_t,
			      slot_device_handle_t, void *));
	int (*sb_deactivate) __P((slot_device_slot_tag_t,
			      slot_device_handle_t, void *));
	int (*sb_vanish) __P((slot_device_slot_tag_t,
			      slot_device_handle_t, void *));
	int (*sb_notify) __P((slot_device_slot_tag_t,
			      slot_device_handle_t, slot_device_event_t));
};

typedef struct slot_device_bus_tag *slot_device_bus_tag_t;

/*************************************************************
 * slot device identifier structure
 *************************************************************/
typedef void *slot_device_magic_t;

struct slot_device_ident {
	/* internal */
	slot_device_bus_tag_t di_sbtag;		/* identifier for bus */
	u_char di_xname[16];			/* device name */

	slot_device_magic_t di_magic;		/* slot specific data */
	u_int di_length;			/* size of di_magic */

	/* exported */
	u_long	di_idn;				/* product number */
	u_char	*di_ids;			/* product string if exist */
};

typedef	struct slot_device_ident *slot_device_ident_t;

/*************************************************************
 * slot device slot tag structure
 *************************************************************/
typedef int slot_device_slot_type_t;
TAILQ_HEAD(slot_device_handle_tab, slot_device_handle);

struct slot_device_intr_arg {
	struct slot_device_intr_arg *sa_next;

	int (*sa_handler) __P((void *));
	void *sa_arg;

	slot_device_handle_t sa_dh;
	int sa_pin;
};

typedef	struct slot_device_intr_arg *slot_device_intr_arg_t;

struct slot_device_service_functions {
	int (*st_open) __P((slot_device_handle_t));
	int (*st_close) __P((slot_device_handle_t));
	int (*st_im_map) __P((slot_device_handle_t, u_int));
	int (*st_im_unmap) __P((slot_device_handle_t, u_int));
	int (*st_intr_map) __P((slot_device_handle_t, u_int));
	int (*st_intr_unmap) __P((slot_device_handle_t, u_int));
	int (*st_intr_ack) __P((void *));
	int (*st_dma_map) __P((slot_device_handle_t, u_int));
	int (*st_dma_unmap) __P((slot_device_handle_t, u_int));
	u_int8_t *((*st_info) __P((slot_device_handle_t, u_int)));
};

struct slot_device_slot_tag {
	slot_device_bus_tag_t st_sbtag;
	void *st_sc;				/* slot controller */
	void *st_bus;				/* my bus */
	void *st_aux;				/* args for slot controller */
	slot_device_slot_type_t st_type;	/* slot type */
	int st_flags;				/* control flags */
#define	SD_STAG_INTRACK	0x0001			/* intr feedback needed */

	struct slot_device_service_functions *st_funcs;
	struct slot_device_handle_tab st_dhtab;
};

/* slot device serivce request functions (slot bus -> slot controller) */
#define	slot_device_service_open(dh) \
	((*(dh)->dh_stag->st_funcs->st_open) (dh))
#define	slot_device_service_close(dh) \
	((*(dh)->dh_stag->st_funcs->st_close) (dh))
#define	slot_device_service_im_map(dh, win) \
	((*(dh)->dh_stag->st_funcs->st_im_map) ((dh), (win)))
#define	slot_device_service_im_unmap(dh, win) \
	((*(dh)->dh_stag->st_funcs->st_im_unmap) ((dh), (win)))
#define	slot_device_service_intr_map(dh, win) \
	((*(dh)->dh_stag->st_funcs->st_intr_map) ((dh), (win)))
#define	slot_device_service_intr_unmap(dh, win) \
	((*(dh)->dh_stag->st_funcs->st_intr_unmap) ((dh), (win)))
#define	slot_device_service_intr_ack(dh, win) \
	((*(dh)->dh_stag->st_funcs->st_intr_ack) ((dh), (win)))
#define	slot_device_service_dma_map(dh, win) \
	((*(dh)->dh_stag->st_funcs->st_dma_map) ((dh), (win)))
#define	slot_device_service_dma_unmap(dh, win) \
	((*(dh)->dh_stag->st_funcs->st_dma_unmap) ((dh), (win)))
#define	slot_device_service_info(dh, no) \
	((*(dh)->dh_stag->st_funcs->st_info) ((dh), (no)))
#define	SLOT_DEVICE_INTR_FUNC(st) ((st)->st_funcs->st_intr_ack)

/*************************************************************
 * slot device handle structure
 *************************************************************/
typedef void *slot_device_space_tag_t;
typedef void *slot_device_space_handle_t;
typedef u_int32_t slot_device_bmap_t;

struct slot_device_space_resources {
	/* range type resource handle (ex: memh, ioh) */
	struct slot_device_space_iomem {
		slot_device_space_tag_t im_tag;
		slot_device_space_handle_t im_handle;
		slot_device_bmap_t im_bmap;
	} sp_imh[SLOT_DEVICE_NIM];

	/* point type resource handle (ex: ih, dmah) */
	struct slot_device_space_channel {
		slot_device_space_tag_t ch_tag;
		slot_device_space_handle_t ch_handle;
	} sp_pinh[SLOT_DEVICE_NPIN];

	struct slot_device_space_channel sp_dmah[SLOT_DEVICE_NDRQ];
};

struct slot_device_space_resources *slot_device_space_res_t;
typedef void *slot_device_resource_t;

struct slot_device_handle {
	struct device *dh_dv;			/* device structure  */

	slot_device_ident_t dh_id;		/* identifier for the device */

	TAILQ_ENTRY(slot_device_handle) dh_chain;
	TAILQ_ENTRY(slot_device_handle) dh_dvgchain;

	slot_device_slot_tag_t dh_stag;		/* my slot if exists */
	u_int dh_flags;				/* flags */
	int dh_state;				/* my state */
	int dh_ref;				/* reference count */

	struct slot_device_space_resources dh_space;	/* space resources */
	slot_device_resource_t dh_ci;		/* device specific resources */
	slot_device_resource_t dh_res;		/* bus specific resources */
};

/* dh_flags values */
#define	DH_ACTIVE	0x0001
#define	DH_INACTIVE	0x0002
#define	DH_CREATE	0x0004			/* under creation */
#define	DH_BUSY		0x0008			/* busy handle */

#define	slot_device_handle_busy(dh)	((dh)->dh_flags |= DH_BUSY)
#define	slot_device_handle_unbusy(dh)	((dh)->dh_flags &= ~DH_BUSY)

#define	SLOT_DEVICE_IDENT_IDN(dh)	((dh)->dh_id->di_idn)
#define	SLOT_DEVICE_IDENT_IDS(dh)	((dh)->dh_id->di_ids)

/*************************************************************
 * bus space resources
 *************************************************************/
#include <machine/bus.h>

struct slot_device_iomem {
	u_long  im_base;
	u_long  im_hwbase;
	u_long  im_size;
	u_long  im_flags;
#define	SDIM_BUS_MASK		0xffff0000	/* lower 16 bits are free */
#define	SDIM_BUS_AUTO		0x00010000	/* auto select */
#define	SDIM_BUS_WIDTH16	0x00020000	/* bus size 16 */
#define	SDIM_BUS_WIDTH32	0x00040000	/* bus size 32 */
#define	SDIM_BUS_WIDTH64	0x00080000	/* bus size 64 */
#define	SDIM_BUS_WEIGHT		0x00100000	/* access weight requried */
#define	SDIM_BUS_WP		0x00200000	/* write access disable */
#define	SDIM_BUS_FOLLOW		0x40000000	/* bus width follows cpu access */
#define	SDIM_BUS_ACTIVE		0x80000000	/* active */

#define	SLOT_DEVICE_SPIO	0
#define	SLOT_DEVICE_SPMEM	1
	int   	im_type;
};

struct slot_device_resources {
	int dr_nio;
	int dr_nmem;
	int dr_nim;

	union {
		struct {
			struct slot_device_iomem  sp_io[SLOT_DEVICE_NIO];
			struct slot_device_iomem  sp_mem[SLOT_DEVICE_NMEM];
		} space_sp;
		struct slot_device_iomem space_im[SLOT_DEVICE_NIM];	
	} dr_space;

#define	dr_io	dr_space.space_sp.sp_io
#define	dr_mem	dr_space.space_sp.sp_mem
#define	dr_im	dr_space.space_im

	int dr_npin;
	struct slot_device_channel {
		u_long dc_chan;
		u_long dc_flags;
		u_long dc_aux;
	} dr_pin[SLOT_DEVICE_NPIN];

	int dr_ndrq;
	struct slot_device_channel dr_drq[SLOT_DEVICE_NDRQ];
		
	u_long dr_dvcfg;
};

/* slot device bus id */
#define	SLOT_DEVICE_BUS_MASK		0xffff0000
#define	SLOT_DEVICE_BUS_NULL		0x00000000	
#define	SLOT_DEVICE_BUS_PISA		0x00010000
#define	SLOT_DEVICE_BUS_CARDBUS		0x00020000

#define	SLOT_DEVICE_BUS_SUBID(id)	((id) & ~SLOT_DEVICE_BUS_MASK)
#define	SLOT_DEVICE_BUS_ID(id)		((id) & SLOT_DEVICE_BUS_MASK)

struct slot_device_attach_bus_args {
	slot_device_bus_tag_t sda_sb;

	bus_space_tag_t sda_iot;
	bus_space_tag_t sda_memt;
	bus_dma_tag_t sda_dmat;
	void *sda_bc;

	void *sda_aux;			/* XXX: should be fixed */
};

typedef struct slot_device_resources *slot_device_res_t;

/*************************************************************
 * util funcs
 *************************************************************/
slot_device_slot_tag_t slot_device_slot_tag_allocate 
	__P((slot_device_bus_tag_t, void *, void *, void *,\
	     slot_device_slot_type_t));

slot_device_ident_t slot_device_ident_allocate 
	__P((slot_device_slot_tag_t, u_char *, u_int));
void slot_device_ident_deallocate __P((slot_device_ident_t));

slot_device_handle_t slot_device_handle_lookup
	__P((slot_device_slot_tag_t, slot_device_ident_t, u_int));

int slot_device_handle_allocate
	__P((slot_device_slot_tag_t, slot_device_ident_t,\
	     int, slot_device_handle_t *));
void slot_device_handle_deallocate
	__P((slot_device_slot_tag_t, slot_device_handle_t));

void slot_device_handle_activate 
	__P((slot_device_slot_tag_t, slot_device_handle_t));
void slot_device_handle_deactivate
	__P((slot_device_slot_tag_t, slot_device_handle_t));

slot_device_intr_arg_t slot_device_allocate_intr_arg 
	__P((slot_device_handle_t, int, int (*) __P((void *)), void *));
int slot_device_deallocate_intr_arg __P((slot_device_intr_arg_t));

#endif /* !_I386_SLOT_DEVICE_H_ */
