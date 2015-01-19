/*	$NecBSD: pccs.c,v 1.39.2.5 1999/09/18 11:17:19 honda Exp $	*/
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

#ifndef	CARDINFO
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/select.h>
#include <sys/signalvar.h>
#include <sys/kthread.h>

#include <vm/vm.h>

#include <dev/cons.h>

#include <machine/slot_device.h>
#include <machine/systmbusvar.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

#include "locators.h"

/********************************************
 * DEVICE ATTACH STRUCTURES
 *******************************************/
#define	PCCS_POLL_INTERVAL	(hz)	/* each 1 sec */
#define	PCCSUNIT(unit)	((unit) & 0x0f)

static int pccsmatch __P((struct device *, struct cfdata *, void *));
static void pccsattach __P((struct device *, struct device *, void *));
static int slotmatch __P((struct device *, struct cfdata *, void *));
static void slotattach __P((struct device *, struct device *, void *));
static int pccsprint __P((void *, const char*));

struct cfattach pccs_ca = {
	sizeof(struct pccs_softc), pccsmatch, pccsattach
};

extern struct cfdriver pccs_cd;

struct cfattach slot_ca = {
	sizeof(struct slot_softc), slotmatch, slotattach
};

extern struct cfdriver slot_cd;
int pccs_kthread_count;

int pccsopen __P((dev_t, int, int, struct proc *));
int pccsclose __P((dev_t, int, int, struct proc *));
int pccsread __P((dev_t, struct uio *, int));
int pccswrite __P((dev_t, struct uio *, int));
int pccsioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
void pccs_shutdown __P((void));
static void pccs_kthread_create __P((void *));
static void pccs_post_sig __P((struct slot_softc *, int));
static int pccss_check_state __P((struct slot_softc *));
static int pccss_slotctrl __P((struct slot_softc *, int, struct proc *));
static int pccs_systmmsg __P((struct device *, systm_event_t));
static void pccs_kthread __P((void *));
static int pccss_slot_init __P((struct slot_softc *));
static __inline void pccs_unlock_slot __P((struct slot_softc *));
static __inline int pccs_lock_slot __P((struct slot_softc *));
static void pccs_deallocate_devices __P((struct slot_softc *));
static __inline struct slot_func_info *pccs_get_si __P((struct slot_softc *, struct card_info *));
static int pccs_notify __P((struct slot_softc *, slot_device_event_t));
static void pccs_attach_card_all __P((struct slot_softc *));

/*********************************************************
 * service functions
 *********************************************************/
int pccs_open __P((slot_device_handle_t));
int pccs_close __P((slot_device_handle_t));
int pccs_im_map __P((slot_device_handle_t, u_int));
int pccs_im_unmap __P((slot_device_handle_t, u_int));
int pccs_intr_map __P((slot_device_handle_t, u_int));
int pccs_intr_unmap __P((slot_device_handle_t, u_int));
int pccs_intr_ack __P((void *));
int pccs_dma_map __P((slot_device_handle_t, u_int));
int pccs_dma_unmap __P((slot_device_handle_t, u_int));
#define	PCCS_RES_DR(dh) (&(((struct card_info *)dh->dh_ci)->ci_res))

struct slot_device_service_functions pccs_service_functions = {
	pccs_open,
	pccs_close,
	pccs_im_map,
	pccs_im_unmap,
	pccs_intr_map,
	pccs_intr_unmap,
	pccs_intr_ack,
	pccs_dma_map,
	pccs_dma_unmap,
	pccscis_cis_tget
};
	
static __inline struct slot_func_info *
pccs_get_si(ssc, ci)
	struct slot_softc *ssc;
	struct card_info *ci;
{
	int mfcid;

	mfcid = PCCS_GET_MFCID(ci);
	return ssc->sc_si[mfcid];
}

int
pccs_open(dh)
	slot_device_handle_t dh;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	struct slot_softc *ssc = (struct slot_softc *) st->st_sc;
	struct card_info *ci = dh->dh_ci;
	struct slot_func_info *si;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	slot_handle_t sid = ssc->sc_id;
	int i;

#if	0
	if (ssc->sc_state < SLOT_READY)
	{
		printf("%s: open: slot not ready\n", ssc->sc_dev.dv_xname);
		return EINVAL;
	}
#endif

	si = pccs_get_si(ssc, ci);
	if (si->si_flags & SLOTF_OPEN)
	{
		printf("%s: function already opened\n", ssc->sc_dev.dv_xname);
		return 0;
	}

	si->si_flags |= SLOTF_OPEN;
	ssc->sc_open ++;
	if (ssc->sc_open == 1)
	{
		pccshw_power(pp, sid, &ssc->sc_mci->ci_pow, PCCSHW_POWER_ON);
		pccshw_reset(pp, sid, ci->ci_tpce.if_ifm);
	}

	for (i = 0; i < SLOT_DEVICE_NIM; i ++)
		si->si_wh[i] = SLOTF_UNKWIN;

	/* open configuration space registers */
	pccs_setup_config(ssc, ci);
	return 0;
}

int
pccs_close(dh)
	slot_device_handle_t dh;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	struct slot_softc *ssc = (struct slot_softc *) st->st_sc;
	struct card_info *ci = dh->dh_ci;
	struct slot_func_info *si;

	si = pccs_get_si(ssc, ci);
	if ((si->si_flags & SLOTF_OPEN) == 0)
	{
		printf("%s: function already closed\n", ssc->sc_dev.dv_xname);
		return 0;
	}
	si->si_flags &= ~SLOTF_OPEN;

	pccss_check_state(ssc);
	if (ssc->sc_state >= SLOT_READY)
		pccs_clear_config(ssc, ci);

	if ((-- ssc->sc_open) == 0)
	{
		pccss_check_state(ssc);
		pccshw_power(PCCSHW_TAG(ssc), ssc->sc_id, NULL, PCCSHW_POWER_OFF);
		if (ssc->sc_state > SLOT_CARDIN)
			ssc->sc_state = SLOT_CARDIN;
	}

	return 0;
}

