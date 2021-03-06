/*	$NetBSD: move.c,v 1.8 1998/02/03 19:12:28 perry Exp $	*/

/*
 * Copyright (c) 1981, 1993, 1994
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
static char sccsid[] = "@(#)move.c	8.2 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: move.c,v 1.8 1998/02/03 19:12:28 perry Exp $");
#endif
#endif	/* not lint */

#include "curses.h"

#ifdef	_X_LIBCURSES
extern esc shift;
#endif	/* _X_LIBCURSES */

/*
 * wmove --
 *	Moves the cursor to the given point.
 */
int
wmove(win, y, x)
	WINDOW *win;
	int y, x;
{

#ifdef DEBUG
	__CTRACE("wmove: (%d, %d)\n", y, x);
#endif
	if (x < 0 || y < 0)
		return (ERR);
	if (x >= win->maxx || y >= win->maxy)
		return (ERR);
	win->curx = x;
	win->lines[win->cury]->flags &= ~__ISPASTEOL;
	win->cury = y;
#ifdef	_X_LIBCURSES
	shift = _WSH_NO;
#endif	/* _X_LIBCURSES */
	win->lines[y]->flags &= ~__ISPASTEOL;
	return (OK);
}
