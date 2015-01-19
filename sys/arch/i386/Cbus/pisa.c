/*	$NecBSD: pisa.c,v 1.49.4.2 1999/08/22 19:45:48 honda Exp $	*/
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
 * Copyright (c) 1995, 1996, 1997 Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>
#include <dev/isa/pisavar.h>
#include <dev/isa/isadmareg.h>
#include <dev/cons.h>

#include <vm/vm.h>

#include <machine/dvcfg.h>

extern int cold;

struct pisa_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;		/* isa i/o space tag */
	bus_space_tag_t sc_memt;	/* isa mem space tag */
	bus_dma_tag_t sc_dmat;		/* DMA tag */
	void *sc_ic;			/* XXX: isa chipset tag */

	struct slot_device_resources sc_dr;	/* working data area */
};

int pisamatch __P((struct device *, struct cfdata *, void *));
void pisaattach __P((struct device *, struct device *, void *));

struct cfattach pisa_ca = {
	sizeof(struct pisa_softc), pisamatch, pisaattach
};

extern struct cfdriver pisa_cd;

_SLOT_DEVICE_BUS_FUNCS_PROTO(pisa)

struct slot_device_bus_tag pisa_slot_device_bus_tag = {
	SLOT_DEVICE_BUS_PISA,
	"pisa",
	NULL,

	_SLOT_DEVICE_BUS_FUNCS_TAB(pisa)
};	

#define	splpisa	splimp

/******************************************************
 * PISA attach probe
 ******************************************************/
int
pisamatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct slot_device_attach_bus_args *sda = aux;

	if (sda->sda_sb != &pisa_slot_device_bus_tag)
		return 0;
	return 1;
}

void
pisaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pisa_softc *sc = (void *) self;
	struct slot_device_attach_bus_args *sda = aux;

	sc->sc_iot = sda->sda_iot;
	sc->sc_memt = sda->sda_memt;
	sc->sc_dmat = sda->sda_dmat;
	sc->sc_ic = sda->sda_bc;
	printf("\n");
}

/******************************************************
 * Static device allocation
 ******************************************************/
#ifdef	STANDARD_PRINTFUNC
int pisaprint __P((void *, const char *));

int
pisaprint(aux, isa)
	void *aux;
	const char *isa;
{
	struct pisa_attach_args *pa = aux;
	slot_device_res_t dr = PISA_RES_DR(pa->pa_dh);
	u_long addr;
	int idx, nwin;

	for (nwin = idx = 0; idx < SLOT_DEVICE_NIO; idx ++)
	{
		if (PISA_DR_IOB(dr, idx) != PISA_UNKVAL)
		{
			if (nwin == 0)
				printf(" io");
			nwin ++;
			printf(" 0x%lx", PISA_DR_IOB(dr, idx));
			addr = PISA_DR_IOB(dr, idx) + PISA_DR_IOS(dr, idx);
			printf("-0x%lx", addr - 1);
		}
	}

	for (nwin = idx = 0; idx < SLOT_DEVICE_NMEM; idx ++)
	{
		if (PISA_DR_MEMB(dr, idx) != PISA_UNKVAL)
		{
			if (nwin == 0)
				printf(" mem");
			nwin ++;
			printf(" 0x%lx", PISA_DR_MEMB(dr, idx));
			addr = PISA_DR_MEMB(dr, idx) + PISA_DR_MEMS(dr, idx);
			printf("-0x%lx", addr - 1);
		}
	}

	if (PISA_DR_IRQ0(dr) != PISA_UNKVAL)
		printf(" pin0 %ld", PISA_DR_IRQ0(dr));
	if (PISA_DR_IRQ1(dr) != PISA_UNKVAL)
		printf(" pin1 %ld", PISA_DR_IRQ1(dr));

	if (PISA_DR_DRQ(dr) != PISA_UNKVAL)
		printf(" dma0 %ld", PISA_DR_DRQ(dr));

	printf(" dvcfg %lx", PISA_DR_DVCFG(dr));
	return (UNCONF);
}

