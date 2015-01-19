/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>
#include <i386/pccs/tuple.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <i386/Cbus/dev/dp8390pio.h>
#include <i386/Cbus/dev/dp8390shm.h>
#include <i386/Cbus/dev/endsubr.h>

struct sic_pisa_vendor_tag;
static int sic_pisa_map __P((struct pisa_attach_args *,\
			     struct sic_pisa_vendor_tag *,\
			     bus_space_tag_t *,\
			     bus_space_tag_t *,\
			     bus_space_tag_t *,\
			     bus_space_handle_t *,\
			     bus_space_handle_t *,\
			     bus_space_handle_t *));
static void sic_pisa_unmap __P((struct pisa_attach_args *,\
			     struct sic_pisa_vendor_tag *,\
			     bus_space_tag_t,\
			     bus_space_tag_t,\
			     bus_space_tag_t,\
			     bus_space_handle_t,\
			     bus_space_handle_t,\
			     bus_space_handle_t));
static int sic_pisa_apex_getnd __P((struct sic_pisa_vendor_tag *,\
			     bus_space_tag_t,\
			     bus_space_tag_t,\
			     bus_space_tag_t,\
			     bus_space_handle_t,\
			     bus_space_handle_t,\
			     bus_space_handle_t,\
			     u_int8_t *));
static struct sic_pisa_vendor_tag *sic_pisa_find_vt __P((pisa_device_handle_t));
static int sic_pisa_probe __P((struct device *, struct cfdata *, void *));
static void sic_pisa_attach __P((struct device *, struct device *, void *));
static int sic_pisa_deactivate __P((pisa_device_handle_t));
static int sic_pisa_activate __P((pisa_device_handle_t));
int sic_pisa_enable __P((struct dp8390_softc *));
void sic_pisa_disable __P((struct dp8390_softc *));

struct sic_pisa_vendor_tag {
	u_char *sv_name;
	u_long sv_bus;
	u_long sv_id;

	u_long sv_nicw;		/* nic window */
	u_long sv_asicw;	/* asic window */
	u_long sv_bufw;		/* buf window    */
	u_long sv_ramstart;	/* buffer offset */
	u_long sv_ramsize;	/* buffer size */

	int sv_bustype;		/* bustype */

	int (*sv_getnd) __P((struct sic_pisa_vendor_tag *,\
			     bus_space_tag_t,\
			     bus_space_tag_t,\
			     bus_space_tag_t,\
			     bus_space_handle_t,\
			     bus_space_handle_t,\
			     bus_space_handle_t,\
			     u_int8_t *));
};

struct sic_pisa_vendor_tag sic_pisa_vendor_tag[] = {
	{
		"apex",
		PISA_SLOT_PCMCIA,
	    	0x130000,

		PISA_MEM0,
		PISA_MEM0,
		PISA_MEM0,
		0x100,
		0x3f00,

		DP8390_BUS_SHM8,
		sic_pisa_apex_getnd,
	},

	{
		NULL,
	}
};

struct sic_pisa_softc {
	dp8390shm_softc_t sc_dsc;

	struct sic_pisa_vendor_tag *sc_svp;
	pisa_device_handle_t sc_dh;
	void	*sc_ih;
};

struct cfattach sic_pisa_ca = {
	sizeof(struct sic_pisa_softc), sic_pisa_probe, sic_pisa_attach
};

struct pisa_functions sic_pisa_pd = {
	sic_pisa_activate, sic_pisa_deactivate,
};

static struct sic_pisa_vendor_tag *
sic_pisa_find_vt(dh)
	pisa_device_handle_t dh;
{
	struct sic_pisa_vendor_tag *svp;

	for (svp = &sic_pisa_vendor_tag[0]; svp->sv_name != NULL; svp ++)
	{
		if (dh->dh_stag->st_type != svp->sv_bus)
			continue;
		
		if (SLOT_DEVICE_IDENT_IDN(dh) == svp->sv_id)
			return svp;
	}

	return NULL;
}

