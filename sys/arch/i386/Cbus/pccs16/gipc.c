/*	$NecBSD: gipc.c,v 1.14.4.1 1999/08/31 09:10:54 honda Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <vm/vm.h>

#include <machine/bus.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>
#include <i386/pccs/pccshw.h>

#include <i386/Cbus/pccs16/i82365reg.h>
#include <i386/Cbus/pccs16/gipcvar.h>

_PCCSHW_FUNCS_PROT(gipc)

struct pccshw_funcs pccshw_gipc_funcs = {
	_PCCSHW_FUNCS_TAB(gipc)
};

extern struct cfdriver gipc_cd;

static void gipc_open_intr __P((pccshw_tag_t, slot_handle_t));
static void gipc_chip_detect __P((struct gipc_softc *));
static int gipc_access_timing __P((pccshw_tag_t, slot_handle_t, u_int));
static __inline void gipc_reg_write_1 
	__P((pccshw_tag_t, slot_handle_t, u_int, u_int8_t));
static __inline u_int8_t gipc_reg_read_1 
	__P((pccshw_tag_t, slot_handle_t, u_int));
static __inline void gipc_set_bits 
	__P((pccshw_tag_t, slot_handle_t, u_int, u_int8_t));
static __inline void gipc_clr_bits 
	__P((pccshw_tag_t, slot_handle_t, u_int, u_int8_t));

#define	IOFGSHIFT 4
#define	GIPC_IS_ID_VALID(id) ((id) == 0x82 || (id) == 0x83 || (id) == 0x84)
#define	GIPC_IS_ID_BRIDGE(id) ((id) == 0x84)

/********************************************
 * Probe Attach
 *******************************************/
int
gipcprobesubr(bst, bsh)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
{
	u_int8_t val;

	bus_space_write_1(bst, bsh, i82365_index, REG_ID);
	val = bus_space_read_1(bst, bsh, i82365_data); 
	if (GIPC_IS_ID_VALID(val) == 0)
		return 0;

	return ((u_int) val);
}

void
gipcattachsubr(sc)
	struct gipc_softc *sc;
{
	pccshw_tag_t pp = &sc->sc_pp;
	int maxslots = GIPC_MAX_SLOTS;
	u_char *s;

	printf("\n");

	pp->pp_hw = &pccshw_gipc_funcs;
	pp->pp_type = PISA_SLOT_PCMCIA;
	pp->pp_sb = &pisa_slot_device_bus_tag;
	pp->pp_pccs = &pccs_config_16bits_funcs;
	pp->pp_csbst = sc->sc_csbst;
	pp->pp_csbsh = sc->sc_csbsh;
	pp->pp_csim = sc->sc_csim;
	pp->pp_iot = sc->sc_busiot;
	pp->pp_memt = sc->sc_busmemt;
	pp->pp_dv = sc;
	pp->pp_psz = NBPG;
	pp->pp_ps = 12;
	pp->pp_nio = 2;
	pp->pp_nmem = 5;
	pp->pp_nim = pp->pp_nio + pp->pp_nmem;

	gipc_chip_detect(sc);
	switch (pp->pp_chip)
	{
	case CHIP_BRIDGE_PCIISA:
		s = "PCI ISA PCCS bridge";
		maxslots = 2;
		break;

	case CHIP_I82365:
		s = "Intel i82365";
		maxslots = 2;
		break;

	case CHIP_PD672X:
		s = "Cirrus PD672x";
		break;

	case CHIP_PD6710:
		s = "Cirrus PD6710";
		break;

	case CHIP_VG468:
		s = "Vadem 468";
		break;

	case CHIP_VG469:
		s = "Vadem 469";
		break;

	default:
		s = "unknown";
		maxslots = 2;
		break;
	}

	pp->pp_maxslots = maxslots;
	printf("%s: chipset <%s>\n", sc->sc_dev.dv_xname, s);
}

/********************************************
 * Base Func
 *******************************************/
