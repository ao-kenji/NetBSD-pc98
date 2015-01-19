/*	$NecBSD: pccsvar.h,v 1.23.2.2 1999/09/18 11:17:19 honda Exp $	*/
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

#ifndef	_PCCSVAR_H_
#define	_PCCSVAR_H_

#include <machine/slot_device.h>
#include <i386/pccs/tuple.h>
#include <sys/lock.h>

/***********************************************************
 * pccshw tag structure
 ***********************************************************/
#define	CHIP_PCCS16		0x0
#define	CHIP_NULL		0x0000
#define	CHIP_I82365		(CHIP_PCCS16 | 0x0001)
#define	CHIP_PD672X		(CHIP_PCCS16 | 0x0002)
#define	CHIP_PD6710		(CHIP_PCCS16 | 0x0003)
#define	CHIP_VG468		(CHIP_PCCS16 | 0x0004)
#define	CHIP_VG469		(CHIP_PCCS16 | 0x0005)
#define	CHIP_NEC		(CHIP_PCCS16 | 0x0100)
#define	CHIP_BRIDGE_PCIISA	(CHIP_PCCS16 | 0x0200)

#define	CHIP_PCCS32		0x8000
#define	CHIP_TI			(CHIP_PCCS32 | 0x0001)

typedef void   *bus_chipset_tag_t;
typedef u_int  window_handle_t;

struct pccshw_funcs;
struct pccs_config_funcs;
struct pccshw_tag {
	void *pp_pcsc;			/* allocated pccs space */
	void *pp_dv;			/* my hardware pointer */
	int  pp_chip;			/* chip */

	struct pccshw_funcs *pp_hw;		/* hardware function call */
	struct pccs_config_funcs *pp_pccs; 	/* pccs protocol call */

	/* bus description */
	slot_device_slot_type_t pp_type;/* slot type */
	slot_device_bus_tag_t pp_sb;	/* connected bus tag */
	bus_chipset_tag_t pp_bc;	/* connected bus specific tag */

	bus_space_tag_t pp_iot;		/* connected bus io space tag */
	bus_space_tag_t pp_memt;	/* connected bus mem space tag */
	bus_dma_tag_t	pp_dmat;	/* connected bus dma tag */

	bus_space_tag_t pp_csbst;	/* configuration space tag */
	bus_space_handle_t pp_csbsh;	/* configuration space access handle */
	struct slot_device_iomem pp_csim;

	/* space description */
	int pp_maxslots;		/* max slots */
	int pp_nio;			/* num io windows */
	int pp_nmem;			/* num memory windows */
	int pp_nim;			/* max io + mem windows */

	bus_addr_t pp_lowmem;		/* mem range: low mem limit */
	bus_addr_t pp_highmem;		/* mem range: high mem lmiit */
	bus_addr_t pp_startmem;		/* mem range: start mem */
	bus_addr_t pp_lowio;		/* io range: low io limit */
	bus_addr_t pp_highio;		/* io range: high io limit */
	bus_addr_t pp_startio;		/* io range: start io */

	vsize_t pp_psz;			/* mem window frame size */
	u_int pp_ps;			/* mem window frame shift */
};

typedef struct pccshw_tag *pccshw_tag_t;

/***********************************************************
 * pccs slot structre
 ***********************************************************/
#define	SLOTF_UNKWIN	((window_handle_t) -1)
typedef	u_int  slot_handle_t;

struct slot_func_info {
	u_int si_flags;			/* mapping state */
#define	SLOTF_POWER	0x0001
#define	SLOTF_MAPPED	0x0002
#define	SLOTF_INTR	0x0004
#define	SLOTF_OPEN	0x0008

	slot_device_handle_t si_dh;	/* slot device handle */
	window_handle_t si_wh[SLOT_DEVICE_NIM];	/* window handle */
	u_char si_xname[PCCS_NAMELEN];	/* xname */
	struct tuple_data si_tdata;	/* working data buffer */
};

struct slot_softc {
	struct device sc_dev;		/* device */

	slot_device_slot_tag_t sc_st;	/* current slot device tag */
	slot_device_slot_tag_t sc_16st;	/* 16 bits slot device tag */
	slot_device_slot_tag_t sc_32st;	/* 32 bits slot device tag */

	slot_handle_t sc_id;		/* slot id */

	TAILQ_ENTRY(slot_softc) sc_chain;	/* slot chain */

	int sc_enabled;			/* enable or disable state */
	struct lock sc_lock;		/* lock */
	struct proc *sc_proc;		/* current process */
	pid_t sc_pid;			/* pid */
	slot_status_t sc_state;		/* status */

#define	HW_REMOVE	0x0001
#define	HW_PENDING	0x0002		/* insert going */
#define	HW_INSERT	0x0004
#define	HW_EVENT	(HW_INSERT | HW_PENDING | HW_REMOVE)
#define	HW_DOWN		0x0010
#define	HW_SPKRUP	0x0020
#define	HW_SHAREWIN	0x0040
#define	HW_AUTORES	0x0080
#define	HW_AUTOINS	0x0100
	u_int sc_hwflags;		/* flags */

	struct card_info *sc_mci;	/* card resrouce info */
	u_int8_t *sc_atbp;		/* configuration space memory buffer */
	int sc_atbsz;			/* real memory buffer size */
	bus_addr_t sc_aoffs;		/* data offset */
	bus_addr_t sc_asize;		/* data size */

