/*	$NecBSD: ippi.c,v 1.44.2.2 1999/08/19 15:47:05 honda Exp $	*/
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
 *      $Id: pnpcfg.c,v 1.6 1996/05/06 02:08:25 smpatel Exp smpatel $
 */

#include "opt_ipp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/systmbusvar.h>
#include <machine/dvcfg.h>

#include <dev/cons.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <i386/Cbus/pnp/pnpreg.h>
#include <i386/Cbus/pnp/ippiio.h>
#include <i386/Cbus/pnp/pnpvar.h>

static int ippimatch __P((struct device *, struct cfdata *, void *));
static void ippiattach __P((struct device *, struct device *, void *));

struct cfattach ippi_ca = {
	sizeof(struct ippi_softc), ippimatch, ippiattach
};

extern struct cfdriver ippi_cd;

int ippi_get_serial __P((struct ippi_softc *sc, struct serial_id *));
int ippi_isolation __P((struct ippi_softc *sc, int));
int ippiopen __P((dev_t, int, int, struct proc *));
int ippiclose __P((dev_t, int, int, struct proc *));
int ippiioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int ippi_chk_resources __P((struct ippi_softc *, slot_device_res_t));
int ippi_scan_board __P((struct ippi_softc *, int));
void ippi_send_initiation_LFSR __P((struct ippi_softc *sc));
void ippi_setup_ib __P((struct ippi_board_info *, u_int, struct serial_id *));
static int ippi_systmmsg __P((struct device *, systm_event_t));
static struct ippi_board_info *ippi_get_ib __P((struct ippi_softc *, struct ippi_ipid *));
static void ippi_read_info __P((struct ippi_softc *, struct ippi_board_info *, struct serial_id *));
void ippi_config_sm __P((struct ippi_softc *));

#define	IPPIUNIT(dev)	((dev) & 0x0f)

/************************************************************
 * port registers
 ************************************************************/
#define	ippi_wdofs	0x800		/* wd port offset */
#define	ippi_rdbase	0x203		/* rd port address */
#define	ippi_rdspan	0x200		/* rd port ragne */
#define	ippi_rdskip	0x40		/* rd port scan gap */

/************************************************************
 * Debug 
 ************************************************************/
#define	IPPI_DEBUG_CALL
/*
 * (fake) suspend resume test!
 */
#ifdef	IPPI_DEBUG_CALL
struct ippi_softc *ippi_gsc;
void ippi_resume_call __P((void));
void ippi_suspend_call __P((void));

void
ippi_suspend_call()
{

	ippi_systmmsg((struct device *) ippi_gsc, SYSTM_EVENT_SUSPEND);
}

void
ippi_resume_call()
{

	ippi_systmmsg((struct device *) ippi_gsc, SYSTM_EVENT_RESUME);
}
#endif	/* IPPI_DEBUG_CALL */

/*********************************************************
 * service functions
 *
 * old pisa interface device exists!
 *********************************************************/
int ipp_open __P((slot_device_handle_t));
int ipp_close __P((slot_device_handle_t));
int ipp_im_map __P((slot_device_handle_t, u_int));
int ipp_im_unmap __P((slot_device_handle_t, u_int));
int ipp_intr_map __P((slot_device_handle_t, u_int));
int ipp_intr_unmap __P((slot_device_handle_t, u_int));
int ipp_intr_ack __P((void *));
int ipp_dma_map __P((slot_device_handle_t, u_int));
int ipp_dma_unmap __P((slot_device_handle_t, u_int));
u_int8_t *ipp_info __P((slot_device_handle_t, u_int));

int ippi_map_io __P((struct ippi_softc *, slot_device_res_t, u_int, int));
int ippi_map_mem __P((struct ippi_softc *, slot_device_res_t, u_int, int));
int ippi_map_dma __P((struct ippi_softc *, slot_device_res_t, u_int, int));
int ippi_map_intr __P((struct ippi_softc *, slot_device_res_t, u_int, int));
int ipp_exec_services __P((slot_device_handle_t, u_int, int, int (*) __P((struct ippi_softc *, slot_device_res_t, u_int, int))));

struct slot_device_service_functions ipp_service_functions = {
	ipp_open,
	ipp_close,
	ipp_im_map,
	ipp_im_unmap,
	ipp_intr_map,
	ipp_intr_unmap,
	ipp_intr_ack,
	ipp_dma_map,
	ipp_dma_unmap,
	ipp_info,
};
	
#define	IPPI_BOARD_INFO(dh) ((dh)->dh_stag->st_aux)
#define	IPPI_SOFTC(dh) ((struct ippi_softc *) ((dh)->dh_stag->st_sc))

int
ipp_exec_services(dh, wn, unmap, func)
	slot_device_handle_t dh;
	u_int wn;
	int unmap;
	int (*func)
		__P((struct ippi_softc *, slot_device_res_t, u_int, int));
{ 
	struct ippi_board_info *ib = IPPI_BOARD_INFO(dh);
	struct ippi_softc *sc = IPPI_SOFTC(dh);
	struct ippi_card_info *ci;
	int error = 0;

	if ((ci = dh->dh_ci) == NULL)
		return EINVAL;

	ippi_wakeup_target(sc, ib, ci->ci_il);
	error = (*func) (sc, &ci->ci_dr, wn, unmap);
	ippi_wfk_target(sc);
	return error;
}

int
ipp_open(dh)
	slot_device_handle_t dh;
{
	struct ippi_board_info *ib = IPPI_BOARD_INFO(dh);
	struct ippi_softc *sc = IPPI_SOFTC(dh);
	struct ippi_card_info *ci;

	if ((ci = dh->dh_ci) == NULL)
		return EINVAL;
	if (ci->ci_flags & IPPI_CIF_OPEN)
	{
		printf("%s: already opened\n", sc->sc_dev.dv_xname);
		return EINVAL;
	}
	ci->ci_flags |= IPPI_CIF_OPEN;

	ippi_wakeup_target(sc, ib, ci->ci_il);
	ippi_activate_target(sc, 1);
	ippi_wfk_target(sc);
	return 0;
}

