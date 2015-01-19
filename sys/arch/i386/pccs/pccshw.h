/*	$NecBSD: pccshw.h,v 1.6.6.1 1999/08/31 09:14:07 honda Exp $	*/
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

#ifndef	_PCCSHW_H_
#define	_PCCSHW_H_

#define	_PCCSHW_FUNCS_PROT(NAME) 					\
void NAME##_reset __P((pccshw_tag_t, slot_handle_t, int));		\
int NAME##_power __P((pccshw_tag_t, slot_handle_t, struct power *, int));\
void NAME##_reclaim __P((pccshw_tag_t, slot_handle_t));			\
int NAME##_map __P((pccshw_tag_t, slot_handle_t,			\
		struct slot_device_iomem *, window_handle_t *, int));	\
int NAME##_unmap __P((pccshw_tag_t, slot_handle_t, window_handle_t)); \
int NAME##_chkirq __P((pccshw_tag_t, slot_handle_t,			\
		    bus_chipset_tag_t, struct slot_device_channel *));  \
void NAME##_routeirq __P((pccshw_tag_t, slot_handle_t,		\
		    struct slot_device_channel *)); 			\
int NAME##_stat __P((pccshw_tag_t, slot_handle_t, int));		\
int NAME##_auxcmd __P((pccshw_tag_t, slot_handle_t, int));		\
int NAME##_swapspace __P((pccshw_tag_t, slot_handle_t,		\
	    		  slot_device_res_t, slot_device_res_t));	\

#define	_PCCSHW_FUNCS_TAB(NAME) 	\
	NAME##_reset,			\
	NAME##_power,			\
	NAME##_reclaim, 		\
	NAME##_map,			\
	NAME##_unmap,			\
	NAME##_chkirq,			\
	NAME##_routeirq,		\
	NAME##_stat,			\
	NAME##_auxcmd,			\
	NAME##_swapspace

struct pccshw_funcs {
	/* power control */
	void (*ph_reset) __P((pccshw_tag_t, slot_handle_t, int));
#define	PCCSHW_POWER_UPDATE	2
#define	PCCSHW_POWER_ON		1
#define	PCCSHW_POWER_OFF	0
	int (*ph_power) __P((pccshw_tag_t, slot_handle_t, struct power *, int));
	void (*ph_reclaim) __P((pccshw_tag_t, slot_handle_t));

	/* mapping spaces (io/mem) */
#define	PCCSHW_CHKRANGE	1
	int (*ph_map) __P((pccshw_tag_t, slot_handle_t,
			   struct slot_device_iomem *, window_handle_t *, int));
	int (*ph_unmap) __P((pccshw_tag_t, slot_handle_t, window_handle_t));

	/* interrupt routing */
	int (*ph_chkirq) __P((pccshw_tag_t, slot_handle_t,
			    bus_chipset_tag_t, struct slot_device_channel *));
	void (*ph_routeirq) __P((pccshw_tag_t, slot_handle_t,
			    struct slot_device_channel *));

	/* misc */
#define	PCCSHW_HASSLOT	0
#define	PCCSHW_CARDIN		1
#define	PCCSHW_STCHG		2
	int (*ph_stat) __P((pccshw_tag_t, slot_handle_t, int));

#define	PCCSHW_CMDMASK	       0xffff0000
#define	PCCSHW_CLRINTR	       0x00000000
#define	PCCSHW_SPKRON	       0x00010001
#define	PCCSHW_SPKROFF	       0x00010000
#define	PCCSHW_TIMING          0x00040000
#define	PCCSHW_TIMING_MASK     0x0000ffff
	int (*ph_auxcmd) __P((pccshw_tag_t, slot_handle_t, int));

	/* space swap */
	int (*ph_swapspace) __P((pccshw_tag_t, slot_handle_t,\
		 slot_device_res_t, slot_device_res_t));
};

#define	pccshw_reset(pp, id, space) \
	((*(pp)->pp_hw->ph_reset) ((pp), (id), (space)))
#define	pccshw_power(pp, id, pow, flags) \
	((*(pp)->pp_hw->ph_power) ((pp), (id), (pow), (flags)))
#define	pccshw_reclaim(pp, id) \
	((*(pp)->pp_hw->ph_reclaim) ((pp), (id)))
#define	pccshw_map(pp, id, win, hp, flag) \
	((*(pp)->pp_hw->ph_map) ((pp), (id), (win), (hp), (flag)))
#define	pccshw_unmap(pp, id, handle) \
	((*(pp)->pp_hw->ph_unmap) ((pp), (id), (handle)))

#define	pccshw_chkirq(pp, id, bc, dc) \
	((*(pp)->pp_hw->ph_chkirq) ((pp), (id), (bc), (dc)))
#define	pccshw_routeirq(pp, id, dc) \
	((*(pp)->pp_hw->ph_routeirq) ((pp), (id), (dc)))

#define	pccshw_stat(pp, id, stat) \
	((*(pp)->pp_hw->ph_stat) ((pp), (id), (stat)))
#define	pccshw_auxcmd(pp, id, cmd) \
	((*(pp)->pp_hw->ph_auxcmd) ((pp), (id), (cmd)))

#define	pccshw_swapspace(pp, id, in, out) \
	((*(pp)->pp_hw->ph_swapspace) ((pp), (id), (in), (out)))

#define	PCCSHW_MAX_WINSIZE	(2 * NBPG)

#define	PCCSHW_TRUNC_PAGE(pp, addr) \
	(CIS_COMM_ADDR((addr)) & (~((pp)->pp_psz - 1)))
#define	PCCSHW_ROUND_PAGE(pp, addr) \
	((CIS_COMM_ADDR((addr)) + (pp)->pp_psz - 1) & (~((pp)->pp_psz - 1)))
#define	PCCSHW_PHYS2PAGE(pp, addr) (CIS_COMM_ADDR((addr)) >> (pp)->pp_ps)

/*
 * PC Card 16 bits io and memory limits
 */
#define	PCC16_MEM_MAXADDR	((bus_addr_t) (1 << 24))
#define	PCC16_IO_MAXADDR	((bus_addr_t) (1 << 16))

#endif	/* !_PCCSHW_H_ */
