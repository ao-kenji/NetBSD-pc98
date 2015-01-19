/*	$NecBSD: necssreg.h,v 1.8 1998/03/14 07:10:29 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
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

#ifndef	_NECSSIF_H_
#define	_NECSSIF_H_

#define WSS_Sdip_Info		0x881e

#define NECSS_PnP_CONFIG	0xc24
#define	NEC_PnPR_ENABLE		0x80
#define	NEC_PnPR_MAPENABLE	0x20
#define	NEC_PnPR_WSS		0x00
#define	NEC_PnPR_CTRL		0x01
#define	NEC_PnPR_YMS		0x04
#define NECSS_PnP_dataLow	0xc2b
#define NECSS_PnP_dataHigh	0xc2d
#define	necss_pnpb		(0)
#define	necss_datal		(7)
#define	necss_datah		(9)

#define NEC_SOUND_BASE		0xa460
#define	necss_base		(0)
#define NEC_SCR_SIDMASK		0xf0
#define NEC_SCR_MASK		0x0f
#define NEC_SCR_EXT_ENABLE	0x01
#define	NEC_SCR_OPNA_DISABLE	0x02

#define	NEC_WSS_BASE		0xf40

#endif	/* !_NECSSIF_H_ */