static __inline u_int8_t
gipc_reg_read_1(pp, sid, reg)
	pccshw_tag_t pp;
	slot_handle_t sid;
	u_int reg;
{
	register struct gipc_softc *sc = pp->pp_dv;
	register bus_space_tag_t bst = sc->sc_bst;
	register bus_space_handle_t bsh = sc->sc_bsh;
	bus_size_t offset = sid * REG_SLOT_SKIP + reg;

	bus_space_write_1(bst, bsh, i82365_index, offset);
	return bus_space_read_1(bst, bsh, i82365_data);
}

static __inline void
gipc_reg_write_1(pp, sid, reg, data)
	pccshw_tag_t pp;
	slot_handle_t sid;
	u_int reg;
	u_int8_t data;
{
	register struct gipc_softc *sc = pp->pp_dv;
	register bus_space_tag_t bst = sc->sc_bst;
	register bus_space_handle_t bsh = sc->sc_bsh;
	bus_size_t offset = sid * REG_SLOT_SKIP + reg;

	bus_space_write_1(bst, bsh, i82365_index, offset);
	bus_space_write_1(bst, bsh, i82365_data, data);
}

static __inline void
gipc_set_bits(pp, sid, reg, mask)
	pccshw_tag_t pp;
	slot_handle_t sid;
	u_int reg;
	u_int8_t mask;
{

	gipc_reg_write_1(pp, sid, reg, gipc_reg_read_1(pp, sid, reg) | mask);
}

static __inline void
gipc_clr_bits(pp, sid, reg, mask)
	pccshw_tag_t pp;
	slot_handle_t sid;
	u_int reg;
	u_int8_t mask;
{

	gipc_reg_write_1(pp, sid, reg, gipc_reg_read_1(pp, sid, reg) & (~mask));
}

static void
gipc_chip_detect(sc)
	struct gipc_softc *sc;
{
	pccshw_tag_t pp = &sc->sc_pp;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	slot_handle_t sid = 0;
	u_int8_t id, val;

	bus_space_write_1(bst, bsh, i82365_index, REG_ID);
	val = bus_space_read_1(bst, bsh, i82365_data); 
	if (GIPC_IS_ID_BRIDGE(val) != 0)
		pp->pp_chip = CHIP_BRIDGE_PCIISA;
	else
		pp->pp_chip = CHIP_I82365;

	/* check Vadem */
	bus_space_write_1(bst, bsh, 0, 0x0e);
	bus_space_write_1(bst, bsh, 0, 0x37);
	gipc_set_bits(pp, sid, 0x3a, 0x40);
	if ((id = gipc_reg_read_1(pp, sid, REG_ID)) & 0x08)
		pp->pp_chip = ((id & 0x07) == 4 ? CHIP_VG469 : CHIP_VG468);
	gipc_clr_bits(pp, sid, 0x3a, 0x40);

	/* check Cirrus */
	gipc_reg_write_1(pp, sid, REG_CIRRUS_ID, 0);
	if ((gipc_reg_read_1(pp, sid, REG_CIRRUS_ID) & 0xc0) == 0xc0)
		if (((id = gipc_reg_read_1(pp, sid, REG_CIRRUS_ID)) & 0xc0) == 0)
			pp->pp_chip = ((id & 0x20) ? CHIP_PD672X : CHIP_PD6710);
}

/********************************************
 * Power and Reset
 *******************************************/
int
gipc_power(pp, sid, powp, on)
	pccshw_tag_t pp;
	slot_handle_t sid;
	struct power *powp;
	int on;
{
	u_int8_t bits;

	switch (on)
	{
	case PCCSHW_POWER_ON:
	case PCCSHW_POWER_UPDATE:
		bits = POWR_AUTOPOW | POWR_RESDRV;
		if (powp->pow_vcc0 || powp->pow_vcc0 == PCCS_POWER_DEFAULT)
			bits |= POWR_VCC5V;
		if (powp->pow_vpp0 == 50 || powp->pow_vpp0 == PCCS_POWER_DEFAULT)
			bits |= POWR_VPP5V;
		else if (powp->pow_vpp0 == 120)
			bits |= POWR_VPP12V;

		gipc_reg_write_1(pp, sid, REG_POW, bits);
		pccs_wait(100);
		gipc_set_bits(pp, sid, REG_POW, POWR_OUTEN);
		break;

	case PCCSHW_POWER_OFF:
		gipc_reg_write_1(pp, sid, REG_IGC, IGCR_DISRST);
		gipc_reg_write_1(pp, sid, REG_POW, 0);
		break;

	}

	return 0;
}

