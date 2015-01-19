/*	$NecBSD: nepc.c,v 1.28.4.5 1999/08/31 09:10:56 honda Exp $	*/
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

/*
 * PC-9801 original PCCS controller code for NS/A, Ne, NX/C, NL/R.
 * by Noriyuki Hosobuchi
 */

#include "opt_pccshw.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <vm/vm.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

#include <i386/Cbus/pccs16/nepcvar.h>
#include <i386/Cbus/pccs16/nepcreg.h>
#include <i386/Cbus/pccs16/busiosubr_nec.h>

#include <machine/bus.h>
#include <machine/systmbusvar.h>

_PCCSHW_FUNCS_PROT(nepc)
struct pccshw_funcs pccshw_nepc_funcs = {
	_PCCSHW_FUNCS_TAB(nepc)
};

static void nepc_sel_macc __P((pccshw_tag_t, int));
static __inline u_int8_t nepc_reg_read_1 __P((pccshw_tag_t, u_int));
static __inline u_int16_t nepc_reg_read_2 __P((pccshw_tag_t, u_int));
static __inline void nepc_reg_write_1 __P((pccshw_tag_t, u_int, u_int8_t));
static __inline void nepc_reg_write_2 __P((pccshw_tag_t, u_int, u_int16_t));
static __inline void nepc_set_bits __P((pccshw_tag_t, u_int, u_int8_t));
static __inline void nepc_clr_bits __P((pccshw_tag_t, u_int, u_int8_t));

extern struct cfdriver nepc_cd;

/********************************************
 * Probe attach subroutines
 *******************************************/
int
nepcmatchsubr(bst, bsh) 
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
{
	u_int8_t reg0v;
	int rv = 1;

	reg0v = bus_space_read_1(bst, bsh, nepc_reg0);
#ifdef	_NEPC_DEBUG
	printf("nepcmatchsubr: initial value of REG0 is %x\n", reg0v);
#endif	/* _NEPC_DEBUG */

	/* select socket 0 */
	bus_space_write_1(bst, bsh, nepc_reg0, 0);
	if (bus_space_read_1(bst, bsh, nepc_reg0) != 0xf8)
		rv = 0;

	bus_space_write_1(bst, bsh, nepc_reg0, reg0v);
	return rv;
}

void
nepc_attachsubr(sc)
	struct nepc_softc *sc;
{
	pccshw_tag_t pp = &sc->sc_pp;

	pp->pp_hw = &pccshw_nepc_funcs;
	pp->pp_type = PISA_SLOT_PCMCIA;
	pp->pp_sb = &pisa_slot_device_bus_tag;
	pp->pp_pccs = &pccs_config_16bits_funcs;
	pp->pp_iot = sc->sc_busiot;
	pp->pp_memt = sc->sc_busmemt;
	pp->pp_csbst = sc->sc_csbst;
	pp->pp_csbsh = sc->sc_csbsh;
	pp->pp_csim = sc->sc_csim;
	pp->pp_nio = SLOT_DEVICE_NIO;			/* XXX */
	pp->pp_nmem = SLOT_DEVICE_NMEM;			/* XXX */
	pp->pp_nim = pp->pp_nio + pp->pp_nmem;
	pp->pp_dv = sc;
	pp->pp_psz = 2 * NBPG;
	pp->pp_ps = 13;
	pp->pp_maxslots = 1;

	pp->pp_chip = CHIP_NEC;
	printf("%s: chipset NEC original\n", sc->sc_dev.dv_xname);

	nepc_reg_write_1(pp, nepc_reg0, 0);
	sc->sc_btype = nepc_reg_read_1(pp, nepc_reg_winsel);
	sc->sc_bpage = nepc_reg_read_2(pp, nepc_reg_pagofs);
#ifdef	DIAGNOSTIC
	if (sc->sc_btype == sc->sc_busim_mem.im_base)
	{
		printf("WARNING: pccs memory window already selected\n");
		printf("PLEASE POWER OFF your note and try again\n");
		sc->sc_btype = PCIC98_MAPROM;	/* dubious code */
	}
#endif	/* DIAGNOSTIC */
}

/********************************************
 * Base Func
 *******************************************/
static __inline u_int8_t
nepc_reg_read_1(pp, reg)
	pccshw_tag_t pp;
	u_int reg;
{
	register struct nepc_softc *sc = pp->pp_dv;
	
	return bus_space_read_1(sc->sc_bst, sc->sc_bsh, reg);
}	

static __inline u_int16_t
nepc_reg_read_2(pp, reg)
	pccshw_tag_t pp;
	u_int reg;
{
	register struct nepc_softc *sc = pp->pp_dv;
	
	return bus_space_read_2(sc->sc_bst, sc->sc_bsh, reg);
}	

