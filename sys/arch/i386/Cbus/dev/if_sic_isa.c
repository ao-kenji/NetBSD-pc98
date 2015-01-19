/*	$NecBSD: if_sic_isa.c,v 1.19.4.10 1999/10/17 06:36:22 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998, 1999
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
#include "rnd.h" 

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <net/if_ether.h>

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

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <i386/Cbus/dev/dp8390pio.h>
#include <i386/Cbus/dev/dp8390shm.h>
#include <i386/Cbus/dev/endsubr.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_stream_2	bus_space_write_2
#define	bus_space_write_multi_stream_2	bus_space_write_multi_2
#define	bus_space_read_multi_stream_2	bus_space_read_multi_2
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

struct sic_vendor_tag;
struct sic_isa_softc {
	dp8390shm_softc_t sc_dsc;

	struct sic_vendor_tag *sc_vtp;

	u_long sc_irq;
	void *sc_ih;			/* interrupt handle */
};

int	sic_isa_match __P((struct device *, struct cfdata *, void *));
void	sic_isa_attach __P((struct device *, struct device *, void *));

struct cfattach sic_isa_ca = {
	sizeof(struct sic_isa_softc), sic_isa_match, sic_isa_attach
};

extern struct cfdriver sic_cd;

#define	SIC_ISA_PIO_PROBEPRI	1
#define	SIC_ISA_SHM_PROBEPRI	2	/* higher pri than pio */

struct sic_vendor_tag {
	u_char *vt_name;
	
#define	SIC_CORE_IAT_TBLSZ	0x10
#define	SIC_ASIC_IAT_TBLSZ	0x10
	bus_addr_t vt_core[SIC_CORE_IAT_TBLSZ];
	bus_addr_t vt_asic[SIC_ASIC_IAT_TBLSZ];
	u_int8_t   vt_irqsets[16];

	bus_addr_t vt_ramstart;
	bus_size_t vt_ramsize;

	u_int8_t   vt_proto;
	u_int8_t   vt_cksum;
	int vt_bustype;

#define	SIC_TYPE_SHM			1
#define	SIC_TYPE_PIO			2
#define	SIC_TYPE_DUALCAP		(SIC_TYPE_SHM | SIC_TYPE_PIO)
#define	SIC_TYPE_NO_MULTI_BUFFERING	4
	u_long vt_flags;

#define	SIC_CMD_INIT		0
#define	SIC_CMD_MEMSEL_ROM	1
#define	SIC_CMD_MEMSEL_RAM	2
#define	SIC_CMD_INT_OPEN	3
	int (*vt_ctrl) __P((struct sic_vendor_tag *, \
		bus_space_tag_t, bus_space_handle_t, int, int, \
		struct isa_attach_args *));

	/* vendor depend detect subroutine for PIO type */
	int (*vt_pio_detect) __P((int, bus_space_tag_t, bus_space_handle_t, \
		bus_space_tag_t, bus_space_handle_t, \
		bus_space_tag_t, bus_space_handle_t, \
		bus_addr_t, bus_size_t, \
		u_int8_t));
	/* vendor depend attach subroutine for PIO type */
	int (*vt_pio_attach) __P((dp8390pio_softc_t *, int, bus_addr_t, \
		bus_size_t, u_int8_t *, int *, int, int));

	/* vendor depend detect subroutine for SHM type */
	int (*vt_shm_detect) __P((int, bus_space_tag_t, bus_space_handle_t, \
		bus_space_tag_t, bus_space_handle_t, \
		bus_space_tag_t, bus_space_handle_t, \
		bus_addr_t, bus_size_t, \
		u_int8_t));
	/* vendor depend attach subroutine for SHM type */
	int (*vt_shm_attach) __P((dp8390pio_softc_t *, int, bus_addr_t, \
		bus_size_t, u_int8_t *, int *, int, int));

	/* how to read my ether address */
	int (*vt_read_ea) __P((int, bus_space_tag_t, bus_space_handle_t,
		bus_space_tag_t, bus_space_handle_t, u_int8_t *, u_int8_t));

	/* media selection related */
	void	(*vt_init_card) __P((struct dp8390_softc *));

	/* media type */
	int *vt_media;
	/* media type size */
	int vt_nmedia;
	int vt_defmedia;

	/* pass to dp8390_softc */
	int	(*vt_mediachange) __P((struct dp8390_softc *));
	void	(*vt_mediastatus) __P((struct dp8390_softc *,
		    struct ifmediareq *));
};

typedef	struct sic_vendor_tag *sic_vendor_tag_t;
#define	sic_ctl_hw(vtag, t, h, m, c, i) \
	((*((vtag)->vt_ctrl)) ((vtag), (t), (h), (m), (c), (i)))

static int sic_shm_detectsubr __P((int, bus_space_tag_t, bus_space_handle_t, \
	bus_space_tag_t, bus_space_handle_t, \
	bus_space_tag_t, bus_space_handle_t, \
	bus_addr_t, bus_size_t, u_int8_t));
static int sic_shm_read_ea
	__P((int, bus_space_tag_t, bus_space_handle_t, bus_space_tag_t, \
	bus_space_handle_t, u_int8_t *, u_int8_t));

static int sic_isa_map
	__P((sic_vendor_tag_t, struct isa_attach_args *, \
	bus_space_handle_t *, bus_space_handle_t *, bus_space_handle_t *));
static sic_vendor_tag_t sic_vendor_tag __P((struct isa_attach_args *));

static int sic_vtag_mode __P((int, int));
static int sic_irq_match __P((sic_vendor_tag_t, u_long));

static int
sic_vtag_mode(maddr, type)
	int maddr;
	int type;
{
	int mode = type;

	/* XXX: EL98/EL98N: pass MULTI type */
	if ((type & SIC_TYPE_SHM) != 0) {
		if (maddr == ISACF_IOMEM_DEFAULT) {
			/* mis or dual (force pio) */
			if ((type & SIC_TYPE_PIO) != 0)
				mode &= ~SIC_TYPE_SHM;	/* dual/force pio */
			else
				return 0;	/* mis config */
		} else {
			mode &= ~SIC_TYPE_PIO;	/* shm/dual force shm */
		}
	} else {
		/* PIO */
		mode &= ~SIC_TYPE_SHM;	/* dual/force pio */
	}

	return mode;
}

static int
sic_irq_match(vtag, irq)
	sic_vendor_tag_t vtag;
	u_long irq;
{

	if (irq >= 16 || vtag->vt_irqsets[irq] == (u_int8_t) -1)
		return EINVAL;

	return 0;
}
	
int
sic_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t asict, nict, memt;
	bus_space_handle_t asich, nich, memh;
	sic_vendor_tag_t vtag;
	u_int8_t nd[ETHER_ADDR_LEN];
	int rv = 0, n, type;
	/* XXX: EL98/EL98N: pass MULTI type */
	int mode;
	int (*detectfunc) __P((int, bus_space_tag_t, bus_space_handle_t, \
		bus_space_tag_t, bus_space_handle_t, \
		bus_space_tag_t, bus_space_handle_t, \
		bus_addr_t, bus_size_t, u_int8_t));

	vtag = sic_vendor_tag(ia);
	if (vtag == NULL)
		return 0;

	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return 0;

	mode = sic_vtag_mode(ia->ia_maddr, vtag->vt_flags);
	if (mode == 0)
		return 0;

	if (sic_irq_match(vtag, ia->ia_irq) != 0)
		return 0;

	asict = ia->ia_iot;
	nict = ia->ia_iot;
	memt = ia->ia_memt;
	type = vtag->vt_bustype;

	if (sic_isa_map(vtag, ia, &nich, &asich, &memh) != 0)
		return 0;

	if (sic_ctl_hw(vtag, asict, asich, mode, SIC_CMD_INIT, ia) != 0)
		goto out;

	if (sic_ctl_hw(vtag, asict, asich, mode, SIC_CMD_MEMSEL_ROM, ia) != 0)
		goto out;

	if (vtag->vt_read_ea != NULL)
	{
		n = (*vtag->vt_read_ea)(type, asict, asich, memt, memh,
					nd, vtag->vt_cksum);
		if (n != 0)
			goto out;
	}

	if (mode & SIC_TYPE_SHM)
	{
		detectfunc = vtag->vt_shm_detect;
		rv = SIC_ISA_SHM_PROBEPRI;
	}
	else if (mode & SIC_TYPE_PIO)
	{
		detectfunc = vtag->vt_pio_detect;
		rv = SIC_ISA_PIO_PROBEPRI;
	}	

	n = (*detectfunc)(type, nict, nich, asict, asich, memt, memh,
			  vtag->vt_ramstart, vtag->vt_ramsize, vtag->vt_cksum);
	if (n == 0)
		rv = 0;