int
ipp_close(dh)
	slot_device_handle_t dh;
{
	struct ippi_board_info *ib = IPPI_BOARD_INFO(dh);
	struct ippi_softc *sc = IPPI_SOFTC(dh);
	struct ippi_card_info *ci;

	if ((ci = dh->dh_ci) == NULL)
		return EINVAL;
	if ((ci->ci_flags & IPPI_CIF_OPEN) == 0)
	{
		printf("%s: already closed\n", sc->sc_dev.dv_xname);
		return EINVAL;
	}
	ci->ci_flags &= ~IPPI_CIF_OPEN;

	ippi_wakeup_target(sc, ib, ci->ci_il);
	ippi_map_intr(sc, &ci->ci_dr, 0, 1);		/* XXX */
	ippi_activate_target(sc, 0);
	ippi_wfk_target(sc);
	return 0;
}

int
ipp_im_map(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{

	if (wn < SLOT_DEVICE_NIO)
		return ipp_exec_services(dh, wn, 0, ippi_map_io);
	else
		return ipp_exec_services(dh, wn - SLOT_DEVICE_NIO, 0,
					 ippi_map_mem);
}

int
ipp_im_unmap(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{

	if (wn < SLOT_DEVICE_NIO)
		return ipp_exec_services(dh, wn, 1, ippi_map_io);
	else
		return ipp_exec_services(dh, wn - SLOT_DEVICE_NIO, 1,
					 ippi_map_mem);
}

int
ipp_intr_map(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{

	return ipp_exec_services(dh, wn, 0, ippi_map_intr);
}

int
ipp_intr_unmap(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{

	return ipp_exec_services(dh, wn, 1, ippi_map_intr);
}

int
ipp_intr_ack(arg)
	void *arg;
{
	slot_device_intr_arg_t sa = arg;

	return ((*sa->sa_handler) (sa->sa_arg));
}

int
ipp_dma_map(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{

	return ipp_exec_services(dh, wn, 0, ippi_map_dma); 
}

int
ipp_dma_unmap(dh, wn)
	slot_device_handle_t dh;
	u_int wn;
{

	return ipp_exec_services(dh, wn, 1, ippi_map_dma);
}

u_int8_t *
ipp_info(dh, no)
	slot_device_handle_t dh;
	u_int no;
{

	return NULL;
}

/************************************************************
 * probe attach
 ************************************************************/
static int
ippimatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct systm_attach_args *sa = aux;
	bus_space_handle_t addrh, wdh;
	long iobase;
	int rv = 1;

	iobase = IPPI_DEFAULT_RDPORT;
	if (bus_space_map(sa->sa_iot, iobase, 1, 0, &addrh))
		return 0;
	if (bus_space_map(sa->sa_iot, iobase + ippi_wdofs, 1, 0, &wdh))
	{
		bus_space_unmap(sa->sa_iot, addrh, 1);
		return 0;
	}

	bus_space_unmap(sa->sa_iot, addrh, 1);
	bus_space_unmap(sa->sa_iot, wdh, 1);
	return rv;
}

static void
ippiattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct slot_device_attach_bus_args sda;
	struct ippi_softc *sc = (void *)self;
	struct systm_attach_args *sa = aux;
	long iobase = IPPI_DEFAULT_RDPORT;

	printf("\n");
	systmmsg_bind(self, ippi_systmmsg);

	sc->sc_iot = sa->sa_iot;
	sc->sc_memt = sa->sa_memt;
	sc->sc_dmat = sa->sa_dmat;
	sc->sc_ic = sa->sa_ic;
	sc->sc_delaybah = sa->sa_delaybah;

	if (bus_space_map(sc->sc_iot, iobase, 1, 0, &sc->sc_addrh) ||
	    bus_space_map(sc->sc_iot, iobase + ippi_wdofs, 1, 0, &sc->sc_wdh))
	{
		panic("%s: port mapping failed", sc->sc_dev.dv_xname);
		return;
	}

	sda.sda_sb = &pisa_slot_device_bus_tag;
	sda.sda_iot = sc->sc_iot;
	sda.sda_memt = sc->sc_memt;
	sda.sda_dmat = sc->sc_dmat;
	sda.sda_bc = sc->sc_ic;
	sc->sc_bus = config_found(self, &sda, NULL);
	if (sc->sc_bus == NULL)
		printf("%s: no bus\n", sc->sc_dev.dv_xname);

	ippi_scan_board(sc, 1);
#ifdef	IPPI_DEBUG_CALL
	ippi_gsc = sc;
#endif	/* IPPI_DEBUG_CALL */
}

/************************************************************
 * open close
 ************************************************************/
int
ippiopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = IPPIUNIT(dev);
	struct ippi_softc *sc;

	if (unit >= ippi_cd.cd_ndevs)
		return ENXIO;
	if ((sc = ippi_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	return 0;
}

int
ippiclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return 0;
}

/************************************************************
 * ioctl
 ************************************************************/
static struct ippi_board_info *
ippi_get_ib(sc, ipd)
	struct ippi_softc *sc;
	struct ippi_ipid *ipd;
{
	register struct ippi_board_info *ib;
	register struct ippi_ld_info *il;
	u_int ldn, i;

	ldn = ipd->ipd_ldn;
	if (ldn >= MAX_LDNS)
		return NULL;

	for (i = 0; i < sc->sc_maxb; i ++)
	{
		ib = sc->sc_ib[i];
		if (ipd->ipd_csn == ib->ib_csn)
		{
			il = &ib->ib_il[ldn];
			return il->il_status >= IPP_FOUND ? ib : NULL;
		}
	}
	return NULL;
}

int
ippi_find_resources(sc, cis, ldn, dr)
	struct ippi_softc *sc;
	u_int8_t *cis;
	u_int ldn;
	slot_device_res_t dr;
{
	struct ippres_request ir;
	ippres_res_t irr;
	u_int maxdfn, dfn;
	int pri, error;


	if ((irr = malloc(sizeof(*irr), M_TEMP, M_NOWAIT)) == NULL)
		return ENOMEM;

	memset(&ir, 0, sizeof(ir));
	ir.ir_ldn = ldn;
	ippi_initres(&irr->irr_dr);

	error = ippres_parse_items(cis, &ir, irr);
	if (error != 0)
		goto out;

	maxdfn = ir.ir_ndfn;
	for (pri = 0; pri < 3; pri ++)
	{
		for (dfn = 0; dfn == 0 || dfn < maxdfn; dfn ++)
		{
			ippi_initres(&irr->irr_dr);
			ir.ir_dfn = dfn;
			error = ippres_parse_items(cis, &ir, irr);
			if (error != 0)
				goto out;

			if (pri < 3 && irr->irr_pri != pri)
				continue;
		
			error = ippi_space_prefer(sc, irr);
			if (error == 0)
			{
				*dr = irr->irr_dr;
				goto out;
			}
		}
	}

	error = ENOSPC;

out:
	free(irr, M_TEMP);
	return error;
}

int
ippi_space_prefer(sc, irr)
	struct ippi_softc *sc;
	ippres_res_t irr;
{
	slot_device_res_t dr = &irr->irr_dr;
	bus_space_tag_t t;
	bus_addr_t hi, al;
	bus_size_t sal;
	int idx, error = 0;
	
	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
	{
		if (dr->dr_im[idx].im_hwbase == SLOT_DEVICE_UNKVAL)
			continue;

		if (idx < SLOT_DEVICE_NIO)
		{
			t = sc->sc_iot;
			hi = irr->irr_io[idx].io_hi;
			al = irr->irr_io[idx].io_al;
		}
		else
		{
			t = sc->sc_memt;
			hi = irr->irr_mem[idx - SLOT_DEVICE_NIO].mem_hi;
			al = irr->irr_mem[idx - SLOT_DEVICE_NIO].mem_al;
			if (al == 0)
				al = NBPG;

			sal = dr->dr_im[idx].im_size;
			sal = ((sal + al - 1) & (~(al - 1)));
			if (sal > al)
				al = sal;
		}

		error = bus_space_map_prefer(t, &dr->dr_im[idx].im_hwbase, 
				     	     hi, al, dr->dr_im[idx].im_size);
		if (error != 0)
			return error;
	}
	return 0;
}

int
ippi_chk_resources(sc, dr)
	struct ippi_softc *sc;
	slot_device_res_t dr;
{
	bus_space_tag_t t;
	bus_addr_t addr;
	bus_size_t size;
	int idx;

	if (dr->dr_nio > SLOT_DEVICE_NIO ||
	    dr->dr_nmem > SLOT_DEVICE_NMEM ||
	    dr->dr_npin > SLOT_DEVICE_NPIN ||
	    dr->dr_ndrq > SLOT_DEVICE_NDRQ)
		return EINVAL;

	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
	{
		if (dr->dr_im[idx].im_hwbase == SLOT_DEVICE_UNKVAL ||
		    dr->dr_im[idx].im_hwbase == 0)
			continue;

		addr = dr->dr_im[idx].im_hwbase;
		size = dr->dr_im[idx].im_size;
		if (idx < SLOT_DEVICE_NIO)
			t = sc->sc_iot;
		else
			t = sc->sc_memt;

		if (bus_space_map_prefer(t, &addr, addr, 0, size))
			return EINVAL;
	}

	return 0;
}

struct ippi_magic {
	struct serial_id ma_str;
	u_long ma_magic0;
};

int
ippi_connect_pisa(sc, ib, il, dr, dvname)
	struct ippi_softc *sc;
	struct ippi_board_info *ib;
	struct ippi_ld_info *il;
	slot_device_res_t dr;
	u_char *dvname;
{
	slot_device_handle_t dh;
	slot_device_ident_t di;
	struct ippi_card_info *ci;
	struct ippi_magic *ma;
	int idx, error = 0;
	struct pisa_slot_device_attach_args psa;

	/*
	 * check already ?
	 */
	if (il->il_dh != NULL)
		return EBUSY;

	/*
	 * allocate interrupt resources
	 */
	if ((error = ippi_allocate_pin(sc, dr)) != 0)
		return error;

	/*
	 * allocate a handle
	 */
	di = slot_device_ident_allocate(ib->ib_stag, dvname, sizeof(*ma));
	if (di == NULL)
		return ENOMEM;

	ma = di->di_magic;
	ma->ma_magic0 = DVCFG_MAJOR(dr->dr_dvcfg);
	bcopy(ib->ib_cis, &ma->ma_str, sizeof(ma->ma_str));

	error = slot_device_handle_allocate(ib->ib_stag, di, 0, &dh);
	if (error != 0)
	{
		slot_device_ident_deallocate(di);
		return EBUSY;
	}

	SLOT_DEVICE_IDENT_IDN(dh) = ib->ib_bid.bid_id;
	SLOT_DEVICE_IDENT_IDS(dh) = "unknown";

	/*
	 * allocate card_info 
	 */
	if ((ci = dh->dh_ci) == NULL)
	{
		ci = malloc(sizeof(*ci), M_DEVBUF, M_NOWAIT);
		if (ci == NULL)
		{
			error = ENOMEM;
			goto bad;
		}

		dh->dh_ci = ci;
		ci->ci_dr = *dr;
	}
	else
	{
		for (idx = 0; idx < MAX_IRQSET; idx ++)
			ci->ci_dr.dr_pin[idx] = dr->dr_pin[idx];
	}

	ci->ci_il = il;

	/*
	 * update pointer
	 */
	dr = &ci->ci_dr;
	il->il_dh = dh;

	/*
	 * attach a device
	 */
	psa.psa_pdr = dr;			/* phsyical device map */
	if ((dh->dh_flags & DH_CREATE) == 0)
	{
		slot_device_ident_deallocate(di);
		error = slot_device_activate(ib->ib_stag, dh, &psa);
		if (error != 0)
			goto bad1;
	}
	else
	{
		error = slot_device_attach(ib->ib_stag, dh, &psa);
		if (error != 0)
			goto bad;
	}

	il->il_status = IPP_DEVALLOC;

bad1:
	slot_device_handle_unbusy(dh);
	if (error != 0)
		il->il_dh = NULL;
	return error;

bad:
	if (dh->dh_ci != NULL)
		free(dh->dh_ci, M_TEMP);
	il->il_dh = NULL;
	slot_device_handle_deallocate(ib->ib_stag, dh);
	return error;
}

int
ippi_allocate_pin(sc, dr)
	struct ippi_softc *sc;
	slot_device_res_t dr;
{
	u_long pin, mask, npin;
	int idx;

	for (idx = 0, npin = 0; idx < SLOT_DEVICE_NPIN; idx ++)
	{
	 	if (dr->dr_pin[idx].dc_aux == 0)
			continue;
		if (dr->dr_pin[idx].dc_chan != SLOT_DEVICE_UNKVAL)
			continue;
		
		mask = systm_intr_routing->sir_allow_ippi & dr->dr_pin[idx].dc_aux;
		pin = isa_get_empty_irq(sc->sc_ic, mask);
		dr->dr_pin[idx].dc_chan = pin;
		if (pin == SLOT_DEVICE_UNKVAL)
		{
			printf("%s: pin%d: no irq left\n",
				sc->sc_dev.dv_xname, idx);
			if (npin == 0)
				return ENOSPC;
		}
		else
			npin ++;
	}
	return 0;
}

int
ippiioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error = 0, unit = IPPIUNIT(dev);
	struct ippi_softc *sc = ippi_cd.cd_devs[unit];
	struct ippi_board_info *ib;
	struct ippi_ld_info *il;

	switch (cmd)
	{
	case IPPI_IOG_RD:
		{
		struct ippi_ipp_info *ip;
		struct ippi_card_info *ci;

		ip = (struct ippi_ipp_info *) data;
		ib = ippi_get_ib(sc, &ip->ip_ipd);
		if (ib == NULL)
			return ENOENT;

		il = &ib->ib_il[ip->ip_ipd.ipd_ldn];
		ip->ip_bid = ib->ib_bid;
		ip->ip_state = il->il_status;

		if (il->il_dh != NULL)
		{
			ci = il->il_dh->dh_ci;
			ip->ip_dr = ci->ci_dr;
			strncpy(ip->ip_dvname, il->il_dh->dh_dv->dv_xname, 16);
		}
		else
			ippi_initres(&ip->ip_dr);

		bcopy(ib->ib_cis, ip->ip_cis, MAX_RDSZ);
		return 0;
		}

	case IPPI_IOC_PREFER:
		{
		struct ippi_ipp_info *ip;
		u_int ldn;

		ip = (struct ippi_ipp_info *) data;
		ib = ippi_get_ib(sc, &ip->ip_ipd);
		if (ib == NULL || ip->ip_bid.bid_id != ib->ib_bid.bid_id)
			return ENOENT;

		ldn = ip->ip_ipd.ipd_ldn;
		il = &ib->ib_il[ldn];

		if (il->il_dh != NULL)
		{
			struct ippi_card_info *ci = il->il_dh->dh_ci;

			ip->ip_dr = ci->ci_dr;
		}
		else if (il->il_flags & IPPI_LD_BOOTACTIVE)
		{
			ip->ip_dr = il->il_bdr;
		}
		else
		{
			u_int8_t *ap = ib->ib_cis;

			error = ippi_find_resources(sc, ap, ldn, &ip->ip_dr);
			if (error != 0)
				error = ippi_allocate_pin(sc, &ip->ip_dr);
		}

		return error;
		}

	case IPPI_IOC_DEVICE:
		{
		struct ippi_ctrl *icp;
		register slot_device_handle_t dh;
		register slot_device_slot_tag_t st;

		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return error;

		icp = (struct ippi_ctrl *) data;
		ib = ippi_get_ib(sc, &icp->ic_ipd);
		if (icp->ic_ctrl != IPPI_DEV_DEACTIVATE || ib == NULL)
			return EINVAL;

		st = ib->ib_stag;
		il = &ib->ib_il[icp->ic_ipd.ipd_ldn];
		if ((dh = il->il_dh) == NULL)
			return EINVAL;

		if (dh->dh_flags & DH_BUSY)
			return EBUSY;

		slot_device_deactivate(st, dh, NULL);
		il->il_dh = NULL;
		il->il_status = IPP_FOUND;

		cnputc(0x07);
		return 0;
		}

	case IPPI_IOC_MAP:
		return 0;

	case IPPI_IOC_CONNECT_PISA:
		{
		struct ippidev_connect_args *ica;

		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return error;

		ica = (struct ippidev_connect_args *) data;
		ib = ippi_get_ib(sc, &ica->ica_ipd);
		if (ib == NULL || ica->ica_bid.bid_id != ib->ib_bid.bid_id)
			return EINVAL;

		il = &ib->ib_il[ica->ica_ipd.ipd_ldn];
		if (il->il_status == IPP_DEVALLOC)
			return EBUSY;

		error = ippi_connect_pisa(sc, ib, il, &ica->ica_dr,
					  ica->ica_name);
		if (error == 0)
			strncpy(ica->ica_name, il->il_dh->dh_dv->dv_xname, 16);
		return error;
		}

	default:
		return ENOTTY;
	}

	return 0;
}

/************************************************************
 * device isolation and kernel registerations
 ************************************************************/
int
ippi_scan_board(sc, init)
	struct ippi_softc *sc;
	int init;
{
	int ndevs;
	u_long gap, eport, sport, port;

	if (sc->sc_rdh != NULL)
	{
		bus_space_unmap(sc->sc_iot, sc->sc_rdh, 1);
		sc->sc_rdh = NULL;
	}

	ndevs = 0;
	gap = ippi_rdskip;
	sport = ippi_rdbase;
	eport = sport + ippi_rdspan;

	for (port = sport; port < eport; port += gap)
	{
		sc->rd_port = port / sizeof(u_int32_t);
	        if (bus_space_map(sc->sc_iot, port, 1, 0, &sc->sc_rdh))
			continue;

		ndevs = ippi_isolation(sc, init);
		if (ndevs != 0)
		{
			ippi_config_sm(sc);
			break;
		}

		bus_space_unmap(sc->sc_iot, sc->sc_rdh, 1);
		sc->sc_rdh = NULL;
	}

	printf("%s: read port 0x%lx(0x%lx).\n",
		sc->sc_dev.dv_xname, sc->rd_port, port);
	printf("%s: kernel found %d pnp device(s).\n",
		sc->sc_dev.dv_xname, ndevs);
	return 0;
}

#define	IPPI_ISOLATION_BUGGY_WAIT(wait)	delay((wait))

int
ippi_isolation(sc, init)
	struct ippi_softc *sc;
	int init;
{
	slot_device_slot_tag_t st;
	bus_space_tag_t iot = sc->sc_iot;
	struct ippi_board_info *ib;
	struct ippi_ld_info *il;
	struct serial_id sdata;
	int i, idx;

	/* Anyway all target should be "Wait for Key" state */
	ippi_wfk_target(sc);
	IPPI_ISOLATION_BUGGY_WAIT(1000);

	/* Wait for key state => sleep state */
	ippi_send_initiation_LFSR(sc);
	IPPI_ISOLATION_BUGGY_WAIT(2000);

	/* Reset CSN of all boards */
	ippi_regw(sc, ippi_ccr, CCR_CSNR);
	IPPI_ISOLATION_BUGGY_WAIT(2000);

	sc->sc_maxb = 0;
	for (idx = 0; idx < MAX_CARDS; idx ++)
	{
#ifdef	IPPI_BUGGY_PROTOCOL_CARDS
		/* once fall back to Wait for Key state */
		ippi_wfk_target(sc);
		IPPI_ISOLATION_BUGGY_WAIT(1000);

		/* Send LFSR and sleep state again */
		ippi_send_initiation_LFSR(sc);
		IPPI_ISOLATION_BUGGY_WAIT(2000);
#endif	/* IPPI_BUGGY_PROTOCOL_CARDS */

		/* Wake[0]: 
	 	 *	sleep state  => isolation state 
		 *	config state => isolation state
		 */
		ippi_regw(sc, ippi_wur, WUR_CONFIG);
		IPPI_ISOLATION_BUGGY_WAIT(1000);

		/* Set RD port */
		ippi_regw(sc, ippi_rdar, sc->rd_port);
		IPPI_ISOLATION_BUGGY_WAIT(1000);

		/* Serialize */
		if (ippi_get_serial(sc, &sdata) == 0)
			break;

		/*
		 * Give a CSN to the serialized board.
		 * isolation state => config state
		 */
		ippi_regw(sc, ippi_csnr, idx + 1);
		bus_space_write_1(iot, sc->sc_addrh, 0, ippi_sr);

		ib = sc->sc_ib[idx];
		if (ib == NULL)
		{
			register slot_device_bus_tag_t sb;

			ib = malloc(sizeof(*ib), M_DEVBUF, M_NOWAIT);
			if (ib == NULL)
				goto out;
			memset(ib, 0, sizeof(*ib));

			sb = &pisa_slot_device_bus_tag;
			st = slot_device_slot_tag_allocate(sb, sc, ib,
				      		sc->sc_bus, PISA_SLOT_PnPISA);
			if (st == NULL)
				panic("%s: no tag\n", sc->sc_dev.dv_xname);
			st->st_funcs = &ipp_service_functions;
			st->st_flags = 0;	
			ib->ib_stag = st;
			sc->sc_ib[idx] = ib;
		}

		ippi_setup_ib(ib, idx + 1, &sdata);
		ippi_read_info(sc, ib, &sdata);

		for (i = 0; i < MAX_LDNS; i ++)
		{
			il = &ib->ib_il[i];
			il->il_ldn = i;
			il->il_flags = 0;
			il->il_status = 0;
			ippi_initres(&il->il_bdr);
		}

		for (i = 0; i < MAX_LDNS; i ++)
		{
			il = &ib->ib_il[i];
			if(ippi_find_resources(sc, ib->ib_cis, i, &il->il_bdr))
			{
				if (i == 0)
				{
					printf("%s: ld(%d:%d) RESOURCE DATA ILLEGAL (use legacy mode)\n", sc->sc_dev.dv_xname, ib->ib_csn, i);
				}
				break;
			}

			il->il_status = IPP_FOUND;
			printf("%s: ld(%d:%d) ", sc->sc_dev.dv_xname,
			       ib->ib_csn, i);
			ippi_print(sc, ib);

			ippi_regw(sc, ippi_ldnr, il->il_ldn);
			if (ippi_is_active(sc) != 0)
			{
				il->il_flags |= IPPI_LD_BOOTACTIVE;
				ippi_read_config(sc, &il->il_bdr);
				ippi_regw(sc, ippi_irlr, 0);	/* XXX */
			}
		}

		ib->ib_maxldn = i;
	}

out:
	ippi_wfk_target(sc);
	sc->sc_maxb = idx;
	return idx;
}

void
ippi_config_sm(sc)
	struct ippi_softc *sc;
{
	struct ippi_board_info *ib;
	struct ippi_ld_info *il;
	int ldn, idx, scanlvl, baselvl;

	for (scanlvl = 0; scanlvl < 2; scanlvl ++)
	{
		for (idx = 0; idx < sc->sc_maxb; idx ++)
		{
			ib = sc->sc_ib[idx];
			for (ldn = 0; ldn < ib->ib_maxldn; ldn ++)
			{
				il = &ib->ib_il[ldn];
				if (il->il_status != IPP_FOUND)
					continue;

				baselvl = 
				   (il->il_flags & IPPI_LD_BOOTACTIVE) ? 0 : 1;
				if (baselvl != scanlvl)
					continue;

				ippi_card_attach(sc, ib, il);
			}
		}
	}
}

/************************************************************
 * systm msg
 ************************************************************/
static void ippi_device_deallocate __P((slot_device_slot_tag_t));
static int ippi_notify __P((slot_device_slot_tag_t, slot_device_event_t));

static void
ippi_device_deallocate(st)
	slot_device_slot_tag_t st;
{
	slot_device_handle_t dh, ndh;	
	struct ippi_card_info *ci;
	struct ippi_ld_info *il;

	for (dh = st->st_dhtab.tqh_first; dh != NULL; dh = ndh)
	{
		ndh = dh->dh_dvgchain.tqe_next;

		if ((dh->dh_flags & DH_BUSY) != 0)
			continue;

		slot_device_deactivate(st, dh, NULL);
		if ((ci = dh->dh_ci) == NULL)
			continue;

		il = ci->ci_il;
		il->il_dh = NULL;
		il->il_status = 0;
	}
}

static int
ippi_notify(st, ev)
	slot_device_slot_tag_t st;
	slot_device_event_t ev;
{
	slot_device_handle_t dh, ndh;	
	int error = 0;

	for (dh = st->st_dhtab.tqh_first; dh != NULL; dh = ndh)
	{
		ndh = dh->dh_dvgchain.tqe_next;
		if ((dh->dh_flags & DH_BUSY) != 0)
		{
			error |= SD_EVENT_STATUS_BUSY;
			continue;
		}
		error |= slot_device_notify(st, dh, ev);
	}

	return error;
}

static int
ippi_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct ippi_softc *sc = (struct ippi_softc *) dv;
	struct ippi_board_info *ib;
	slot_device_event_t sev;
	int i, error = 0;

	switch (ev)
	{
	case SYSTM_EVENT_SUSPEND:
		for (i = 0; i < sc->sc_maxb; i++)
		{
			ib = sc->sc_ib[i];
			ippi_device_deallocate(ib->ib_stag);
		}
		return error;

	case SYSTM_EVENT_RESUME:
		ippi_scan_board(sc, 0);
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
		for (i = 0; i < sc->sc_maxb; i++)
		{
			ib = sc->sc_ib[i];
			error |= ippi_notify(ib->ib_stag, sev);
		}
		if (error != 0)
			return SYSTM_EVENT_STATUS_BUSY;	/* XXX */
		return 0;

	default:
		return error;
	}
}

/************************************************************
 * misc utils
 ************************************************************/
void
ippi_initres(dr)
	slot_device_res_t dr;
{
	int i;

	memset(dr, 0, sizeof(*dr));

	dr->dr_ndrq = 2;
	for (i = 0; i < SLOT_DEVICE_NDRQ; i ++)
		dr->dr_drq[i].dc_chan = SLOT_DEVICE_UNKVAL;

	dr->dr_npin = 2;
	for (i = 0; i < SLOT_DEVICE_NPIN; i++)
	{
		dr->dr_pin[i].dc_flags = IRTR_HIGH;
		dr->dr_pin[i].dc_chan = SLOT_DEVICE_UNKVAL;
	}

	dr->dr_nio = SLOT_DEVICE_NIO;
	dr->dr_nmem = SLOT_DEVICE_NMEM;
	dr->dr_nim = SLOT_DEVICE_NIM;
	for (i = 0; i < SLOT_DEVICE_NIM; i++)
	{
		dr->dr_im[i].im_hwbase = SLOT_DEVICE_UNKVAL;
		dr->dr_im[i].im_size = SLOT_DEVICE_UNKVAL;
		if (i >= SLOT_DEVICE_NIO)
		{
			dr->dr_im[i].im_flags = (SDIM_BUS_WIDTH16 | MCR_BUS16);
			dr->dr_im[i].im_type = SLOT_DEVICE_SPMEM;
		}
		else
		{
			dr->dr_im[i].im_flags = 0;
			dr->dr_im[i].im_type = SLOT_DEVICE_SPIO;
		}
	}
}

void
ippi_setup_ib(ib, csn, sbp)
	struct ippi_board_info *ib;
	u_int csn;
	struct serial_id *sbp;
{

	ib->ib_csn = csn;
	ib->ib_bid.bid_id = htonl(sbp->si_product);
	ib->ib_bid.bid_serial = htonl(sbp->si_un);
}

/************************************************************
 * HW primitive access (dirty part)
 ************************************************************/
void
ippi_regw(sc, addr, val)
	struct ippi_softc *sc;
	bus_addr_t addr;
	u_int8_t val;
{

	bus_space_write_1(sc->sc_iot, sc->sc_addrh, 0, addr);
	bus_space_write_1(sc->sc_iot, sc->sc_wdh,   0, val);
}

u_int8_t
ippi_regr(sc, addr)
	struct ippi_softc *sc;
	bus_addr_t addr;	
{

	bus_space_write_1(sc->sc_iot, sc->sc_addrh, 0, addr);
	return bus_space_read_1(sc->sc_iot, sc->sc_rdh, 0);
}

void
ippi_regw_2(sc, offs, val)
	struct ippi_softc *sc;
	bus_addr_t offs;
	u_int16_t val;
{
	
	ippi_regw(sc, offs, (u_int8_t) (val >> NBBY));
	ippi_regw(sc, offs + 1, (u_int8_t) val);
}

u_int16_t
ippi_regr_2(sc, offs)
	struct ippi_softc *sc;
	bus_addr_t offs;
{
	register u_int16_t val;

	val = ippi_regr(sc, offs);
	val = (val << NBBY) | (u_int16_t) ippi_regr(sc, offs + 1);
	return val;
}

/*
 * Send initiation LFSR as described in "Plug and Play ISA Specification,
 * Intel May 94."
 */
void
ippi_send_initiation_LFSR(sc)
	struct ippi_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_addrh;
	u_int key, i;

	for (i = 0; i < 2; i ++)
		bus_space_write_1(iot, ioh, 0, 0);
	for (key = 0x6a, i = 0; i < 32; i++)
	{
		bus_space_write_1(iot, ioh, 0, key);
		key = (key >> 1) | (((key ^ (key >> 1)) << 7) & 0xff);
	}
}