#else	/* !STANDARD_PRINTFUNC */
/*
 * print really mapped resources.
 */
int pisaprint __P((pisa_device_handle_t));

int
pisaprint(dh)
	pisa_device_handle_t dh;
{
	struct device *dev = dh->dh_dv;
	slot_device_res_t dr = PISA_RES_DR(dh);
	int idx, i;

	printf("%s: ", dev->dv_xname);

	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
	{
		u_long addr;
		u_char *s;

		if (PISA_IMHAND(dh, idx) == NULL)
			continue;

		s = (idx >= SLOT_DEVICE_NIO) ? "mem" : "io";
		printf("%s%d ", s, idx);
		for (i = 0; i < SLOT_DEVICE_NIM; i ++)
		{
			if (PISA_IMWMAP(dh, idx) & (1 << i))
			{
				addr = PISA_DR_IOB(dr, i) + PISA_DR_IOS(dr, i);
				printf("0x%lx-0x%lx ", 
					PISA_DR_IOB(dr, i), addr - 1);
			}
		}
	}

	for (idx = 0; idx < SLOT_DEVICE_NPIN; idx ++)
	{
		if (PISA_PINHAND(dh, idx) == NULL)
			continue;

		printf("pin%d %ld ", idx, PISA_DR_IRQN(dr, idx));
	}

	if (PISA_DR_DRQ(dr) != PISA_UNKVAL)
		printf("dma0 %ld ", PISA_DR_DRQ(dr));

	printf("dvcfg %lx\n", PISA_DR_DVCFG(dr));
	return 0;
}
#endif	/* !STANDARD_PRINTFUNC */

/* static attach */
int pisascan __P((struct device *, struct cfdata *,  void *));

int
pisascan(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct pisa_attach_args *pa;
	pisa_device_handle_t dh;
	slot_device_slot_tag_t st;
	struct pisa_softc *sc;
	slot_device_ident_t di;

 	pa = aux;
 	dh = pa->pa_dh;
 	st = dh->dh_stag;
	sc = st->st_bus;
 	di = dh->dh_id;
	if (pa->pa_state || strncmp(di->di_xname, cf->cf_driver->cd_name, 16))
		return 0;

	sc->sc_dr = *PISA_RES_DR(dh);
	pa->pa_state = (*cf->cf_attach->ca_match)(parent, match, pa);
	*PISA_RES_DR(dh) = sc->sc_dr;	

	return pa->pa_state;
}

/****************************************************************
 * activate & deactivate
 ***************************************************************/
static int pisa_device_activate __P((slot_device_handle_t));
static int pisa_device_deactivate __P((slot_device_handle_t));
static int spltarg __P((u_int));
int pisa_space_iomem_activate __P((pisa_device_handle_t, int));
int pisa_space_iomem_deactivate __P((pisa_device_handle_t, int));
int pisa_space_intr_activate __P((struct pisa_softc *, slot_device_handle_t, int));
int pisa_space_intr_deactivate __P((struct pisa_softc *, slot_device_handle_t, int, int));
int pisa_update_resources __P((slot_device_res_t, slot_device_res_t));

static int
spltarg(irq)
	u_int irq;
{
	register u_int mask;

	if (irq != IRQUNK)
		mask = (1 << irq);
	else	
		mask = 0;

	return splraise(mask);
}

static int
pisa_device_activate(dh)
	slot_device_handle_t dh;
{
	struct pisa_functions *pd;

	if ((pd = PISA_RES_FUNCTIONS(dh)) == NULL || pd->pd_activate == NULL)
		return 0;

	return ((*pd->pd_activate) (dh));
}

static int
pisa_device_deactivate(dh)
	slot_device_handle_t dh;

{
	struct pisa_functions *pd;

	if ((pd = PISA_RES_FUNCTIONS(dh)) == NULL || pd->pd_deactivate == NULL)
		return 0;

	return ((*pd->pd_deactivate) (dh));
}

/****************************************************************
 * slot device funcs
 ***************************************************************/