int
pccs_im_map(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	struct slot_softc *ssc = (struct slot_softc *) st->st_sc;
	struct slot_device_iomem *wp;
	struct slot_func_info *si;
	struct card_info *ci = dh->dh_ci;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	window_handle_t wh;
	int error;

	si = pccs_get_si(ssc, ci);
	if (wn >= pp->pp_nim || (si->si_flags & SLOTF_OPEN) == 0)
		return EINVAL;

	wp = &ci->ci_res.dr_im[wn];
	error = pccshw_map(pp, ssc->sc_id, wp, &wh, PCCSHW_CHKRANGE);
	if (error == 0)
	{
		si->si_wh[wn] = wh;
		ssc->sc_mci->ci_res.dr_im[wh] = *wp;
		if (ssc->sc_state < SLOT_MAPPED)
			ssc->sc_state = SLOT_MAPPED;
		if (si->si_dh != NULL)
			ssc->sc_state = SLOT_HASDEV;
	}

	return error;
}

int
pccs_im_unmap(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	struct slot_softc *ssc = (struct slot_softc *) st->st_sc;
	struct slot_func_info *si;
	struct card_info *ci = dh->dh_ci;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	window_handle_t wh;

	si = pccs_get_si(ssc, ci);
	if (wn >= pp->pp_nim || (si->si_flags & SLOTF_OPEN) == 0)
		return EINVAL;

	wh = si->si_wh[wn];
	si->si_wh[wn] = SLOTF_UNKWIN;

	if (wh == SLOTF_UNKWIN)
	{
		printf("%s: window already unmapped\n", ssc->sc_dev.dv_xname);
		return EINVAL;
	}
	pccshw_unmap(pp, ssc->sc_id, wh);
	return 0;
}

int
pccs_intr_map(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	struct slot_softc *ssc = (struct slot_softc *) st->st_sc;
	struct slot_func_info *si;
	struct card_info *ci = dh->dh_ci;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);

	si = pccs_get_si(ssc, ci);
	if (wn > 0 || (si->si_flags & (SLOTF_OPEN | SLOTF_INTR)) != SLOTF_OPEN)
		return EINVAL;
	si->si_flags |= SLOTF_INTR;

	pccshw_routeirq(pp, ssc->sc_id, &ci->ci_res.dr_pin[wn]);
	ssc->sc_mci->ci_res.dr_pin[wn] = ci->ci_res.dr_pin[wn];
	ssc->sc_intr ++;
	return 0;
}

int
pccs_intr_unmap(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	struct slot_softc *ssc = (struct slot_softc *) st->st_sc;
	struct slot_func_info *si;
	struct card_info *ci = dh->dh_ci;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);

	si = pccs_get_si(ssc, ci);
	if (wn > 0 || (si->si_flags & (SLOTF_OPEN | SLOTF_INTR)) != 
				      (SLOTF_OPEN | SLOTF_INTR))
		return EINVAL;

	si->si_flags &= ~SLOTF_INTR;
	if ((-- ssc->sc_intr) <= 0) 
	{
#ifdef	DIAGNOSTIC
		if (ssc->sc_intr < 0)
		{
			printf("%s: negative intr\n", ssc->sc_dev.dv_xname);
			ssc->sc_intr = 0;
		}
#endif	/* DIAGNOSTIC */

		pccshw_routeirq(pp, ssc->sc_id, NULL);
		ssc->sc_mci->ci_res.dr_pin[0].dc_chan = SLOT_DEVICE_UNKVAL;
	}
	return 0;
}

int
pccs_intr_ack(arg)
	void *arg;
{
	slot_device_intr_arg_t sa = arg;
	slot_device_handle_t dh = sa->sa_dh;
	slot_device_slot_tag_t st;
	struct slot_softc *ssc;

	if ((st = dh->dh_stag) == NULL)
		return 0;

 	ssc = (struct slot_softc *) st->st_sc;
	if (pccshw_stat(PCCSHW_TAG(ssc), ssc->sc_id, PCCSHW_CARDIN) == 0)
		return 0;

#ifdef	PCCS_CHECK_AND_CLEAR_CONFIG_SPACE_INTRREG
	if (pccs_config_intr_ack(ssc, dh->dh_ci) != 0)
		return 0;
#endif	/* PCCS_CHECK_AND_CLEAR_CONFIG_SPACE_INTRREG */

	return ((*sa->sa_handler) (sa->sa_arg));
}