out:
	bus_space_unmap(nict, nich, SIC_CORE_IAT_TBLSZ);
	bus_space_unmap(asict, asich, SIC_ASIC_IAT_TBLSZ);
	if (memh != NULL)
		bus_space_unmap(memt, memh, ia->ia_msize);

	if (rv != 0)
		ia->ia_iosize = SIC_CORE_IAT_TBLSZ + SIC_ASIC_IAT_TBLSZ;
	return rv;
}

void
sic_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sic_isa_softc *ssc = (struct sic_isa_softc *) self;
	dp8390shm_softc_t *dsc = &ssc->sc_dsc;
	struct dp8390_softc *sc = &dsc->sc_dp8390;
	struct isa_attach_args *ia = aux;
	sic_vendor_tag_t vtag;
	u_int8_t *ndp;
	u_int8_t ndb[ETHER_ADDR_LEN];
	int error, mode;
	bus_size_t msize;
	int (*attachfunc) __P((dp8390shm_softc_t *, int,
		bus_addr_t, bus_size_t, u_int8_t *, int *, int, int));

	printf("\n");

	ssc->sc_vtp = vtag = sic_vendor_tag(ia);
	mode = sic_vtag_mode(ia->ia_maddr, vtag->vt_flags);
	if (mode == 0)
		return;

#ifdef	DIAGNOSTIC
	if (sic_irq_match(vtag, ia->ia_irq) != 0)
		return;
#endif	/* DIAGNOSTIC */

	/* init sc */
	sc->sc_regt = ia->ia_iot;
	sc->sc_buft = ia->ia_memt;
	sc->sc_flags = DVCFG_MINOR(ia->ia_cfgflags);
	if (mode & SIC_TYPE_NO_MULTI_BUFFERING)
		sc->sc_flags |= DP8390_NO_MULTI_BUFFERING;
	sc->cr_proto = vtag->vt_proto;
	sc->sc_enabled = 1;
	sc->init_card = vtag->vt_init_card;
	sc->sc_mediachange = vtag->vt_mediachange;
	sc->sc_mediastatus = vtag->vt_mediastatus;

	/* init dsc */
	dsc->sc_asict = ia->ia_iot;

	/* init ssc */
	ssc->sc_irq = ia->ia_irq;

	/* map */
	if (sic_isa_map(vtag, ia, &sc->sc_regh, &dsc->sc_asich, &sc->sc_bufh))
	{
		printf("%s: can't map nic i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	/* open hardware */
	sic_ctl_hw(vtag, dsc->sc_asict, dsc->sc_asich, mode, 
		   SIC_CMD_INIT, ia);

	/* get ea */
	sic_ctl_hw(vtag, dsc->sc_asict, dsc->sc_asich, mode,
		   SIC_CMD_MEMSEL_ROM, ia);

	ndp = NULL;
	if (vtag->vt_read_ea != NULL)  
	{
		ndp = &ndb[0];
		error = (*vtag->vt_read_ea)(vtag->vt_bustype,
				dsc->sc_asict, dsc->sc_asich,
				sc->sc_buft, sc->sc_bufh, ndp, vtag->vt_cksum);
		if (error)
			return;
	}

	sic_ctl_hw(vtag, dsc->sc_asict, dsc->sc_asich, mode,
		   SIC_CMD_MEMSEL_RAM, ia);

	if (mode & SIC_TYPE_SHM)
	{
		attachfunc = vtag->vt_shm_attach;
		msize = ia->ia_msize;
	}
	else
	{
		attachfunc = vtag->vt_pio_attach;
		msize = vtag->vt_ramsize;
	}

	error = (*attachfunc)(dsc, vtag->vt_bustype,
		vtag->vt_ramstart, msize, ndp,
		vtag->vt_media, vtag->vt_nmedia, vtag->vt_defmedia);
	if (error != 0)
		goto bad;

	/* XXX: and get encoded irq */
	error = sic_ctl_hw(vtag, dsc->sc_asict, dsc->sc_asich, mode,
			   SIC_CMD_INT_OPEN, ia);
	if (error != 0)
	{
		printf("%s: irq %d invalid", sc->sc_dev.dv_xname, ia->ia_irq);
bad:
		sc->sc_enabled = 0;
		return;
	}

	ssc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
					IPL_NET, dp8390_intr, sc);
	if (ssc->sc_ih == NULL)
		printf("%s: can't establish interrupt\n", sc->sc_dev.dv_xname);
}

static int
sic_shm_detectsubr(bustype, nict, nich, asict, asich, memt, memh, ramstart, ramsize, cksum)
	int bustype;
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_addr_t ramstart;
	bus_size_t ramsize;
	u_int8_t cksum;
{

	return NE2000_TYPE_NE2000;
}

static int
sic_shm_read_ea(bustype, asict, asich, memt, memh, ndp, cksum)
	int bustype;
	bus_space_tag_t asict, memt;
	bus_space_handle_t asich, memh;
	u_int8_t *ndp;
	u_int8_t cksum;
{
	bus_size_t busw;
	u_int16_t sum, xsum;
	int n;
	u_int8_t nd[ETHER_ADDR_LEN + 1];

	busw = (bustype & DP8390_BUS_SHM16) ? 2 : 1;
	for (xsum = sum = 0, n = 0; n < ETHER_ADDR_LEN + 1; ++ n)
	{
		nd[n] = bus_space_read_1(memt, memh, busw * n);
#ifdef	SIC_DEBUG
		printf(" %02x", nd[n]);
#endif	/* SIC_DEBUG */
		xsum ^= nd[n];
		if (n < ETHER_ADDR_LEN)
			sum += nd[n];
		else
			sum = (sum & 0x00ff) + ((sum & 0xff00) >> 8);
	}

	if (nd[0] == 0xff || (nd[0] | nd[1] | nd[2]) == 0)
		return EINVAL;		

	if (sum == nd[ETHER_ADDR_LEN] || xsum == cksum)
	{
		bcopy(nd, ndp, ETHER_ADDR_LEN);
		return 0;
	}

#ifdef	SIC_DEBUG
	printf("sic_shm_read_ea: checksum mismatch\n");
#endif	/* SIC_DEBUG */
	return EINVAL;
}

/*************************************************************
 * Vendor settings
 *************************************************************/
static int
sic_isa_map(vtag, ia, corehp, asichp, memhp)
	sic_vendor_tag_t vtag;
	struct isa_attach_args *ia;
	bus_space_handle_t *memhp, *corehp, *asichp;
{
	bus_addr_t coreiat[SIC_CORE_IAT_TBLSZ];
	bus_addr_t asiciat[SIC_ASIC_IAT_TBLSZ];
	bus_space_handle_t asich, coreh, memh;
	bus_space_tag_t iot, memt;
	bus_addr_t iobase = ia->ia_iobase;
	bus_size_t msize = ia->ia_msize;
	int n;
	int mode;

	iot = ia->ia_iot;
	memt = ia->ia_memt;
	memh = NULL;

	for (n = 0; n < SIC_CORE_IAT_TBLSZ; n ++)
		coreiat[n] = vtag->vt_core[n];
	for (n = 0; n < SIC_ASIC_IAT_TBLSZ; n ++)
		asiciat[n] = vtag->vt_asic[n];

	/* XXX dirty hack! */
	if (strncmp(vtag->vt_name, "cnet98", 6) == 0)
	{
		bus_addr_t adj = iobase;

		if ((DVCFG_MAJOR(ia->ia_cfgflags) & 1) != 0)
			adj += 0x5500;
		asiciat[0xe] -= adj;
		asiciat[0xf] -= adj;
	}

	if (bus_space_map(iot, iobase, 0, 0, &coreh) != 0 ||
	    bus_space_map_load(iot, coreh, SIC_CORE_IAT_TBLSZ, coreiat,
	    		       BUS_SPACE_MAP_FAILFREE) != 0)
		return ENOSPC;

	if (bus_space_map(iot, iobase, 0, 0, &asich) != 0 ||
	    bus_space_map_load(iot, asich, SIC_ASIC_IAT_TBLSZ, asiciat,
	    		       BUS_SPACE_MAP_FAILFREE) != 0)
		goto bad1;

	mode = sic_vtag_mode(ia->ia_maddr, vtag->vt_flags);
	if (mode == 0)
		goto bad;

	if (mode & SIC_TYPE_SHM)
	{
		if (msize > NBPG * 8 || msize < NBPG)
			msize = NBPG * 4;
		if (bus_space_map(memt, ia->ia_maddr, msize, 0, &memh) != 0)
			goto bad;
	}

	*corehp = coreh;
	*asichp = asich;
	*memhp = memh;

	if (mode & SIC_TYPE_SHM)
		ia->ia_msize = msize;

	return 0;

bad:
	bus_space_unmap(iot, asich, SIC_ASIC_IAT_TBLSZ);
bad1:
	bus_space_unmap(iot, coreh, SIC_CORE_IAT_TBLSZ);
	return ENOSPC;
}

