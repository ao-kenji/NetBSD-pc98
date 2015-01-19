/*	$NecBSD: if_ne_pisa.c,v 1.37.6.7 1999/09/26 08:03:27 kmatsuda Exp $	*/
/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
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
#include <i386/Cbus/dev/endsubr.h>

static int ne_pisa_map __P((pisa_device_handle_t, bus_space_tag_t, u_int *, u_int *, bus_space_handle_t *, bus_space_handle_t *));
static void ne_pisa_unmap __P((pisa_device_handle_t, bus_space_tag_t, u_int, u_int, bus_space_handle_t, bus_space_handle_t));

static int ne_pisa_probe __P((struct device *, struct cfdata *, void *));
static void ne_pisa_attach __P((struct device *, struct device *, void *));
static int ne_pisa_deactivate __P((pisa_device_handle_t));
static int ne_pisa_activate __P((pisa_device_handle_t));
static void ne_resume_check __P((void *));
static int ne_pisa_intr __P((void *));
int ne_pisa_enable __P((struct dp8390_softc *));
void ne_pisa_disable __P((struct dp8390_softc *));

struct ne_vendor_tag;
struct ne_pisa_softc {
	dp8390pio_softc_t sc_dsc;

	pisa_device_handle_t sc_dh;
	struct ne_vendor_tag *sc_vtp;

	LIST_ENTRY(ne_pisa_softc) sc_rchain;
	int sc_rstep;
	int sc_rintr;

	u_int8_t sc_enaddr[ETHER_ADDR_LEN];
	void	*sc_ih;
};

static LIST_HEAD(ne_pisa_resume_tab, ne_pisa_softc) ne_pisa_resume_tab;

struct cfattach ne_pisa_ca = {
	sizeof(struct ne_pisa_softc), ne_pisa_probe, ne_pisa_attach
};

struct pisa_functions ne_pisa_pd = {
	ne_pisa_activate, ne_pisa_deactivate,
};

struct ne_vendor_id_tag {
	u_char *vid_name;
	
	int vid_bustype;
	u_int vid_vnd;
	u_int vid_vndmask;
};

struct ne_vendor_tag {
	struct ne_vendor_id_tag *vt_vid;

	int vt_flags;
#define	NE_PISA_RESUME_OK	0x0001
#define	NE_PISA_RAMSIZE_DETECT	0x0002
#define	NE_PISA_FULLDUPLEX_BLOCK  0x0004

	int (*vt_get_nd) __P((struct ne_vendor_tag *, pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, u_int8_t *));
	paddr_t vt_ndofs;

	int *vt_media;
	int vt_nmedia;
	int vt_defmedia;

	void 	(*vt_init_card) __P((struct dp8390_softc *));
	void 	(*vt_set_media) __P((struct ne_pisa_softc *, int));
	int	(*vt_mediachange) __P((struct dp8390_softc *));
	void	(*vt_mediastatus) __P((struct dp8390_softc *, struct ifmediareq *));
};

static struct ne_vendor_tag *ne_pisa_find_vt __P((pisa_device_handle_t));
static int PCCard_get_nd_from_asic_regs __P((struct ne_vendor_tag *, pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, u_int8_t *));
static int PCCard_get_nd __P((struct ne_vendor_tag *, pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, u_int8_t *));
static int PCCard_get_nd_from_cis __P((struct ne_vendor_tag *, pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, u_int8_t *));
static int default_get_nd __P((struct ne_vendor_tag *, pisa_device_handle_t, bus_space_tag_t, bus_space_handle_t, u_int8_t *));
static int ne_pisa_mediachange __P((struct dp8390_softc *));
static void ne_pisa_mediastatus __P((struct dp8390_softc *, struct ifmediareq *));
static void dl109_set_media __P((struct ne_pisa_softc *, int));
static void de660_set_media __P((struct ne_pisa_softc *, int));
struct ne_vendor_tag ne_pisa_vendor_tag[];

