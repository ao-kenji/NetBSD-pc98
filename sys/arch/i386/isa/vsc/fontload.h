/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
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

#ifndef _I386_VSC_FONTLOAD_H_
#define _I386_VSC_FONTLOAD_H_

#define	USERGAIJIBASE	0x7621
#define	USERGAIJI1MAX	0x7680
#define	USERGAIJI2BASE	0x7721
#define	USERGAIJIMAX	0x7780
#define	GAIJISIZE (USERGAIJI1MAX - USERGAIJIBASE + USERGAIJIMAX - USERGAIJI2BASE)

struct Gaiji {
	int cc;
	int hint;
#define	GAIJIBUSY	0x01
#define	JIS83LOAD	0x02
#define	TIMEOUTOK	0x04
	int stat;
	u_char gaiji[GAIJISIZE];
};

struct ConvTbl {
	FontCode org;
	FontCode alt;
};

struct FontBlockList {
	struct ConvTbl *tbl;
	u_int size;
	u_int cc;
};

#define	ISO2022_CODESET_ASCII	0
#define	ISO2022_CODESET_JIS	ISO2022_CODESET_ASCII
#define	ISO2022_CODESET_ROMAJI	1
#define	ISO2022_CODESET_EUC	2
#define	ISO2022_CODESET_SJIS	3
#define	ISO2022_CODESET_AUX	4

struct kanji {
	struct iso2022set iso;
	char *name;
};

#ifdef	_KERNEL
void init_fontload_system __P((struct vsc_softc *));
int clear_all_fonts __P((struct vsc_softc *));
int enter_fonts __P((struct vsc_softc *, struct FontList *));
int load_jis83_fonts __P((struct vsc_softc *));
int SearchCodeSwap __P((FontCode *, struct FontBlockList *));
int set_kanjimode __P((struct video_state * , int));

extern struct kanji kanjiset[];
#endif	/* _KERNEL */
#endif /* !_I386_VSC_FONTLOAD_H_ */
