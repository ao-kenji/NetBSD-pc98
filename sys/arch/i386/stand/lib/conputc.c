/*	$NecBSD: conputc.c,v 1.1 1998/03/20 03:05:01 kmatsuda Exp $	*/
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

/* extracted from netbsd_pc98:sys/arch/i386/boot/io.c */

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>

#define NORMAL_TEXT_VRAM	(0xA0000 - 0x01000)   /* -BOOTSEG */
#define	NORMAL_COL		80
#define	NORMAL_ROW		25

#define	CHR		2
#define TEXT_GDC	0x60

/* set default normal *//* for save code size */

u_short *Crtat;
u_int	row;
u_int	col;

void
cursor(pos)
	int pos;
{
	while((inb(TEXT_GDC + 0) & 0x04) == 0) {} /* GDC wait */
	outb(TEXT_GDC + 2, 0x49);	/* CSRW */
	outb(TEXT_GDC + 0, pos & 0xff);	/* EADl */
	outb(TEXT_GDC + 0, pos >> 8);	/* EADh */
}

void
conputc(c)
	int c;
{
	static u_short *crtat = 0;
	u_short	*cp;
	int i;

	if (crtat == 0) 
	{
		Crtat = (u_short *)NORMAL_TEXT_VRAM;
		row = NORMAL_ROW;
		col = NORMAL_COL;
		crtat = Crtat;
		for (i = 0, cp = Crtat; i < row * col; i++) 
		{
			*cp++ = ' ';
		}
	}

	switch (c) {
	case '\t':
		do {
			conputc(' ');
		} while ((int)crtat % (8 * CHR));
		break;

	case '\010':
		crtat--;
		break;

	case '\r':
		crtat -= (crtat - Crtat) % col;
		break;

	case '\n':
		crtat += col;
		break;

	default:
		if (c == 0x5c) c = 0xfc;
		if (c == 0x7c) c = 0x96;
		*crtat++ = c;
		break ;
	}

	if (crtat >= Crtat + col * row) {
		/* scroll */
		cp = Crtat;
		for (i = 1; i < row; i++) {
			bcopy((void *)(cp + col), (void *)cp, col * 2);
			cp += col;
		}
		for (i = 0; i < col; i++) {
			*cp++ = ' ';
		}
		crtat -= col;
	}

	cursor(crtat - Crtat);
}