/*
 * device handle should be marked with busy, create! 
 */

int
pisa_slot_device_attach(st, dh, aux)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
	void *aux;
{
	struct pisa_softc *sc = st->st_bus;
	struct pisa_slot_device_attach_args *psa = aux;
	slot_device_res_t dr = psa->psa_pdr;
	struct pisa_attach_args pa;
	pisa_res_t pv;
	int s, error = 0;
	struct cfdata *cf;

 	s = splpisa();
	if ((dh->dh_flags & (DH_CREATE | DH_BUSY)) != (DH_CREATE | DH_BUSY))
	{
		splx(s);
		return EBUSY;
	}
	splx(s);

	/*
	 * allocate pisa bus resources
	 */
	if (dh->dh_res == NULL)
	{
		int idx;

		dh->dh_res = malloc(sizeof(*pv), M_DEVBUF, M_NOWAIT);
		if (dh->dh_res == NULL)
		{
			error = ENOMEM;
			goto out;
		}

		memset(dh->dh_res, 0, sizeof(*pv));
		memset(&dh->dh_space, 0, sizeof(dh->dh_space));

		for (idx = 0; idx < SLOT_DEVICE_NIO; idx ++)
			PISA_IMTAG(dh, idx) = sc->sc_iot;

		for (idx = 0; idx < SLOT_DEVICE_NMEM; idx ++)
			PISA_IMTAG(dh, SLOT_DEVICE_NIO + idx) = sc->sc_memt;
	}

	/*
	 * setup physical device windows info
	 */
	PISA_RES_DR(dh) = dr;

	/*
	 * make pisa attach args
	 */
	pa.pa_dh = dh;
	pa.pa_memt = sc->sc_memt;
	pa.pa_iot = sc->sc_iot;
	pa.pa_dmat = sc->sc_dmat;
	pa.pa_ic = sc->sc_ic;
	pa.pa_state = 0;

	/*
	 * activate the handle and open devices
	 */
	slot_device_handle_activate(st, dh);
	error = slot_device_service_open(dh);
	if (error != 0)
	{
		slot_device_handle_deactivate(st, dh);
		goto out;
	}

	/*
	 * raise spl
	 */
	s = splpisa();

	/*
	 * scan devices
	 */
	spltarg(PISA_DR_IRQ(dr));
	cf = config_search(pisascan, (struct device *) sc, &pa);
	if (cf != NULL)
	{
#ifdef	STANDARD_PRINTFUNC
		config_attach((struct device *) sc, cf, &pa, pisaprint);
#else	/* !STANDARD_PRINTFUNC */
		config_attach((struct device *) sc, cf, &pa, NULL);
		pisaprint(dh);
#endif	/* !STANDARD_PRINTFUNC */
	}
	else
	{
		slot_device_service_close(dh);
		slot_device_handle_deactivate(st, dh);
		error = ENODEV;
	}
	splx(s);

out:
	/*
	 * done
	 */
	if (error != 0 && dh->dh_res != NULL)
	{
		free(dh->dh_res, M_DEVBUF);
		dh->dh_res = NULL;
	}

	if (cold == 0)
		cnputc(0x07);
	return error;
}

int
pisa_slot_device_dettach(st, dh, aux)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
	void *aux;
{
	int s = splpisa();

	if (dh->dh_res != NULL)
	{
		free(dh->dh_res, M_DEVBUF);
		dh->dh_res = NULL;
	}

	if (dh->dh_flags & DH_ACTIVE)
		slot_device_handle_deactivate(st, dh);
	splx(s);
	return 0;
}


