/*	$NecBSD: pisavar.h,v 1.50 1998/10/28 06:10:29 honda Exp $	*/
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

#ifndef	_PISAVAR_H_
#define	_PISAVAR_H_

#include <machine/slot_device.h>

/********************************************************
 * pisa specific data stuctures
 ********************************************************/
typedef	slot_device_handle_t pisa_device_handle_t;

/* slot type: bus category PISA */
#define	PISA_SLOT_NULL		(SLOT_DEVICE_BUS_PISA | 0x0000)
#define	PISA_SLOT_PCMCIA	(SLOT_DEVICE_BUS_PISA | 0x0001)
#define	PISA_SLOT_PnPISA	(SLOT_DEVICE_BUS_PISA | 0x0002)
#define	PISA_SLOT_PINT		(SLOT_DEVICE_BUS_PISA | 0x0003)
#define	PISA_SLOT_DOCK		(SLOT_DEVICE_BUS_PISA | 0x0004)

typedef	int pisa_event_t;

/* hardware low level events */
#define	PISA_EVENT_SUSPEND		0
#define	PISA_EVENT_RESUME		1
#define	PISA_EVENT_INSERT		2
#define	PISA_EVENT_REMOVE		3
#define	PISA_EVENT_ACTIVATE		4
#define	PISA_EVENT_DEACTIVATE		5
/* software upper level events */
#define	PISA_EVENT_NOTIFY_SUSPEND	SD_EVENT_NOTIFY_SUSPEND
#define	PISA_EVENT_QUERY_SUSPEND	SD_EVENT_QUERY_SUSPEND
#define	PISA_EVENT_NOTIFY_RESUME	SD_EVENT_RESUME
#define	PISA_EVENT_QUERY_RESUME		SD_EVENT_QUERY_RESUME
#define	PISA_EVENT_REQUEST_ABORT	SD_EVENT_NOTIFY_ABORT

struct pisa_functions {
	int (*pd_activate) __P((pisa_device_handle_t));
	int (*pd_deactivate) __P((pisa_device_handle_t));
	int (*pd_attach) __P((pisa_device_handle_t));
	int (*pd_detach) __P((pisa_device_handle_t));
	int (*pd_notify) __P((pisa_device_handle_t, pisa_event_t));
};

struct pisa_resources {
	struct slot_device_resources *pv_dr;	/* device resources map */
	struct pisa_functions *pv_pd;		/* funcs tab */
	pisa_event_t pv_recev;			/* event record */	
};

/********************************************************
 * resource definitions
 ********************************************************/
typedef	struct pisa_resources *pisa_res_t;

#define	PISA_UNKVAL	SLOT_DEVICE_UNKVAL

#define	PISA_PIN0	0
#define	PISA_PIN1	1
#define	PISA_DMA0	0
#define	PISA_DMA1	1
#define	PISA_IO0	0
#define	PISA_IO1	1
#define	PISA_IO2	2
#define	PISA_IO3	3
#define	PISA_IO4	4
#define	PISA_MEM0	5
#define	PISA_MEM1	6
#define	PISA_MEM2	7
#define	PISA_MEM3	8
#define	PISA_MEM4	9

/* handle */
#define	PISA_PINHAND(dh, chno)	((dh)->dh_space.sp_pinh[(chno)].ch_handle)
#define	PISA_IMTAG(dh, imno) 	((bus_space_tag_t)((dh)->dh_space.sp_imh[(imno)].im_tag))
#define	PISA_IMHAND(dh, imno) 	((bus_space_handle_t) ((dh)->dh_space.sp_imh[(imno)].im_handle))
#define	PISA_IMWMAP(dh, imno) 	((dh)->dh_space.sp_imh[(imno)].im_bmap)

/* dev softc */
#define	PISA_DEV_SOFTC(dh)	((void *) ((dh)->dh_dv))

/* resources */
#define	PISA_RES(dh)		((pisa_res_t) ((dh)->dh_res))
#define	PISA_RES_FUNCTIONS(dh)  (PISA_RES(dh)->pv_pd)
#define	PISA_RES_EVENT(dh) 	(PISA_RES(dh)->pv_recev)
#define	PISA_RES_DR(dh)		(PISA_RES(dh)->pv_dr)
#define	PISA_RES_IA(dh)		(&PISA_RES(dh)->pv_ia)

