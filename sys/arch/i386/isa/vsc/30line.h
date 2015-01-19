/*	$NecBSD: 30line.h,v 1.15.16.1 1999/08/31 21:01:56 honda Exp $	*/
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

#ifndef _I386_VSC_30LINE_H_
#define _I386_VSC_30LINE_H_

#define	LINE30_MODE	3	/* 30line:80x30 *//* normal only */

#define	LINE30_COL	80
#define	LINE30_ROW	30

#define	_GDC_RESET	0x00
#define	_GDC_SYNC	0x0e
#define	_GDC_MASTER	0x6f
#define	_GDC_SLAVE	0x6e
#define	_GDC_START	0x0d
#define	_GDC_STOP	0x0c
#define	_GDC_SCROLL	0x70
#define	_GDC_PITCH	0x47

#define	GDC_CR	0
#define	GDC_VS	1
#define	GDC_HS	2
#define	GDC_HFP	3
#define	GDC_HBP	4
#define	GDC_VFP	5
#define	GDC_VBP	6
#define	GDC_LF	7

#define	_2_5MHZ	0
#define	_5MHZ	1

#define	_25L		0
#define	_30L		1

#define	T25_G400	0
#define	T30_G400	1
#define	T30_G480	2

struct vsc_softc;
void master_gdc_cmd __P((struct vsc_softc *, u_int));
void master_gdc_prm __P((struct vsc_softc *, u_int));
void master_gdc_word_prm __P((struct vsc_softc *, u_int));
void master_gdc_fifo_empty __P((struct vsc_softc *));
void master_gdc_wait_vsync __P((struct vsc_softc *));
void initialize_gdc __P((struct vsc_softc *, u_int));

void gdc_cmd __P((struct vsc_softc *, u_int));
void gdc_prm __P((struct vsc_softc *, u_int));
void gdc_word_prm __P((struct vsc_softc *, u_int));
void gdc_fifo_empty __P((struct vsc_softc *));
void gdc_wait_vsync __P((struct vsc_softc *));

int check_gdc_clock __P((struct vsc_softc *));

static u_int master_param[2][8] = {
{78,	8,	7,	9,	7,	7,	25,	400},
{78,	7,	7,	7,	7,	7,	20,	480}};

static u_int slave_param[6][8] = {
{38,	8,	3,	4,	3,	7,	25,	400},	/* normal */
{78,	8,	7,	9,	7,	7,	25,	400},
{38,	7,	3,	3,	3,	55,	52,	400},	/* 30 & 400 */
{78,	7,	7,	7,	7,	55,	52,	400},
{38,	7,	3,	3,	3,	7,	20,	480},	/* 30 & 480 */
{78,	7,	7,	7,	7,	7,	20,	480}
};

static int SlavePCH[2] = {40,80};
static int MasterPCH = 80;
static int SlaveScrlLF[3] = {400,400,480};
#endif	/* !_I386_VSC_30LINE_H_ */