int
pisa_slot_device_activate(st, dh, aux)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
	void *aux;
{
	struct pisa_softc *sc = st->st_sc;
	struct pisa_slot_device_attach_args *psa = aux;
	slot_device_res_t dr = psa->psa_pdr;
	int s, error = 0;

	PISA_RES_EVENT(dh) = PISA_EVENT_INSERT;

	s = splpisa();
	if (dh->dh_flags & (DH_ACTIVE | DH_CREATE))
	{
		splx(s);
		return EBUSY;
	}
	splx(s);

	/* 
	 * update resources
	 */
	if ((error = pisa_update_resources(PISA_RES_DR(dh), dr)) != 0)
		return error;

	/* 
	 * activate the handle
	 */
	slot_device_handle_activate(st, dh);
	error = slot_device_service_open(dh);
	if (error != 0)
	{
		slot_device_handle_deactivate(st, dh);
		return error;
	}

	/* 
	 * activate the device
	 */
	s = splpisa();

	(void) spltarg(PISA_DR_IRQ(dr));
	if (pisa_space_iomem_activate(dh, 1) != 0)
		goto bad;
	if (pisa_device_activate(dh) != 0)
	{
		pisa_space_iomem_deactivate(dh, 1);
bad:
		slot_device_service_close(dh);
		slot_device_handle_deactivate(st, dh);
		splx(s);
		return EIO;
	}

	/* 
	 * enable the interrupt pin
	 */
	pisa_space_intr_activate(sc, dh, 1);
	splx(s);

	return 0;
}

int
pisa_slot_device_deactivate(st, dh, aux)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
	void *aux;
{
	struct pisa_softc *sc = st->st_sc;
	int s;

	PISA_RES_EVENT(dh) = PISA_EVENT_REMOVE;

	s = splpisa();
	if ((dh->dh_flags & DH_ACTIVE) == 0)
		goto out;

	/* 
	 * shutdown the hardware interrupt routings
 	 */
	pisa_space_intr_deactivate(sc, dh, ISA_INTR_INACTIVE, 1);

	/* 
	 * inform the device
 	 */
	pisa_device_deactivate(dh);

	/* 
	 * reclaim the interrupt handlers
 	 */
	pisa_space_intr_deactivate(sc, dh, ISA_INTR_RECLAIM, 1);

	/* 
	 * deactivate space resources
 	 */
	pisa_space_iomem_deactivate(dh, 1);

	/* 
	 * close the device
 	 */
	slot_device_service_close(dh);

	/* 
	 * deactivate handle
 	 */
	slot_device_handle_deactivate(st, dh);

out:
	splx(s);
	return 0;
}

int
pisa_slot_device_vanish(st, dh, aux)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
	void *aux;
{

	pisa_space_intr_deactivate(st->st_sc, dh, ISA_INTR_INACTIVE, 1);
	return 0;
}

int
pisa_slot_device_notify(st, dh, ev)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
	slot_device_event_t ev;
{
	struct pisa_functions *pd;
	int s, error = 0;

	if ((pd = PISA_RES_FUNCTIONS(dh)) == NULL || pd->pd_notify == NULL)
		return error;

	s = splpisa();
	if ((dh->dh_flags & DH_ACTIVE) == 0)
		goto out;
	if ((dh->dh_flags & DH_BUSY) != 0)
	{
		error = SD_EVENT_STATUS_BUSY;
		goto out;
	}
	
	error = ((*pd->pd_notify) (dh, ev));

out:
	splx(s);
	return error;
}

/******************************************************
 * compatiblity functions (stupid!)
 ******************************************************/
int
pisa_slot_device_enable(dh)
	slot_device_handle_t dh;
{
	int s, error = 0;

	s = splpisa();
	if ((dh->dh_flags & (DH_ACTIVE | DH_BUSY)) != DH_ACTIVE)
	{
		splx(s);
		return EINVAL;
	}
	slot_device_handle_busy(dh);
	splx(s);

	error = slot_device_service_open(dh);

	s = splpisa();
	if (error == 0)
	{
		pisa_space_intr_activate(NULL, dh, 0);
		error = pisa_space_iomem_activate(dh, 0);
		if (error != 0)
		{
			pisa_space_intr_deactivate(NULL, dh,
						   ISA_INTR_INACTIVE, 0);
			slot_device_service_close(dh);
		}
	}
	slot_device_handle_unbusy(dh);
	splx(s);

	return error;
}