int
pccs_dma_map(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{

	/* dma open */
	return EOPNOTSUPP;
}

int
pccs_dma_unmap(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{

	/* dma close */
	return EOPNOTSUPP;
}

u_int pccs_show_info;
/*********************************************************
 * slot attach, probe
 *********************************************************/
static int slotmatchsubr __P((pccshw_tag_t, slot_handle_t));
static slot_device_slot_tag_t slotattachsubr __P((struct slot_softc *, pccshw_tag_t));

struct slot_attach_args {
	slot_handle_t sa_id;
	struct pccs_attach_args *sa_pa;
};

static int
slotmatchsubr(pp, id)
	pccshw_tag_t pp;
	slot_handle_t id;
{

	if (pp != NULL && id < pp->pp_maxslots &&
	    pccshw_stat(pp, id, PCCSHW_HASSLOT) != 0)
		return 1;
	return 0;
}

static int
slotmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct slot_attach_args *sa = aux;
	struct pccs_attach_args *pa = sa->sa_pa;
	struct cfdata *cf = match;

	if (cf->cf_loc[PCCSCF_DRIVE] != PCCSCF_DRIVE_DEFAULT &&
	    cf->cf_loc[PCCSCF_DRIVE] != sa->sa_id)
		return 0;

	if (slotmatchsubr(pa->pa_16pp, sa->sa_id) != 0)
		return 1;
	if (slotmatchsubr(pa->pa_32pp, sa->sa_id) != 0)
		return 1;
	return 0;
}

static slot_device_slot_tag_t
slotattachsubr(ssc, pp)
	struct slot_softc *ssc;
	pccshw_tag_t pp;
{
	struct slot_device_attach_bus_args sda;
	slot_device_slot_tag_t st = NULL;
	void *bus;

	if (pp == NULL)
		return st;

	sda.sda_sb = pp->pp_sb;
	sda.sda_iot = pp->pp_iot;
	sda.sda_memt = pp->pp_memt;
	sda.sda_dmat = pp->pp_dmat;
	sda.sda_bc = pp->pp_bc;
	bus = config_found((void *) ssc, &sda, NULL);
	if (bus == NULL)
	{
		printf("%s: no bus found\n", ssc->sc_dev.dv_xname);
	}
	else
	{
		st = slot_device_slot_tag_allocate(pp->pp_sb,
			ssc, pp, bus, pp->pp_type);
		if (st == NULL)
		{
			panic("%s: null slot tag",
			      ssc->sc_dev.dv_xname);
		}
		st->st_funcs = &pccs_service_functions;
		st->st_flags = SD_STAG_INTRACK;
	}
	return st;
}

static void
pccs_attach_card_all(ssc)
	struct slot_softc *ssc;
{
	int mfcid;

	if (ssc->sc_state == SLOT_NULL)
		return;

	for (mfcid = 0; mfcid < PCCS_MAXMFC; mfcid ++)
		pccs_attach_card(ssc, mfcid);
}

static void
slotattach(parent, match, aux)
	struct device *parent, *match;
	void *aux;
{
	struct pccs_softc *sc = (struct pccs_softc *) parent;
	struct slot_softc *ssc = (struct slot_softc *) match;
	struct slot_attach_args *sa = aux;
	struct pccs_attach_args *pa = sa->sa_pa;
	struct card_info *ci;
	pccshw_tag_t pp;
	u_int8_t *bp;

	lockinit(&ssc->sc_lock, PLOCK | PCATCH, "slotlk", 0, 0);
	TAILQ_INSERT_TAIL(&sc->sc_ssctab, ssc, sc_chain);
	systmmsg_bind(match, pccs_systmmsg);

	ssc->sc_id = sa->sa_id;
	if (pa->pa_16pp != NULL)
		pccshw_reclaim(pa->pa_16pp, ssc->sc_id);
	if (pa->pa_32pp != NULL)
		pccshw_reclaim(pa->pa_32pp, ssc->sc_id);

	printf("\n");

	/* allocate data memories */
	ssc->sc_atbsz = CARD_ATTR_SZ;
	bp = malloc(ssc->sc_atbsz, M_DEVBUF, M_NOWAIT);
	ci = malloc(sizeof(*ci), M_DEVBUF, M_NOWAIT);
	if (bp == NULL || ci == NULL)
		panic("%s: no memories", ssc->sc_dev.dv_xname);

	ssc->sc_atbp = bp;
	ssc->sc_mci = ci;
	fillw((u_short) -1, bp, ssc->sc_atbsz / sizeof(u_short));
	pccs_init_ci(ci);

	/* inform 16/32 service supports */
	printf("%s: support services: ", ssc->sc_dev.dv_xname);
	if (pa->pa_16pp != NULL)
		printf("pccs16 ");
	if (pa->pa_32pp != NULL)
		printf("pccs32 ");
	printf("\n");

	/* allocate 16 bits bus */
	if (pa->pa_16pp != NULL)
	{
		ssc->sc_16st = slotattachsubr(ssc, pa->pa_16pp);
		if (ssc->sc_16st != NULL)
		{
			pa->pa_16pp->pp_pcsc = (void *) parent;

			/* XXX:
			 * Some stupid machines made strange settings,
		 	 * thus, clean up all settings here!
			 */
			pp = pa->pa_16pp;
			pccshw_reclaim(pp, ssc->sc_id);
			pccshw_power(pp, ssc->sc_id, NULL, PCCSHW_POWER_OFF);
			pccshw_auxcmd(pp, ssc->sc_id, PCCSHW_CLRINTR);
		}
	}

	/* allocate 32 bits bus */
	if (pa->pa_32pp != NULL)
	{
		ssc->sc_32st = slotattachsubr(ssc, pa->pa_32pp);
		if (ssc->sc_32st != NULL)
		{
			pa->pa_32pp->pp_pcsc = (void *) parent;

			pp = pa->pa_32pp;
			pccshw_reclaim(pp, ssc->sc_id);
			pccshw_power(pp, ssc->sc_id, NULL, PCCSHW_POWER_OFF);
			pccshw_auxcmd(pp, ssc->sc_id, PCCSHW_CLRINTR);
		}
	}
	delay(1000);

	ssc->sc_enabled = 1;
	ssc->sc_hwflags |= HW_AUTORES | HW_AUTOINS;
	PCCS_SEV_WRITE(ssc, PCCS_SEV_MRESUME);
	pccss_slot_init(ssc);
	if (ssc->sc_state == SLOT_NULL)
		return;

	pccs_attach_card_all(ssc);
}

/*************************************************
 * slot lock
 *************************************************/
static __inline int
pccs_lock_slot(ssc)
	struct slot_softc *ssc;
{

	return lockmgr(&ssc->sc_lock, LK_EXCLUSIVE, NULL);
}

static __inline void
pccs_unlock_slot(ssc)
	struct slot_softc *ssc;
{

	lockmgr(&ssc->sc_lock, LK_RELEASE, NULL);
}

#endif	/* !CARDINFO */

/********************************************
 * data init
 *******************************************/
void
pccs_init_iomem(wp)
	struct slot_device_iomem *wp;
{

	wp->im_size = SLOT_DEVICE_UNKVAL;
	wp->im_hwbase = SLOT_DEVICE_UNKVAL;
	wp->im_base = SLOT_DEVICE_UNKVAL;
	wp->im_flags = 0;
	wp->im_type = 0;
}

void
pccs_initres(dr)
	slot_device_res_t dr;
{
	register int i;

	for (i = 0; i < dr->dr_nim; i++)
		pccs_init_iomem(&dr->dr_im[i]);
	dr->dr_pin[0].dc_chan = SLOT_DEVICE_UNKVAL;
}

struct card_info *
pccs_init_ci(ci)
	struct card_info *ci;
{
	slot_device_res_t dr = &ci->ci_res;

	memset(ci, 0, sizeof(*ci));
	ci->ci_iobits = DEFAULT_IOBITS;
	ci->ci_delay = 100;
	dr->dr_nio = SLOT_DEVICE_NIO;
	dr->dr_nmem = SLOT_DEVICE_NMEM;
	dr->dr_nim = dr->dr_nio + dr->dr_nmem;
	dr->dr_npin = 1;
	dr->dr_ndrq = 0;

	pccs_initres(dr);
	dr->dr_pin[0].dc_aux = -1;
	ci->ci_pow.pow_vcc0 = PCCS_POWER_DEFAULT;
	ci->ci_pow.pow_vcc1 = PCCS_POWER_NONE;
	ci->ci_pow.pow_vpp0 = PCCS_POWER_DEFAULT;
	ci->ci_pow.pow_vpp1 = PCCS_POWER_NONE;

	PCCS_SET_INDEX(ci, PUNKINDEX);
	return ci;
}

#ifndef	CARDINFO
void
pccs_wait(waits)
	u_int waits;
{
	static int pccs_sleep;
	extern int cold;

	if (cold == 0)
	{
		waits = waits * hz / 1000 + 1;

		(void) tsleep((caddr_t) &pccs_sleep, PZERO, "card_act",
			       waits);
	}
	else
		delay(waits * 1000);
}

int
pccs_alloc_si(ssc, mfcid)
	struct slot_softc *ssc;
	int mfcid;
{
	struct slot_func_info *si;

	if (ssc->sc_si[mfcid] != NULL)
		return 0;

	si = malloc(sizeof(*si), M_DEVBUF, M_NOWAIT);
	if (si == NULL)
		return ENOMEM;

	memset(si, 0, sizeof(*si));
	ssc->sc_si[mfcid] = si;
	return 0;
}	
/************************************************************
 * ioctl internal functions
 ************************************************************/
int
pccss_check_state(ssc)
	struct slot_softc *ssc;
{
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	slot_handle_t id = ssc->sc_id;
	int s;
	
	s = splpccs();
	if (pccshw_stat(pp, id, PCCSHW_CARDIN))
	{
		if (ssc->sc_state < SLOT_CARDIN)
		{
			ssc->sc_state = SLOT_CARDIN;
			ssc->sc_hwflags |= HW_INSERT;
		}
	}
	else 
	{
		if (ssc->sc_state > SLOT_NULL)
		{
			ssc->sc_hwflags |= HW_REMOVE;
			ssc->sc_hwflags &= ~(HW_INSERT | HW_PENDING);
			ssc->sc_state = SLOT_NULL;
		}
	}
	splx(s);
	return 0;
}

/* reset card and init data */
static int
pccss_slot_init(ssc)
	struct slot_softc *ssc;
{
	struct card_info *ci = ssc->sc_mci;
	slot_handle_t sid = ssc->sc_id;
	pccshw_tag_t pp = NULL;
	int error = 0;

	if (ssc->sc_state >= SLOT_HASDEV)
		return EBUSY;
	if (ssc->sc_hwflags & HW_DOWN)
		return EINVAL;

	/* check 16 bits interface */
	ssc->sc_state = SLOT_NULL;
	if (ssc->sc_16st != NULL)
	{
		PCCS_SLOT_TAG(ssc) = ssc->sc_16st;
		pp = PCCSHW_TAG(ssc);
		if (pccshw_stat(pp, sid, PCCSHW_CARDIN))
			ssc->sc_state = SLOT_CARDIN;
		else
			ssc->sc_state = SLOT_NULL;
	}

	/* check 32 bits interface */
	if (ssc->sc_state == SLOT_NULL && ssc->sc_32st != NULL)
	{
		PCCS_SLOT_TAG(ssc) = ssc->sc_32st;
		pp = PCCSHW_TAG(ssc);
		if (pccshw_stat(pp, sid, PCCSHW_CARDIN))
			ssc->sc_state = SLOT_CARDIN;
		else
			ssc->sc_state = SLOT_NULL;
	}

	if (ssc->sc_state == SLOT_NULL)
		return EINVAL;

	pccs_init_ci(ci);
	pccshw_power(pp, sid, &ci->ci_pow, PCCSHW_POWER_ON);
	pccshw_reset(pp, sid, IF_MEMORY);
	pccshw_reclaim(pp, sid);		/* all mapping clear */

	error = pccs_load_config(ssc, CIS_ATTR_ADDR(0), 0);
	if (error != 0)
		return error;	
	(void) pccs_parse_config(ssc, ci, PCCS_FOLLOW_LINK);

	/* again loading here (patched data now) */
	error = pccs_load_config(ssc, CIS_ATTR_ADDR(0), 0);
	pccs_init_ci(ci);
	if (error != 0)
		return error;	
	(void) pccs_parse_config(ssc, ci, PCCS_FOLLOW_LINK);

	pccs_initres(&ci->ci_res);
	ssc->sc_state = SLOT_READY;
	ssc->sc_intr = ssc->sc_open = 0;
	return error;
}

/* allocate shared interrupt pin */
int
pccss_shared_pin(ssc, ci)
	struct slot_softc *ssc;
	struct card_info *ci;
{
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	struct card_info *kci = ssc->sc_mci;
	int error = 0;

	ci->ci_res.dr_pin[1].dc_chan = SLOT_DEVICE_UNKVAL;

	if (kci->ci_res.dr_pin[0].dc_chan != SLOT_DEVICE_UNKVAL)
	{
		if (ci->ci_res.dr_pin[0].dc_chan != SLOT_DEVICE_UNKVAL)
			ci->ci_res.dr_pin[0] = kci->ci_res.dr_pin[0];
	}	
	else 
	{
		error = pccshw_chkirq(pp, ssc->sc_id, pp->pp_bc,
					&ci->ci_res.dr_pin[0]);
		if (error != 0)
			return error;
	}
	return error;
}

/* device attach */
static void
pccs_post_sig(ssc, event)
	struct slot_softc *ssc;
	int event;
{

	if (ssc->sc_proc == NULL)
		return;

	if (ssc->sc_proc != pfind(ssc->sc_pid))
	{
		ssc->sc_proc = NULL;
		return;
	}

	psignal(ssc->sc_proc, SIGUSR1);
	PCCS_SEV_WRITE(ssc, event);
}

static int
pccss_slotctrl(ssc, req, p)
	struct slot_softc *ssc;
	int req;
	struct proc *p;
{
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	int error = 0, s;

	switch (req & PCCS_CTRLCMD_MASK)
	{
	case PCCS_SLOT_DOWN:
	case PCCS_SLOT_UP:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return error;

		s = splpccs();
		if (req == PCCS_SLOT_DOWN)
		{
			if (ssc->sc_hwflags & HW_DOWN)
			{
				splx(s);
				return error;
			}
			ssc->sc_hwflags |= (HW_DOWN | HW_REMOVE);
			wakeup(&pccs_kthread_count);
		}
		else if (req == PCCS_SLOT_UP)
		{
			if ((ssc->sc_hwflags & HW_DOWN) == 0)
			{
				splx(s);
				return error;
			}
			ssc->sc_hwflags &= ~HW_DOWN;
			ssc->sc_hwflags |= HW_INSERT;
			wakeup(&pccs_kthread_count);
		}
		else
		{
			error = EINVAL;
		}
		splx(s);
		break;

	case PCCS_SPKR_DOWN:
	case PCCS_SPKR_UP:
		if (req == PCCS_SPKR_DOWN)
		{
			ssc->sc_hwflags &= ~HW_SPKRUP;
			pccshw_auxcmd(pp, ssc->sc_id, PCCSHW_SPKROFF);
		}
		else if (req == PCCS_SPKR_UP)
		{
			ssc->sc_hwflags |= HW_SPKRUP;
			pccshw_auxcmd(pp, ssc->sc_id, PCCSHW_SPKRON);
		}
		else
		{
			error = EINVAL;
		}
		break;

	case PCCS_AUTO_RESUME_DOWN:
	case PCCS_AUTO_RESUME_UP:
		if (req == PCCS_AUTO_RESUME_DOWN)
		{
			ssc->sc_hwflags &= ~HW_AUTORES;
		}
		else if (req == PCCS_AUTO_RESUME_UP)
		{
			ssc->sc_hwflags |= HW_AUTORES;
		}
		else
		{
			error = EINVAL;
		}
		break;

	case PCCS_AUTO_INSERT_DOWN:
	case PCCS_AUTO_INSERT_UP:
		if (req == PCCS_AUTO_INSERT_DOWN)
		{
			ssc->sc_hwflags &= ~HW_AUTOINS;
		}
		else if (req == PCCS_AUTO_INSERT_UP)
		{
			ssc->sc_hwflags |= HW_AUTOINS;
		}
		else
		{
			error = EINVAL;
		}
		break;

	case PCCS_HW_TIMING:
		pccshw_auxcmd(pp, ssc->sc_id,
			      PCCSHW_TIMING | (req & PCCS_HW_TIMING_MASK));
		break;

	default:
		error = ENOTTY;
		break;
	}

	return error;
}

/****************************************************
 * NetBSD Kernel Upper Interface
 ****************************************************/
#define	PCCS_MAXSLOTS	4

static int
pccsmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{

	return 1;
}

static int
pccsprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct slot_attach_args *sa = aux;

	printf(" drive %d", sa->sa_id);
	return (UNCONF);
}