/********************************************************
 * sic98  cmd operations
 ********************************************************/
static int sic_sic98_ctrl
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, int, struct isa_attach_args *));

#define	sicr_ctrl		0
#define	SIC_CTRL_WIN_EN		0x90
#define	SIC_CTRL_ROMSEL		0x04

static int
sic_sic98_ctrl(vtag, bst, bsh, mode, who, ia)
	sic_vendor_tag_t vtag;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	int mode, who;
	struct isa_attach_args *ia;
{

	switch (who)
	{
	case SIC_CMD_INIT:
		bus_space_write_1(bst, bsh, sicr_ctrl, 0);
		break;

	case SIC_CMD_MEMSEL_ROM:
		bus_space_write_1(bst, bsh, sicr_ctrl,
				  SIC_CTRL_WIN_EN | SIC_CTRL_ROMSEL);
		delay(100);
		bus_space_write_1(bst, bsh, sicr_ctrl,
				  SIC_CTRL_WIN_EN | SIC_CTRL_ROMSEL);
		break;

	case SIC_CMD_MEMSEL_RAM:
		bus_space_write_1(bst, bsh, sicr_ctrl, SIC_CTRL_WIN_EN);
		break;

 	default:
		return 0;
	}

	delay(100);
	return 0;
}

/********************************************************
 * cnet98  cmd operations
 ********************************************************/
static int sic_cnet98_ctrl
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, int, struct isa_attach_args *));
static int sic_cnet98el_ctrl
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, int, struct isa_attach_args *));
static int sic_cnet98_initirq
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	u_long));
static int sic_cnet98_winsel
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	u_long, u_long));
static int sic_cnet98_init
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	u_long));

#define	SIC_CNET_MAP_REG0L	0	/* MAPPING register0 Low */
#define	SIC_CNET_MAP_REG1L	1	/* MAPPING register1 Low */
#define	SIC_CNET_MAP_REG2L	2	/* MAPPING register2 Low */
#define	SIC_CNET_MAP_REG3L	3	/* MAPPING register3 Low */
#define	SIC_CNET_MAP_REG0H	4	/* MAPPING register0 Hi */
#define	SIC_CNET_MAP_REG1H	5	/* MAPPING register1 Hi */
#define	SIC_CNET_MAP_REG2H	6	/* MAPPING register2 Hi */
#define	SIC_CNET_MAP_REG3H	7	/* MAPPING register3 Hi */
#define	SIC_CNET_WIN_REG	8	/* window register */
#define	SIC_CNET_WIN_RAMSEL	0x4000
#define	SIC_CNET_WIN_ROMSEL	0x4800
#define	SIC_CNET_ILR		9	/* Interrupt Level Register */
#define	SIC_CNET_IRR		10	/* Interrupt Request Register */
#define	SIC_CNET_IMR		11	/* Interrupt Mask Register (write) */
#define	SIC_CNET_IMR_AIE	0x7e	/* All Interrupt Enable */
#define	SIC_CNET_ISR		12	/* Interrupt Status Register */
#define	SIC_CNET_ICR		12	/* Interrupt Clear Register */
#define	SIC_CNET_RESET		14
#define SIC_CNET_IAR		15
#define	SIC_CNET_IAR_BUS16	0x01	/* Slot is 16-bit mode */

static int
sic_cnet98_ctrl(vtag, asict, asich, mode, who, ia)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int mode, who;
	struct isa_attach_args *ia;
{
	int error;

	switch (who)
	{
	case SIC_CMD_INIT:
		sic_cnet98_init(vtag, asict, asich, ia->ia_iobase);
		break;

	case SIC_CMD_MEMSEL_ROM:
		sic_cnet98_winsel(vtag, asict, asich, SIC_CNET_WIN_ROMSEL,
				  ia->ia_maddr);
		break;

	case SIC_CMD_MEMSEL_RAM:
		sic_cnet98_winsel(vtag, asict, asich, SIC_CNET_WIN_RAMSEL,
				  ia->ia_maddr);
		break;

	case SIC_CMD_INT_OPEN:
		error = sic_cnet98_initirq(vtag, asict, asich, ia->ia_irq);
		return error;

	default:
		return 0;
	}

	return 0;
}

static int
sic_cnet98el_ctrl(vtag, asict, asich, mode, who, ia)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int mode, who;
	struct isa_attach_args *ia;
{
	int error;
	u_int8_t regv;

	switch (who)
	{
	case SIC_CMD_INIT:
		regv = bus_space_read_1(asict, asich, SIC_CNET_RESET);
		bus_space_write_1(asict, asich, SIC_CNET_RESET, regv);
		delay(5000);
		sic_cnet98_init(vtag, asict, asich, ia->ia_iobase);
		break;

	case SIC_CMD_INT_OPEN:
		error = sic_cnet98_initirq(vtag, asict, asich, ia->ia_irq);
		return error;

	default:
		return 0;
	}

	return 0;
}

static int
sic_cnet98_init(vtag, asict, asich, iobase)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_long iobase;
{
	u_long regv;

	bus_space_write_1(asict, asich, SIC_CNET_RESET, 0x00);
	delay(5000);
	bus_space_write_1(asict, asich, SIC_CNET_RESET, 0x01);
	delay(5000);

	regv = ((iobase & 0xf000) >> 8) | 0x08 | SIC_CNET_IAR_BUS16;
	bus_space_write_1(asict, asich, SIC_CNET_IAR, regv);
	delay(1000);
	return 0;
}

static int
sic_cnet98_initirq(vtag, asict, asich, irq)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_long irq;
{
	u_int8_t *irqtp = vtag->vt_irqsets;

	bus_space_write_1(asict, asich, SIC_CNET_ILR, irqtp[irq]);
	delay(1000);
	bus_space_write_1(asict, asich, SIC_CNET_IMR, SIC_CNET_IMR_AIE);
	return 0;
}

static int
sic_cnet98_winsel(vtag, asict, asich, winbase, membase)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_long winbase;
	u_long membase;
{
	u_long banklo, bankhi;

	membase = (membase & 0xff000) >> 12;
	bus_space_write_1(asict, asich, SIC_CNET_WIN_REG, membase);
	delay(10);

	banklo = winbase & 0xff;
	bankhi = (winbase >> NBBY) & 0xff;
	bus_space_write_1(asict, asich, SIC_CNET_MAP_REG0L, banklo);
	delay(10);
	bus_space_write_1(asict, asich, SIC_CNET_MAP_REG0H, bankhi);
	delay(10);
	bus_space_write_1(asict, asich, SIC_CNET_MAP_REG1L, 0x00);
	delay(10);
	bus_space_write_1(asict, asich, SIC_CNET_MAP_REG1H, 0x41);
	delay(10);
	bus_space_write_1(asict, asich, SIC_CNET_MAP_REG2L, 0x00);
	delay(10);
	bus_space_write_1(asict, asich, SIC_CNET_MAP_REG2H, 0x42);
	delay(10);
	bus_space_write_1(asict, asich, SIC_CNET_MAP_REG3L, 0x00);
	delay(10);
	bus_space_write_1(asict, asich, SIC_CNET_MAP_REG3H, 0x43);
	delay(10);
	return 0;
}

/********************************************************
 * NEC PC-9801-77/78 cmd operations
 ********************************************************/
static int sic_pc980178_ctrl
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, int, struct isa_attach_args *));
static int sic_pc980178_initirq
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	u_long));
#define	SIC_PC980177_IRQCFG	1

static int
sic_pc980178_ctrl(vtag, asict, asich, mode, who, ia)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int mode, who;
	struct isa_attach_args *ia;
{
	int error;

	switch (who)
	{
	case SIC_CMD_INT_OPEN:
		error = sic_pc980178_initirq(vtag, asict, asich, ia->ia_irq);
		return error;

	default:
		return 0;
	}

	return 0;
}

static int
sic_pc980178_initirq(vtag, asict, asich, irq)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_long irq;
{
	u_int8_t *irqtp = vtag->vt_irqsets;	
	u_long regv;

	bus_space_write_1(asict, asich, SIC_PC980177_IRQCFG, irqtp[irq]);

	/* read back, should be check if it is same as write value */
	regv = bus_space_read_1(asict, asich, SIC_PC980177_IRQCFG) & 0x0E;
	if (regv != irqtp[irq])
		return EINVAL;
	/* XXX: check if our iobase is valid */

	return 0;
}

