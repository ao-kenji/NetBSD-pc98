/*	$NecBSD: vsc_sm.h,v 1.8.16.1 1999/09/02 10:57:26 honda Exp $	*/
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

#ifndef	_I386_VSC_VSC_SCREENMODE_H_
#define	_I386_VSC_VSC_SCREENMODE_H_

struct screenmode {
	u_int	sm_khz;
	u_int	sm_lines;
	u_int	sm_dots;
#define	SM_PEGC	0
#define	SM_EGC	1
	u_int	sm_flags;

	u_int32_t sm_reg_eax;
	u_int32_t sm_reg_ebx;
	u_int32_t sm_reg_ecx;
};

extern	struct screenmode screenmodes[];
#define	VSC_30_SM	(&screenmodes[0])
#define	VSC_25_SM	(&screenmodes[1])
#define	VSC_MAX_SM	4

#ifdef	VSC_SCREENMODE_DATA
struct screenmode screenmodes[] = {
	{31, 30, 480, SM_PEGC, 0x300c, 0x3200, 0x0100},
	{25, 25, 400, SM_EGC,  0x3008, 0x2100, 0x0000},
	{31, 25, 480, SM_PEGC, 0x300c, 0x3100, 0x0100},
	{25, 25, 400, SM_PEGC, 0x3008, 0x2100, 0x0100},
};
#endif	/* VSC_SCREENMODE_DATA */
#endif	/* !_I386_VSC_VSC_SCREEN_MODE_H_ */