static __inline void
nepc_reg_write_1(pp, reg, val)
	pccshw_tag_t pp;
	u_int reg;
	u_int8_t val;
{
	register struct nepc_softc *sc = pp->pp_dv;

	bus_space_write_1(sc->sc_bst, sc->sc_bsh, reg, val);
}

static __inline void
nepc_reg_write_2(pp, reg, val)
	pccshw_tag_t pp;
	u_int reg;
	u_int16_t val;
{
	register struct nepc_softc *sc = pp->pp_dv;

	bus_space_write_2(sc->sc_bst, sc->sc_bsh, reg, val);
}

static void
nepc_sel_macc(pp, acc)
	pccshw_tag_t pp;
	int acc;
{

	nepc_reg_write_1(pp, nepc_reg0, 0);
	nepc_clr_bits(pp, nepc_reg1, PCIC98_CARDRAM | PCIC98_CARDEMS);
	if (acc == 1)
		nepc_set_bits(pp, nepc_reg1, PCIC98_CARDRAM);
}

static __inline void
nepc_set_bits(pp, reg, mask)
	pccshw_tag_t pp;
	u_int reg;
	u_int8_t mask;
{

	nepc_reg_write_1(pp, reg, nepc_reg_read_1(pp, reg) | mask);
}

static __inline void
nepc_clr_bits(pp, reg, mask)
	pccshw_tag_t pp;
	u_int reg;
	u_int8_t mask;
{

	nepc_reg_write_1(pp, reg, nepc_reg_read_1(pp, reg) & (~mask));
}
/********************************************
 * Power and Reset
 *******************************************/
int
nepc_power(pp, sid, powp, on)
	pccshw_tag_t pp;
	slot_handle_t sid;
	struct power *powp;
	int on;
{
	u_int8_t reg;

	switch (on)
	{
	case PCCSHW_POWER_ON:
	case PCCSHW_POWER_UPDATE:
		nepc_reg_write_1(pp, nepc_reg0, 0);
		reg = nepc_reg_read_1(pp, nepc_reg6) & (~PCIC98_VPP12V);
		switch (powp->pow_vpp0)
		{
		case 50:
		default:
			break;
		case 120:
			reg |= PCIC98_VPP12V;
			break;
		}
		nepc_reg_write_1(pp, nepc_reg6, reg);
		pccs_wait(100);
		reg = nepc_reg_read_1(pp, nepc_reg2) & (~PCIC98_VCC3P3V);
		if (powp->pow_vcc0 == PCCS_POWER_DEFAULT || powp->pow_vcc0 == 50)
		{
			/* default */
		}
		else if (powp->pow_vcc0 == 33)
		{
			reg |= PCIC98_VCC3P3V;
		}
		nepc_reg_write_1(pp, nepc_reg2, reg);
		pccs_wait(100);
		break;

	case PCCSHW_POWER_OFF:
#ifdef	notyet
		/* XXX */
		nepc_reg_write_1(pp, nepc_reg0, 7);
#endif	/* notyet */
		break;
	}

	return 0;
}

void
nepc_reset(pp, sid, type)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int type;
{
	struct nepc_softc *sc = pp->pp_dv;

	nepc_reg_write_1(pp, nepc_reg0, 0);

	nepc_clr_bits(pp, nepc_reg2, PCIC98_MAPIO);
	nepc_reg_write_1(pp, nepc_reg3, PCIC98_INTDISABLE);

	nepc_sel_macc(pp, 0);
	nepc_reg_write_1(pp, nepc_reg_winsel, sc->sc_btype);
	nepc_reg_write_2(pp, nepc_reg_pagofs, sc->sc_bpage);
	nepc_clr_bits(pp, nepc_reg6, PCIC98_ATTRMEM);
	pccs_wait(100);

	nepc_clr_bits(pp, nepc_reg2, PCIC98_VCC3P3V);
	nepc_clr_bits(pp, nepc_reg6, PCIC98_VPP12V);
	pccs_wait(100);

	nepc_reg_write_1(pp, nepc_reg1, 0);
	/* XXX:
	 * How to check a RDY/BSY state of io interfaces? 
	 * anyway, wait about 500 ms.
	 */
	pccs_wait(500);
}

/********************************************
 * Io and Mem Mapping
 *******************************************/
