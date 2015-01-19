/*	$NecBSD: lpthw.h,v 1.6 1998/09/27 01:52:55 honda Exp $	*/
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
 * LPT HW DEFINITIONS
 *	written by N. Honda.
 */

#ifndef	_LPTHW_H_
#define	_LPTHW_H_

#include <machine/dvcfg.h>

#ifdef	_KERNEL
struct lpt_hw {
#define	LPT_HW_TRADNEC	0
#define	LPT_HW_NEC	1
#define	LPT_HW_PS2	2
	int subtype;

	int iobase;
	int irq;

	int nports;

	bus_space_iat_t iat;
	size_t iatsz;

	void ((*output)(void *));
	int ((*not_ready)(void *));
	int ((*not_ready_error)(void *));
};

#undef	lpt_data
#undef	lpt_status
#undef	lpt_control
#define	lpt_data	(0)
#define	lpt_status	(1)
#define	lpt_control	(2)
#define	lpt_configA	(3)
#define	lpt_configB	(4)
#define	lpt_mode	(5)
#define	lpt_extmode	(6)

#undef	LPT_NPORTS
#define	LPT_NPORTS(hw)	((hw)->nports)

/* 0 STAND 1 PS2 MODE 2 FIFO MODE 3 EPC MODE */
#define	LPT_ACCESS_BITS(XXX)	((XXX) & 0x03)
#define	LPT_ACCESS_MODE(XXX)	\
	(LPT_ACCESS_BITS(XXX) ? (0x10 << LPT_ACCESS_BITS(XXX)) : 0)
#endif	/* _KERNEL */

#endif	/* !_LPTHW_H_ */