static int ne_pisa_auto_media_standard[] = {
	IFM_ETHER|IFM_AUTO,
	IFM_ETHER|IFM_AUTO|IFM_FDX,
#ifdef	notyet
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_T|IFM_FDX,
	IFM_ETHER|IFM_100_TX,
	IFM_ETHER|IFM_100_TX|IFM_FDX,
#endif	/* notyet */
};

static struct ne_vendor_tag *
ne_pisa_find_vt(dh)
	pisa_device_handle_t dh;
{
	struct ne_vendor_tag *vtp;
	struct ne_vendor_id_tag *vidp;
	u_int id;

	for (vtp = &ne_pisa_vendor_tag[0]; vtp->vt_vid != NULL; vtp ++)
	{
		for (vidp = vtp->vt_vid; vidp->vid_name != NULL; vidp ++)
		{
			if (dh->dh_stag->st_type != vidp->vid_bustype)
				continue;
		
			id = SLOT_DEVICE_IDENT_IDN(dh) | (~vidp->vid_vndmask);
			if (id == vidp->vid_vnd)
				return vtp;
		}
	}
	return vtp;
}


static int
ne_pisa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t coreh, asich;
	u_int core, asic;
	int rv, bustype = DP8390_BUS_PIO16;
	struct ne_vendor_tag *vtp;

	if (ne_pisa_map(dh, iot, &core, &asic, &coreh, &asich))
		return 0;

	vtp = ne_pisa_find_vt(dh);
	rv = dp8390pio_detectsubr(bustype, iot, coreh, iot, asich,
		pa->pa_memt, NULL, 0, 0, 0);
	rv = (rv > 0);

	ne_pisa_unmap(dh, iot, core, asic, coreh, asich);
	return rv;
}

static void
ne_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ne_pisa_softc *nsc = (void *) self;
	dp8390pio_softc_t *dsc = &nsc->sc_dsc;
	struct dp8390_softc *sc = &dsc->sc_dp8390;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_handle_t coreh, asich;
	struct ne_vendor_tag *vtp;
	u_int core, asic;
	int rv, bustype = DP8390_BUS_PIO16;
	int (*rfun) __P((void *));
	bus_size_t ramsize;
	bus_addr_t ramstart;
	int error;
	static int ne_pisa_attach_already = 0;

	printf("\n");
	nsc->sc_vtp = vtp = ne_pisa_find_vt(dh);
	if (ne_pisa_map(dh, pa->pa_iot, &core, &asic, &coreh, &asich))
	{
		printf("%s: can not map io spaces\n", sc->sc_dev.dv_xname);
		return;
	}

	nsc->sc_dh = dh;
	pisa_register_functions(dh, self, &ne_pisa_pd);

	sc->sc_regt = pa->pa_iot;
	sc->sc_regh = coreh;
	sc->sc_enabled = 1;
	sc->sc_enable = ne_pisa_enable;
	sc->sc_disable = ne_pisa_disable;
	sc->sc_mediachange = vtp->vt_mediachange;
	sc->sc_mediastatus = vtp->vt_mediastatus;
	sc->init_card = vtp->vt_init_card;

	dsc->sc_asict = pa->pa_iot;
	dsc->sc_asich = asich;

	/*
	 * setup the media and node addr.
	 */
	if (vtp->vt_set_media != NULL)
		(*vtp->vt_set_media) (nsc, vtp->vt_defmedia);
	rv = ((*vtp->vt_get_nd) (vtp, dh, pa->pa_iot, asich, nsc->sc_enaddr));

	/*
	 * detect the ramsize.
	 */
	ramsize = 0;
	ramstart = 0;
	if ((vtp->vt_flags & NE_PISA_RAMSIZE_DETECT) != 0)
	{
		int res;
		struct ne_pisa_ramsize {
			bus_addr_t nr_addr;
			bus_size_t nr_size;
		} *nrp, ne_pisa_ramsize[] = {
			{0, 0x10000},		/* 64k */
			{0, 0x8000},		/* 32k */
			{0x4000, 0x8000},	/* 32k */
			{0x4000, 0x4000},	/* 16k */
			{0, 0}
		};

		for (nrp = &ne_pisa_ramsize[0]; nrp->nr_size != 0; nrp ++)
		{
			res = dp8390pio_detectsubr(bustype, pa->pa_iot, coreh,
						   pa->pa_iot, asich, 
						   pa->pa_memt, NULL,
						   nrp->nr_addr, nrp->nr_size, 0);
			if (res > 0)
			{
				printf("%s: ramsize=0x%lx, ramstart=0x%lx\n",
					sc->sc_dev.dv_xname, nrp->nr_size, nrp->nr_addr);
				break;
			}
		}
		ramstart = nrp->nr_addr;
		ramsize = nrp->nr_size;
	}

	/*
	 * attach
	 */
	error = dp8390pio_attachsubr(dsc, bustype, ramstart, ramsize, 
			     rv != 0 ? NULL : nsc->sc_enaddr,
	    		     vtp->vt_media, vtp->vt_nmedia, vtp->vt_defmedia);
	if (error != 0)
	{
		sc->sc_enabled = 0;
		return;
	}

	/*
	 * resume hook
	 */
	if (vtp->vt_flags & NE_PISA_RESUME_OK) 
	{
		if (ne_pisa_attach_already == 0)
		{
			ne_pisa_attach_already = 1;
			LIST_INIT(&ne_pisa_resume_tab);
			timeout(ne_resume_check, NULL, hz);
		}
		LIST_INSERT_HEAD(&ne_pisa_resume_tab, nsc, sc_rchain);
		rfun = ne_pisa_intr;
	}
	else
		rfun = dp8390_intr;

	nsc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_NET, rfun, sc);
}