/********************************************************
 * Networld EP98X cmd operations
 ********************************************************/
static int sic_ep98x_ctrl
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, int, struct isa_attach_args *));
static int sic_ep98x_initirq
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	u_long));
#define	SIC_EP98X_IRQCFG	1

static int
sic_ep98x_ctrl(vtag, asict, asich, mode, who, ia)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int mode, who;
	struct isa_attach_args *ia;
{
	int error;

	switch (who)
	{
	case SIC_CMD_INT_OPEN:
		error = sic_ep98x_initirq(vtag, asict, asich, ia->ia_irq);
		return error;

	default:
		return 0;
	}

	return 0;
}

static int
sic_ep98x_initirq(vtag, asict, asich, irq)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_long irq;
{
	u_int8_t *irqtp = vtag->vt_irqsets;	
	u_long regv;

	bus_space_write_1(asict, asich, SIC_EP98X_IRQCFG, irqtp[irq]);

	/* read back, should be check if it is same as write value */
	regv = bus_space_read_1(asict, asich, SIC_EP98X_IRQCFG) & 0x0E;
	if (regv != irqtp[irq])
		return EINVAL;
	/* XXX: check if our iobase is valid */

	return 0;
}

/********************************************************
 * Soliton SB-9801-T/SN-9801-T, Fujikura FN1700, FN1800 
 * cmd operations
 ********************************************************/
static int sic_sb98_ctrl
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, int, struct isa_attach_args *));
static int sic_sb98_init
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	u_long, u_long));
static int sic_sb98_read_ea
	__P((int, bus_space_tag_t, bus_space_handle_t, bus_space_tag_t, \
	bus_space_handle_t, u_int8_t *, u_int8_t));
	
/*
 * Registers on Soliton SB-9801-T/SN-9801-T.
 */
#define	SIC_SB98_BCFG		1	/* IRQ/MEDIA configration */
#define	SIC_SB98_EEPEN		2	/* enable EEPROM access */
#define	SIC_SB98_EEP		3	/* EEPROM access (X24C01) */
#define	SIC_SB98_INTR		4	/* enable interrupt */
#define	SIC_SB98_RESET		15	/* board reset (7) */

#define	SIC_SB98_BCFG_IRQ	0x0C	/* IRQ configuration mask */
#define	SIC_SB98_BCFG_AUI	0x40	/* use EXTERNAL media (10BASE-5) */
#define	SIC_SB98_BCFG_NOAUI	0x00	/* use INTERNAL media (10BASE-2/T) */
#define	SIC_SB98_BCFG_EN		0xA0	/* enable configuration */

#define	SIC_SB98_EEPEN_EEPEN	0x01	/* enable X24C01 EEPROM access */

#define	SIC_SB98_EEP_DATA	0x01	/* X24C01 Serial Data (SDA) pin HIGH */
#define	SIC_SB98_EEP_CLOCK	0x02	/* X24C01 Serial Clock (SCL) pin HIGH */

#define	SIC_SB98_EEP_DATABIT	0x01	/* Data bit from EEPROM */

#define	SIC_SB98_EEPROM_SIZE	16
#define	SIC_SB98_EEP_DELAY	1000

/* EEPROM allocation of Soliton SB-9801-T, SN-9801-T. */
#define	SIC_SB98_EEPROM_ADDR	1	/* Station address. (1-6) */

#define	SIC_SB98_INTR_EN		0x01	/* enable interrupt */

#define	SIC_SB98_RESET_00	0x00	/* board reset (7) */
#define	SIC_SB98_RESET_02	0x02	/* board reset (7) */
#define	SIC_SB98_RESET_09	0x09	/* board reset (7) */

/*
 * Probe routines of
 *	Soliton SB-9801-T/SN-9801-T
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
 */
/*
 * Soliton SB-9801-T use National Semiconductor DP83902 as
 * Ethernet Controller and X24C01 as (128 * 8 bits) I2C Serial EEPROM.
 */

static int
sic_sb98_ctrl(vtag, asict, asich, mode, who, ia)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int mode, who;
	struct isa_attach_args *ia;
{
	int error;

	switch (who)
	{
	case SIC_CMD_INIT:
		if ((ia->ia_iobase & ~0x000E) != 0x00D0)
			return EINVAL;
		error = sic_sb98_init(vtag, asict, asich,
				      ia->ia_iobase, ia->ia_irq);
		return error;

	default:
		return 0;
	}

	/* XXX */
	return 0;
}

static int
sic_sb98_init(vtag, asict, asich, iobase, irq)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_long iobase;
	u_long irq;
{
	u_int8_t *irqtp = vtag->vt_irqsets;
	int n;

	/* XXX:
	 * OUT OF IOMAP! SHOULD FIX!!
	 */
	/*
	 * XXX: DUBIOUS!
	 * if it is already active, we do not require it.
	 */
	outb(IO_LPT, iobase & 0xff);
	for (n = 0; n < 4; n++) {
		delay(10);
		(void) inb(IO_LPT);
	}
	/* ********* */

	/* XXX: should tread external ethernet media. */
	bus_space_write_1(asict, asich, SIC_SB98_BCFG,
	    irqtp[irq] | SIC_SB98_BCFG_EN | SIC_SB98_BCFG_NOAUI);
	delay(1000);
	bus_space_write_1(asict, asich, SIC_SB98_INTR, SIC_SB98_INTR_EN);
	delay(1000);
	if (bus_space_read_1(asict, asich, SIC_SB98_RESET) != 0) {
		bus_space_write_1(asict, asich, SIC_SB98_RESET, 2);
		delay(1000);
		bus_space_write_1(asict, asich, SIC_SB98_RESET, 0);
		delay(1000);
	}

	bus_space_write_1(asict, asich, SIC_SB98_RESET, 9);
	delay(1000);

	return 0;
}

void	sic_sb98_set_media __P((struct sic_isa_softc *, int));
int	sic_sb98_mediachange __P((struct dp8390_softc *));
void	sic_sb98_mediastatus __P((struct dp8390_softc *, struct ifmediareq *));

void	sic_sb98_init_card __P((struct dp8390_softc *));

int sic_sb98_media[] = {
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_2,
	IFM_ETHER|IFM_10_5,
};

int
sic_sb98_mediachange(sc)
	struct dp8390_softc *sc;
{

	/*
	 * Current media is already set up.  Just reset the interface
	 * to let the new value take hold.  The new media will be
	 * set up in sic_sb98_init_card() called via dp8390_init().
	 */
	dp8390_reset(sc);
	return (0);
}

void
sic_sb98_mediastatus(sc, ifmr)
	struct dp8390_softc *sc;
	struct ifmediareq *ifmr;
{
	struct ifmedia *ifm = &sc->sc_media;

	/*
	 * The currently selected media is always the active media.
	 */
	ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

void
sic_sb98_init_card(sc)
	struct dp8390_softc *sc;
{
	struct sic_isa_softc *ssc = (struct sic_isa_softc *)sc;
	struct ifmedia *ifm = &sc->sc_media;