static void
pccsattach(parent, match, aux)
	struct device *parent, *match;
	void *aux;
{
	struct pccs_softc *sc = (struct pccs_softc *) match;
	struct pccs_attach_args *pa = aux;
	struct slot_attach_args sa;
	register struct cfdata *m;
	int nslots = 0;

	printf("\n");
	lockinit(&sc->sc_lock, PRIBIO, "pccslk", 0, 0);
	TAILQ_INIT(&sc->sc_ssctab);

	sa.sa_pa = pa;
	for (sa.sa_id = 0; sa.sa_id < PCCS_MAXSLOTS; sa.sa_id ++)
	{
		if ((m = config_search(slotmatch, match, &sa)) != NULL)
		{
			nslots ++;
			config_attach(match, m, &sa, pccsprint);
		}
	}

	/* start polling watch dog */
	if (nslots > 0)
	{
		kthread_create_deferred(pccs_kthread_create, sc);
	}
}

static void 
pccs_kthread_create(arg)
	void *arg;
{

	(void) kthread_create(pccs_kthread, arg, NULL, "pccs_kthread");
}	

int
pccsopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = PCCSUNIT(dev);
	struct slot_softc *ssc;
	int error = 0;

	if (unit >= slot_cd.cd_ndevs)
		return ENXIO;
	if ((ssc = slot_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	error = pccs_lock_slot(ssc);
	if (error != 0)
		return error;
	pccss_check_state(ssc);
	pccs_unlock_slot(ssc);
	return error;
}

int
pccsclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int error, unit = PCCSUNIT(dev);
	struct slot_softc *ssc = slot_cd.cd_devs[unit];

	error = pccs_lock_slot(ssc);
	if (error != 0)
		return error;
	pccss_check_state(ssc);
	pccs_unlock_slot(ssc);
	return 0;
}