static int
ne_pisa_intr(arg)
	void *arg;
{
	struct ne_pisa_softc *nsc = (void *) arg;

	nsc->sc_rstep = 0;
	nsc->sc_rintr ++;
	return dp8390_intr(arg);
}

static void
ne_resume_check(arg)
	void *arg;
{
	struct ne_pisa_softc *nsc;
	dp8390pio_softc_t *dsc;
	struct dp8390_softc *sc;
	u_int8_t isr;
	int s;

	for (nsc = LIST_FIRST(&ne_pisa_resume_tab); nsc != NULL;
	     nsc = LIST_NEXT(nsc, sc_rchain))
	{
		dsc = &nsc->sc_dsc;
		sc = &dsc->sc_dp8390;
		if (sc->sc_enabled == 0)
			continue;

		if (nsc->sc_rintr != 0)
		{
			nsc->sc_rintr = 0;
			continue;
		}

		s = splimp();
		isr = bus_space_read_1(sc->sc_regt, sc->sc_regh, ED_P0_ISR);
		if (isr != 0 && isr != 0xff)
		{
			nsc->sc_rstep ++;
			if (nsc->sc_rstep == 3)
			{
				nsc->sc_rstep = 0;
				dp8390_reset(sc);
				log(LOG_WARNING, "%s: resume activity\n",
				    sc->sc_dev.dv_xname);
			}
		}
		splx(s);
	}
	timeout(ne_resume_check, NULL, hz / 2);
}		


static int
ne_pisa_activate(dh)
	pisa_device_handle_t dh;
{
	struct ne_pisa_softc *nsc = PISA_DEV_SOFTC(dh);
	dp8390pio_softc_t *dsc = &nsc->sc_dsc;
	struct dp8390_softc *sc = &dsc->sc_dp8390;
	struct ifmedia *ifm = &sc->sc_media;
	struct ne_vendor_tag *vtp = nsc->sc_vtp;
	int s = splimp();

	nsc->sc_rstep = 0;
	sc->sc_enabled = 1;
	if (vtp->vt_set_media != NULL)
		(*vtp->vt_set_media) (nsc, ifm->ifm_media);
	splx(s);
	return 0;
}

static int
ne_pisa_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct ne_pisa_softc *nsc = PISA_DEV_SOFTC(dh);
	dp8390pio_softc_t *dsc = &nsc->sc_dsc;
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
ne_pisa_enable(sc)
	struct dp8390_softc *sc;
{
	struct ne_pisa_softc *nsc = (void *) sc;
	struct ifmedia *ifm = &sc->sc_media;
	struct ne_vendor_tag *vtp = nsc->sc_vtp;
	int error;

