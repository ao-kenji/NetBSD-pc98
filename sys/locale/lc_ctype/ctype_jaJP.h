/*	$NecBSD: ctype_jaJP.h,v 1.9 1998/03/14 07:13:54 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	Isao Ohishi. All rights reserved.
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

#ifndef	_CTYPE_JAJP_H_
#define	_CTYPE_JAJP_H_
/*
 * Code Conversion Shift-JIS <-> EUC
 */
#define	JAJP_BYTE_WORD(u, l)	((u_char)(u) * 0x100u + (u_char)(l))
#define	JAJP_WORD_L(d)	((d) & 0xFFU)
#define JAJP_WORD_U(d)	(((d) >> 8) & 0xFF)
#define IS_JAJPSJIS(c)	is_jaJPsjis(c)
#define IS_JAJPEUC(c)	is_jaJPeuc(c)

static inline int is_jaJPsjis __P((register u_char));
static inline int is_jaJPeuc __P((register u_char));

static inline int
is_jaJPsjis(c)
	register u_char c;
{

	return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
}

static inline int
is_jaJPeuc(c)
	register u_char c;
{

	return (c >= 0x8E && c <= 0xFE && c != 0x8F && c != 0xA0);
}

u_short jis_sjis __P((u_short));
u_short sjis_jis __P((u_short));
#if ENABLE_UNICODE == 1
int euc2uni __P((u_char*, u_char*));
int euc2uni_n __P((u_char*, u_char*, int));
int uni2euc __P((u_char*, u_char*));
int uni2euc_n __P((u_char*, u_char*, int));
int pack_euc __P((u_char*, int));
int unpack_euc __P((const u_char*, u_char*, int));
#endif
#endif	/* !_CTYPE_JAJP_H_ */