int nepc_chkmem __P((pccshw_tag_t, slot_handle_t, int, struct slot_device_iomem *));
int nepc_mapmem __P((pccshw_tag_t, slot_handle_t, int, struct slot_device_iomem *));
int nepc_chkio __P((pccshw_tag_t, slot_handle_t, int, struct slot_device_iomem *));
int nepc_mapio __P((pccshw_tag_t, slot_handle_t, int, struct slot_device_iomem *));

void
nepc_reclaim(pp, sid)
	pccshw_tag_t pp;
	slot_handle_t sid;
{
	struct nepc_softc *sc = pp->pp_dv;

#ifdef	_NEPC_DEBUG
	printf("nepc_clear: IN\n");
#endif	/* _NEPC_DEBUG */
	sc->sc_immask = 0;

	nepc_reg_write_1(pp, nepc_reg0, 0);

	/* shutdown an interrupt pin */
	nepc_reg_write_1(pp, nepc_reg3, PCIC98_INTDISABLE);

	/* unmap io windows (dubious) */
	nepc_clr_bits(pp, nepc_reg2, PCIC98_MAPIO);

	/* unmap memory windows and clear attribute access */
	nepc_sel_macc(pp, 0);
	nepc_reg_write_1(pp, nepc_reg_winsel, sc->sc_btype);
	nepc_reg_write_2(pp, nepc_reg_pagofs, sc->sc_bpage);
	nepc_clr_bits(pp, nepc_reg6, PCIC98_ATTRMEM);

#ifdef	_NEPC_DEBUG
	printf("nepc_clear: OUT\n");
#endif	/* _NEPC_DEBUG */
}

int
nepc_map(pp, sid, wp, sph, flags)
	pccshw_tag_t pp;
	slot_handle_t sid;
	struct slot_device_iomem *wp;
	window_handle_t *sph;
	int flags;
{
	struct nepc_softc *sc = pp->pp_dv;
	u_int id, maxid;
	int error = 0;

#ifdef	_NEPC_DEBUG
	printf("nepc_map: IN\n");
#endif	/* _NEPC_DEBUG */
	if ((wp->im_flags & SDIM_BUS_ACTIVE) == 0)
		return EINVAL;

	switch(wp->im_type)
	{
	case SLOT_DEVICE_SPMEM:
		id = SLOT_DEVICE_NIO;
		maxid = id + pp->pp_nmem;
		break;

	case SLOT_DEVICE_SPIO:
	default:
		id = 0;
		maxid = pp->pp_nio;
#ifdef	_NEPC_DEBUG
		printf("nepc_map: maxid %x\n", maxid);
#endif	/* _NEPC_DEBUG */
		break;
	}

	for ( ; id < maxid; id ++)
		if ((sc->sc_immask & (1 << id)) == 0)
			break;
	if (id == maxid)
		return EBUSY;

	sc->sc_immask |= 1 << id;
	if (wp->im_size == 0)
		goto out;

	switch(wp->im_type)
	{
	case SLOT_DEVICE_SPMEM:
		if (flags & PCCSHW_CHKRANGE)
		{
			error = nepc_chkmem(pp, sid, id - SLOT_DEVICE_NIO, wp);
			if (error)
			{
				sc->sc_immask &= ~(1 << id);
				return error;
			}
		}
		nepc_mapmem(pp, sid, id - SLOT_DEVICE_NIO, wp);
		break;

	case SLOT_DEVICE_SPIO:
		if (flags & PCCSHW_CHKRANGE)
		{
#ifdef	_NEPC_DEBUG
			printf("nepc_map: PCCSHW_CHKRANGE: id %x\n", id);
#endif	/* _NEPC_DEBUG */
			error = nepc_chkio(pp, sid, id, wp);
			if (error)
			{
				sc->sc_immask &= ~(1 << id);
				return error;
			}
		}
		nepc_mapio(pp, sid, id, wp);
		break;
	}

out:
#ifdef	_NEPC_DEBUG
	printf("nepc_map: OUT\n");
#endif	/* _NEPC_DEBUG */
	*sph = id;
	return error;
}