	sic_sb98_set_media(ssc, ifm->ifm_cur->ifm_media);
}

void
sic_sb98_set_media(ssc, media)
	struct sic_isa_softc *ssc;
	int media;
{
	dp8390pio_softc_t *dsc = &ssc->sc_dsc;
	bus_space_tag_t asict = dsc->sc_asict;
	bus_space_handle_t asich = dsc->sc_asich;
	u_int8_t *confp;
	u_int8_t new;

	if (IFM_TYPE(media) != IFM_ETHER)
		return;

	switch (IFM_SUBTYPE(media)) {
	case IFM_10_2:
	case IFM_10_T:
		new = SIC_SB98_BCFG_NOAUI;
		break;

	case IFM_10_5:
		new = SIC_SB98_BCFG_AUI;
		break;

	default:
		return;
	}

	confp = ssc->sc_vtp->vt_irqsets;
	new |= confp[ssc->sc_irq];
	bus_space_write_1(asict, asich, SIC_SB98_BCFG, SIC_SB98_BCFG_EN | new);
	return;
}

/* 
 * Routines to read bytes sequentially from EEPROM.
 * 
 * This algorism is generic to read data sequentially from I2C Serial
 * EEPROM. (XXX: Use i2c.)
 */
static int
sic_sb98_read_ea(bustype, bst, bsh, memt, memh, data, cksum)
	int bustype;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	u_int8_t *data;
	u_int8_t cksum;
{
	u_int8_t val, bit, sum = 0;
	u_int8_t eeprom[SIC_SB98_EEPROM_SIZE];
	int error, n;

	bus_space_write_1(bst, bsh, SIC_SB98_EEP,
	    SIC_SB98_EEP_CLOCK | SIC_SB98_EEP_DATA);
	delay(SIC_SB98_EEP_DELAY);

	bus_space_write_1(bst, bsh, SIC_SB98_EEPEN, SIC_SB98_EEPEN_EEPEN);

	bus_space_write_1(bst, bsh, SIC_SB98_EEP,
	    SIC_SB98_EEP_CLOCK | SIC_SB98_EEP_DATA);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);

	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);

	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_DATA);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP,
	    SIC_SB98_EEP_CLOCK | SIC_SB98_EEP_DATA);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_DATA);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP,
	    SIC_SB98_EEP_CLOCK | SIC_SB98_EEP_DATA);
	delay(SIC_SB98_EEP_DELAY);

	error = bus_space_read_1(bst, bsh, SIC_SB98_EEP) & SIC_SB98_EEP_DATABIT;
	delay(SIC_SB98_EEP_DELAY);

	if (error != 0)
		goto stop_condition;

	/* error == 0 */
	for (n = 0; n < SIC_SB98_EEPROM_SIZE; n++) {
		val = 0;

		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(bst, bsh, SIC_SB98_EEP,
			    SIC_SB98_EEP_DATA);
			delay(SIC_SB98_EEP_DELAY);
			bus_space_write_1(bst, bsh, SIC_SB98_EEP,
			    SIC_SB98_EEP_CLOCK | SIC_SB98_EEP_DATA);
			delay(SIC_SB98_EEP_DELAY);

			if (bus_space_read_1(bst, bsh, SIC_SB98_EEP)
			    & SIC_SB98_EEP_DATABIT)
				val |= bit;
		}

		eeprom[n] = val;
		sum += val;

		if (n != SIC_SB98_EEPROM_SIZE - 1) {
			bus_space_write_1(bst, bsh, SIC_SB98_EEP,
			    SIC_SB98_EEP_DATA);
			delay(SIC_SB98_EEP_DELAY);
			bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
			delay(SIC_SB98_EEP_DELAY);
			bus_space_write_1(bst, bsh, SIC_SB98_EEP,
			    SIC_SB98_EEP_CLOCK);
			delay(SIC_SB98_EEP_DELAY);
			bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
			delay(SIC_SB98_EEP_DELAY);
		}
	}

	if (sum != 0)
		error = 1;

stop_condition:
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_DATA);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, 0);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP, SIC_SB98_EEP_CLOCK);
	delay(SIC_SB98_EEP_DELAY);
	bus_space_write_1(bst, bsh, SIC_SB98_EEP,
	    SIC_SB98_EEP_CLOCK | SIC_SB98_EEP_DATA);
	delay(SIC_SB98_EEP_DELAY);

	bus_space_write_1(bst, bsh, SIC_SB98_EEPEN, 0);

#ifdef	SIC_DEBUG
	if (error == 0) {
		/* Report what we got. */
		log(LOG_INFO, "%s: EEPROM:"
		    " %02x%02x%02x%02x %02x%02x%02x%02x -"
		    " %02x%02x%02x%02x %02x%02x%02x%02x\n",
		    "sic_sb98_read_ea",
		    eeprom[ 0], eeprom[ 1], eeprom[ 2], eeprom[ 3],
		    eeprom[ 4], eeprom[ 5], eeprom[ 6], eeprom[ 7],
		    eeprom[ 8], eeprom[ 9], eeprom[10], eeprom[11],
		    eeprom[12], eeprom[13], eeprom[14], eeprom[15]);
	}
#endif	/* SIC_DEBUG */

	bcopy(eeprom + SIC_SB98_EEPROM_ADDR, data, ETHER_ADDR_LEN);

	return error;
}

/********************************************************
 * 3COM/NextCom EL9801 cmd operations
 ********************************************************/
static int sic_el98_ctrl
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, int, struct isa_attach_args *));
static int sic_el98_read_ea
	__P((int, bus_space_tag_t, bus_space_handle_t, bus_space_tag_t, \
	bus_space_handle_t, u_int8_t *, u_int8_t));

#define	SIC_EL98_DATA	0	/* data */
#define	SIC_EL98_SA0	1	/* Station Address #1 */
#define	SIC_EL98_SA1	2	/* Station Address #2 */
#define	SIC_EL98_SA2	3	/* Station Address #3 */
#define	SIC_EL98_SA3	4	/* Station Address #4 */
#define	SIC_EL98_SA4	5	/* Station Address #5 */
#define	SIC_EL98_SA5	6	/* Station Address #6 */
#define	SIC_EL98_RESET	15	/* reset */

static int
sic_el98_ctrl(vtag, asict, asich, mode, who, ia)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int mode, who;
	struct isa_attach_args *ia;
{

	switch (who)
	{
	case SIC_CMD_INIT:
		/* check iobase is valid */
		bus_space_write_1(asict, asich, SIC_EL98_RESET, 0);
		delay(5000);
		bus_space_write_1(asict, asich, SIC_EL98_RESET, 1);
		delay(5000);
		break;

	default:
		return 0;
	}

	return 0;
}

static int
sic_el98_read_ea(bustype, asict, asich, memt, memh, data, cksum)
	int bustype;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	u_int8_t *data;
	u_int8_t cksum;
{
	int i;

	/* 
	 * read physical address and check vendor code (00:40:B4)
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		data[i] = bus_space_read_1(asict, asich, SIC_EL98_SA0 + i);

	/* check 3COM vendor code */
	if (data[0] != 0x00 && data[1] != 0x40 && data[2] != 0x4B)
		return EINVAL;		

	return 0;
}

/********************************************************
 * 3COM/NextCom EtherLink/98N cmd operations
 ********************************************************/
static int sic_el98n_ctrl
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, int, struct isa_attach_args *));
static int sic_el98n_init
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, \
	int, u_long, u_long, u_long));
static int sic_el98n_initirq
	__P((sic_vendor_tag_t, bus_space_tag_t, bus_space_handle_t, u_long));
static int sic_el98n_read_ea
	__P((int, bus_space_tag_t, bus_space_handle_t, bus_space_tag_t, \
	bus_space_handle_t, u_int8_t *, u_int8_t));
static u_int16_t sic_el98n_read_eeprom
	__P((bus_space_tag_t, bus_space_handle_t, u_int8_t));

/*
 * ASIC Registers on 3COM EtherLink/98N.
 */
#define	SIC_EL98N_MCF		0	/* W: config MEM base or PIO */
#define		SIC_EL98N_MCF_MSEL	0x0001	/* MEM enable */
#define		SIC_EL98N_MCF_PIO	0x0002	/* PIO enable */
#define		SIC_EL98N_MCF_C0	0x0000	/* membase 0xC0000 */
#define		SIC_EL98N_MCF_C8	0x0008	/* membase 0xC8000 */
#define		SIC_EL98N_MCF_D0	0x0010	/* membase 0xD0000 */
#define	SIC_EL98N_PCF		0	/* R: IRQ config/EEPROM data */
#define		SIC_EL98N_PCF_IRQ3	0x0010	/* configured IRQ is 3 */
#define		SIC_EL98N_PCF_IRQ5	0x0020	/* configured IRQ is 5 */
#define		SIC_EL98N_PCF_IRQMSK	0x0030	/* configured IRQ mask */
#define		SIC_EL98N_PCF_DATABIT	0x0001	/* eeprom data bit */

#define	SIC_EL98N_EEP		1	/* W: config IRQ/EEPROM access */
#define		SIC_EL98N_EEP_IRQ3	0x0009	/* config IRQ as 3 */
#define		SIC_EL98N_EEP_IRQ5	0x000A	/* config IRQ as 5 */
#define		SIC_EL98N_EEP_IRQCFG	0x0100	/* enable config IRQ */
#define		SIC_EL98N_EEP_SELECT	0x0080	/* EEPROM select */
#define		SIC_EL98N_EEP_CLOCK	0x0040	/* EEPROM clock */
#define		SIC_EL98N_EEP_DATA	0x0020	/* EEPROM data */
#define	SIC_EL98N_IDMSB		1	/* R: ident "3C" */

#define	SIC_EL98N_CR0		2	/* W: control register 0 */
#define		SIC_EL98N_CR0_RESET	0x0000	/* reset */
#define	SIC_EL98N_IDLSB		2	/* R: ident "om" */

#define	SIC_EL98N_BCF		3	/* W: config MEM/PIO */
#define		SIC_EL98N_BCF_CFGPIO	0x8000	/* config as PIO */
#define		SIC_EL98N_BCF_CFGMEM	0x0000	/* config as MEM */
#define		SIC_EL98N_BCF_ENBCFG	0x0001	/* config enable */
#define	SIC_EL98N_FIFOR		3	/* R: PIO/FIFO DATA */