void
ippi_wakeup_target(sc, ib, il)
	struct ippi_softc *sc;
	struct ippi_board_info *ib;
	struct ippi_ld_info *il;
{

	ippi_send_initiation_LFSR(sc);
	ippi_regw(sc, ippi_wur, ib->ib_csn);
	ippi_regw(sc, ippi_ldnr, il->il_ldn);
}

void
ippi_wfk_target(sc)
	struct ippi_softc *sc;
{

	ippi_regw(sc, ippi_ccr, CCR_WFK);
}

void
ippi_activate_target(sc, flags)
	struct ippi_softc *sc;
	int flags;
{

	ippi_regw(sc, ippi_iorcr, 0);
	ippi_regw(sc, ippi_ar, flags ? AR_ACTIVE : 0);
}

int
ippi_is_active(sc)
	struct ippi_softc *sc;
{

	return (ippi_regr(sc, ippi_ar) & AR_ACTIVE);
}

/*
 * Get the device's serial number. Returns 1 if the serial is valid.
 */
int
ippi_get_serial(sc, sdata)
	struct ippi_softc *sc;
	struct serial_id *sdata;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t rdh = sc->sc_rdh;
	u_int i, bit, valid = 0, sum = 0x6a;
	u_int8_t *data;

	bus_space_write_1(iot, sc->sc_addrh, 0, ippi_sir);
	delay(1000);

	data = (u_int8_t *) sdata;
	memset(sdata, 0, sizeof(*sdata));
	for (i = 0; i < NBBY * sizeof(*sdata); i++)
	{
		bit = bus_space_read_1(iot, rdh, 0) == 0x55;
		delay(250);
		bit = (bus_space_read_1(iot, rdh, 0) == 0xaa) && bit;
		delay(250);

		valid = valid || bit;
		if (i < 64)
			sum = (sum >> 1) |
			    (((sum ^ (sum >> 1) ^ bit) << 7) & 0xff);
		data[i / 8] = (data[i / 8] >> 1) | (bit ? 0x80 : 0);
	}

	valid = valid && (sdata->si_cksum == sum);
	return valid;
}

