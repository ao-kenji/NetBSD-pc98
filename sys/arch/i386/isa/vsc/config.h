/*	$NecBSD: config.h,v 1.28 1998/03/14 07:09:00 kmatsuda Exp $	*/
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
 * Virtual Console For PC-98 Written By N. Honda. 
 */

#ifndef _I386_VSC_CONFIG_H_	/* XXX */
#define _I386_VSC_CONFIG_H_	/* XXX */

/* kanji & font */
#define	JIS83FONTLOAD		1
#define	SWAPCODE_TBL_SIZE	512	/* fonts swap table size */

/* initial start up */
#define	VSC_USE_AWM		1

/* saver */
#define	DEFAULT_SAVER_TIME	180

/* step */
#define	DEFAULT_TAB_STEP	8
#define	DEFAULT_VSC_STEP	1

/* max vsc */
#ifndef	DEFAULT_MAX_VSC
#define	DEFAULT_MAX_VSC		10
#endif	/* !DEFAULT_MAX_VSC */
#define	DEFAULT_VSC_MEMSZ	0x8000

/* default attribute */
#define	DEFAULT_ATTRIBUTE	FG_WHITE
#define	DEFAULT_VSC_ATTRIBUTE	FG_GREEN
#define	DEFAULT_VSC_RATTRIBUTE	FG_YELLOW

/* pcvt compat parameter */
#define PCVT_NETBSD	110		/* minimum version number for us */
#define	PCVT_NSCREENS	DEFAULT_MAX_VSC
#define	PCVT_SCANSET	1

/* misc */
/*#define	VSC_META_ESC		1*/	/* graph key */

#define	AVOID_LONG_SPLTTY

#endif	/* !_I386_VSC_CONFIG_H_	*/