void
gipc_reset(pp, sid, type)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int type;
{
	struct gipc_softc *sc = pp->pp_dv;
	int tout;

	gipc_reg_write_1(pp, sid, REG_IGC, (type == IF_IO) ? IGCR_IOCARD : 0);
	pccs_wait(100);
	gipc_set_bits(pp, sid, REG_IGC, IGCR_DISRST);
	pccs_wait(100);
	for (tout = 0; (gipc_reg_read_1(pp, sid, REG_STAT) & STAT_RDY)  == 0 &&
			tout < 30; tout ++)
		pccs_wait(100);

	gipc_set_bits(pp, sid, REG_POW, POWR_RESDRV);
	gipc_open_intr(pp, sid);

	gipc_access_timing(pp, sid, sc->sc_timing[sid]);
}

/********************************************
 * Io and Mem Mapping
 *******************************************/
#define	GETFG(topval, shift) ((busflags / (topval)) << (shift))
int gipc_chkmem __P((pccshw_tag_t, slot_handle_t, int, struct slot_device_iomem *));
int gipc_mapmem __P((pccshw_tag_t, slot_handle_t, int, struct slot_device_iomem *));
int gipc_chkio __P((pccshw_tag_t, slot_handle_t, int, struct slot_device_iomem *));
int gipc_mapio __P((pccshw_tag_t, slot_handle_t, int, struct slot_device_iomem *));

void
gipc_reclaim(pp, sid)
	pccshw_tag_t pp;
	slot_handle_t sid;
{
	struct gipc_softc *sc = pp->pp_dv;

	sc->sc_immask[sid] = 0;

	/* unmap io wins */
	gipc_clr_bits(pp, sid, REG_WE, WE_IOMASK | WE_MEMMASK);
	gipc_set_bits(pp, sid, REG_WE, WE_MEMCS16);

	/* unmap irq & memory card type */
	gipc_clr_bits(pp, sid, REG_IGC, IGCR_IRQM | IGCR_INTR);
}

int
gipc_map(pp, sid, wp, sph, flags)
	pccshw_tag_t pp;
	slot_handle_t sid;
	struct slot_device_iomem *wp;
	window_handle_t *sph;
	int flags;
{
	struct gipc_softc *sc = pp->pp_dv;
	u_int id, maxid;
	int error = 0;

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
		break;
	}

	for ( ; id < maxid; id ++)
		if ((sc->sc_immask[sid] & (1 << id)) == 0)
			break;
	if (id == maxid)
		return EBUSY;

	sc->sc_immask[sid] |= 1 << id;
	if (wp->im_size == 0)
		goto out;

	switch(wp->im_type)
	{
	case SLOT_DEVICE_SPMEM:
		if (flags & PCCSHW_CHKRANGE)
		{
			error = gipc_chkmem(pp, sid, id - SLOT_DEVICE_NIO, wp);
			if (error)
			{
				sc->sc_immask[sid] &= ~(1 << id);
				return error;
			}
		}
		gipc_mapmem(pp, sid, id - SLOT_DEVICE_NIO, wp);
		break;

	case SLOT_DEVICE_SPIO:
		if (flags & PCCSHW_CHKRANGE)
		{
			error = gipc_chkio(pp, sid, id, wp);
			if (error)
			{
				sc->sc_immask[sid] &= ~(1 << id);
				return error;
			}
		}
		gipc_mapio(pp, sid, id, wp);
		break;
	}