#define	SIC_EL98N_FIFOW		4	/* W: PIO/FIFO DATA */

#define	SIC_EL98N_DA		5	/* W: PIO/DMA address */

#define	SIC_EL98N_CR1		6	/* W: control register 1 */
#define		SIC_EL98N_CR1_RESET	0x0000	/* reset */

#define	SIC_EL98N_CR2		7	/* W: control register 2 */
#define		SIC_EL98N_CR2_RESET	0x0000	/* reset */

#define	SIC_EL98N_EEPROM_SIZE	16
#define	SIC_EL98N_EEP_DELAY	1000

/* EEPROM allocation of 3COM EtherLink/98N. */
#define	SIC_EL98N_EEPROM_ADDR	0	/* Station address. (0-5) */

/*
 * Probe routines of
 *	3COM EtherLink/98N
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
 */
static int
sic_el98n_ctrl(vtag, asict, asich, mode, who, ia)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int mode, who;
	struct isa_attach_args *ia;
{
	int error;

	switch (who)
	{
	case SIC_CMD_INIT:
		if (ia->ia_iobase != 0x10D0)
			return EINVAL;
		error = sic_el98n_init(vtag, asict, asich, mode, 
				       ia->ia_iobase, ia->ia_maddr, ia->ia_irq);
		return error;
	case SIC_CMD_INT_OPEN:
		error = sic_el98n_initirq(vtag, asict, asich, ia->ia_irq);
		return error;
	default:
		return 0;
	}

	return 0;
}

static int
sic_el98n_init(vtag, asict, asich, rmode, iobase, maddr, irq)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int rmode;		/* requested mode */
	u_long iobase;
	u_long maddr;
	u_long irq;
{
	u_int16_t tmp;
	int mode;
	u_int32_t vcode;

	bus_space_write_2(asict, asich, SIC_EL98N_CR0, SIC_EL98N_CR0_RESET);
	delay(1000);

	if ((rmode & SIC_TYPE_SHM) == 0) {
		tmp = SIC_EL98N_MCF_PIO;
		mode = SIC_EL98N_BCF_CFGPIO;
	} else {
		mode = SIC_EL98N_BCF_CFGMEM;
		switch (maddr) {
		case 0xC0000:
			tmp = SIC_EL98N_MCF_C0;
			break;
		case 0xC8000:
			tmp = SIC_EL98N_MCF_C8;
			break;
		case 0xD0000:
			tmp = SIC_EL98N_MCF_D0;
			break;
		default:
			return EINVAL;
		}
	}

	bus_space_write_2(asict, asich, SIC_EL98N_MCF,
	    tmp | SIC_EL98N_MCF_MSEL);

	if (sic_el98n_initirq(vtag, asict, asich, irq) != 0)
		return EINVAL;

	bus_space_write_2(asict, asich, SIC_EL98N_BCF,
		mode | SIC_EL98N_BCF_ENBCFG);

	bus_space_write_2(asict, asich, SIC_EL98N_CR1, SIC_EL98N_CR1_RESET);
	bus_space_write_2(asict, asich, SIC_EL98N_CR2, SIC_EL98N_CR2_RESET);

	vcode = bus_space_read_2(asict, asich, SIC_EL98N_IDMSB) << 16;
	vcode |= bus_space_read_2(asict, asich, SIC_EL98N_IDLSB);

	if (vcode != 0x33436F6D)	/* "3Com" */
		return 1;

	return 0;
}

static int
sic_el98n_initirq(vtag, asict, asich, irq)
	sic_vendor_tag_t vtag;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_long irq;
{
	u_int16_t tmp;

	switch (irq) {
	case 3:
		tmp = SIC_EL98N_EEP_IRQ3;
		break;
	case 5:
		tmp = SIC_EL98N_EEP_IRQ5;
		break;
	default:
		return EINVAL;
	}

	bus_space_write_2(asict, asich, SIC_EL98N_EEP,
	    tmp | SIC_EL98N_EEP_IRQCFG);
	delay(1000);

	tmp = bus_space_read_2(asict, asich, SIC_EL98N_PCF) &
	    SIC_EL98N_PCF_IRQMSK;
	switch (tmp) {
	case SIC_EL98N_PCF_IRQ3:
		if (irq != 3)
			return 1;
		break;
	case SIC_EL98N_PCF_IRQ5:
		if (irq != 5)
			return 1;
		break;
	default:
		return 1;
	}

	return 0;
}

/* 
 * Routine to read one byte from EEPROM.
 */
static u_int16_t
sic_el98n_read_eeprom(bst, bsh, offset)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t offset;
{
	u_int8_t val, bit;
	u_int16_t rval, magic;

	magic = bus_space_read_2(bst, bsh, SIC_EL98N_IDMSB);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_CLOCK);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT);
	delay(SIC_EL98N_EEP_DELAY);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_DATA);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_CLOCK | SIC_EL98N_EEP_DATA);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_DATA);
	delay(SIC_EL98N_EEP_DELAY);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_DATA);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_CLOCK | SIC_EL98N_EEP_DATA);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_DATA);
	delay(SIC_EL98N_EEP_DELAY);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_CLOCK);
	delay(SIC_EL98N_EEP_DELAY);

	bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
	    SIC_EL98N_EEP_SELECT);
	delay(SIC_EL98N_EEP_DELAY);
	delay(SIC_EL98N_EEP_DELAY);

	for (bit = 0x20; bit != 0x00; bit >>= 1) {
		bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
		    SIC_EL98N_EEP_SELECT |
		    ((offset & bit) ? SIC_EL98N_EEP_DATA : 0));
		delay(SIC_EL98N_EEP_DELAY);

		bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
		    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_CLOCK |
		    ((offset & bit) ? SIC_EL98N_EEP_DATA : 0));
		delay(SIC_EL98N_EEP_DELAY);

		bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
		    SIC_EL98N_EEP_SELECT |
		    ((offset & bit) ? SIC_EL98N_EEP_DATA : 0));
		delay(SIC_EL98N_EEP_DELAY);
	}

	val = 0;

	for (bit = 0x80; bit != 0x00; bit >>= 1) {
		bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
		    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_CLOCK);
		delay(SIC_EL98N_EEP_DELAY);
		delay(SIC_EL98N_EEP_DELAY);

		bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
		    SIC_EL98N_EEP_SELECT);
		delay(SIC_EL98N_EEP_DELAY);

		if (bus_space_read_2(bst, bsh, SIC_EL98N_PCF)
		    & SIC_EL98N_PCF_DATABIT)
			val |= bit;
	}

	/* XXX: MSB, don't use */
	rval = (val << 8);

	val = 0;

	for (bit = 0x80; bit != 0x00; bit >>= 1) {
		bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
		    SIC_EL98N_EEP_SELECT | SIC_EL98N_EEP_CLOCK);
		delay(SIC_EL98N_EEP_DELAY);

		bus_space_write_2(bst, bsh, SIC_EL98N_EEP,
		    SIC_EL98N_EEP_SELECT);
		delay(SIC_EL98N_EEP_DELAY);

		if (bus_space_read_2(bst, bsh, SIC_EL98N_PCF)
		    & SIC_EL98N_PCF_DATABIT)
			val |= bit;
	}

	/* LSB, use it */
	rval |= val;

	/* stop */
	bus_space_write_2(bst, bsh, SIC_EL98N_EEP, magic);
	delay(SIC_EL98N_EEP_DELAY);

	return (rval);
}

static int
sic_el98n_read_ea(bustype, asict, asich, memt, memh, data, cksum)
	int bustype;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	u_int8_t *data;
	u_int8_t cksum;
{
	int i;

	/* 
	 * read physical address and check vendor code (00:40:B4)
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		data[i] = sic_el98n_read_eeprom(asict, asich, i);

	/* check 3COM vendor code */
	if (data[0] != 0x00 && data[1] != 0x40 && data[2] != 0x4B)
		return EINVAL;		

	return 0;
}

/*
 * detect, attach staff for PIO
 */
int	sic_el98n_pio_detectsubr
	__P((int, bus_space_tag_t, bus_space_handle_t, bus_space_tag_t, \
	bus_space_handle_t, bus_space_tag_t, bus_space_handle_t, \
	bus_addr_t, bus_size_t, u_int8_t));
int	sic_el98n_pio_attachsubr
	__P((dp8390pio_softc_t *, int, bus_addr_t, bus_size_t, u_int8_t *, \
	int *, int, int));