int
pccsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int error, unit = PCCSUNIT(dev);
	struct slot_softc *ssc = slot_cd.cd_devs[unit];

	if (ssc->sc_state < SLOT_READY)
		return EIO;
	
	error = pccs_lock_slot(ssc);
	if (error != 0)
		return error;
	error = pccs_space_uio(ssc, uio);
	pccs_unlock_slot(ssc);
	return error;
}

int
pccswrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int error, unit = PCCSUNIT(dev);
	struct slot_softc *ssc = slot_cd.cd_devs[unit];
	struct proc *p = uio->uio_procp;

	if (ssc->sc_state < SLOT_READY)
		return EIO;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return error;

	error = pccs_lock_slot(ssc);
	if (error != 0)
		return error;
	error = pccs_space_uio(ssc, uio);
	pccs_unlock_slot(ssc);
	return error;
}

int
pccsioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct slot_softc *ssc = slot_cd.cd_devs[PCCSUNIT(dev)];
	struct pcdv_attach_args *ca;
	struct card_prefer_config *pcp;
	slot_device_handle_t dh;
	struct slot_func_info *sfip;
	struct slot_info *si;
	struct slot_event *sevp;
	int s, error, size, mfcid;

	error = pccs_lock_slot(ssc);
	if (error)
		return error;

	pccss_check_state(ssc);

	switch (cmd)
	{
	case PCCS_IOC_INIT:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			goto done;

		if ((ssc->sc_hwflags & HW_DOWN) == 0)
			error = pccss_slot_init(ssc);
		else
			error = EIO;
		goto done;

	case PCCS_IOC_CONNECT:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			goto done;

		if (ssc->sc_proc && pfind(ssc->sc_pid) != ssc->sc_proc)
			ssc->sc_proc = NULL;

		if (ssc->sc_proc && ssc->sc_proc != p)
		{
			error = EBUSY;
			goto done;
		}

		ssc->sc_proc = p;
		ssc->sc_pid = p->p_pid;
		goto done;

	case PCCS_IOG_SSTAT:
		si = (struct slot_info *) data;

		si->si_st = ssc->sc_state;

		si->si_cst = 0;
		if ((ssc->sc_hwflags & HW_DOWN) == 0)
			si->si_cst |= PCCS_SS_POW_UP;
		if (ssc->sc_hwflags & HW_SPKRUP) 
			si->si_cst |= PCCS_SS_SPKR_UP;
		if (ssc->sc_hwflags & HW_AUTORES)
			si->si_cst |= PCCS_SS_AUTORES_UP;
		if (ssc->sc_hwflags & HW_AUTOINS)
			si->si_cst |= PCCS_SS_AUTOINS_UP;

		si->si_ci = *ssc->sc_mci;

		if (PCCS_SLOT_TAG(ssc) == ssc->sc_16st)
			si->si_buswidth = PCCS_BUSWIDTH_16;
		else if (PCCS_SLOT_TAG(ssc) == ssc->sc_32st)
			si->si_buswidth = PCCS_BUSWIDTH_32;
		else
			si->si_buswidth = PCCS_BUSWIDTH_NONE;

		pccs_init_ci(&si->si_fci);
		si->si_fst = 0;
		mfcid = si->si_mfcid;
		if (mfcid < 0 || mfcid >= PCCS_MAXMFC)
			goto done;
		if ((sfip = ssc->sc_si[mfcid]) == NULL)
			goto done;
		
		s = splpccs();				/* XXX */
		dh = sfip->si_dh;
		if (dh != NULL && dh->dh_ci != NULL)
			si->si_fci = *((struct card_info *) dh->dh_ci);
		splx(s);				/* XXX */
		si->si_fst = sfip->si_flags;
		bcopy(sfip->si_xname, si->si_xname, PCCS_NAMELEN);
		goto done;

	case PCCS_IOC_CTRL:
		error = pccss_slotctrl(ssc, *(int *) data, p);
		goto done;

	case PCCS_IOG_EVENT:
		if ((ssc->sc_proc && pfind(ssc->sc_pid) != ssc->sc_proc) ||
		    (ssc->sc_proc != p))
		{
			error = EINVAL;
			goto done;
		}
	
		sevp = (struct slot_event *) data;
		sevp->sev_code = PCCS_SEV_NULL;
		sevp->sev_time = time.tv_sec;
		if (PCCS_SEV_EVENT(ssc) != 0)
		{
			sevp->sev_code = PCCS_SEV_READ(ssc);
		}
		goto done;
	}

	/**************************************
	 * cmd only in the case of slot active.
	 **************************************/
	if ((ssc->sc_hwflags & HW_DOWN) || ssc->sc_state < SLOT_READY)
	{
		error = EIO;
		goto done;
	}

	switch (cmd)
	{
	case PCCS_IOG_ATTR:
		pccs_load_config(ssc, CIS_ATTR_ADDR(0), PCCS_LOAD_CONFIG_CACHEOK);
		size = ssc->sc_atbsz;
		if (size > CARD_ATTR_SZ)
			size = CARD_ATTR_SZ;
		bcopy(ssc->sc_atbp, (u_char *) data, size);
		goto done;

	default:
		break;
	}

	/*************************************
	 * cmd only for super user cred.
	 *************************************/
	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		goto done;

	switch (cmd)
	{
	case PCCS_IOC_PREFER:
		pcp = (struct card_prefer_config *) data;
		error = pccs_prefer_config(ssc, pcp);
		break;

	case PCCS_IOC_CONNECT_BUS:
		ca = (struct pcdv_attach_args *) data;
		if (ca->ca_version != PCCSIO_VERSION)
		{
			printf("WARNING: pcsd version mismatch\n");
			printf("WARNING: please update pcsd daemon\n");
			error = EINVAL;
		}
		else
		{
			error = pccs_connect_bus(ssc, ca);
		}
		break;

	default:
		error = ENOTTY;
	}