out:
	*sph = id;
	return error;
}

int
gipc_unmap(pp, sid, sph)
	pccshw_tag_t pp;
	slot_handle_t sid;
	window_handle_t sph;
{
	struct gipc_softc *sc = pp->pp_dv;
	u_int id;
	u_int8_t regv = 0;

	id = sph;
	sc->sc_immask[sid] &= ~(1 << id);

	if (id >= SLOT_DEVICE_NIO)
	{
		id -= SLOT_DEVICE_NIO;
		if (id >= pp->pp_nmem)
			return 0;

		regv = (WE_MEMBASE << id);
		gipc_clr_bits(pp, sid, REG_WE, regv);
	}
	else
	{
		if (id >= pp->pp_nio)
			return 0;

		regv = (WE_IOBASE << id);
		gipc_clr_bits(pp, sid, REG_WE, regv);
	}
	return 0;
}

int
gipc_chkmem(pp, sid, id, wp)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int id;
	struct slot_device_iomem *wp;
{
	bus_addr_t phys, endphys;

	if (wp->im_hwbase >= PCC16_MEM_MAXADDR ||
	    wp->im_base == PUNKADDR ||
 	    wp->im_size == PUNKSZ)
		return EINVAL;

	phys = PCCSHW_TRUNC_PAGE(pp, wp->im_hwbase);
	endphys = PCCSHW_ROUND_PAGE(pp, wp->im_hwbase + wp->im_size);
	if (endphys >= PCC16_MEM_MAXADDR)
		return EINVAL;

	return 0;
}

int
gipc_mapmem(pp, sid, id, wp)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int id;
	struct slot_device_iomem *wp;
{
	u_int rb, enable, busflags;
	bus_addr_t phys, endphys, offset;

	enable = (WE_MEMBASE << id);

	phys = PCCSHW_TRUNC_PAGE(pp, wp->im_hwbase);
	offset = PCCSHW_TRUNC_PAGE(pp, wp->im_base);
	offset = PCCSHW_PHYS2PAGE(pp, offset - phys) & 0x3fff;
	phys = PCCSHW_PHYS2PAGE(pp, phys);
	endphys = PCCSHW_ROUND_PAGE(pp, wp->im_hwbase + wp->im_size);
	endphys = PCCSHW_PHYS2PAGE(pp, endphys) - 1;

	/* setup hw */
	rb = REG_MWBASE + id * REG_MEM_WIN_SKIP;

	busflags = 0;
	if ((wp->im_flags & (SDIM_BUS_WEIGHT | SDIM_BUS_WIDTH16)) == 0)
		busflags |= 0x40;
	if (wp->im_flags & SDIM_BUS_WIDTH16)
		busflags |= 0x80;
	gipc_reg_write_1(pp, sid, rb, phys);
	gipc_reg_write_1(pp, sid, rb + 1, (phys >> 8) | busflags);

	busflags = 0;
	if (wp->im_flags & SDIM_BUS_WEIGHT)
		busflags |= 0xc0;
	gipc_reg_write_1(pp, sid, rb + 2, endphys);
	gipc_reg_write_1(pp, sid, rb + 3, (endphys >> 8) | busflags);

	busflags = 0;
	if (wp->im_flags & WFMEM_ATTR)
		busflags |= 0x40;
	if (wp->im_flags & SDIM_BUS_WP)
		busflags |= 0x80;
	gipc_reg_write_1(pp, sid, rb + 4, offset);
	gipc_reg_write_1(pp, sid, rb + 5, (offset >> 8) | busflags);

	gipc_set_bits(pp, sid, REG_WE, WE_MEMCS16 | enable);
	delay(50);
	return 0;
}

int
gipc_chkio(pp, sid, winid, wp)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int winid;
	struct slot_device_iomem *wp;
{
	bus_addr_t phys, endphys;

	if (wp->im_hwbase >= PCC16_IO_MAXADDR || wp->im_size > 256)
		return EINVAL;