static void
ippi_read_info(sc, ib, sbp)
	struct ippi_softc *sc;
	struct ippi_board_info *ib;
	struct serial_id *sbp;
{
	u_int8_t *ids, *mids;
	int cnt;

 	ids = (u_int8_t *) ib->ib_cis;
	mids = ids + MAX_RDSZ;
	bcopy(sbp, ids, sizeof(*sbp));
	for (ids = ids + sizeof(*sbp); ids < mids; ids ++)
	{
		cnt = 0x100;
		while((ippi_regr(sc, ippi_sr) & SR_READY) == 0)
		{
			if (cnt -- < 0)
				return;
			delay(1);
		}
		*ids = ippi_regr(sc, ippi_rdr);
	}
}

/***************************************************************
 * setup resources or read resources
 ***************************************************************/
#define	IPPI_IOB_MASK (~(0x07U))
#define	IPPI_ADDRESS_LINE 24

int ippi_debug = 0;
void ippi_print_dr __P((slot_device_res_t));

void
ippi_print_dr(dr)
	slot_device_res_t dr;
{
	int idx;

	for (idx = 0; idx < dr->dr_nio; idx ++)
		printf("io[%d]: 0x%lx 0x%lx\n", idx,
			dr->dr_io[idx].im_hwbase,
			dr->dr_io[idx].im_size);

	for (idx = 0; idx < dr->dr_npin; idx ++)
		printf("irq[%d]: 0x%lx 0x%lx\n", idx, 
			dr->dr_pin[idx].dc_chan, 
			dr->dr_pin[idx].dc_flags);

	for (idx = 0; idx < dr->dr_ndrq; idx ++)
		printf("drq[%d]: 0x%lx 0x%lx\n", idx, 
			dr->dr_drq[idx].dc_chan, 
			dr->dr_drq[idx].dc_flags);

	for (idx = 0; idx < dr->dr_nmem; idx ++)
		printf("mem[%d]: 0x%lx 0x%lx 0x%lx\n", idx,
			dr->dr_mem[idx].im_hwbase,
			dr->dr_mem[idx].im_size,
			dr->dr_mem[idx].im_flags);
}