int
pisa_slot_device_disable(dh)
	slot_device_handle_t dh;
{
	int s;

	s = splpisa();
	if ((dh->dh_flags & (DH_ACTIVE | DH_BUSY)) != DH_ACTIVE)
	{
		splx(s);
		return EINVAL;
	}

	slot_device_handle_busy(dh);
	pisa_space_intr_deactivate(NULL, dh, ISA_INTR_INACTIVE, 0);
	pisa_space_iomem_deactivate(dh, 0);
	splx(s);

	slot_device_service_close(dh);

	s = splpisa();
	slot_device_handle_unbusy(dh);
	splx(s);

	return 0;
}

/******************************************************
 * space handle register
 ******************************************************/
#define	PISA_DEBUG(s)	{printf("%s\n", s);}

int pisa_im_unmap __P((slot_device_handle_t, slot_device_bmap_t));
int pisa_im_map __P((slot_device_handle_t, slot_device_bmap_t));
#ifdef	PHYSWIN_NE_HOSTWIN
int pisa_scan_bmap __P((slot_device_handle_t, bus_space_tag_t t, bus_space_handle_t, bus_size_t, slot_device_bmap_t *));
#endif	/* PHYSWIN_NE_HOSTWIN */
int pisa_space_map_subr __P((pisa_device_handle_t, u_int, bus_size_t, bus_space_tag_t *, bus_space_handle_t *, int *));
int pisa_handle_lookup __P((pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, int *));

int
pisa_im_unmap(dh, wm)
	slot_device_handle_t dh;
	slot_device_bmap_t wm;
{
	u_int idx;

	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
		if (wm & (1 << idx))
			(void) slot_device_service_im_unmap(dh, idx);
	return 0;
}

int
pisa_im_map(dh, wm)
	slot_device_handle_t dh;
	slot_device_bmap_t wm;
{
	slot_device_bmap_t nwm = 0;
	u_int idx;
	int error = 0;

	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
	{
		if ((wm & (1 << idx)) == 0)
			continue;

		error = slot_device_service_im_map(dh, idx);
		if (error != 0)
		{
			pisa_im_unmap(dh, nwm);
			return error;
		}
		nwm |= (1 << idx);
	}
	return error;
}

#ifdef	PHYSWIN_NE_HOSTWIN
int
pisa_scan_bmap(dh, t, bah, size, bpp)
	slot_device_handle_t dh;
	bus_space_tag_t t;
	bus_space_handle_t bah;
	bus_size_t size;
	slot_device_bmap_t *bpp;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	struct pisa_softc *sc = st->st_bus;
	slot_device_res_t dr = PISA_RES_DR(dh);
	struct slot_device_iomem *imp;
	bus_addr_t addr;
	u_int idx, i;

	*bpp = 0;

	/* XXX:
	 * much better method requried!
	 */
	for(i = 0; i < size; i ++)
	{
		BUS_ADDR_CALCULATE(addr, bah, i);	/* XXX */
		if (t == sc->sc_memt)
			addr = vtophys(addr);

		imp = &dr->dr_im[0];
		for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++, imp ++)
		{
			if (imp->im_hwbase == SLOT_DEVICE_UNKVAL)
				continue;

			if (t == sc->sc_memt && idx < SLOT_DEVICE_NIO)
				continue;

			if (addr >= imp->im_hwbase &&
			    addr < imp->im_hwbase + imp->im_size)
			{
				*bpp |= 1 << idx;
				break;
			}
		}
	
		if (idx == SLOT_DEVICE_NIM)
		{
			*bpp = 0;
			PISA_DEBUG("scan_map: out of range");
			return EINVAL;
		}
	}

	if (*bpp == 0)
		return (size == 0) ? 0 : EINVAL;
	return 0;
}
#endif	/* PHYSWIN_NE_HOSTWIN */
	