	phys = wp->im_hwbase;
	endphys = phys + wp->im_size;
	if (endphys >= PCC16_IO_MAXADDR)
		return EINVAL;

	return 0;
}

int
gipc_mapio(pp, sid, winid, wp)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int winid;
	struct slot_device_iomem *wp;
{
	bus_addr_t phys, endphys;
	u_int rb, busflags, enable, mask;

	busflags = 0;
	if (wp->im_flags & SDIM_BUS_WEIGHT)
		busflags |= 0x08;
	if ((wp->im_flags & (SDIM_BUS_WEIGHT | SDIM_BUS_WIDTH16)) == 0)
		busflags |= 0x04;
	if (wp->im_flags & SDIM_BUS_AUTO)
		busflags |= 0x02;
	if (wp->im_flags & SDIM_BUS_WIDTH16)
		busflags |= 0x01;

	phys = wp->im_hwbase;
	endphys = phys + wp->im_size - 1;
	rb = REG_IOWBASE + winid * REG_IO_WIN_SKIP;
	gipc_reg_write_1(pp, sid, rb, phys & 0xff);
	gipc_reg_write_1(pp, sid, rb + 1, (phys >> 8) & 0xff);
	gipc_reg_write_1(pp, sid, rb + 2, endphys & 0xff);
	gipc_reg_write_1(pp, sid, rb + 3, (endphys >> 8) & 0xff);

	/* open windows */
	enable = (WE_IOBASE << winid);
	gipc_set_bits(pp, sid, REG_WE, enable);

	mask = 0x0f << (4 * winid);
	gipc_clr_bits(pp, sid, REG_IOC, mask);

	busflags = busflags << (4 * winid);
	gipc_set_bits(pp, sid, REG_IOC, busflags);

	delay(50);
	return 0;
}

/********************************************
 * Intr Mapping
 *******************************************/
#define	PGIRQ(bc, mask)	(isa_get_empty_irq((bc), (mask)))
#define PCCSHW_EMPTY_IRQ(bc, dc) PGIRQ((bc), (dc)->dc_aux)
#define	PCCSHW_VALID_IRQ(bc, irq) PGIRQ((bc), 1 << (irq))

int
gipcintr(arg)
	void *arg;
{
	struct gipc_softc *sc = arg;

	if (sc->sc_pp.pp_pcsc == NULL)
		return 0;

	return pccsintr(sc->sc_pp.pp_pcsc);
}

int
gipc_chkirq(pp, sid, bc, dc)
	pccshw_tag_t pp;
	slot_handle_t sid;
	bus_chipset_tag_t bc;
	struct slot_device_channel *dc;
{
	struct gipc_softc *sc = pp->pp_dv;
	
	dc->dc_aux &= sc->sc_irqmask;
	if (dc->dc_chan == 0)
		dc->dc_chan = PUNKIRQ;
	else if (dc->dc_chan == PAUTOIRQ)
	{
		dc->dc_chan = PCCSHW_EMPTY_IRQ(bc, dc);
		if (dc->dc_chan == PUNKIRQ)
		{
			printf("%s: no irq left\n", sc->sc_dev.dv_xname);
			return EINVAL;
		}
	}
	else if (dc->dc_chan != PUNKIRQ &&
		 ((dc->dc_aux & (1 << dc->dc_chan)) == 0 ||
		  PCCSHW_VALID_IRQ(bc, dc->dc_chan) == PUNKIRQ))
	{
		printf("%s: invalid irq\n", sc->sc_dev.dv_xname);
		return EINVAL;
	}	
	return 0;
}

void
gipc_routeirq(pp, sid, dc)
	pccshw_tag_t pp;
	slot_handle_t sid;
	struct slot_device_channel *dc;
{

	gipc_clr_bits(pp, sid, REG_IGC, IGCR_IRQM);
 	if (dc != NULL && dc->dc_chan != 0 && dc->dc_chan < 16)
		gipc_set_bits(pp, sid, REG_IGC, dc->dc_chan & IGCR_IRQM);
}