done:
	pccs_unlock_slot(ssc);
	return error;
}

static void
pccs_kthread(arg)
	void *arg;
{
	struct pccs_softc *sc = arg;
	register struct slot_softc *ssc;
	int bellon, s, error;

	for (;;)
   	{
		bellon = 0;
		ssc = sc->sc_ssctab.tqh_first;
		for ( ; ssc != NULL; ssc = ssc->sc_chain.tqe_next)
		{
			error = pccs_lock_slot(ssc);
			if (error != 0)
				continue;

			s = splpccs();
			pccss_check_state(ssc);
			if ((ssc->sc_hwflags & HW_EVENT) == 0)
				goto out;

			if (ssc->sc_hwflags & HW_REMOVE)
			{
				pccs_deallocate_devices(ssc) ;
				if ((ssc->sc_hwflags & HW_REMOVE) == 0)
				{
					pccs_post_sig(ssc, PCCS_SEV_HWREMOVE);
					bellon = 1;
				}
			}
			else if (ssc->sc_hwflags & HW_INSERT)
			{
				if (ssc->sc_enabled == 0 ||
				    (ssc->sc_hwflags & HW_DOWN) != 0)
				{
					ssc->sc_hwflags &= ~HW_PENDING;
					goto out;
				}

				if ((ssc->sc_hwflags & HW_PENDING) == 0)
				{
					ssc->sc_hwflags |= HW_PENDING;
					goto out;
				}

				ssc->sc_hwflags &= ~HW_EVENT;
				if ((ssc->sc_hwflags & HW_AUTOINS) != 0)
				{
					error = pccss_slot_init(ssc);
					if (error == 0)
						pccs_attach_card_all(ssc);
				}
				bellon = 1;
				pccs_post_sig(ssc, PCCS_SEV_HWINSERT);
			}
out:
			splx(s);
			pccs_unlock_slot(ssc);
		}
	
		if (bellon)
			cnputc(0x07);

		(void) tsleep((caddr_t) &pccs_kthread_count, PZERO + 1,
			      "pccs_kthread", PCCS_POLL_INTERVAL);
	}
}

