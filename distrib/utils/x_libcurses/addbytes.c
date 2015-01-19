/*	$NetBSD: addbytes.c,v 1.12 1998/02/03 19:12:16 perry Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)addbytes.c	8.4 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: addbytes.c,v 1.12 1998/02/03 19:12:16 perry Exp $");
#endif
#endif	/* not lint */

#include "curses.h"

#ifdef	_X_LIBCURSES
esc shift;
#endif	/* _X_LIBCURSES */

#define	SYNCH_IN	{y = win->cury; x = win->curx;}
#define	SYNCH_OUT	{win->cury = y; win->curx = x;}

/*
 * waddbytes --
 *	Add the character to the current position in the given window.
 */
int
__waddbytes(win, bytes, count, so)
	WINDOW *win;
	const char *bytes;
	int count;
	int so;
{
	static char blanks[] = "        ";
	int c, newx, x, y;
	char stand;
#ifdef	_X_LIBCURSES
	static char stack = 0;
	unsigned char code;
#endif	/* _X_LIBCURSES */
	__LINE *lp;

#ifdef	_X_LIBCURSES
#ifdef __GNUC__
	lp = NULL;	/* XXX gcc -Wuninitialized */
#endif
#endif	/* _X_LIBCURSES */

	SYNCH_IN;

#ifdef	_X_LIBCURSES
	code = win->flags & _WCODE_MASK >> 28;
#endif	/* _X_LIBCURSES */

	while (count--) {
#ifdef	_X_LIBCURSES
		if (stack) {
			c = stack;
			count ++;
			stack = 0;
			code = _CODE_WB1;
			goto skip_code;
		}
#endif	/* _X_LIBCURSES */
		c = *bytes++;
#ifdef DEBUG
	__CTRACE("ADDBYTES('%c') at (%d, %d)\n", c, y, x);
#endif
		switch (c) {
		case '\t':
			SYNCH_OUT;
			if (waddbytes(win, blanks, 8 - (x % 8)) == ERR)
				return (ERR);
			SYNCH_IN;
			break;

		default:
#ifdef DEBUG
	__CTRACE("ADDBYTES(%0.2o, %d, %d)\n", win, y, x);
#endif
#ifdef	_X_LIBCURSES
			if (c == 0x1B) {
				shift = _WSH_ESC;
				continue;
			}
			switch (shift) {
			case _WSH_ESC:
				if (c == '$')
					shift = _WSH_WB0;
				else if (c == '(')
					shift = _WSH_SB0;
				else
					shift = _WSH_NO;
				continue;
			case _WSH_WB0:
				if (c == 'B')
					shift = _WSH_WB1;
				else
					shift = _WSH_NO;
				continue;
			case _WSH_SB0:
				if (c == 'B')
					shift = _WSH_SB1;
				else
					shift = _WSH_NO;
				continue;
			default:
				if (c & 0x80) {
					if (code == _CODE_WB1)
						code = _CODE_WB2;
					else
						code = _CODE_WB1;
				} else if (shift == _WSH_WB1) {
					code = _CODE_WB1;
					shift = _WSH_WB2;
					c |= 0x80;
				} else if (shift == _WSH_WB2) {
					code = _CODE_WB2;
					shift = _WSH_WB1;
					c |= 0x80;
				} else
					code = _CODE_SB;
			}
			if (count == 0 && code == _CODE_WB1) {
				stack = c;
				return (OK);
			}
skip_code:

			if (x == win->maxx - 1 && (code & _CODE_WB1)) {
				lp->flags |= __ISPASTEOL;
			}
#endif	/* _X_LIBCURSES */
			
			lp = win->lines[y];
			if (lp->flags & __ISPASTEOL) {
				lp->flags &= ~__ISPASTEOL;
newline:			if (y == win->maxy - 1) {
					if (win->flags & __SCROLLOK) {
						SYNCH_OUT;
						scroll(win);
						SYNCH_IN;
						lp = win->lines[y];
					        x = 0;
					} else
						return (ERR);
				} else {
					y++;
					lp = win->lines[y];
					x = 0;
				}
				if (c == '\n')
					break;
			}
				
			stand = '\0';
			if (win->flags & __WSTANDOUT || so)
				stand |= __STANDOUT;
#ifdef DEBUG
	__CTRACE("ADDBYTES: 1: y = %d, x = %d, firstch = %d, lastch = %d\n",
	    y, x, *win->lines[y]->firstchp, *win->lines[y]->lastchp);
#endif
			if (lp->line[x].ch != c || 
			    !(lp->line[x].attr & stand)) {
				newx = x + win->ch_off;
				if (!(lp->flags & __ISDIRTY)) {
					lp->flags |= __ISDIRTY;
					*lp->firstchp = *lp->lastchp = newx;
				}
				else if (newx < *lp->firstchp)
					*lp->firstchp = newx;
				else if (newx > *lp->lastchp)
					*lp->lastchp = newx;
#ifdef DEBUG
	__CTRACE("ADDBYTES: change gives f/l: %d/%d [%d/%d]\n",
	    *lp->firstchp, *lp->lastchp,
	    *lp->firstchp - win->ch_off,
	    *lp->lastchp - win->ch_off);
#endif
			}
			lp->line[x].ch = c;
#ifdef	_X_LIBCURSES
			lp->line[x].attr &= _CODE_MASK;
			lp->line[x].attr |= code;
#endif	/* _X_LIBCURSES */
			if (stand)
				lp->line[x].attr |= __STANDOUT;
			else
				lp->line[x].attr &= ~__STANDOUT;
			if (x == win->maxx - 1)
				lp->flags |= __ISPASTEOL;
			else
				x++;
#ifdef DEBUG
	__CTRACE("ADDBYTES: 2: y = %d, x = %d, firstch = %d, lastch = %d\n",
	    y, x, *win->lines[y]->firstchp, *win->lines[y]->lastchp);
#endif
			break;
		case '\n':
			SYNCH_OUT;
			wclrtoeol(win);
			SYNCH_IN;
			if (!NONL)
				x = 0;
			goto newline;
		case '\r':
			x = 0;
			break;
		case '\b':
			if (--x < 0)
				x = 0;
			break;
		}
	}
#ifdef	_X_LIBCURSES
	win->flags = (win->flags & ~_WCODE_MASK) | ((unsigned int)code << 28);
#endif	/* _X_LIBCURSES */
	SYNCH_OUT;
	return (OK);
}