static void
gipc_open_intr(pp, sid)
	pccshw_tag_t pp;
	slot_handle_t sid;
{
	register struct gipc_softc *sc = pp->pp_dv;
	u_int8_t val;

	val  = (sc->sc_irq != PUNKIRQ) ? ((sc->sc_irq << 4) | CCS_ANY) : 0;
	gipc_reg_write_1(pp, sid, REG_CCS, val);
	(void) gipc_reg_read_1(pp, sid, REG_CS);
	(void) gipc_reg_read_1(pp, sid, REG_STAT);
}

static int
gipc_access_timing(pp, sid, timing)
	pccshw_tag_t pp;
	slot_handle_t sid;
	u_int timing;
{
	struct gipc_softc *sc = pp->pp_dv;

	sc->sc_timing[sid] = timing;

	switch (pp->pp_chip)
	{
	case CHIP_PD6710:
	case CHIP_PD672X:
	{
		struct cl_timing {
			u_int8_t clt0_setup;
			u_int8_t clt0_cmd;
			u_int8_t clt0_recov;
		};
		static struct cl_timing cl_timing[4] = {
			{0x1, 0x6, 0},
			{0x1, 0x4, 0},
			{0x1, 0x2, 0},
			{0x0, 0x1, 0},
		};

		if (timing > 3)
			break;
		gipc_reg_write_1(pp, sid, REG_CIRRUS_SETUP, 
				 cl_timing[timing].clt0_setup);
		gipc_reg_write_1(pp, sid, REG_CIRRUS_CMD, 
				 cl_timing[timing].clt0_cmd);
		gipc_reg_write_1(pp, sid, REG_CIRRUS_RECOV, 
				 cl_timing[timing].clt0_recov);
		break;
	}

	default:
		break;
	}
	return 0;
}

int
gipc_stat(pp, sid, scmd)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int scmd;
{
	u_int8_t id;

	switch (scmd)	
	{
	case PCCSHW_HASSLOT:
		id = gipc_reg_read_1(pp, sid, REG_ID);
		return GIPC_IS_ID_VALID(id);

	case PCCSHW_CARDIN:
		return ((gipc_reg_read_1(pp, sid, REG_STAT) & STAT_CD) == STAT_CD);

	case PCCSHW_STCHG:
		return (gipc_reg_read_1(pp, sid, REG_CS) & CS_CCHG);

	default:
		return 0;
	}
}

int
gipc_auxcmd(pp, sid, cmd)
	pccshw_tag_t pp;
	slot_handle_t sid;
	int cmd;
{

	switch (cmd)	
	{
	case PCCSHW_SPKRON:
		if (pp->pp_chip == CHIP_PD6710 || pp->pp_chip == CHIP_PD672X)
			gipc_set_bits(pp, sid,
				      REG_CIRRUS_MC1, CIRRUS_MC1_SIGNAL);
		break;

	case PCCSHW_SPKROFF:
		if (pp->pp_chip == CHIP_PD6710 || pp->pp_chip == CHIP_PD672X)
			gipc_clr_bits(pp, sid,
				      REG_CIRRUS_MC1, CIRRUS_MC1_SIGNAL);
		break;

	case PCCSHW_CLRINTR:
		(void) gipc_reg_read_1(pp, sid, REG_CS);
		(void) gipc_reg_read_1(pp, sid, REG_STAT);
		break;

	case (PCCSHW_TIMING | 0x0000):
	case (PCCSHW_TIMING | 0x0001):
	case (PCCSHW_TIMING | 0x0002):
	case (PCCSHW_TIMING | 0x0003):
		gipc_access_timing(pp, sid, cmd & PCCSHW_TIMING_MASK);
		break;

	default:
		break;
	}

	return 0;
}

int
gipc_swapspace(pp, sid, idr, odr)
	pccshw_tag_t pp;
	slot_handle_t sid;
	slot_device_res_t idr, odr;
{

	if (idr == odr)
		return 0;

	*odr = *idr;
	return 0;
}