int
ippi_map_io(sc, dr, idx, unmap)
	struct ippi_softc *sc;
	slot_device_res_t dr;
	u_int idx;
	int unmap;
{
	u_int val;

	if (idx >= MAX_IOWIN)
		return EINVAL;

	if (dr->dr_nio >= SLOT_DEVICE_NIO)
		dr->dr_nio = SLOT_DEVICE_NIO;

	if (idx >= dr->dr_nio || unmap != 0 || 
	    dr->dr_io[idx].im_hwbase == SLOT_DEVICE_UNKVAL)
		val = 0;
	else
		val = (u_int16_t) dr->dr_io[idx].im_hwbase;

	ippi_regw_2(sc, ippi_ioar + IPPI_IOR_SKIP * idx, val);

	return 0;
}

int
ippi_map_mem(sc, dr, idx, unmap)
	struct ippi_softc *sc;
	slot_device_res_t dr;
	u_int idx;
	int unmap;
{
	bus_addr_t offs = idx * IPPI_MEMR_SKIP;
	u_int val, flags;
	
	if (idx >= MAX_MEMWIN)
		return EINVAL;

	if (dr->dr_nmem >= SLOT_DEVICE_NMEM)
		dr->dr_nmem = SLOT_DEVICE_NMEM;

	if (idx >= dr->dr_nmem || unmap != 0 ||
	    dr->dr_mem[idx].im_hwbase == SLOT_DEVICE_UNKVAL)
		val = 0;
	else
		val = dr->dr_mem[idx].im_hwbase >> NBBY;

	ippi_regw_2(sc, ippi_mar + offs, val);

	if (idx >= dr->dr_nmem)
		return 0;

	flags = ippi_regr(sc, ippi_mcr + offs);
	if (dr->dr_mem[idx].im_flags != SLOT_DEVICE_UNKVAL)
	{
		val = dr->dr_mem[idx].im_flags;
		val |= flags;	
		if (val & SDIM_BUS_WIDTH16)
			val |= MCR_BUS16;
		ippi_regw(sc, ippi_mcr + offs, val);
	}

	if (dr->dr_mem[idx].im_size != SLOT_DEVICE_UNKVAL)
	{
		val = dr->dr_mem[idx].im_size;
		if (flags & MCR_ULA)
			val += dr->dr_mem[idx].im_hwbase;
		else if (val != 0)
			val = (~(val - 1)) & ((1 << IPPI_ADDRESS_LINE) - 1);
#ifdef	IPPI_DEBUG
		printf("val 0x%x\n", val);
#endif	/* IPPI_DEBUG */
		ippi_regw_2(sc, ippi_mular + offs, val >> NBBY);
	}

	return 0;
}