/* device windows */
#define	PISA_DR_NIO(dr)		((dr)->dr_nio)
#define	PISA_DR_NMEM(dr)	((dr)->dr_nmem)
#define	PISA_DR_NIRQ(dr)	((dr)->dr_npin)
#define	PISA_DR_NDRQ(dr)	((dr)->dr_ndrq)

#define	PISA_DR_IMB(dr, idx)	((dr)->dr_im[(idx)].im_hwbase)
#define	PISA_DR_IMS(dr, idx)	((dr)->dr_im[(idx)].im_size)
#define	PISA_DR_IMF(dr, idx)	((dr)->dr_im[(idx)].im_flags)
#define	PISA_DR_IOB(dr, idx)	((dr)->dr_io[(idx)].im_hwbase)
#define	PISA_DR_IOS(dr, idx)	((dr)->dr_io[(idx)].im_size)
#define	PISA_DR_IOF(dr, idx)	((dr)->dr_io[(idx)].im_flags)
#define	PISA_DR_MEMB(dr, idx)	((dr)->dr_mem[(idx)].im_hwbase)
#define	PISA_DR_MEMS(dr, idx)	((dr)->dr_mem[(idx)].im_size)
#define	PISA_DR_MEMF(dr, idx)	((dr)->dr_mem[(idx)].im_flags)
#define	PISA_DR_IRQN(dr, idx)	((dr)->dr_pin[(idx)].dc_chan)
#define	PISA_DR_DRQN(dr, idx)	((dr)->dr_drq[(idx)].dc_chan)

#define	PISA_DR_IRQ0(dr)	PISA_DR_IRQN((dr), 0)
#define	PISA_DR_IRQ1(dr)	PISA_DR_IRQN((dr), 1)
#define	PISA_DR_DRQ0(dr)	PISA_DR_DRQN((dr), 0)
#define	PISA_DR_DRQ1(dr)	PISA_DR_DRQN((dr), 1)
#define	PISA_DR_IOBASE(dr)	PISA_DR_IOB((dr), 0)
#define	PISA_DR_IOSIZE(dr)	PISA_DR_IOS((dr), 0)
#define	PISA_DR_MEMBASE(dr)	PISA_DR_MEMB((dr), 0)
#define	PISA_DR_MEMSIZE(dr)	PISA_DR_MEMS((dr), 0)
#define	PISA_DR_IRQ(dr)		PISA_DR_IRQ0((dr))
#define	PISA_DR_DRQ(dr)		PISA_DR_DRQ0((dr))
#define	PISA_DR_DVCFG(dr)	((dr)->dr_dvcfg)

/********************************************************
 * attach args
 ********************************************************/
struct pisa_attach_args {
	pisa_device_handle_t pa_dh;		/* slot device handle */

	bus_space_tag_t pa_iot;			/* io tag of our bus */
	bus_space_tag_t pa_memt;		/* dma tag of our bus */
	bus_dma_tag_t pa_dmat;			/* dmat tag of our bus */
	void *pa_ic;				/* pisa chipset tag */

	int pa_state;				/* internal use Only */
};

struct pisa_slot_device_attach_args {
	slot_device_res_t psa_pdr;		/* phys device map */
};

/********************************************************
 * funcs proto
 ********************************************************/
extern struct slot_device_bus_tag pisa_slot_device_bus_tag;

u_int8_t *pisa_device_info __P((pisa_device_handle_t, u_int));
pisa_device_handle_t pisa_register_functions
	__P((pisa_device_handle_t, struct device *, struct pisa_functions *));

void *pisa_intr_establish
	__P((pisa_device_handle_t, int, int, int (*) __P((void *)), void *));
void pisa_intr_disestablish
	__P((pisa_device_handle_t, isa_chipset_tag_t ic, void *handler));

int pisa_space_map
	__P((pisa_device_handle_t, u_int, bus_space_handle_t *));
int pisa_space_unmap 
	__P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t));

int pisa_space_map_load __P((slot_device_handle_t, u_int, bus_size_t, bus_space_iat_t, bus_space_handle_t *));

int pisa_slot_device_enable __P((slot_device_handle_t));
int pisa_slot_device_disable __P((slot_device_handle_t));

static inline int pisa_isactive __P((pisa_device_handle_t));
#define	pisa_space_map_unload(dh, bc, bah) bus_space_map_load((bc), (bah))

static inline int
pisa_isactive(dh)
	slot_device_handle_t dh;
{

	return (dh == NULL || (dh->dh_flags & DH_ACTIVE));
}
#endif	/* !_PISAVAR_H_ */