int
pccsintr(arg)
	void *arg;
{
	struct pccs_softc *sc = arg;
	register struct slot_softc *ssc;
	pccshw_tag_t pp;

	ssc = sc->sc_ssctab.tqh_first;
	for ( ; ssc != NULL; ssc = ssc->sc_chain.tqe_next)
	{
		pp = PCCSHW_TAG(ssc);
		if (!pccshw_stat(pp, ssc->sc_id, PCCSHW_STCHG))
			continue;

		pccss_check_state(ssc);
		if (ssc->sc_hwflags & HW_REMOVE)
		{
			slot_device_handle_t dh;
			slot_device_slot_tag_t st = PCCS_SLOT_TAG(ssc);

			for (dh = st->st_dhtab.tqh_first; dh != NULL;
			     dh = dh->dh_dvgchain.tqe_next)
			{
				if (dh->dh_flags & DH_BUSY)
					continue;
				slot_device_vanish(st, dh, NULL);
			}
		}
	}
	return 1;
}

/********************************************
 * Final shutdown
 ******************************************/
static int
pccs_notify(ssc, ev)
	struct slot_softc *ssc;
	slot_device_event_t ev;
{
	slot_device_slot_tag_t st = PCCS_SLOT_TAG(ssc);
	slot_device_handle_t dh, ndh;
	int error = 0;

	for (dh = st->st_dhtab.tqh_first; dh != NULL; dh = ndh)
	{
		ndh = dh->dh_dvgchain.tqe_next;
		if (dh->dh_flags & DH_BUSY)
		{
			error |= SD_EVENT_STATUS_BUSY;
			continue;
		}
		error |= slot_device_notify(st, dh, ev);
	}
	return error;
}

static void
pccs_deallocate_devices(ssc) 
	struct slot_softc *ssc;
{
	slot_device_slot_tag_t st = PCCS_SLOT_TAG(ssc);
	slot_device_handle_t dh, ndh;
	struct card_info *ci;
	int idx;

	for (dh = st->st_dhtab.tqh_first; dh != NULL; dh = ndh)
	{
		ndh = dh->dh_dvgchain.tqe_next;
		if (dh->dh_flags & DH_BUSY)
			continue;

		slot_device_deactivate(st, dh, NULL);
		if ((ci = dh->dh_ci) == NULL)
			continue;

		idx = PCCS_GET_MFCID(ci);
		ssc->sc_si[idx]->si_dh = NULL;
	}

	if (st->st_dhtab.tqh_first == NULL)
		ssc->sc_hwflags &= ~HW_REMOVE;