int
pisa_handle_lookup(dh, t, bah, idxp)
	pisa_device_handle_t dh;
	bus_space_tag_t t;
	bus_space_handle_t bah;
	int *idxp;
{
	int idx;

	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
		if (PISA_IMTAG(dh, idx) == t && PISA_IMHAND(dh, idx) == bah)
			break;

	if (idx >= SLOT_DEVICE_NIM)
		return ENOENT;

	*idxp = idx;
	return 0;
}

int
pisa_space_map_subr(dh, wn, size, tp, bshp, idxp)
	pisa_device_handle_t dh;
	u_int wn;
	bus_size_t size;
	bus_space_tag_t *tp;
	bus_space_handle_t *bshp;
	int *idxp;
{
	slot_device_slot_tag_t st = dh->dh_stag;
	struct pisa_softc *sc = st->st_bus;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t t;
	bus_addr_t addr;
	int idx, error, type;

	if (wn >= SLOT_DEVICE_NIM)
		return EINVAL;

	/* resource type sanity check ! */
	type = dr->dr_im[wn].im_type;
	if ((type == SLOT_DEVICE_SPIO && wn >= SLOT_DEVICE_NIO) ||
	    (type == SLOT_DEVICE_SPMEM && wn < SLOT_DEVICE_NIO))
	{
		PISA_DEBUG("space_map: inconsistent resource type");
		return EINVAL;
	}

	t = (wn < SLOT_DEVICE_NIO) ? sc->sc_iot : sc->sc_memt;
	addr = PISA_DR_IMB(dr, wn);
	if (size == PISA_UNKVAL)
		size = PISA_DR_IMS(dr, wn);

	if (addr == PISA_UNKVAL || size == PISA_UNKVAL)
		return EINVAL;

	/*
	 *  scan a free slot for a space handle 
	 */
	if (pisa_handle_lookup(dh, t, NULL, &idx))
		return ENOSPC;

	/*
	 *  allocate a space handle corresponding to the device window.
	 */
	error = bus_space_map(t, addr, size, 0, bshp);
	if (error != 0)
		return error;

	/*
	 * translate a device window to physical device windows.
	 */
	if (size > 0)
		PISA_IMWMAP(dh, idx) = 1 << wn;

	/*
	 *  open physical device windows
	 */
	error = pisa_im_map(dh, PISA_IMWMAP(dh, idx));
	if (error != 0)
	{
		PISA_IMWMAP(dh, idx) = 0;
		PISA_DEBUG("space_map: physical map failure");
		bus_space_unmap(t, *bshp, 0);
		return error;
	}

	/*
	 *  register them.
	 */
	PISA_IMHAND(dh, idx) = *bshp;
	*tp = t;
	*idxp = idx;
	return 0;
}

int
pisa_space_map(dh, wn, bshp)
	pisa_device_handle_t dh;
	u_int wn;
	bus_space_handle_t *bshp;
{
	bus_space_tag_t t;
	int idx;
	
	return pisa_space_map_subr(dh, wn, PISA_UNKVAL, &t, bshp, &idx);
}

int
pisa_space_unmap(dh, t, bah)
	pisa_device_handle_t dh;
	bus_space_tag_t t;
	bus_space_handle_t bah;
{
	int idx;

	/*
	 *  scan the space handle slot.
	 */
	if (pisa_handle_lookup(dh, t, bah, &idx))
		goto out;

	if (PISA_IMWMAP(dh, idx) != 0)
	{
		/*
		 * unmap physical device windows.
		 */
		pisa_im_unmap(dh, PISA_IMWMAP(dh, idx));
		PISA_IMWMAP(dh, idx) = 0;
	}

	/*
	 * free the space handle slot.
	 */
	PISA_IMHAND(dh, idx) = NULL;

out:
	bus_space_unmap(t, bah, 0);
	return 0;
}
	