	nsc->sc_rstep = 0;
	error = pisa_slot_device_enable(nsc->sc_dh);
	if (error == 0 && vtp->vt_set_media != NULL)
		(*vtp->vt_set_media) (nsc, ifm->ifm_media);

	return error;
}

void
ne_pisa_disable(sc)
	struct dp8390_softc *sc;
{
	struct ne_pisa_softc *nsc = (void *) sc;

	pisa_slot_device_disable(nsc->sc_dh);
}

/********************************************************************
 * mapping spaces
 ********************************************************************/
static void
ne_pisa_unmap(dh, iot, core, asic, nich, asich)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	u_int core, asic;
	bus_space_handle_t nich, asich;
{

	if (asic == core)
		bus_space_unmap(iot, asich, NE2000_NPORTS);
	else
		pisa_space_unmap(dh, iot, asich);

	pisa_space_unmap(dh, iot, nich);
}

static int
ne_pisa_map(dh, iot, corep, asicp, nichp, asichp)
	pisa_device_handle_t dh;
	bus_space_tag_t iot;
	u_int *corep, *asicp;
	bus_space_handle_t *nichp, *asichp;
{
	slot_device_res_t dr = PISA_RES_DR(dh);

	if (PISA_DR_IMS(dr, PISA_IO0) == NE2000_NPORTS)
	{
		*asicp = *corep = PISA_IO0;

		PISA_DR_IMF(dr, *corep) |= SDIM_BUS_FOLLOW;
		PISA_DR_IMF(dr, *corep) &= ~SDIM_BUS_WIDTH16;
		if (pisa_space_map(dh, *corep, nichp))
			return ENOSPC;
		if (bus_space_subregion(iot, *nichp, NE2000_ASIC_OFFSET,
					NE2000_ASIC_NPORTS, asichp))
		{
			pisa_space_unmap(dh, iot, *nichp);
			return ENOSPC;
		}
		return 0;
	}
	else if (PISA_DR_IMS(dr, PISA_IO0) == NE2000_NIC_NPORTS  &&
		 PISA_DR_IMB(dr, PISA_IO1) != PISA_UNKVAL &&
		 PISA_DR_IMS(dr, PISA_IO1) == NE2000_ASIC_NPORTS)
	{
		if (PISA_DR_IMB(dr, PISA_IO1) > PISA_DR_IMB(dr, PISA_IO0))
		{
			*corep = PISA_IO0;
			*asicp = PISA_IO1;
		}
		else
		{
			*corep = PISA_IO1;
			*asicp = PISA_IO0;
		}
		
		PISA_DR_IMF(dr, *corep) |= SDIM_BUS_FOLLOW;
		PISA_DR_IMF(dr, *asicp) |= SDIM_BUS_FOLLOW;
		if (pisa_space_map(dh, *corep, nichp))
			return ENOSPC;
		if (pisa_space_map(dh, *asicp, asichp))
		{
			pisa_space_unmap(dh, iot, *nichp);
			return ENOSPC;
		}
		return 0;
	}

	return EINVAL;
}

static int
ne_pisa_mediachange(sc)
	struct dp8390_softc *sc;
{
	struct ne_pisa_softc *nsc = (void *) sc;
	struct ifmedia *ifm = &sc->sc_media;
	struct ne_vendor_tag *vtp = nsc->sc_vtp;
	int s;

	if (sc->sc_enabled == 0)
		return EINVAL;

	s = splimp();
	if (vtp->vt_set_media != NULL)
		(*vtp->vt_set_media) (nsc, ifm->ifm_media);
	dp8390_reset(sc);
	splx(s);
	return 0;
}

