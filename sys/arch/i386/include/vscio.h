/*	$NecBSD: vscio.h,v 3.9 1998/03/14 07:08:06 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)pccons.c	5.11 (Berkeley) 5/21/91
 */

#ifndef	_I386_VSC_VSCIO_H_
#define	_I386_VSC_VSCIO_H_

/*********************************************************************
 * PCVT ioctl
 *********************************************************************/
/* pcvt common ioctl */
#include <machine/pcvt_ioctl.h>

/* key code extension */
#define	KBD_KANA	15

/* vt stuff key code (Japanese extension) */
#define LED_KANA	8
#define LED_SHIFT	0x100	/* Virtual */
#define LED_ALT		0x200
#define LED_CTL		0x400
#define LED_MASK	0xff

/*********************************************************************
 * VSC ioctl
 *********************************************************************/
/* Screen Saver */
#define	VSC_SET_SAVERTIME	_IOW('P', 20, int)

/* Kanji */
struct iso2022set {
	int g[4];
	int charset[4];
	int ctlset[2];
	int GL;
	int GR;
	int SS;
};

#define	VSC_GET_KANJIMODE	_IOR('P', 10, int)
#define	VSC_SET_KANJIMODE	_IOW('P', 11, int)
#define	VSC_GET_ISO2022TABLE	_IOR('P', 12, struct iso2022set)
#define	VSC_SET_ISO2022TABLE	_IOW('P', 13, struct iso2022set)
#define	VSC_FORCE_JIS83		_IOW('P', 15, int)

/* Crt & Vram */
#define	VSC_USE_VRAM		_IOW('P', 35, int)
#define	VSC_SET_CRTMODE		_IOW('P', 36, int)
#define	VSC_SET_CRTLINES	_IOW('P', 37, int)

/* Font and Keymap */
typedef u_short FontCode;

#define	FONTROM		0x0
#define	FONTGAIJI	0x1
#define	FONTASCII	0x2

#define	MAXASCIICODE	128

struct FontList {
	FontCode code;
	u_int type;
	FontCode romcode;
	u_short font[16];
	FontCode result;
};

#define	VSC_LOAD_FONT	_IOW('P', 40, struct FontList)
#define	VSC_RESET_FONT	_IO('P', 41)
#define	VSC_LOAD_JIS83	_IO('P', 42)

/* Misc */
#define	CYRIX_CACHE_ON	_IO('P', 48)
#define	CYRIX_CACHE_OFF	_IO('P', 49)
#define	VSC_GET_ROOTDEV	_IOR('P', 50, dev_t)	/* XXX: should move */

/* Status line */
#define	VSC_STATUS_LINE	_IOW('P', 51, int)
#define	VSC_SPLIT_SC	_IOW('P', 52, int)

struct status_line {
	int length;
	int flags;

#define	MAX_ST_BYTES 256
	u_char msg[MAX_ST_BYTES];
};

#define	VSC_WRITE_ST	_IOW('P', 53, struct status_line)

/*********************************************************************
 * BSDI Interface
 *********************************************************************/
#define	CONSOLE_X_MODE	_IOW('K', 22, int)
#define	X_MODE_ON	1
#define	X_MODE_OFF	0

/*********************************************************************
 * SYSCON Interface
 *********************************************************************/
/*
 * If PCVT_FAKE_SYSCONS10 has been defined, this command is also understood.
 * pcvt then fakes to be syscons 1.0; this should not be taken by any prog-
 * ram since it is a lie (but for at least XFree86 2.0 the only way to make
 * use of the VT_OPENQRY command - this has been broken in syscons prior
 * to 1.0).
 * Once that XFree86 has been fixed, we will cease to support this command
 * anymore.
 */

#define CONS_GETVERS	_IOR('c', 74, long)

#endif	/* !_I386_VSC_VSCIO_H_ */
