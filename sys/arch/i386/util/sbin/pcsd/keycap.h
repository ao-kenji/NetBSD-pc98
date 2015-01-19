/*	$NecBSD: keycap.h,v 1.7 1998/09/20 19:58:14 honda Exp $	*/
/*-
 * Copyright (c) 1992, 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Holger Veit
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
 * @(#)keycap.h, 3.20, Last Edit-Date: [Tue Dec 20 14:52:11 1994]
 */
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
 */

#ifndef _KEYCAP_H_
#define _KEYCAP_H_

int kinit __P((void));
int kgetnextent __P((char *));
int kgetent __P((char *, char *));
int kgetnum __P((char *));
int kgetflag __P((char *));
void ksetup __P((char *));
char *kgetstr __P((char *, char **));

u_char *pcmcia_conf_path;
u_char *ConfBuf;
u_int ConfBufSize;

#define PCCSCAP_PATH "/etc/pcmcia.conf"
#endif /* _KEYCAP_H_ */

/*-------------------------------- EOF -------------------------------------*/