/*	$NecBSD: ctype_jaJP.c,v 1.9 1998/03/14 07:13:52 kmatsuda Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <locale/lc_ctype/ctype_jaJP.h>
#if ENABLE_UNICODE == 1
#include <locale/lc_ctype/uni2eucjp.h>
#endif

/*
 * Code Conversion Shift-JIS <-> EUC
 */
u_short
jis_sjis(c)
	u_short c;
{
	u_char c1 = JAJP_WORD_U(c), c2 = JAJP_WORD_L(c);

	if (c1 & 1)
		c2 += 0x1F;
	else
		c2 += 0x7d;

	c1 = ((c1 - 0x21) >> 1) + 0x81;
	if (c2 >= 0x7F)
		c2++;

	if (c1 > 0x9F)
		c1 += 0x40;

	return (c1 << 8 | c2);
}

u_short
sjis_jis(c)
	u_short c;
{
	u_int c1 = JAJP_WORD_U(c), c2 = JAJP_WORD_L(c);

	if (c1 < 0xA0)
		c1 -= 0x70;
	else if (c1 < 0xF0)
		c1 -= 0xB0;
	else
		return (JAJP_BYTE_WORD(c1, c2));

	if (c2 >= 0x80)
		c2--;

	c1 *= 2;

	if (c2 >= 0x9E)
		c2 -= 0x5E;
	else
		c1--;

	c2 -= 0x1F;

	return (JAJP_BYTE_WORD(c1, c2));
}

#if ENABLE_UNICODE == 1
int euc2uni(unsigned char *euc, unsigned char *uni)
{
	unsigned char	buff[2];
	unsigned short	sh1, sh2;
	int				i;

	for (i=0;(euc[i]!=0x00)||(euc[i+1]!=0x00);i+=2) {
		buff[0] = euc[i+1];
		buff[1] = euc[i];
		memcpy(&sh1, buff, sizeof(short));
		sh2 = e2u[ sh1 ];
		memcpy(buff, &sh2, sizeof(short));
		uni[i] = buff[0];
		uni[i+1] = buff[1];
	}

	return( i );
}

int euc2uni_n(unsigned char *euc, unsigned char *uni, int len)
{
	unsigned char	buff[2];
	unsigned short	sh1, sh2;
	int				i;

	for (i=0;i<len;i+=2) {
		buff[0] = euc[i+1];
		buff[1] = euc[i];
		memcpy(&sh1, buff, sizeof(short));
		sh2 = e2u[ sh1 ];
		memcpy(buff, &sh2, sizeof(short));
		uni[i] = buff[0];
		uni[i+1] = buff[1];
	}

	return( i );
}

int uni2euc(unsigned char *uni, unsigned char *euc)
{
	unsigned char	buff[2];
	unsigned short	sh1, sh2;
	int				i;

	for (i=0;(uni[i]!=0x00)||(uni[i+1]!=0x00);i+=2)
	{
		buff[0] = uni[i];
		buff[1] = uni[i+1];
		memcpy(&sh1, buff, sizeof(short));
		sh2 = u2e[ sh1 ];
		memcpy(buff, &sh2, sizeof(short));
		euc[i+1] = buff[0];
		euc[i] = buff[1];
	}
	return( i );
}

int uni2euc_n(unsigned char *uni, unsigned char *euc, int len)
{
	unsigned char	buff[2];
	unsigned short	sh1, sh2;
	int				i;

	for (i=0;i<len;i+=2)
	{
		buff[0] = uni[i];
		buff[1] = uni[i+1];
		memcpy(&sh1, buff, sizeof(short));
		sh2 = u2e[ sh1 ];
		memcpy(buff, &sh2, sizeof(short));
		euc[i+1] = buff[0];
		euc[i] = buff[1];
	}
	return( i );
}

int	pack_euc(unsigned char *code, int len)
{
	unsigned char	*ptr;
	int	size;
	int	i;

	ptr = code;
	size = 0;
	for (i=0;i<len;i+=2) {
		if ( IS_JAJPEUC(code[i])) {
			*ptr = code[i];
			ptr++;
			*ptr = code[i+1];
			ptr++;
			size += 2;
		} else {
			*ptr = code[i+1];
			ptr++;
			size ++;
		}
	}

	bzero(&code[size], (len-size));

	return(size);
}

int	unpack_euc(unsigned const char *code1, unsigned char *code2, int len)
{
	int	size;
	int	i;

	bzero(code2, (len*2));
	size = 0;
	for (i=0;i<len;) {
		if ( IS_JAJPEUC(code1[i])) {
			code2[size] = code1[i];
			size++;
			i++;
		} else {
			code2[size] = '\0';
			size++;
		}
		code2[size] = code1[i];
		size++;
		i++;
	}
	code2[size] = '\0';

	return(size);
}
#endif