static int
sic_pisa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_tag_t nict, asict, buft;
	bus_space_handle_t nich, asich, bufh;
	struct sic_pisa_vendor_tag *svp;

	svp = sic_pisa_find_vt(dh);
	if (svp == NULL)
		return 0;

	if (sic_pisa_map(pa, svp, &nict, &asict, &buft, &nich, &asich, &bufh))
		return 0;
	sic_pisa_unmap(pa, svp, nict, asict, buft, nich, asich, bufh);
	return 1;
}

static void
sic_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sic_pisa_softc *nsc = (void *) self;
	dp8390shm_softc_t *dsc = &nsc->sc_dsc;
	struct dp8390_softc *sc = &dsc->sc_dp8390;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_tag_t nict, asict, buft;
	bus_space_handle_t nich, asich, bufh;
	struct sic_pisa_vendor_tag *svp;
	bus_size_t ramsize;
	bus_addr_t ramstart;
	int error, bustype;
	u_int8_t  nodep[ETHER_ADDR_LEN];

	printf("\n");
	pisa_register_functions(dh, self, &sic_pisa_pd);

	svp = sic_pisa_find_vt(dh);
	if (sic_pisa_map(pa, svp, &nict, &asict, &buft, &nich, &asich, &bufh))
		return;

	nsc->sc_dh = dh;
	nsc->sc_svp = svp;
	sc->sc_enabled = 1;
	sc->sc_enable = sic_pisa_enable;
	sc->sc_disable = sic_pisa_disable;
	sc->cr_proto = ED_CR_RD2;
	sc->sc_regt = nict;
	sc->sc_regh = nich;
	sc->sc_buft = buft;
	sc->sc_bufh = bufh;
	dsc->sc_asict = asict;
	dsc->sc_asich = asich;

	ramstart = svp->sv_ramstart;
	ramsize = svp->sv_ramsize;
	bustype = svp->sv_bustype;

	error = ((*svp->sv_getnd) (svp, nict, asict, buft,
				   nich, asich, bufh, nodep));
	if (error != 0)
		goto bad;

	error = dp8390shm_attachsubr(dsc, bustype, ramstart, ramsize, nodep,
				     0, 0, 0);
	if (error != 0)
		goto bad;

	nsc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_NET,
					 dp8390_intr, sc);
	return;

bad:
	sc->sc_enabled = 0;
}

static int
sic_pisa_activate(dh)
	pisa_device_handle_t dh;
{
	struct sic_pisa_softc *nsc = PISA_DEV_SOFTC(dh);
	dp8390shm_softc_t *dsc = &nsc->sc_dsc;
	struct dp8390_softc *sc = &dsc->sc_dp8390;
	int s = splimp();

	sc->sc_enabled = 1;
	splx(s);
	return 0;
}

static int
sic_pisa_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct sic_pisa_softc *nsc = PISA_DEV_SOFTC(dh);
	dp8390shm_softc_t *dsc = &nsc->sc_dsc;
	struct dp8390_softc *sc = &dsc->sc_dp8390;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int s = splimp();

	if_down(ifp);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_UP);
	sc->sc_enabled = 0;
	splx(s);
	return 0;
}

int
sic_pisa_enable(sc)
	struct dp8390_softc *sc;
{
	struct sic_pisa_softc *nsc = (void *) sc;
	int error;

	error = pisa_slot_device_enable(nsc->sc_dh);
	return error;
}

void
sic_pisa_disable(sc)
	struct dp8390_softc *sc;
{
	struct sic_pisa_softc *nsc = (void *) sc;

	pisa_slot_device_disable(nsc->sc_dh);
}

/********************************************************************
 * mapping spaces
 ********************************************************************/