static void
ne_pisa_mediastatus(sc, ifmr)
	struct dp8390_softc *sc;
	struct ifmediareq *ifmr;
{
	struct ifmedia *ifm = &sc->sc_media;

	ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

/********************************************************************
 * load nic addr
 ********************************************************************/
static int
default_get_nd(vtp, dh, asict, asich, p)
	struct ne_vendor_tag *vtp;
	pisa_device_handle_t dh;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *p;
{

	return EINVAL;
}

struct ne_nicoffset {
	u_int offset;
};

static struct ne_nicoffset ne_valid_nicoffs[] = {
	{ 0x07f8 }, { 0x0088 }, { 0x00e0 }, { 0x01ba },
	{ 0x0000 }, { 0x001d }, { 0x0020 }, { 0x03f8 },
	{ MADDRUNK }	/* terminate */
};

static int
PCCard_get_nd(vtp, dh, asict, asich, enaddrp)
	struct ne_vendor_tag *vtp;
	pisa_device_handle_t dh;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *enaddrp;
{
	u_int8_t *p;
	struct ne_nicoffset *np;
	u_int rc;

	/* I: cis function entry */
	pisa_device_info(dh, CIS_REQINIT);
	p = pisa_device_info(dh, CIS_REQCODE(CIS_FUNCE, 0x04));
	if (p != NULL)
	{
		p = (u_int8_t *) CIS_DATAP(p);
		goto load;
	}

	/* II: cis illegal entry */
	for (np = &ne_valid_nicoffs[0]; np->offset != MADDRUNK; np ++)
	{
		rc = CIS_REQCODE(CIS_REQINIT, CIS_PAGECODE(np->offset));
		p = pisa_device_info(dh, rc);
		if (p == NULL)
			continue;
		if (end_lookup(&p[CIS_PAGEOFFS(np->offset)]) == 0)
		{
			p = &p[np->offset];
			goto load;
		}
	}

	if (p[0] == 0xff || (p[0] | p[1] | p[2]) == 0 || p[1] & 1)
		return ENOENT;

load:
	bcopy(p, enaddrp, ETHER_ADDR_LEN);
	return 0;
}

static int
PCCard_get_nd_from_cis(vtp, dh, asict, asich, enaddrp)
	struct ne_vendor_tag *vtp;
	pisa_device_handle_t dh;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *enaddrp;
{
	bus_size_t ofs = vtp->vt_ndofs;
	u_int8_t *p;
	u_int rc;

	rc = CIS_REQCODE(CIS_REQINIT, CIS_PAGECODE(ofs));
	p = pisa_device_info(dh, rc);
	if (p == NULL)
		return EINVAL;
	if (end_lookup(&p[CIS_PAGEOFFS(ofs)]) != 0)
		return EINVAL;
	bcopy(&p[ofs], enaddrp, ETHER_ADDR_LEN);
	return 0;
}

static int
PCCard_get_nd_from_asic_regs(vtp, dh, asict, asich, p)
	struct ne_vendor_tag *vtp;
	pisa_device_handle_t dh;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *p;
{
	bus_size_t ofs = vtp->vt_ndofs;

	bus_space_read_region_1(asict, asich, ofs, p, ETHER_ADDR_LEN);
	return end_lookup(p);
}

/****************************************************************
 * Pc cards dl109 family
 ****************************************************************/
/* Remark:
 * Auto negotiation mechanisms and timings of 0x014902 and 0x0149c1 are
 * completely differet.
 */
#define	dl109_misccr 0x0e
#define	DL109_CMDLEN	11
#define	DL109_MCRRCLK	0x80
#define	DL109_MCREEPROM 0x40
#define	DL109_MCRRECALL	0x20
#define	DL109_MCRASIC	0x10
#define	DL109_MCRCS	0x08
#define	DL109_MCRCLK	0x04
#define	DL109_MCRDIN	0x02
#define	DL109_MCRDBIT	0x01

#define	DL109_OPERASE	0x700
#define	DL109_OPREAD	0x600
#define	DL109_OPWRITE	0x500

#define	DL109_EEPROM	0x01
#define	DL109_ASIC	0x02

static void dl109_serial_write __P((bus_space_tag_t, bus_space_handle_t, u_int8_t, u_int, u_int));
static void dl109_serialize __P((bus_space_tag_t, bus_space_handle_t, u_int8_t *, u_int, u_int));
static void dl109_write_data __P((bus_space_tag_t, bus_space_handle_t, u_int, u_int, u_int, u_int));
static u_int dl109_read_data __P((bus_space_tag_t, bus_space_handle_t, u_int, u_int, u_int));
static void dl109_select_0 __P((bus_space_tag_t, bus_space_handle_t, u_int, u_int));
static void dl109_select_1 __P((bus_space_tag_t, bus_space_handle_t, u_int, u_int));
static void dl109_set_asic_and_read_eeprom __P((struct ne_pisa_softc *, bus_addr_t, u_int, u_int));

static void
de660_set_media(nsc, media)
	struct ne_pisa_softc *nsc;
	int media;
{

	if (media & IFM_FDX)
	{
		/* dl109_set_asic_and_read_eeprom(nsc, 4, 0x20, 0);
		dl109_set_asic_and_read_eeprom(nsc, 6, 0x20, 0x0); */

	}
	else
	{
		/* dl109_set_asic_and_read_eeprom(nsc, 4, 0, 0x20);
		dl109_set_asic_and_read_eeprom(nsc, 6, 0, 0x20); */
	}
}

static void
dl109_set_media(nsc, media)
	struct ne_pisa_softc *nsc;
	int media;
{

	if (media & IFM_FDX)
		dl109_set_asic_and_read_eeprom(nsc, 4, 0x400, 0x100);
	else
		dl109_set_asic_and_read_eeprom(nsc, 4, 0, 0x400 | 0x100);
}

static void
dl109_set_asic_and_read_eeprom(nsc, channel, orl, andl)
	struct ne_pisa_softc *nsc;
	bus_addr_t channel;
	u_int orl, andl;
{
	dp8390pio_softc_t *dsc = &nsc->sc_dsc;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int data, cmd;

	static u_int8_t dl109_serialize_start[] = 
		{0x9, 0xd, 0x9, 0xd, 0x9, 0xff};
	static u_int8_t dl109_serialize_sync[] = 
		{0x8, 0xc, 0xc, 0x8, 0xff};
	static u_int8_t dl109_serialize_stop[] = 
		{0x9, 0x1, 0x5, 0x1, 0x5, 0x1, 0x5, 0x1, 0xff};
	static u_int8_t dl109_serialize_end[] = 
		{0x1, 0x5, 0x1, 0x1, 0x5, 0x1, 0xff};

	asict = dsc->sc_asict;
	asich = dsc->sc_asich;

	/* eeprom read operation */
	cmd = DL109_OPREAD | channel;
	dl109_serialize(asict, asich, dl109_serialize_start, DL109_EEPROM, 0);
	dl109_write_data(asict, asich, DL109_CMDLEN, cmd, DL109_EEPROM, 0);
	data = dl109_read_data(asict, asich, 16, DL109_EEPROM, 0);
	dl109_serialize(asict, asich, dl109_serialize_stop, DL109_EEPROM, 0);

	data &= ~(orl | andl);
	data |= orl;

	/* asic setup operation */
	cmd = ((DL109_OPREAD | channel) >> 1);
	dl109_serialize(asict, asich, dl109_serialize_start, DL109_ASIC, 0);
	dl109_write_data(asict, asich, DL109_CMDLEN, cmd, DL109_ASIC, 0);

	dl109_serialize(asict, asich, dl109_serialize_sync, DL109_ASIC, 1);
	dl109_write_data(asict, asich, 16, data, DL109_ASIC, 1);
	dl109_serialize(asict, asich, dl109_serialize_stop, DL109_ASIC, 1);
	dl109_serialize(asict, asich, dl109_serialize_end, DL109_ASIC, 1);

	bus_space_write_1(asict, asich, dl109_misccr, 0);
}

static void
dl109_serial_write(asict, asich, val, esp, ehd)
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t val;
	u_int esp, ehd;
{
	u_int8_t tmp;
	int state = 0, count = 15;

	switch (esp)
	{
	case DL109_ASIC:
		val |= DL109_MCRASIC;
		break;

	case DL109_EEPROM:
		val |= DL109_MCREEPROM;
		break;

	default:
		break;
	}
		
	bus_space_write_1(asict, asich, dl109_misccr, val);
	if (esp == DL109_EEPROM)
	{
		while (count -- > 0)
		{
			tmp = bus_space_read_1(asict, asich, dl109_misccr);
			tmp = bus_space_read_1(asict, asich, dl109_misccr);
			tmp = bus_space_read_1(asict, asich, dl109_misccr);
			tmp &= DL109_MCRRCLK;
			switch (state)
			{
			case 0:
				if (tmp == 0)
					state ++;
				continue;
			case 1:
				if (tmp != 0)
					state ++;
				continue;
			case 2:
				if (tmp == 0)
					state ++;
				continue;
			case 3:
				break;
			}
		}
	}
	delay(1);
}

static void
dl109_serialize(asict, asich, datasp, esp, ehd)
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *datasp;
	u_int esp, ehd;
{
	
	while (*datasp != (u_int8_t) -1)
		dl109_serial_write(asict, asich, *datasp ++, esp, ehd);
}

static void
dl109_write_data(asict, asich, count, data, esp, ehd)
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int count, data;
	u_int esp, ehd;
{
	
	data <<= (16 - count);
	while (count -- > 0)
	{
		if (data & 0x8000)
			dl109_select_0(asict, asich, esp, ehd);
		else
			dl109_select_1(asict, asich, esp, ehd);
		data <<= 1;
	}
}

static u_int
dl109_read_data(asict, asich, count, esp, ehd)
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int count;
	u_int esp, ehd;
{
	u_int tmp, data = 0;
	
	while (count -- > 0)
	{
		data <<= 1;
		dl109_select_1(asict, asich, esp, ehd);
		tmp = bus_space_read_1(asict, asich, dl109_misccr);
#ifdef	DL109_DEBUG
		printf("[0x%x] ", tmp);
#endif	/* DL109_DEBUG */
		data |= (tmp & DL109_MCRDBIT);
	}
#ifdef	DL109_DEBUG
	printf("0x%x\n ", data);
#endif	/* DL109_DEBUG */
	return data;
}

static void
dl109_select_0(asict, asich, esp, ehd)
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int esp, ehd;
{
	u_int8_t *s;

	static u_int8_t dl109_select_e0[] = {0xb, 0xf, 0xff};
	static u_int8_t dl109_select_ai0[] = {0xb, 0xf, 0xb, 0xff};
	static u_int8_t dl109_select_ao0[] = {0x9, 0xd, 0x9, 0xff};

	switch (esp)
	{
	case DL109_EEPROM:
		s = dl109_select_e0;
		break;

	case DL109_ASIC:
		s = ((ehd == 0) ? dl109_select_ai0 : dl109_select_ao0);
		break;

	default:
		s = dl109_select_e0; 
		break;
	}

	dl109_serialize(asict, asich, s, esp, ehd);
}

static void
dl109_select_1(asict, asich, esp, ehd)
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int esp, ehd;
{
	u_int8_t *s;

	static u_int8_t dl109_select_e1[] = {0x9, 0xd, 0xff};
	static u_int8_t dl109_select_ai1[] = {0x9, 0xd, 0x9, 0xff};
	static u_int8_t dl109_select_ao1[] = {0x8, 0xc, 0x8, 0xff};

	switch (esp)
	{
	case DL109_EEPROM:
		s = dl109_select_e1;
		break;

	case DL109_ASIC:
		s = ((ehd == 0) ? dl109_select_ai1 : dl109_select_ao1);
		break;

	default:
		s = dl109_select_e1;
		break;
	}

	dl109_serialize(asict, asich, s, esp, ehd);
}

/****************************************************************
 * ax190
 ****************************************************************/
static void ne_pisa_ax190_init_card __P((struct dp8390_softc *sc));
#define	ED_RCR_INTT	0x40
#define	ED_TCR_FDU	0x80

static void
ne_pisa_ax190_init_card(sc)
	struct dp8390_softc *sc;
{
	bus_space_tag_t regt = sc->sc_regt;
	bus_space_handle_t regh = sc->sc_regh;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct ifmedia *ifm = &sc->sc_media;
	u_int8_t regv;