int
nepc_unmap(pp, sid, sph)
	pccshw_tag_t pp;
	slot_handle_t sid;
	window_handle_t sph;
{
	struct nepc_softc *sc = pp->pp_dv;
	u_int id, mask;

#ifdef	_NEPC_DEBUG
	printf("nepc_unmap: IN\n");
#endif	/* _NEPC_DEBUG */

	id = sph;
	sc->sc_immask &= ~(1 << id);
	mask = (1 << SLOT_DEVICE_NIO) - 1;

	if ((sc->sc_immask & (~mask)) == 0)
	{
		nepc_sel_macc(pp, 0);
		nepc_reg_write_1(pp, nepc_reg_winsel, sc->sc_btype);
		nepc_reg_write_2(pp, nepc_reg_pagofs, sc->sc_bpage);
		nepc_clr_bits(pp, nepc_reg6, PCIC98_ATTRMEM);
#ifdef	_NEPC_DEBUG
		printf("nepc_unmap: mem win disable\n");
#endif	/* _NEPC_DEBUG */
	}

	if ((sc->sc_immask & mask) == 0)
	{
		nepc_clr_bits(pp, nepc_reg2, PCIC98_MAPIO);
#ifdef	_NEPC_DEBUG
		printf("nepc_unmap: io win disable\n");
#endif	/* _NEPC_DEBUG */
	}

#ifdef	_NEPC_DEBUG
	printf("nepc_unmap: OUT\n");
#endif	/* _NEPC_DEBUG */
	return 0;
}

int
nepc_chkmem(pp, sid, id, wp)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int id;
	struct slot_device_iomem *wp;
{
	struct nepc_softc *sc = pp->pp_dv;

#ifdef	_NEPC_DEBUG
	printf("nepc_chkmem: IN\n");
#endif	/* _NEPC_DEBUG */
	if (wp->im_hwbase != sc->sc_busim_mem.im_hwbase ||
	    wp->im_base == PUNKADDR || wp->im_size > pp->pp_psz)
		return EINVAL;
#ifdef	_NEPC_DEBUG
	printf("nepc_chkmem: OUT\n");
#endif	/* _NEPC_DEBUG */
	return 0;
}

int
nepc_mapmem(pp, sid, id, wp)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int id;
	struct slot_device_iomem *wp;
{
	struct nepc_softc *sc = pp->pp_dv;

#ifdef	_NEPC_DEBUG
	printf("nepc_mapmem: IN\n");
#endif	/* _NEPC_DEBUG */
	if (id > 0)
		return 0;

	nepc_sel_macc(pp, 1);
	nepc_reg_write_1(pp, nepc_reg_winsel, sc->sc_busim_mem.im_base);
	nepc_reg_write_2(pp, nepc_reg_pagofs, wp->im_base >> pp->pp_ps);
	if (wp->im_flags & WFMEM_ATTR)
		nepc_set_bits(pp, nepc_reg6, PCIC98_ATTRMEM);
	else
		nepc_clr_bits(pp, nepc_reg6, PCIC98_ATTRMEM);

#if 0
	if (wp->im_flags & SDIM_BUS_WIDTH16)
	    nepc_clr_bits(pp, nepc_reg2, PCIC98_8BIT);
	else					/* 8bit */
	    nepc_set_bits(pp, nepc_reg2, PCIC98_8BIT);
#endif
	delay(50);

#ifdef	_NEPC_DEBUG
	printf("nepc_mapmem: OUT\n");
#endif	/* _NEPC_DEBUG */
	return 0;
}

int
nepc_chkio(pp, sid, winid, wp)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int winid;
	struct slot_device_iomem *wp;
{

#ifdef	_NEPC_DEBUG
	printf("nepc_chkio: IN winid %x\n", winid);
#endif	/* _NEPC_DEBUG */
	if (wp->im_size > 128)
		return EINVAL;
#ifdef	_NEPC_DEBUG
	printf("nepc_chkio: OUT\n");
#endif	/* _NEPC_DEBUG */
	return 0;
}

int
nepc_mapio(pp, sid, winid, wp)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int winid;
	struct slot_device_iomem *wp;
{
	struct nepc_softc *sc = pp->pp_dv;
	u_int8_t val;

#ifdef	_NEPC_DEBUG
	printf("nepc_mapio: IN winid %x\n", winid);
#endif	/* _NEPC_DEBUG */
	if (winid > 0)
		return 0;

	val = nepc_reg_read_1(pp, nepc_reg2) & 0x0f;
	if ((wp->im_flags & (SDIM_BUS_WIDTH16 | SDIM_BUS_AUTO)) == 0 ||
	    (wp->im_flags & (SDIM_BUS_WIDTH16 | SDIM_BUS_FOLLOW)) == SDIM_BUS_FOLLOW)
		val |= PCIC98_8BIT;

	val |= PCIC98_IOMAP128;
	val |= PCIC98_MAPIO;

	nepc_reg_write_2(pp, nepc_reg4, sc->sc_busim_io.im_hwbase);
	nepc_reg_write_2(pp, nepc_reg5, wp->im_base & (~0x7f));

	/* finally open */
	nepc_reg_write_1(pp, nepc_reg2, val);
	delay(50);

#ifdef	_NEPC_DEBUG
	printf("nepc_mapio: REG4 %lx <-> REG5 %lx (%lx)\n",
	    wp->im_base, wp->im_hwbase, sc->sc_busim_io.im_hwbase);
#endif	/* _NEPC_DEBUG */
	return 0;
}