int	sic_el98n_pio_write_mbuf
	__P((struct dp8390_softc *, struct mbuf *, int));
void	sic_el98n_pio_writemem __P((bus_space_tag_t, bus_space_handle_t,
	    bus_space_tag_t, bus_space_handle_t, u_int8_t *, int, size_t, int));
void	sic_el98n_pio_readmem __P((bus_space_tag_t, bus_space_handle_t,
	    bus_space_tag_t, bus_space_handle_t, int, u_int8_t *, size_t, int));

int
sic_el98n_pio_attachsubr(nsc, type, start, size, myea, media, nmedia, defmedia)
	dp8390pio_softc_t *nsc;
	int type;
	bus_addr_t start;
	bus_size_t size;
	u_int8_t *myea;
	int *media, nmedia, defmedia;
{
	struct dp8390_softc *dsc = &nsc->sc_dp8390;

	dsc->write_mbuf = sic_el98n_pio_write_mbuf;
	nsc->sc_writemem = sic_el98n_pio_writemem;
	nsc->sc_readmem = sic_el98n_pio_readmem;

	return dp8390pio_attachsubr(nsc, type, start, size, myea, media, 
			     	    nmedia, defmedia);
}

/*
 * Detect an NE-2000 or compatible.  Returns a model code.
 */
int
sic_el98n_pio_detectsubr(type, nict, nich, asict, asich, memt, memh, ramstart, ramsize, cksum)
	int type;
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_addr_t ramstart;
	bus_size_t ramsize;
	u_int8_t cksum;
{
	struct dp8390pio_probe_args daa;

	dp8390pio_setup_probe_args(&daa, type,
				   nict, nich, asict, asich, memt, memh,
				   ramstart, ramsize);
	daa.da_pio_writemem = sic_el98n_pio_writemem;
	daa.da_pio_readmem = sic_el98n_pio_readmem;

	return __dp8390pio_detectsubr(&daa);
}

void
sic_el98n_pio_writemem(nict, nich, asict, asich, src, dst, len, useword)
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *src;
	int dst;
	size_t len;
	int useword;
{

	bus_space_write_2(asict, asich, SIC_EL98N_DA, dst);

	if (useword)
		bus_space_write_multi_stream_2(asict, asich, SIC_EL98N_FIFOW,
		    (u_int16_t *)src, len >> 1);
	else
		bus_space_write_multi_1(asict, asich, SIC_EL98N_FIFOW,
		    src, len);
}

/*
 * Given a NIC memory source address and a host memory destination address,
 * copy 'amount' from NIC to host using programmed i/o.  The 'amount' is
 * rounded up to a word - ok as long as mbufs are word sized.
 */
void
sic_el98n_pio_readmem(nict, nich, asict, asich, src, dst, amount, useword)
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int src;
	u_int8_t *dst;
	size_t amount;
	int useword;
{

	if (amount & 1)
		++amount;

	bus_space_write_2(asict, asich, SIC_EL98N_DA, src);

	if (useword)
		/* XXX: stream ? */
		bus_space_read_multi_2(asict, asich, SIC_EL98N_FIFOR,
		    (u_int16_t *)dst, amount >> 1);
	else
		bus_space_read_multi_1(asict, asich, SIC_EL98N_FIFOR, dst, amount);
}

/*
 * Write an mbuf chain to the destination NIC memory address using programmed
 * I/O.
 */
int
sic_el98n_pio_write_mbuf(sc, m, buf)
	struct dp8390_softc *sc;
	struct mbuf *m;
	int buf;
{
	struct sic_isa_softc *ssc = (struct sic_isa_softc *)sc;
	dp8390shm_softc_t *nsc = &ssc->sc_dsc;
#if 0	/* if 1, dp8398pio */
	bus_space_tag_t nict = sc->sc_regt;
	bus_space_handle_t nich = sc->sc_regh;
#endif
	bus_space_tag_t asict = nsc->sc_asict;
	bus_space_handle_t asich = nsc->sc_asich;
	int savelen;

	savelen = m->m_pkthdr.len;

	/* Set up destination address in NIC mem. */
	bus_space_write_2(asict, asich, SIC_EL98N_DA, buf);

	/*
	 * Transfer the mbuf chain to the NIC memory.  NE2000 cards
	 * require that data be transferred as words, and only words,
	 * so that case requires some extra code to patch over odd-length
	 * mbufs.
	 */
	if (nsc->sc_type & DP8390_BUS_PIO8) {
		/* NE1000s are easy. */
		for (; m != 0; m = m->m_next) {
			if (m->m_len) {
				bus_space_write_multi_1(asict, asich,
				    SIC_EL98N_FIFOW, mtod(m, u_int8_t *),
				    m->m_len);
			}
		}
	} else {
		/* NE2000s are a bit trickier. */
		u_int8_t *data, savebyte[2];
		int l, leftover;
#ifdef DIAGNOSTIC
		u_int8_t *lim;
#endif
		/* Start out with no leftover data. */
		leftover = 0;
		savebyte[0] = savebyte[1] = 0;

		for (; m != 0; m = m->m_next) {
			l = m->m_len;
			if (l == 0)
				continue;
			data = mtod(m, u_int8_t *);
#ifdef DIAGNOSTIC
			lim = data + l;
#endif
			while (l > 0) {
				if (leftover) {
					/*
					 * Data left over (from mbuf or
					 * realignment).  Buffer the next
					 * byte, and write it and the
					 * leftover data out.
					 */
					savebyte[1] = *data++;
					l--;
					bus_space_write_stream_2(asict, asich,
					    SIC_EL98N_FIFOW,
					    *(u_int16_t *)savebyte);
					leftover = 0;
				} else if (ALIGNED_POINTER(data,
					   u_int16_t) == 0) {
					/*
					 * Unaligned data; buffer the next
					 * byte.
					 */
					savebyte[0] = *data++;
					l--;
					leftover = 1;
				} else {
					/*
					 * Aligned data; output contiguous
					 * words as much as we can, then
					 * buffer the remaining byte, if any.
					 */
					leftover = l & 1;
					l &= ~1;
					bus_space_write_multi_stream_2(asict,
					    asich, SIC_EL98N_FIFOW,
					    (u_int16_t *)data, l >> 1);
					data += l;
					if (leftover)
						savebyte[0] = *data++;
					l = 0;
				}
			}
			if (l < 0)
				panic("ne2000_write_mbuf: negative len");
#ifdef DIAGNOSTIC
			if (data != lim)
				panic("ne2000_write_mbuf: data != lim");
#endif
		}
		if (leftover) {
			savebyte[1] = 0;
			bus_space_write_stream_2(asict, asich, SIC_EL98N_FIFOW,
			    *(u_int16_t *)savebyte);
		}
	}

	return (savelen);
}

/********************************************************
 * vendor tag definitions
 ********************************************************/
/*** sic98 ***/
static struct sic_vendor_tag sic_sic98_vendor_tag = {
	"sic98",

	{0x0, 0x200, 0x400, 0x600, 0x800, 0xa00, 0xc00, 0xe00,
	 0x1000, 0x1200, 0x1400, 0x1600, 0x1800, 0x1a00, 0x1c00, 0x1e00},

	{0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000,
	 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000},

	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},

	0,				/* ramstart */
	0,				/* ramsize */

	0,
	0,
	DP8390_BUS_SHM16,		/* bus type */

	SIC_TYPE_SHM | SIC_TYPE_NO_MULTI_BUFFERING,
	sic_sic98_ctrl,

	NULL,
	NULL,
	sic_shm_detectsubr,
	dp8390shm_attachsubr,
	sic_shm_read_ea,		/* vt_read_ea */
	NULL,				/* vt_init_card */
	NULL,				/* vt_media */
	0,				/* vt_nmedia */
	0,				/* vt_defmedia */
	NULL,				/* vt_mediachange */
	NULL,				/* vt_mediastatus */
};

/*** cnet98 ***/
static struct sic_vendor_tag sic_cnet98_vendor_tag = {
	"cnet98",

	{0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe,
	 0x400, 0x402, 0x404, 0x406, 0x408, 0x40a, 0x40c, 0x40e},

	{0x1, 0x3, 0x5, 0x7, 0x9, 0xb, 0xd, 0xf, 
	 0x401, 0x403, 0x405, 0x407, 0x409, 0x40b, 0xaaed, 0xaaef},

	{ -1, -1, -1, 0x01, -1, 0x02, 0x04,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1},

	0,				/* ramstart */
	0,				/* ramsize */

	ED_CR_RD2,
	0xff,
	DP8390_BUS_SHM8,		/* bus type */

	SIC_TYPE_SHM | SIC_TYPE_NO_MULTI_BUFFERING,
	sic_cnet98_ctrl,