	regv = ED_RCR_AB | ED_RCR_AM | ED_RCR_INTT;
	if (ifp->if_flags & IFF_PROMISC) {
		regv |= ED_RCR_PRO | ED_RCR_AR | ED_RCR_SEP;
	}
	bus_space_write_1(regt, regh, ED_P0_RCR, regv);

	if (ifm->ifm_media & IFM_FDX)
		bus_space_write_1(regt, regh, ED_P0_TCR, ED_TCR_FDU);
}
		

/****************************************************************
 * Vendor tags
 ****************************************************************/
static struct ne_vendor_id_tag dl109_vendor_id_tag[] = {
	{	"Allide 10/100",

		PISA_SLOT_PCMCIA,
		0xc00f00ff,
		0xffffff00,
	},

	{	"PCET/TX",

		PISA_SLOT_PCMCIA,
		0x0149023f,
		0xfffffff0,
	},

	{	"LinkSys",

		PISA_SLOT_PCMCIA,
		0x0149c1ff,
		0xffffff00,
	},

	{	"TDK DFL5610WS",

		PISA_SLOT_PCMCIA,
		0x0105ea15,
		0xffffffff,
	},

	{ NULL, }
};

static struct ne_vendor_id_tag ax190_vendor_id_tag[] = {
	{	"LPC3-TX",