/********************************************
 * Intr
 *******************************************/
#define	PGIRQ(bc, mask)	(isa_get_empty_irq((bc), (mask)))
#define PCCSHW_EMPTY_IRQ(bc, dc) PGIRQ((bc), (dc)->dc_aux)
#define	PCCSHW_VALID_IRQ(bc, irq) PGIRQ((bc), 1 << (irq))

int
nepc_chkirq(pp, sid, bc, dc)
	pccshw_tag_t pp;
	slot_handle_t sid;
	bus_chipset_tag_t bc;
	struct slot_device_channel *dc;
{
	struct nepc_softc *sc = pp->pp_dv;
	
	dc->dc_aux &= sc->sc_irqmask;
	if (dc->dc_chan == 0)
		dc->dc_chan = IRQUNK;
	else if (dc->dc_chan == PAUTOIRQ)
	{
		dc->dc_chan = PCCSHW_EMPTY_IRQ(bc, dc);
		if (dc->dc_chan == IRQUNK)
		{
			printf("%s: no irq left\n", sc->sc_dev.dv_xname);
			return EINVAL;
		}
	}
	else if (dc->dc_chan != IRQUNK &&
		 ((dc->dc_aux & (1 << dc->dc_chan)) == 0 ||
		  PCCSHW_VALID_IRQ(bc, dc->dc_chan) == IRQUNK))
	{
		printf("%s: invalid irq\n", sc->sc_dev.dv_xname);
		return EINVAL;
	}	
	return 0;
}

void
nepc_routeirq(pp, sid, dc)
	pccshw_tag_t pp;
	slot_handle_t sid;
	struct slot_device_channel *dc;
{
	u_int8_t x;

	nepc_reg_write_1(pp, nepc_reg3, PCIC98_INTDISABLE);
	if (dc == NULL)
		return;

	switch (dc->dc_chan)
	{
	case 3:
		x = PCIC98_INT0; break;
	case 5:
		x = PCIC98_INT1; break;
	case 6:
		x = PCIC98_INT2; break;
	case 10:
		x = PCIC98_INT4; break;
	case 12:
		x = PCIC98_INT5; break;
	default:
		return;
	}

	nepc_reg_write_1(pp, nepc_reg3, x);
}

int
nepc_stat(pp, sid, scmd)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int scmd;
{

	switch (scmd)	
	{
	case PCCSHW_HASSLOT:
		return (sid == 0);

	case PCCSHW_CARDIN:
		return nepc_reg_read_1(pp, nepc_reg1) & PCIC98_CARDEXIST;

	default:
		return 0;
	}
}

int
nepc_auxcmd(pp, sid, cmd)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int cmd;
{
	
	return 0;
}

bus_addr_t
_nepc_io_swapaddr(bst, addr)
	bus_space_tag_t bst;
	register bus_addr_t addr;
{
	struct nepc_softc *sc = bst->bs_busc;
	register bus_addr_t ofst;

#ifdef	_NEPC_DEBUG
	printf("_nepc_swapaddr: IN addr %lx\n", addr);
#endif	/* _NEPC_DEBUG */
	ofst = addr & 0x0f;
	ofst += ((addr & 0x70) << 4);
#ifdef	_NEPC_DEBUG
	printf("_nepc_swapaddr: OUT ofst %lx\n",
		ofst + sc->sc_busim_io.im_hwbase);
#endif	/* _NEPC_DEBUG */
	return ofst + sc->sc_busim_io.im_hwbase;
}

bus_addr_t
_nepc_mem_swapaddr(bst, addrp, bshp)
	bus_space_tag_t bst;
	bus_addr_t *addrp;
	bus_space_handle_t *bshp;
{
	struct nepc_softc *sc = bst->bs_busc;

	if (addrp != NULL)
		*addrp = sc->sc_busim_mem.im_hwbase + *addrp;
	if (bshp != NULL)
		*bshp = sc->sc_csbsh;
	return 0;
}

int
nepc_swapspace(pp, sid, idr, odr)
	pccshw_tag_t pp;
	slot_handle_t sid;
	slot_device_res_t idr, odr;
{
	
#ifdef	_NEPC_DEBUG
	printf("nepc_swapspace: IN\n");
#endif	/* _NEPC_DEBUG */

	*odr = *idr;

#ifdef	_NEPC_DEBUG
	printf("nepc_swapspace: OUT");
#endif	/* _NEPC_DEBUG */
	return 0;
}