int
pisa_space_map_load(dh, wn, size, iat, bshp)
	slot_device_handle_t dh;
	u_int wn;
	bus_size_t size;
	bus_space_iat_t iat;
	bus_space_handle_t *bshp;
{
	int idx, error;
	bus_space_tag_t t;
	bus_space_handle_t bah;

	error = pisa_space_map_subr(dh, wn, 0, &t, &bah, &idx);
	if (error != 0)
		return error;

	/*
	 * load address relocate table.
	 */
	error = bus_space_map_load(t, bah, size, iat, 0);
	if (error != 0)
		goto out;	

#ifdef	PHYSWIN_NE_HOSTWIN
	/*
	 * translate a device window to physical device windows.
	 */
	error = pisa_scan_bmap(dh, t, bah, size, &PISA_IMWMAP(dh, idx));
	if (error != 0)
	{
		PISA_DEBUG("space_map_load: bmap failure");
		goto out;
	}
#else	/* !PHYSWIN_NE_HOSTWIN */
	PISA_IMWMAP(dh, idx) = 1 << wn;
#endif	/* !PHYSWIN_NE_HOSTWIN */

	/*
	 *  open physical device windows
	 */
	error = pisa_im_map(dh, PISA_IMWMAP(dh, idx));
	if (error != 0)
	{
		PISA_DEBUG("space_map_load: physical map failure");
		PISA_IMWMAP(dh, idx) = 0;

out:
		PISA_IMHAND(dh, idx) = NULL;
		bus_space_unmap(t, bah, 0);
		return error;
	}

	*bshp = bah;
	return 0;
}

int
pisa_space_iomem_activate(dh, bus)
	pisa_device_handle_t dh;
	int bus;
{
	bus_space_tag_t t;
	bus_space_handle_t bah;
	slot_device_bmap_t imm;
	int idx, error = 0;

	imm = 0;
	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
	{
		if (PISA_IMHAND(dh, idx) != NULL)
		{
			t = PISA_IMTAG(dh, idx);
			bah = PISA_IMHAND(dh, idx);

			if (bus)
			{
				error = bus_space_map_activate(t, bah, 0);
				if (error != 0)
					break;
			}

			if (pisa_im_map(dh, PISA_IMWMAP(dh, idx)) != 0)
			{
				if (bus)
					bus_space_map_deactivate(t, bah, 0);
				break;
			}

			imm |= (1 << idx);
		}
	}

	if (error != 0)
	{
		for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
		{
			if ((imm & (1 << idx)) != 0)
			{
				t = PISA_IMTAG(dh, idx);
				bah = PISA_IMHAND(dh, idx);
				pisa_im_unmap(dh, PISA_IMWMAP(dh, idx));
				if (bus)
					bus_space_map_deactivate(t, bah, 0);
			}
		}
	}
	
	return error;
}

int
pisa_space_iomem_deactivate(dh, bus)
	pisa_device_handle_t dh;
	int bus;
{
	int idx;

	for (idx = 0; idx < SLOT_DEVICE_NIM; idx ++)
	{
		if (PISA_IMHAND(dh, idx) != NULL)
		{
			pisa_im_unmap(dh, PISA_IMWMAP(dh, idx));
			if (bus)
			{
				bus_space_tag_t t;
				bus_space_handle_t bah;

				t = PISA_IMTAG(dh, idx);
				bah = PISA_IMHAND(dh, idx);
				bus_space_map_deactivate(t, bah, 0);
			}
		}
	}

	return 0;
}

int
pisa_space_intr_activate(sc, dh, bus)
	struct pisa_softc *sc;
	slot_device_handle_t dh;
	int bus;
{
	struct device *dv = dh->dh_dv;
	slot_device_res_t dr = PISA_RES_DR(dh);
	void *ih;
	int pin, s;

	s = splpisa();
	for (pin = 0; pin < SLOT_DEVICE_NPIN; pin ++)
	{
		ih = PISA_PINHAND(dh, pin);
		if (ih == NULL)
			continue; 

		if (bus)
			isa_intr_activate(sc->sc_ic, ih, PISA_DR_IRQN(dr, pin),
				  	  dv->dv_xname);
		(void) slot_device_service_intr_map(dh, pin);
	}
	splx(s);

	return 0;
}