int
ippi_map_intr(sc, dr, idx, unmap)
	struct ippi_softc *sc;
	slot_device_res_t dr;
	u_int idx;
	int unmap;
{	
	bus_addr_t offs = idx * IPPI_ICR_SKIP;
	u_int val;

	if (idx >= MAX_IRQSET)
		return EINVAL;

	if (dr->dr_npin >= SLOT_DEVICE_NPIN)
		dr->dr_npin = SLOT_DEVICE_NPIN;

	if (idx >= dr->dr_npin || unmap != 0 || 
	    dr->dr_pin[idx].dc_chan == SLOT_DEVICE_UNKVAL)
		val = 0;
	else
		val = dr->dr_pin[idx].dc_chan;
	ippi_regw(sc, ippi_irlr + offs, val);

	if (idx < dr->dr_npin && dr->dr_pin[idx].dc_flags != SLOT_DEVICE_UNKVAL)
		ippi_regw(sc, ippi_irtr + offs, dr->dr_pin[idx].dc_flags);
	return 0;
}

int
ippi_map_dma(sc, dr, idx, unmap)
	struct ippi_softc *sc;
	slot_device_res_t dr;
	u_int idx;
	int unmap;
{	
	bus_addr_t offs = idx * IPPI_DMAR_SKIP;
	u_int val;

	if (idx >= MAX_DRQSET)
		return EINVAL;

	if (dr->dr_ndrq >= SLOT_DEVICE_NDRQ)
		dr->dr_ndrq = SLOT_DEVICE_NDRQ;

	if (idx >= dr->dr_ndrq || dr->dr_drq[idx].dc_chan >= 4 || unmap != 0)
		val = 4;
	else
		val = dr->dr_drq[idx].dc_chan;

	ippi_regw(sc, ippi_dcsr + offs, val);
	return 0;
}