static int
sic_pisa_map(pa, svp, nictp, asictp, buftp, nichp, asichp, bufhp)
	struct pisa_attach_args *pa;
	struct sic_pisa_vendor_tag *svp;
	bus_space_tag_t *nictp, *asictp, *buftp;
	bus_space_handle_t *nichp, *asichp, *bufhp;
{
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	int type;

	if (PISA_DR_IMS(dr, svp->sv_nicw) == PISA_UNKVAL ||
	    PISA_DR_IMS(dr, svp->sv_asicw) == PISA_UNKVAL ||
	    PISA_DR_IMS(dr, svp->sv_bufw) == PISA_UNKVAL)
		return EINVAL;

	if (svp->sv_nicw != svp->sv_asicw)
	{
		/* XXX: currently assume nicw == asicw */
		return EINVAL;
	}

	PISA_DR_IMF(dr, svp->sv_nicw) |= SDIM_BUS_FOLLOW | SDIM_BUS_WEIGHT;
	PISA_DR_IMF(dr, svp->sv_asicw) |= SDIM_BUS_FOLLOW | SDIM_BUS_WEIGHT;
	PISA_DR_IMF(dr, svp->sv_bufw) |= SDIM_BUS_FOLLOW | SDIM_BUS_WEIGHT;

	type = dr->dr_im[svp->sv_nicw].im_type;
	*nictp = (type == SLOT_DEVICE_SPMEM) ?  pa->pa_memt : pa->pa_iot;
	*asictp = *nictp;

	type = dr->dr_im[svp->sv_bufw].im_type;
	*buftp = (type == SLOT_DEVICE_SPMEM) ?  pa->pa_memt : pa->pa_iot;

	if (pisa_space_map(dh, svp->sv_nicw, nichp))
		return ENOSPC;

	if (bus_space_subregion(*nictp, *nichp, NE2000_ASIC_OFFSET,
				NE2000_ASIC_NPORTS, asichp))
	{
		pisa_space_unmap(dh, *nictp, *nichp);
		return ENOSPC;
	}

	if (svp->sv_nicw != svp->sv_bufw)
	{
		if (pisa_space_map(dh, svp->sv_bufw, bufhp) == 0)
			return 0;
	}
	else
	{
		*bufhp = *nichp;
		return 0;
	}

	bus_space_unmap(*asictp, *asichp, NE2000_ASIC_NPORTS);
	pisa_space_unmap(dh, *nictp, *nichp);
	return EINVAL;
}

static void
sic_pisa_unmap(pa, svp, nict, asict, buft, nich, asich, bufh)
	struct pisa_attach_args *pa;
	struct sic_pisa_vendor_tag *svp;
	bus_space_tag_t nict, asict, buft;
	bus_space_handle_t nich, asich, bufh;
{
	pisa_device_handle_t dh = pa->pa_dh;

	if (svp->sv_nicw != svp->sv_bufw)
	{
		pisa_space_unmap(dh, buft, bufh);
	}
	bus_space_unmap(asict, asich, NE2000_ASIC_NPORTS);
	pisa_space_unmap(dh, nict, nich);
}

/***************************************************
 * APEX MODEM/ETHER
 ***************************************************/
#define	APEX_INDEX_CRR		0x6
#define	APEX_INDEX_DATAR	0x7
#define	APEX_RESET_CRR		0x410
#define	APEX_BUF_MEMSIZE	0x3f00
#define	APEX_BUF_MEMSTART	0x100

static int
sic_pisa_apex_getnd(svp, nict, asict, buft, nich, asich, bufh, ndp)
	struct sic_pisa_vendor_tag *svp;
	bus_space_tag_t nict, asict, buft;
	bus_space_handle_t nich, asich, bufh;
	u_int8_t *ndp;
{
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
	{
		bus_space_write_1(asict, asich, APEX_INDEX_CRR, i);
		ndp[i] = bus_space_read_1(asict, asich, APEX_INDEX_DATAR);
	}
	return 0;
}