	ssc->sc_ndev = 0;
}

int
pccs_activate(arg)
	void *arg;
{
	struct pccs_softc *sc = arg;
	struct slot_softc *ssc;
	int s;

	ssc = sc->sc_ssctab.tqh_first;
	s = splpccs();
	for ( ; ssc != NULL; ssc = ssc->sc_chain.tqe_next)
		pccs_systmmsg((struct device *) ssc, SYSTM_EVENT_RESUME);
	splx(s);

	return 0;
}
	
int
pccs_deactivate(arg)
	void *arg;
{
	struct pccs_softc *sc = arg;
	struct slot_softc *ssc;
	int s;

	ssc = sc->sc_ssctab.tqh_first;
	s = splpccs();
	for ( ; ssc != NULL; ssc = ssc->sc_chain.tqe_next)
		pccs_systmmsg((struct device *) ssc, SYSTM_EVENT_SUSPEND);
	splx(s);

	return 0;
}

static int
pccs_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct slot_softc *ssc = (struct slot_softc *) dv;
	pccshw_tag_t pp = PCCSHW_TAG(ssc);
	slot_device_event_t sev;
	int error = 0;

	switch (ev)
	{
	case SYSTM_EVENT_SUSPEND:
		if (ssc->sc_enabled == 0)
			return error;

		error = pccs_lock_slot(ssc);
		if (error != 0)
			return error;
		pccs_deallocate_devices(ssc);

		pccshw_power(pp, ssc->sc_id, NULL, PCCSHW_POWER_OFF);
		pccshw_reclaim(pp, ssc->sc_id);
		pccshw_auxcmd(pp, ssc->sc_id, PCCSHW_CLRINTR);

		ssc->sc_hwflags &= ~HW_EVENT;
		ssc->sc_enabled = 0;
		PCCS_SEV_WRITE(ssc, PCCS_SEV_MSUSPEND);
		return error;

	case SYSTM_EVENT_RESUME:
		if (ssc->sc_enabled != 0)
			return 0;

		ssc->sc_enabled = 1;	
		pccs_post_sig(ssc, PCCS_SEV_MRESUME);
		if (ssc->sc_hwflags & HW_AUTORES)
		{
			error = pccss_slot_init(ssc);
			if (error == 0)
				pccs_attach_card_all(ssc);
		}
		pccs_unlock_slot(ssc);
		return error;

	case SYSTM_EVENT_NOTIFY_SUSPEND:
		sev = SD_EVENT_NOTIFY_SUSPEND;
		goto query;

	case SYSTM_EVENT_QUERY_SUSPEND:
		sev = SD_EVENT_QUERY_SUSPEND;
		goto query;

	case SYSTM_EVENT_NOTIFY_RESUME:
		sev = SD_EVENT_NOTIFY_RESUME;
		goto query;

	case SYSTM_EVENT_QUERY_RESUME:
		sev = SD_EVENT_QUERY_RESUME;
query:
		error = pccs_notify(ssc, sev);
		if (error != 0)
			return SYSTM_EVENT_STATUS_BUSY;	/* XXX */
		return 0;

	default:
		return error;
	}
}

void
pccs_shutdown(void)
{
	struct slot_softc *ssc;
	pccshw_tag_t pp;
	int unit;

	for (unit = 0 ; unit < slot_cd.cd_ndevs; ++unit)
	{
		if ((ssc = slot_cd.cd_devs[unit]) != NULL)
		{
			pp = PCCSHW_TAG(ssc);
			pccshw_reclaim(pp, ssc->sc_id);
			pccshw_power(pp, ssc->sc_id, NULL, PCCSHW_POWER_OFF);
			pccs_initres(&ssc->sc_mci->ci_res);
		}
	}
}
#endif	/* !CARDINFO */

/********************************************
 * Debug and Info
 ******************************************/
static void show_win_info __P((int, struct slot_device_iomem *, int));

static void
show_win_info(wno, wp, type)
	int wno;
	struct slot_device_iomem *wp;
	int type;
{
	int flags;
	if ((wp->im_flags & SDIM_BUS_ACTIVE) == 0)
		return;

	flags = wp->im_flags & ~SDIM_BUS_ACTIVE;
	if (type == 0)
	{
		printf("io[%d] port 0x%lx, sz 0x%lx, bus ", 
			wno, (u_long) wp->im_hwbase, (u_long) wp->im_size);
#ifdef _KERNEL
		printf("0x0%b\n", flags, WFIO_FLAGSBITS);
#else
		bits_expand(flags, WFIO_FLAGSBITS);
#endif
	}
	else
	{
		printf("mem[%d] host 0x%lx, offs 0x%lx, sz 0x%lx, bus ",
			wno, (u_long) wp->im_hwbase, (u_long) wp->im_base, 
			(u_long) wp->im_size);
#ifdef _KERNEL
		printf("0x0%b\n", flags, WFMEM_FLAGSBITS);
#else
		bits_expand(flags, WFMEM_FLAGSBITS);
#endif
	}
}

void
show_card_info(ci)
	struct card_info *ci;
{
	int i;
	
	printf("[Configuration Entry] <index 0x%x>\n", 
		(u_int) PCCS_GET_INDEX(ci));
	printf("Intr: type(%s) irq(%2ld)\n",
		ci->ci_res.dr_pin[0].dc_flags ? "level" : "pulse",
		ci->ci_res.dr_pin[0].dc_chan);
	for (i = 0; i < MAX_IOWF; i++)
		show_win_info(i, &ci->ci_res.dr_io[i], 0);
	for (i = 0; i < MAX_MEMWF; i++)
		show_win_info(i, &ci->ci_res.dr_mem[i], 1);
}