		PISA_SLOT_PCMCIA,
		0x8a01c1ff,
		0xffffff00,
	},

	{ NULL, }
};

static struct ne_vendor_id_tag de660_vendor_id_tag[] = {
	{	"DE660",

		PISA_SLOT_PCMCIA,
		0x0149021f,
		0xfffffff0,
	},

	{ NULL, }
};

static struct ne_vendor_id_tag PCCard_vendor_id_tag[] = {
	{	"PCCard Default",

		PISA_SLOT_PCMCIA,
		0xffffffff,
		0x0,
	},
	
	{ NULL, }
};

struct ne_vendor_tag ne_pisa_vendor_tag[] = {
	{	 &dl109_vendor_id_tag[0],

		NE_PISA_RESUME_OK | NE_PISA_RAMSIZE_DETECT,

		PCCard_get_nd_from_asic_regs,
		0x4,

		ne_pisa_auto_media_standard,
		2,
		IFM_ETHER|IFM_AUTO,
		
		NULL,
		dl109_set_media,
		ne_pisa_mediachange,
		ne_pisa_mediastatus,
	},

	{	 &ax190_vendor_id_tag[0],

		NE_PISA_RESUME_OK | NE_PISA_RAMSIZE_DETECT,

		PCCard_get_nd_from_cis,
		0xe4,

		ne_pisa_auto_media_standard,
		2,
		IFM_ETHER|IFM_AUTO,
		
		ne_pisa_ax190_init_card,
		NULL,
		ne_pisa_mediachange,
		ne_pisa_mediastatus,
	},

	{	 &de660_vendor_id_tag[0],

		NE_PISA_RESUME_OK | NE_PISA_RAMSIZE_DETECT | NE_PISA_FULLDUPLEX_BLOCK,

		default_get_nd,
		0,

		ne_pisa_auto_media_standard,
		2,
		IFM_ETHER|IFM_AUTO,
		
		NULL,
		de660_set_media,
		ne_pisa_mediachange,
		ne_pisa_mediastatus,
	},

	{	&PCCard_vendor_id_tag[0],

		NE_PISA_RAMSIZE_DETECT,

		PCCard_get_nd,

		NULL,
		0,
		0,
		
		NULL,
		NULL,
		NULL,
		NULL,
	},

	{	NULL,

		0,

		default_get_nd,

		NULL,
		0,
		0,
		
		NULL,
		NULL,
		NULL,
		NULL,
	},
};