	NULL,
	NULL,
	sic_shm_detectsubr,
	dp8390shm_attachsubr,
	sic_shm_read_ea,		/* vt_read_ea */
	NULL,				/* vt_init_card */
	NULL,				/* vt_media */
	0,				/* vt_nmedia */
	0,				/* vt_defmedia */
	NULL,				/* vt_mediachange */
	NULL,				/* vt_mediastatus */
};

/*** cnet98el ***/
static struct sic_vendor_tag sic_cnet98el_vendor_tag = {
	"cnet98el",

	{0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
	 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf},

	{0x40e, 0x400, 0x406, 0x407, 0x408, 0x409, 0x40a, 0x40b,
	 0x401, 0x402, 0x403, 0x404, 0x405, 0x40c, 0xaaed, 0xaaef},

	{-1, -1, -1, 0x01, -1, 0x02, 0x04,
	 -1, -1, 0x08, 0x10, -1, 0x20, 0x40, -1, -1},

	0,				/* ramstart */
	8192 * 2,			/* ramsize */

	ED_CR_RD2,
	0xff,
	DP8390_BUS_PIO16,		/* bus type */

	SIC_TYPE_PIO | SIC_TYPE_NO_MULTI_BUFFERING,
	sic_cnet98el_ctrl,

	dp8390pio_detectsubr,
	dp8390pio_attachsubr,
	NULL,
	NULL,
	NULL,				/* vt_read_ea */
	NULL,				/* vt_init_card */
	NULL,				/* vt_media */
	0,				/* vt_nmedia */
	0,				/* vt_defmedia */
	NULL,				/* vt_mediachange */
	NULL,				/* vt_mediastatus */
};

/*** NEC PC-9801-77/78 ***/
static struct sic_vendor_tag sic_pc980178_vendor_tag = {
	"pc980178",

	{0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000,
	 0x8000, 0x9000, 0xA000, 0xB000, 0xC000, 0xD000, 0xE000, 0xF000},

	{0x0100, 0x0200, 0x1100, 0x1200, 0x0100, 0x2200, 0x0100, 0x0100,
	 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x2100},

	{-1, -1, -1, 4, -1, 6, 8, -1, -1, -1, -1, -1, 10, 2, -1, -1},

	0,				/* ramstart */
	0,				/* ramsize */

	ED_CR_RD2,
	0xff,
	DP8390_BUS_PIO16,		/* bus type */

	SIC_TYPE_PIO,
	sic_pc980178_ctrl,

	dp8390pio_detectsubr,
	dp8390pio_attachsubr,
	NULL,
	NULL,
	NULL,				/* vt_read_ea */
	NULL,				/* vt_init_card */
	NULL,				/* vt_media */
	0,				/* vt_nmedia */
	0,				/* vt_defmedia */
	NULL,				/* vt_mediachange */
	NULL,				/* vt_mediastatus */
};

/*** Networld EP98X  ***/
static struct sic_vendor_tag sic_ep98x_vendor_tag = {
	"ep98x",

	{0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
	 0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00},

	{0x1000, 0x2000, 0x1200, 0x1300, 0x1600, 0x1700, 0x1A00, 0x1B00,
	 0x1E00, 0x1F00, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x1200},

	{-1, -1, -1, 4, -1, 6, 8, -1, -1, -1, -1, -1, 10, 2, -1, -1},

	0,				/* ramstart */
	0,				/* ramsize */

	ED_CR_RD2,
	0xff,
	DP8390_BUS_PIO16,		/* bus type */

	SIC_TYPE_PIO,
	sic_ep98x_ctrl,

	dp8390pio_detectsubr,
	dp8390pio_attachsubr,
	NULL,
	NULL,
	NULL,				/* vt_read_ea */
	NULL,				/* vt_init_card */
	NULL,				/* vt_media */
	0,				/* vt_nmedia */
	0,				/* vt_defmedia */
	NULL,				/* vt_mediachange */
	NULL,				/* vt_mediastatus */
};

/*** sb9801 ***/
static struct sic_vendor_tag sic_sb98_vendor_tag = {
	"sb98",

	{0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000,
	 0x8000, 0x9000, 0xA000, 0xB000, 0xC000, 0xD000, 0xE000, 0xF000},

	{0x0400, 0x1400, 0x2400, 0x3400, 0x4400, 0x5400, 0x6400, 0x0400,
	 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x7400},

	{-1, -1, -1, 0, -1, 4, 8, -1, -1, -1, -1, -1, 12, -1, -1, -1},

	0,				/* ramstart */
	0,				/* ramsize */

	ED_CR_RD2,			/* vt_proto */
	0xff,				/* vt_cksum */
	DP8390_BUS_PIO16,		/* bus type */

	SIC_TYPE_PIO,
	sic_sb98_ctrl,

	dp8390pio_detectsubr,
	dp8390pio_attachsubr,
	NULL,
	NULL,
	sic_sb98_read_ea,		/* vt_read_ea */
	sic_sb98_init_card,		/* vt_init_card */
	sic_sb98_media,			/* vt_media */
	3,				/* vt_nmedia */
	IFM_ETHER | IFM_10_T,	/* XXX *//* vt_defmedia */
	sic_sb98_mediachange,		/* vt_mediachange */
	sic_sb98_mediastatus,		/* vt_mediastatus */
};

/*** el98 ***/
static struct sic_vendor_tag sic_el98_vendor_tag = {
	"el98",

	{0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
	 0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00},

	{0x2000, 0x4000, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x2000,
	 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x6000},

	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},

	0,				/* ramstart */
	0x8000,				/* ramsize */

	ED_CR_RD2,			/* vt_proto */
	0xff,				/* vt_cksum */
	DP8390_BUS_PIO16 | DP8390_BUS_SHM16, 

	SIC_TYPE_PIO | SIC_TYPE_SHM,	/* XXX */
	sic_el98_ctrl,

	dp8390pio_detectsubr,
	dp8390pio_attachsubr,
	sic_shm_detectsubr,
	dp8390shm_attachsubr,
	sic_el98_read_ea,		/* vt_read_ea */
	NULL,				/* vt_init_card */
	NULL,				/* vt_media */
	0,				/* vt_nmedia */
	0,				/* vt_defmedia */
	NULL,				/* vt_mediachange */
	NULL,				/* vt_mediastatus */
};

/*** el98n ***/
static struct sic_vendor_tag sic_el98n_vendor_tag = {
	"el98n",

	{0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
	 0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00},

	{0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700,
	 0x1400, 0x1400, 0x1400, 0x1400, 0x1400, 0x1400, 0x1400, 0x1700},

	{-1, -1, -1,  3, -1,  5, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1},

	0,				/* ramstart */
	0x8000,				/* ramsize */

	ED_CR_RD2,			/* vt_proto */
	0xff,				/* vt_cksum */
	DP8390_BUS_PIO16 | DP8390_BUS_SHM16,

	SIC_TYPE_PIO | SIC_TYPE_SHM,	/* XXX */
	sic_el98n_ctrl,

	sic_el98n_pio_detectsubr,
	sic_el98n_pio_attachsubr,
	sic_shm_detectsubr,
	dp8390shm_attachsubr,
	sic_el98n_read_ea,		/* vt_read_ea */
	NULL,				/* vt_init_card */
	NULL,				/* vt_media */
	0,				/* vt_nmedia */
	0,				/* vt_defmedia */
	NULL,				/* vt_mediachange */
	NULL,				/* vt_mediastatus */
};

static dvcfg_hw_t sic_hwsel_array[] = {
/* 0x00 */	&sic_sic98_vendor_tag,
/* 0x01 */	&sic_sic98_vendor_tag,
/* 0x02 */	&sic_cnet98_vendor_tag,
/* 0x03 */	&sic_cnet98el_vendor_tag,	/* dubious */
/* 0x04 */	&sic_pc980178_vendor_tag,	/* (PIO) */
/* 0x05 */	&sic_ep98x_vendor_tag,		/* (PIO) */
/* 0x06 */	&sic_sb98_vendor_tag,		/* (PIO) */
/* 0x07 */	&sic_el98_vendor_tag,		/* (PIO/SHM) */
/* 0x08 */	&sic_el98n_vendor_tag,		/* (PIO/SHM) */
};

struct dvcfg_hwsel sic_hwsel = {
	DVCFG_HWSEL_SZ(sic_hwsel_array),
	sic_hwsel_array
};

static sic_vendor_tag_t
sic_vendor_tag(ia)
	struct isa_attach_args *ia;
{

	return DVCFG_HW(&sic_hwsel, (DVCFG_MAJOR(ia->ia_cfgflags) >> 4));
}
