/*	$NecBSD: video.h,v 1.27.4.5 1999/09/08 08:26:26 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef	_I386_VSC_VIDEO_H_
#define	_I386_VSC_VIDEO_H_

/* gdc1 offset */
#define	gdc1_stat	0x0		/* (R) status */
#define	gdc1_params	0x0		/* (W) gdc paramaters */
#define	gdc1_data	0x1		/* (R) data data */
#define	gdc1_cmd	0x1		/* (W) cmd write */
#define	gdc1_mode	0x4		/* (W) mode registers (1) */
#define	gdc1_mode2	0x5		/* (W) mode registers (2) */

/* gdc2 offset */
#define	gdc2_stat	0x0		/* (R) status */
#define	gdc2_params	0x0		/* (W) gdc paramaters */
#define	gdc2_data	0x2		/* (R) data */
#define	gdc2_cmd	0x2		/* (W) cmd */
#define	gdc2_mode	0x4		/* (W) mode registers */
#define	gdc2_color	0x8		/* (W) pallet registers */
#define	gdc2_blue	0xe
#define	gdc2_green	0xa
#define	gdc2_red	0xc

/* character gen */
#define	cg_data1	0x1		/* (W) character data (1) */
#define	cg_data2	0x3		/* (W) character data (2) */
#define	cg_line		0x5		/* (W) cg line counter */
#define	cg_pattern	0x9		/* (W) cg patern data */

/* screen defaults */
#define	NORMAL_COL	MAX_COL
#define	NORMAL_ROW	25
#define	VSC_ROW		14
#define	STATUS_ROW	1

/* Cbus holes */
#define	TRAM_VADDR(vscp)	((vram_t *) ((vscp)->sc_crtbase))
#define	VRAM_VADDR(vscp)	((u_int8_t *) ((vscp)->sc_crtbase))
#define	G_RAM_SIZE		CBUSHOLE_GRAM_SIZE

#define	TEXT_RAM_ADDR(vscp)	(TRAM_VADDR(vscp) + CBUSHOLE_TRAM_OFFSET)
#define	ATTR_RAM_ADDR(vscp)	(TRAM_VADDR(vscp) + CBUSHOLE_TARAM_OFFSET)
#define	G_RAM0_ADDR(vscp)	(VRAM_VADDR(vscp) + CBUSHOLE_GRAM0_OFFSET)
#define	G_RAM1_ADDR(vscp)	(VRAM_VADDR(vscp) + CBUSHOLE_GRAM1_OFFSET)
#define	G_RAM2_ADDR(vscp)	(VRAM_VADDR(vscp) + CBUSHOLE_GRAM2_OFFSET)
#define	G_RAM3_ADDR(vscp)	(VRAM_VADDR(vscp) + CBUSHOLE_GRAM3_OFFSET)
#define	PEGC_BANK_OFFSET(vscp)  (VRAM_VADDR(vscp) + CBUSHOLE_PEGC_REGISTER_OFFSET)
#define	PEGC_WINDOW_OFFSET(vscp) (VRAM_VADDR(vscp) + CBUSHOLE_PEGC_WINDOW_OFFSET)
#define	PEGC_WINDOW_SIZE CBUSHOLE_PEGC_WINDOW_SIZE
#define	PEGC_WINDOW_MAX		0x10
#define	PEGC_MAX_PALLET	256

struct pegc_pallet_data {
	int pd_num;

	u_int8_t pd_red;
	u_int8_t pd_green;
	u_int8_t pd_blue;
};

struct pegc_draw_position {
	int pd_xsz;
	int pd_ysz;

	u_int pd_flags;
#define	PEGC_DRAW_POSITION_MASK		0xf
#define	PEGC_DRAW_POSITION_UL		0
#define	PEGC_DRAW_POSITION_UR		1
#define	PEGC_DRAW_POSITION_DL		2
#define	PEGC_DRAW_POSITION_DR		3
#define	PEGC_DRAW_POSITION_CENTER	4
#define	PEGC_DRAW_POSITION_FIXED	5

	int pd_x;
	int pd_y;
};

/* GDC Control */
static __inline void vsc_text_on __P((struct vsc_softc *));
static __inline void vsc_text_off __P((struct vsc_softc *));
static __inline void vsc_graphic_on __P((struct vsc_softc *));
static __inline void vsc_graphic_off __P((struct vsc_softc *));
static __inline void vsc_vsync_wait __P((struct vsc_softc *));

static __inline void
vsc_text_on(vscp)
	struct vsc_softc *vscp;
{

	bus_space_write_1(vscp->sc_iot, vscp->sc_GDC1ioh, gdc1_cmd, 0xd);
}

static __inline void
vsc_text_off(vscp)
	struct vsc_softc *vscp;
{

	bus_space_write_1(vscp->sc_iot, vscp->sc_GDC1ioh, gdc1_cmd, 0xc);
}

static __inline void
vsc_graphic_on(vscp)
	struct vsc_softc *vscp;
{

	if (ISSET(vscp->cvsp->flags, VSC_GRAPH))
		bus_space_write_1(vscp->sc_iot, vscp->sc_GDC2ioh, gdc2_cmd, 0xd);
}

static __inline void
vsc_graphic_off(vscp)
	struct vsc_softc *vscp;
{

	bus_space_write_1(vscp->sc_iot, vscp->sc_GDC2ioh, gdc2_cmd, 0xc);
}

static __inline void
vsc_vsync_wait(vscp)
	struct vsc_softc *vscp;
{
	int dct = 10000;
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC1ioh;
	u_int8_t regv;

	do {
		regv = bus_space_read_1(iot, ioh, gdc1_stat);
	}
	while ((regv & 0x20) == 0 && -- dct > 0);
}

#if	DEFAULT_VSC_STEP >= MAX_ROW
#define	SKIPROW	1
#else	/* DEFAULT_VSC_STEP < MAX_ROW */
#define	SKIPROW	DEFAULT_VSC_STEP
#endif	/* DEFAULT_VSC_STEP < MAX_ROW */

/* attribute (hw) */
#define	ATTR_ON		0x01
#define	ATTR_BL		0x02
#define	ATTR_RV		0x04
#define	ATTR_UL		0x08
#define	ATTR_CSHIFT	5
#define	ATTR_CMASK	0xe0

/* reverse attribute */
extern u_char ibmpc_to_pc98[];
#define	VSC_ATTRIBRO	(ibmpc_to_pc98[DEFAULT_ATTRIBUTE] | ATTR_RV)
#define	VSC_SATTRIBRO	(ibmpc_to_pc98[DEFAULT_ATTRIBUTE] | ATTR_RV)
#define	VSC_SYS_ATTRIBRO (ibmpc_to_pc98[FG_WHITE] | ATTR_RV)
#define	VSC_VATTRIBRO	(ibmpc_to_pc98[DEFAULT_VSC_RATTRIBUTE] | ATTR_RV)

/* normal attribute */
#define	VSC_ATTRIBO	(ibmpc_to_pc98[DEFAULT_ATTRIBUTE])
#define	VSC_SATTRIBO	(ibmpc_to_pc98[DEFAULT_ATTRIBUTE])
#define	VSC_VATTRIBO	(ibmpc_to_pc98[DEFAULT_VSC_ATTRIBUTE])
#define	VSC_SYS_ATTRIBO	(ibmpc_to_pc98[FG_WHITE])
#endif	/* !_I386_VSC_VIDEO_H_ */