	int sc_ndev;			/* attached devices */
	int sc_open;			/* open count */
	int sc_intr;			/* intr mapped count */
	struct slot_func_info *sc_si[PCCS_MAXMFC]; /* info for mfc */

#define	PCCS_MAXEVENT	8
	struct slot_event sc_event[PCCS_MAXEVENT];/* event recorder */
	u_int sc_sevrp, sc_sevwp;
};

#define	PCCS_SEV_POS(pos) ((pos) % PCCS_MAXEVENT)
#define	PCCS_SEV_EVENT(ssc) (PCCS_SEV_POS((ssc)->sc_sevrp) != PCCS_SEV_POS((ssc)->sc_sevwp))
#define	PCCS_SEV_READ(ssc) \
		((ssc)->sc_event[(((ssc)->sc_sevrp ++) % PCCS_MAXEVENT)].sev_code)
#define	PCCS_SEV_WRITE(ssc, evcode) \
		((ssc)->sc_event[(((ssc)->sc_sevwp ++) % PCCS_MAXEVENT)].sev_code = (evcode))

/***********************************************************
 * pccs structre
 ***********************************************************/
struct pccs_softc {
	struct device sc_dev;			/* device */
	struct lock sc_lock;			/* pccs private lock */

	TAILQ_HEAD( , slot_softc) sc_ssctab;	/* slot chain tab */
};

/***********************************************************
 * pccs attach args
 ***********************************************************/
struct pccs_attach_args {
	pccshw_tag_t pa_16pp;	/* 16 bits interface */
	pccshw_tag_t pa_32pp;	/* 32 bits interface (cardbus) */
};

#define	PCCS_SLOT_TAG(ssc) ((ssc)->sc_st)
#define	PCCSHW_TAG(ssc)    ((pccshw_tag_t) ((ssc)->sc_st->st_aux))

/***********************************************************
 * Pc card standard {16|32} config functions
 ***********************************************************/
#define	PCCS_LOAD_CONFIG_CACHEOK	0x0001
struct pccs_config_funcs {
	int (*pccs_prefer_config) __P((struct slot_softc *, struct card_prefer_config *));
	int (*pccs_load_config) __P((struct slot_softc *, u_long, int));
	int (*pccs_parse_config) __P((struct slot_softc *, struct card_info *, int));
	int (*pccs_setup_config) __P((struct slot_softc *, struct card_info *));
	int (*pccs_clear_config) __P((struct slot_softc *, struct card_info *));
	int (*pccs_connect_bus) \
		__P((struct slot_softc *, struct pcdv_attach_args *));
	int (*pccs_attach_card) \
		__P((struct slot_softc *, int));
	int (*pccs_config_intr_ack) __P((struct slot_softc *, struct card_info *));

	/* XXX: should die! */
	int (*pccs_space_uio) __P((struct slot_softc *, struct uio *));
};

#define	pccs_prefer_config(ssc, cpc) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_prefer_config) ((ssc), (cpc)))
#define	pccs_load_config(ssc, offs, flags) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_load_config) ((ssc), (offs), (flags)))
#define	pccs_parse_config(ssc, ci, flags) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_parse_config) ((ssc), (ci), (flags)))
#define	pccs_setup_config(ssc, ci) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_setup_config) ((ssc), (ci)))
#define	pccs_clear_config(ssc, ci) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_clear_config) ((ssc), (ci)))
#define	pccs_connect_bus(ssc, ca) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_connect_bus) ((ssc), (ca)))
#define	pccs_attach_card(ssc, mfcid) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_attach_card) ((ssc), (mfcid)))
#define	pccs_config_intr_ack(ssc, ca) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_intr_ack) ((ssc), (ca)))
#define	pccs_space_uio(ssc, uio) \
	((*PCCSHW_TAG((ssc))->pp_pccs->pccs_space_uio) ((ssc), (uio)))
	
/***********************************************************
 * funcs proto
 ***********************************************************/
/* pccs.c */
extern u_int pccs_show_info;

struct card_info *pccs_init_ci __P((struct card_info *));
int pccsintr __P((void *));
void pccs_init_iomem __P((struct slot_device_iomem *));
void pccs_initres __P((slot_device_res_t));
void show_card_info __P((struct card_info *));
int pccs_alloc_si __P((struct slot_softc *, int));

/* power control */	/* pccs.c */
int pccss_shared_pin __P((struct slot_softc *, struct card_info *));

/* misc util */	/* pccs.c */
void pccs_wait __P((u_int));

/* cis exported function (only used in card.c) */	/* pccscis.c */
#define	PCCS_FOLLOW_LINK	0
#define	PCCS_NOT_FOLLOW_LINK	1

#ifdef	_KERNEL
typedef struct slot_softc *pccs_cis_tag_t;
int pccscis_find_config __P((struct slot_softc *, struct card_info *, u_int32_t));
u_int8_t *pccscis_cis_tget __P((slot_device_handle_t, u_int));
#else	/* !_KERNEL */
typedef u_int8_t *pccs_cis_tag_t;
int pccscis_merge_config __P((struct card_info *, struct card_info *, int));
int pccscis_match_config __P((struct card_info *, struct card_info *));
#endif	/* !_KERNEL */
int pccscis_parse_cis __P((pccs_cis_tag_t, struct card_info *, int));

/* pccs pisa interface funcs */
int cis_patch_scan __P((struct slot_softc *));
int pccs16_attach_card __P((struct slot_softc *, int));

#ifdef	_KERNEL
extern struct pccs_config_funcs pccs_config_16bits_funcs;
int pccs_activate __P((void *));
int pccs_deactivate __P((void *));

#define	splpccs splimp
#define	IPL_PCCS IPL_IMP
#endif	/* _KERNEL */
#endif	/* !_PCCSVAR_H_ */