int
ippi_read_config(sc, dr)
	struct ippi_softc *sc;
	slot_device_res_t dr;
{
	bus_addr_t offs;
	u_int32_t val;
	int idx;

	/* get io windows resources */
	offs = 0;
	for (idx = 0; idx < SLOT_DEVICE_NIO; idx ++, offs += IPPI_IOR_SKIP)
	{
		if (idx >= MAX_IOWIN)
			continue;

		val = ippi_regr_2(sc, ippi_ioar + offs);
		if (val == 0)
			val = SLOT_DEVICE_UNKVAL;
		dr->dr_io[idx].im_hwbase = val;
	}

	/* get irq resources */
	offs = 0;
	for (idx = 0; idx < SLOT_DEVICE_NPIN; idx ++, offs += IPPI_ICR_SKIP)
	{
		if (idx >= MAX_IRQSET)
			continue;

		val = ippi_regr(sc, ippi_irlr + offs);
		if (val == 0)
			val = SLOT_DEVICE_UNKVAL;
		dr->dr_pin[idx].dc_chan = val;
		dr->dr_pin[idx].dc_flags = ippi_regr(sc, ippi_irtr + offs);
	}

	/* get drq resources */
	offs = 0;
	for (idx = 0; idx < SLOT_DEVICE_NDRQ; idx ++, offs += IPPI_DMAR_SKIP)
	{
		if (idx >= MAX_DRQSET)
			continue;

		val = ippi_regr(sc, ippi_dcsr + offs);
		if (val > 3)
			val = SLOT_DEVICE_UNKVAL;
		dr->dr_drq[idx].dc_chan = val;
	}

	/* get mem resources */
	offs = 0;
	for (idx = 0; idx < SLOT_DEVICE_NMEM; idx ++, offs += IPPI_MEMR_SKIP)
	{
		if (idx >= MAX_MEMWIN)
			continue;

		val = ((u_long) ippi_regr_2(sc, ippi_mar + offs)) << NBBY;
		if (val == 0)
			val = SLOT_DEVICE_UNKVAL;
		dr->dr_mem[idx].im_hwbase = val;
		dr->dr_mem[idx].im_flags = ippi_regr(sc, ippi_mcr + offs);
		val = ((u_long) ippi_regr_2(sc, ippi_mular + offs)) << NBBY;
		if (dr->dr_mem[idx].im_flags & MCR_ULA)
			val -= dr->dr_mem[idx].im_hwbase;
		else if (val != 0)
			val = ((~val) & ((1 << IPPI_ADDRESS_LINE) - 1)) + 1;
		dr->dr_mem[idx].im_size = val;
			
	}

	if (ippi_debug != 0)
		ippi_print_dr(dr);
	return 0;
}

/***************************************************************
 * misc print
 ***************************************************************/
void
ippi_print(sc, ib)
	struct ippi_softc *sc;
	struct ippi_board_info *ib;
{
	u_int8_t *data = ib->ib_cis;

	printf("vendor:%c%c%c%02x%02x(0x%08x) serial: 0x%08x\n",
	    ((data[0] & 0x7c) >> 2) + 64,
	    (((data[0] & 0x03) << 3) | ((data[1] & 0xe0) >> 5)) + 64,
	    (data[1] & 0x1f) + 64, data[2], data[3],
	     ib->ib_bid.bid_id, ib->ib_bid.bid_serial);
}