int
pisa_space_intr_deactivate(sc, dh, type, bus)
	struct pisa_softc *sc;
	slot_device_handle_t dh;
	int type, bus;
{
	void *ih;
	int pin, s;

	s = splpisa();
	for (pin = 0; pin < SLOT_DEVICE_NPIN; pin ++)
	{
		ih = PISA_PINHAND(dh, pin);
		if (ih == NULL)
			continue;

		if (type == ISA_INTR_INACTIVE)
			(void) slot_device_service_intr_unmap(dh, pin);
		if (bus)
			isa_intr_deactivate(sc->sc_ic, ih, type);
	}
	splx(s);
	return 0;
}

void *
pisa_intr_establish(dh, pin, level, fun, arg)
	pisa_device_handle_t dh;
	int pin;
	int level;
	int (*fun) __P((void *));
	void *arg;
{
	slot_device_res_t dr = PISA_RES_DR(dh);
	slot_device_slot_tag_t st = dh->dh_stag;
	slot_device_intr_arg_t sa = NULL;
	struct pisa_softc *sc = st->st_bus;
	int (*rfun) __P((void *));
	void *ih = NULL, *rarg;;
	
	if (pin > PISA_PIN1 || dr->dr_pin[pin].dc_chan == PISA_UNKVAL)
		return ih;
		
	/*
	 * map the physical interrupt pin.
	 */
	if (slot_device_service_intr_map(dh, pin) != 0)
		return ih;

	/*
	 * check direct or indirect interrupt call.
	 */
	if ((st->st_flags & SD_STAG_INTRACK) != 0)
	{
		sa = slot_device_allocate_intr_arg(dh, pin, fun, arg);
		if (sa == NULL)
			goto bad;

		rfun = SLOT_DEVICE_INTR_FUNC(st);
		rarg = sa;
	}
	else
	{
		rfun = fun;
		rarg = arg;
	}

	/*
	 * allocate an intr handle.
	 */
	ih = isa_intr_establish(sc->sc_ic, dr->dr_pin[pin].dc_chan, IST_EDGE,
				level, rfun, rarg);
	if (ih == NULL)
		goto bad;

	/*
	 * register it.
	 */
	if (PISA_PINHAND(dh, pin) != NULL)
		return ih;

	PISA_PINHAND(dh, pin) = ih;
	isa_intr_activate(NULL, ih, IRQUNK, dh->dh_dv->dv_xname);
	return ih;

bad:
	if (sa != NULL)
		slot_device_deallocate_intr_arg(sa);
	slot_device_service_intr_unmap(dh, pin);
	return ih;
}
	
int
pisa_update_resources(dr, ndr)
	slot_device_res_t dr, ndr;
{
	int pin;

	for (pin = 0; pin < SLOT_DEVICE_NPIN; pin ++)
	{
		if ((PISA_DR_IRQN(ndr, pin) == PISA_UNKVAL) != 
		    (PISA_DR_IRQN(dr, pin) == PISA_UNKVAL))
			return EINVAL;

		PISA_DR_IRQN(dr, pin) = PISA_DR_IRQN(ndr, pin);
	}

	PISA_DR_DVCFG(dr) = PISA_DR_DVCFG(ndr);
	return 0;
}

pisa_device_handle_t
pisa_register_functions(dh, dev, pd)
	pisa_device_handle_t dh;
	struct device *dev;
	struct pisa_functions *pd;
{

	if (dh == NULL || dh->dh_res == NULL)
		panic("pisa: null device handle");

	if (PISA_RES_FUNCTIONS(dh) != NULL)
		return NULL;

	PISA_RES_FUNCTIONS(dh) = pd;
	dh->dh_dv = dev;
	return dh;
}

/******************************************************
 * callback information request
 ******************************************************/
u_int8_t *
pisa_device_info(dh, code)
	slot_device_handle_t dh;
	u_int code;
{
	slot_device_slot_tag_t st;

	if (dh == NULL || (st = dh->dh_stag) == NULL)
		return NULL;

	return slot_device_service_info(dh, code);
}
