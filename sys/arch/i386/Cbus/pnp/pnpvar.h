/*	$NecBSD: pnpvar.h,v 1.19 1999/07/23 05:39:25 kmatsuda Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Sujal M. Patel
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *      $Id: pnpcfg.h,v 1.2 1996/01/12 21:35:10 smpatel Exp smpatel $
 */

#ifndef _PNPVAR_H_
#define _PNPVAR_H_

/*********************************************************
 * macros
 *********************************************************/
#define	IRQBIT(pin)	(1 << (pin))

/*********************************************************
 * logical device information structure
 *********************************************************/
struct ippi_ld_info {
	slot_device_handle_t il_dh;		/* device handle */
	u_int il_ldn;				/* logical device number */

	ippi_status_t il_status;		/* status of the device */

#define	IPPI_LD_BOOTACTIVE 0x01			/* active by bios */
	u_int il_flags;				/* flags */
	struct slot_device_resources il_bdr;	/* boot bios config info */
};

struct ippi_card_info {
	u_int ci_flags;				/* control flags flags */
#define	IPPI_CIF_OPEN	0x01

	struct ippi_ld_info *ci_il;		/* current ldn info */
	struct slot_device_resources ci_dr;	/* resources */
};

/*********************************************************
 * PnP board information structure
 *********************************************************/
struct ippi_board_info {
	slot_device_slot_tag_t ib_stag;		/* slot tag of the board */

	u_int ib_csn;				/* card selection number */
	struct ippi_bid	ib_bid;			/* board id */

	int ib_maxldn;				/* max logical devices */
	struct ippi_ld_info ib_il[MAX_LDNS];	/* logical device info */

	u_int8_t ib_cis[MAX_RDSZ];		/* cis buffer */
};

/*********************************************************
 * Isa PnP Initiator structure
 *********************************************************/
struct ippi_softc {
	struct device sc_dev;			/* device structure */

	bus_space_tag_t sc_iot;			/* io space tag */
	bus_space_tag_t sc_memt;		/* mem space tag */
	bus_dma_tag_t sc_dmat;			/* dma tag */

	void *sc_bus;				/* bus */
	isa_chipset_tag_t sc_ic;		/* bus chipset tag */

	bus_space_handle_t sc_addrh;		/* address ioh */
	bus_addr_t rd_port;			/* address port XXX */		
	bus_space_handle_t sc_wdh;		/* data(W) ioh */
	bus_space_handle_t sc_rdh;		/* data(R) ioh */
	bus_space_handle_t sc_delaybah;		/* stupid delayioh XXX */

	int sc_maxb;				/* num of detected boards */
	struct ippi_board_info *sc_ib[MAX_CARDS];	/* board info */
};

#ifdef	_KERNEL
void ippi_print __P((struct ippi_softc *, struct ippi_board_info *));
void ippi_initres __P((slot_device_res_t));

int ippi_read_config __P((struct ippi_softc *, slot_device_res_t));
void ippi_wakeup_target __P((struct ippi_softc *, struct ippi_board_info *, struct ippi_ld_info *));
void ippi_wfk_target __P((struct ippi_softc *));
void ippi_activate_target __P((struct ippi_softc *, int));
int ippi_is_active __P((struct ippi_softc *));
int ippi_allocate_pin __P((struct ippi_softc *, slot_device_res_t));

int ippi_connect_pisa __P((struct ippi_softc *, struct ippi_board_info *, struct ippi_ld_info *, slot_device_res_t, u_char *));

void ippi_regw __P((struct ippi_softc *, bus_addr_t, u_int8_t));
u_int8_t ippi_regr __P((struct ippi_softc *, bus_addr_t));
void ippi_regw_2 __P((struct ippi_softc *, bus_addr_t, u_int16_t));
u_int16_t ippi_regr_2 __P((struct ippi_softc *, bus_addr_t));

int ippi_space_prefer __P((struct ippi_softc *, ippres_res_t));
int ippi_find_resources __P((struct ippi_softc *, u_int8_t *, u_int, slot_device_res_t));
int ippi_space_prefer __P((struct ippi_softc *, ippres_res_t));
int ippres_parse_items __P((u_int8_t *, struct ippres_request *, ippres_res_t));
int ippi_card_attach __P((struct ippi_softc *, struct ippi_board_info *, struct ippi_ld_info *));
#endif	/* _KERNEL */
#endif /* !_PNPREG_H_ */
