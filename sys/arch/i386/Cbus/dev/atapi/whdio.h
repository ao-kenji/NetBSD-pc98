/*	$NecBSD: whdio.h,v 1.4 1998/03/14 07:05:50 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
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
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef	_WHDIO_H_
#define	_WHDIO_H_

#define	IDEDRV_RCACHE			0x0001
#define	IDEDRV_RCACHE_FEAT_ON		0
#define	IDEDRV_RCACHE_FEAT_OFF		1

#define	IDEDRV_WCACHE			0x0002
#define	IDEDRV_WCACHE_FEAT_ON		0
#define	IDEDRV_WCACHE_FEAT_OFF		1

#define	IDEDRV_POWSAVE			0x0003

struct idedrv_ctrl {
	u_int idc_cmd;			/* ide cmd */

	u_int8_t idc_feat;		/* feature flags */
	u_int8_t idc_secc;		/* feature arg */
};

#define	IDEIOSMODE	_IOW('I', 100, struct idedrv_ctrl)
#define	IDEIOGMODE	_IOWR('I', 100, struct idedrv_ctrl)
#endif	/* !_WHDIO_H_ */
