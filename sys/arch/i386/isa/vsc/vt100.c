/*	$NecBSD: vt100.c,v 1.45.2.4 1999/09/03 06:31:41 honda Exp $	*/
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
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include "opt_kbd.h"
#include "opt_vsc.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <i386/isa/vsc/config.h>
#include <i386/isa/vsc/vsc.h>
#include <i386/isa/vsc/vt100.h>

static __inline void backward_line __P((wf_t *));
static __inline void check_next_line __P((wf_t *));
static __inline void MoveCursor __P((int, int, wf_t *));
#ifdef	HOW_TO_TREATE
static void ControlSpace __P((wf_t *));
#endif	/* HOW_TO_TREATE */
static __inline void BackSpace __P((wf_t *));
static void ForwardTab __P((wf_t *));
static void BackwardTab __P((wf_t *));
static void respond __P((u_char *, wf_t *));
static void ReqParam __P((wf_t *));
static void DeviceAt __P((wf_t *));
static void DeviceStatus __P((int, wf_t *));
static void set_ansi_led __P((wf_t *));
static void set_dec_mode __P((vsc_wchar_t, wf_t *));
static __inline void change_scr_region __P((int, int, wf_t *));
static int switch_status_win __P((wf_t *));
static int switch_normal_win __P((wf_t *));
static int DoDecSpecial __P((vsc_wchar_t, wf_t *));
static int ProcessAnsiEsc __P((vsc_wchar_t, wf_t *));
static __inline int CheckEsc __P((vsc_wchar_t, wf_t *));
static __inline void MoveCursorR __P((int, wf_t *));
static __inline void MoveCursorL __P((int, wf_t *));
static __inline void MoveCursorU __P((int, wf_t *));
static __inline void MoveCursorD __P((int, wf_t *));
static void RestoreCursor __P((wf_t *));
static __inline void LineFeed __P((wf_t *));

/*********************************************************
 * MOVE CURSOR (ABSTRACT)
 *********************************************************/
static __inline void
MoveCursor(x, y, wf)
	int x;
	int y;
	wf_t *wf;
{

	y += (ISSET(wf->decmode, DECOM) ? (wf->sc.lw - 1) : wf->o.row);
	x += wf->o.col;
	wf->r.row = (y < wf->o.row ? wf->o.row :
		     (y >= wf->n.row ? wf->n.row - 1 : y));
	wf->r.col = (x < wf->o.col ? wf->o.col :
		     (x >= wf->n.col ? wf->n.col : x));
}

#ifdef	HOW_TO_TREATE
static void
ControlSpace(wf)
	wf_t *wf;
{

	if (wf->r.col < wf->n.col)
		wf->r.col++;
	else
		wf->r.col = wf->n.col;
}
#endif	/* HOW_TO_TREATE */

static __inline void
BackSpace(wf)
	wf_t *wf;
{

	if (wf->r.col > wf->o.col)
		wf->r.col--;
	else if (ISSET(wf->decmode, DECAWM) && wf->r.row > wf->o.row)
	{
		wf->r.col = wf->n.col - 1;
		wf->r.row--;
	}
}

static __inline void
MoveCursorR(n, wf)
	int n;
	wf_t *wf;
{

	check_next_line(wf);
	wf->r.col += n;
	if (wf->r.col >= wf->n.col)
		wf->r.col = wf->n.col - 1;
}

static __inline void
MoveCursorL(n, wf)
	int n;
	wf_t *wf;
{

	wf->r.col -= n;
	if (wf->r.col < wf->o.col)
		wf->r.col = wf->o.col;
}

static __inline void
MoveCursorU(n, wf)
	int n;
	wf_t *wf;
{
	int top = wf->sc.lw - 1;

	if (wf->r.row < top)
	{
		if ((wf->r.row -= n) < wf->o.row)
			wf->r.row = wf->o.row;
	}
	else if ((wf->r.row -= n) < top)
		wf->r.row = top;
}

static __inline void
MoveCursorD(n, wf)
	int n;
	wf_t *wf;
{
	int bot = wf->sc.hg - 1;

	if (wf->r.row > bot)
	{
		if ((wf->r.row += n) >= wf->n.row)
			wf->r.row = wf->n.row - 1;
	}
	else if ((wf->r.row += n) > bot)
		wf->r.row = bot;
}

void
SaveCursor(wf)
	wf_t *wf;
{

	bcopy((u_char *) &wf->attr, wf->bs, CURSTATESIZE);
}

static void
RestoreCursor(wf)
	wf_t *wf;
{
	int s = spltty();

	bcopy(wf->bs, (u_char *) &wf->attr, CURSTATESIZE);
	splx(s);
}

/**********************************************************************
 * Line Control
 *********************************************************************/
static __inline void
LineFeed(wf)
	wf_t *wf;
{

	if (wf->r.row == wf->sc.hg - 1)
		scroll_screen(GO_FORWARD, 1, wf);
	else if (wf->r.row < wf->n.row - 1)
		wf->r.row++;
	else
		wf->r.row = wf->n.row - 1;
}

static __inline void
backward_line(wf)
	wf_t *wf;
{

	if (wf->r.row == wf->sc.lw - 1)
		scroll_screen(GO_BACKWARD, 1, wf);
	else
		MoveCursorU(1, wf);
}

static __inline void
check_next_line(wf)
	wf_t *wf;
{

	if (wf->r.col < wf->n.col)
		return;

	if (ISSET(wf->decmode, DECAWM))
	{
		LineFeed(wf);
		wf->r.col = wf->o.col;
	}
	else
		wf->r.col = wf->n.col - 1;
}

/**********************************************************************
 * Tabs
 *********************************************************************/
static void
ForwardTab(wf)
	wf_t *wf;
{
	int x;

	check_next_line(wf);
	x = wf->r.col;
	if (x < wf->n.col - 1 && wf->tabs[x])
		x++;
	while (x < wf->n.col - 1 && !wf->tabs[x])
		x++;
	wf->r.col = x;
}

static void
BackwardTab(wf)
	wf_t *wf;
{
	int x = VSPCOL(wf);

	if (x > wf->o.col && wf->tabs[x])
		x--;
	while (x > wf->o.col && !wf->tabs[x])
		x--;
	wf->r.col = x;
}

/**********************************************************************
 * Status report
 *********************************************************************/
static void
respond(status, wf)
	u_char *status;
	wf_t *wf;
{
	struct tty *tp = wf->vsp->tp;

	if (!tp)
		return;
	while (*(status))
		(*linesw[tp->t_line].l_rint) ((*status++) & 0xff, tp);
}

#define	DA_VT220	"\033[?62;1;2;6;7;8;9c"
#define	DA_VT100	"\033[?1;2c"

static void
ReqParam(wf)
	wf_t *wf;
{
	static u_char *answr0 = (u_char *) "\033[2;1;1;112;112;1;0x";
	static u_char *answr1 = (u_char *) "\033[3;1;1;112;112;1;0x";

	if (VSPCX(wf) == 0)
		respond(answr0, wf);
	else
		respond(answr1, wf);
}

static void
DeviceAt(wf)
	wf_t *wf;
{

#ifdef	VT220
	respond(DA_VT220, wf);
#else	/* !VT220 */
	respond(DA_VT100, wf);
#endif	/* !VT220 */
}

static void
DeviceStatus(select, wf)
	int select;
	wf_t *wf;
{
	static u_char *answr = (u_char *) "\033[0n";
	static u_char *panswr = (u_char *) "\033[?13n";	/* Printer Unattached */
	static u_char *udkanswr = (u_char *) "\033[?21n";	/* UDK Locked */
	static u_char *langanswr = (u_char *) "\033[?27;1n";	/* North American */
	static u_char buffer[16];
	int i = 0;

	switch (select)
	{
	case 5:
		respond(answr, wf);
		break;

	case 6:		/* return cursor position */
		buffer[i++] = 0x1b;
		buffer[i++] = '[';
		if ((wf->r.row + 1) > 10)
			buffer[i++] = ((wf->r.row + 1) / 10) + '0';
		buffer[i++] = ((wf->r.row + 1) % 10) + '0';
		buffer[i++] = ';';
		if ((wf->r.col + 1) > 10)
			buffer[i++] = ((wf->r.col + 1) / 10) + '0';
		buffer[i++] = ((wf->r.col + 1) % 10) + '0';
		buffer[i++] = 'R';
		buffer[i++] = '\0';

		respond(buffer, wf);
		break;

	case 15:		/* return printer status */
		respond(panswr, wf);
		break;

	case 25:		/* return udk status */
		respond(udkanswr, wf);
		break;

	case 26:		/* return language status */
		respond(langanswr, wf);
		break;

	default:		/* nothing else valid */
		break;
	}
}

/***********************************************************
 * Misc
 ***********************************************************/
static void
set_ansi_led(wf)
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;
	int i, flags = 0;

	for (i = 0; i <= wf->nparm && i < MAXPARM; i++)
	{
		switch (wf->parm[i])
		{
		case 0:
			flags = 0;
			break;

		case 1:
			SET(flags, LED_CAP);
			break;

		case 2:
			SET(flags, LED_KANA);
			break;

		case 3:
			SET(flags, LED_NUM);
			break;
		}
	}

	vsp->vs_vi.vi_leds = flags;
	vscp->sc_ks->ks_set_led(vscp->sc_kscp, vsp->id); /* update */
}

static void
set_dec_mode(c, wf)
	vsc_wchar_t c;
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;
	int i;

	for (i = 0; i <= wf->nparm && i < MAXPARM; i++)
	{
		switch (wf->parm[i])
		{
		case 4:
			if (c == 'h')
				SET(wf->decmode, DECINS);
			else
				CLR(wf->decmode, DECINS);
			break;

		case 20:
			if (c == 'h')
			{
				SET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_KEYLNM);
			}
			else
			{
				CLR(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_KEYLNM);
			}
			break;
		}
	}
}

static __inline void
change_scr_region(cx, cy, wf)
	int cx;
	int cy;
	wf_t *wf;
{

	cx += wf->o.row;
	cy += wf->o.row;
	if (cx <= wf->o.row)
		cx = wf->o.row + 1;
	if (cy <= wf->o.row || cy > wf->n.row)
		cy = wf->n.row;
	if (cx >= cy)
		return;

	wf->sc.lw = cx;
	wf->sc.hg = cy;
	wf->sc.sz = wf->sc.hg - wf->sc.lw + 1;
	wf->r.col = wf->o.col;

	if (ISSET(wf->decmode, DECOM))
		wf->r.row = wf->sc.lw - 1;
	else
		wf->r.row = wf->o.row;
}

/**********************************************************************
 * ISO2022
 *********************************************************************/
#define	DECSTATE	4
#define	STRSTATE	5

static int
switch_status_win(wf)
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;
	wf_t *newwf = vsp->lwf[SWINID];

	if (ISSET(newwf->flags, OPENF) == 0)
	{
		wf->state = STRSTATE;
		return ESCSEQ;
	}

	if (vsp->cwf != newwf)
	{
		vsp->backupwf = wf;
		vsp->cwf = newwf;
		newwf->r = newwf->o;
		ClearChar(FROMORG, vsp->vs_vscp->coltab[newwf->rsz], newwf);
	}

	return CHGWINID;
}

static int
switch_normal_win(wf)
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;

	if (vsp->backupwf)
	{
		vsp->cwf = (ISSET(vsp->backupwf->flags, OPENF) ?
			    vsp->backupwf : vsp->lwf[MWINID]);
		vsp->backupwf = NULL;
	}

	return CHGWINID;
}

#define	ESCSTART					\
{							\
	wf->nparm = 0;					\
	*((int *)wf->parm) = 0;				\
	wf->state = 1;					\
}
#define	INITSTATE	{ wf->state = 0; }
#define	STATECONTINUE	{ wf->EscSeq[wf->state++] = c; }
#define	BINDCTRL(num)	{ wf->iso.ctlset[(num)] = c; }
#define	BINDCHAR(num, set)				\
{							\
	wf->iso.charset[(num)] = (set);			\
	wf->iso.g[(num)] = c;				\
}

void
InitISO2022(iso, wf)
	struct iso2022set *iso;
	wf_t *wf;
{

	if (wf == NULL || iso == NULL)
		return;

	wf->iso = *iso;
	if (wf->iso.GL >= 4 || wf->iso.GL < 0)
		wf->iso.GL = 0;
	if (wf->iso.GR >= 4 || wf->iso.GR < 0)
		wf->iso.GR = 1;
	if (wf->iso.SS >= 4 || wf->iso.SS < -1)
		wf->iso.SS = -1;
}

static int
DoDecSpecial(c, wf)
	vsc_wchar_t c;
	wf_t *wf;
{
	int i;
	struct attribute *atp;
	struct video_state *vsp = wf->vsp;

	switch (wf->state)
	{
	case 2:
		if (wf->EscSeq[1] == '[' && c == '?')
		{
			wf->state = DECSTATE;
			wf->EscSeq[2] = c;
			return ESCSEQ;
		}
		return ERROR;

	case DECSTATE:
		if (c >= '0' && c <= '9')
		{
			wf->parm[wf->nparm] *= 10;
			wf->parm[wf->nparm] += c - '0';
			return ESCSEQ;
		}
		else if (c == ';')
		{
			wf->nparm++;
			if (wf->nparm >= MAXPARM)
				wf->nparm = MAXPARM - 1;
			wf->parm[wf->nparm] = 0;
			return ESCSEQ;
		}

		switch (c)
		{
		case 'h':
		case 'l':
			for (i = 0; i <= wf->nparm && i < MAXPARM; i++)
			{
				switch (wf->parm[i])
				{
				case 5:
					if (ISSET(vsp->flags, VSC_RCRT))
						atp = (c == 'h') ?
						      &vsp->vs_vscp->attr :
						      &vsp->vs_vscp->rattr;
					else
						atp = (c == 'h') ?
						      &vsp->vs_vscp->rattr :
						      &vsp->vs_vscp->attr;
					ReverseVideo(wf, atp);
					break;

				case 6:
					if (c == 'h')
					{
						SET(wf->decmode, DECOM);
						wf->r.row = wf->sc.lw - 1;
						wf->r.col = wf->o.col;
					}
					else
					{
						CLR(wf->decmode, DECOM);
						wf->r = wf->o;
					}
					break;

				case 7:
					if (c == 'h')
						SET(wf->decmode, DECAWM);
					else
						CLR(wf->decmode, DECAWM);
					break;

				case 25:
					if (ISSET(vsp->vs_vi.vi_flags,\
						  VSC_KBDMODE_SET_XMODE) != 0)
						break;

					if (c == 'h')
					{
						SET(vsp->flags, VSC_CURSOR);
						cursor_switch(vsp->vs_vscp->curcursor, vsp);
					}
					else
					{
						CLR(vsp->flags, VSC_CURSOR);
						cursor_switch(vsp->vs_vscp->curcursor, vsp);
					}
					break;

				default:
					break;
				}
			}
			return ESCEND;

		case 'n':
			DeviceStatus(VSPCX(wf), wf);
			return ESCEND;

		case 'K':
		case 'J':
			return ESCEND;

		}
		return ERROR;

	default:
		break;
	}

	return ERROR;
}

static int
ProcessAnsiEsc(c, wf)
	vsc_wchar_t c;
	wf_t *wf;
{
	int n;

	switch (c)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		wf->parm[wf->nparm] *= 10;
		wf->parm[wf->nparm] += c - '0';
		return ESCSEQ;
	case ';':
		wf->nparm++;
		if (wf->nparm >= MAXPARM)
			return ERROR;
		wf->parm[wf->nparm] = 0;
		return ESCSEQ;
	case 'A':
		MoveCursorU((VSPCX(wf) == 0 ? 1 : VSPCX(wf)), wf);
		break;
	case 'B':
		MoveCursorD((VSPCX(wf) == 0 ? 1 : VSPCX(wf)), wf);
		break;
	case 'C':
		MoveCursorR((VSPCX(wf) == 0 ? 1 : VSPCX(wf)), wf);
		break;
	case 'D':
		MoveCursorL((VSPCX(wf) == 0 ? 1 : VSPCX(wf)), wf);
		break;
	case 'f':
	case 'H':
		MoveCursor(VSPCY(wf) - 1, VSPCX(wf) - 1, wf);
		break;

		/*************************************
		 * Clear Screen
		 *************************************/
	case 'J':
		switch (VSPCX(wf))
		{
		case 0:
			n = wf->vsp->vs_vscp->coltab[wf->n.row - wf->r.row] - wf->r.col;
			ClearChar(FROMCUR, n, wf);
			break;
		case 1:
			n = wf->vsp->vs_vscp->coltab[wf->r.row - wf->o.row] + wf->r.col;
			ClearChar(FROMORG, (wf->r.col < wf->n.col ? n + 1 : n), wf);
			break;
		case 2:
			n = wf->vsp->vs_vscp->coltab[wf->rsz];
			ClearChar(FROMORG, n, wf);
			break;
		}
		break;

		/*************************************
		 * S Scroll up
		 * T Scroll down
		 *************************************/
	case 'S':
		if (wf->r.row + 1 < wf->sc.lw || wf->r.row >= wf->sc.hg)
			break;
		n = (VSPCX(wf) <= 0 ? 1 :
		     (VSPCX(wf) > wf->sc.sz ? wf->sc.sz : VSPCX(wf)));
		scroll_screen(GO_FORWARD, n, wf);
		break;
	case 'T':
		if (wf->r.row + 1 < wf->sc.lw || wf->r.row >= wf->sc.hg)
			break;
		n = (VSPCX(wf) <= 0 ? 1 :
		     (VSPCX(wf) > wf->sc.sz ? wf->sc.sz : VSPCX(wf)));
		scroll_screen(GO_BACKWARD, n, wf);
		break;

		/*************************************
		 * Clear Line
		 *************************************/
	case 'K':
		switch (VSPCX(wf))
		{
		case 0:
			ClearChar(FROMCUR, wf->n.col - wf->r.col, wf);
			break;
		case 1:
			n = (wf->r.col >= wf->n.col ? wf->n.col : wf->r.col + 1);
			ClearChar(FROMLINE, n, wf);
			break;
		case 2:
			ClearChar(FROMLINE, wf->n.col, wf);
			break;
		}
		break;

		/*************************************
		 * L Inserts n lines
		 * M Deletes n lines
		 *************************************/
	case 'L':
		if (wf->r.row + 1 < wf->sc.lw || wf->r.row >= wf->sc.hg)
			break;
		n = (VSPCX(wf) < 1 ? 1 : (VSPCX(wf) > wf->sc.hg - wf->r.row ?
				      wf->sc.hg - wf->r.row : VSPCX(wf)));
		InsertChar(FROMLINE, n, wf);
		wf->r.col = wf->o.col;	/* this is right ? */
		break;
	case 'M':
		if (wf->r.row + 1 < wf->sc.lw || wf->r.row >= wf->sc.hg)
			break;
		n = (VSPCX(wf) < 1 ? 1 : (VSPCX(wf) > wf->sc.hg - wf->r.row ?
				      wf->sc.hg - wf->r.row : VSPCX(wf)));
		DeleteChar(FROMLINE, n, wf);
		wf->r.col = wf->o.col;	/* this is right ? */
		break;

		/*************************************
		 * @ Inserts n chars
		 * P Deletes n chars
		 *************************************/
	case '@':
		n = (VSPCX(wf) < 1 ? 1 : (VSPCX(wf) > wf->n.col - wf->r.col ?
				      wf->n.col - wf->r.col : VSPCX(wf)));
		InsertChar(FROMCUR, n, wf);
		break;
	case 'P':
		n = (VSPCX(wf) < 1 ? 1 : (VSPCX(wf) > wf->n.col - wf->r.col ?
				      wf->n.col - wf->r.col : VSPCX(wf)));
		DeleteChar(FROMCUR, n, wf);
		break;

		/*************************************
		 * Clear Char
		 *************************************/
	case 'X':
		n = (VSPCX(wf) < 1 ? 1 : (VSPCX(wf) > wf->n.col - wf->r.col ?
				      wf->n.col - wf->r.col : VSPCX(wf)));
		ClearChar(FROMCUR, n, wf);
		break;

		/*************************************
		 * Change Scroll Region
		 *************************************/
	case 'r':
		change_scr_region((int) VSPCX(wf), (int) VSPCY(wf), wf);
		break;

		/****************************************
		 * Others
		 ****************************************/
	case 'Z':
		n = (VSPCX(wf) == 0 ? 1 : VSPCX(wf));
		while (n--)
			BackwardTab(wf);
		break;

	case 'c':
		DeviceAt(wf);
		break;

	case 'g':		/* Tab Clear */
		if (VSPCX(wf) == 0)
			wf->tabs[wf->r.col] = 0;
		else if (VSPCX(wf) == 3)
			memset(wf->tabs, 0, wf->n.col);
		break;

	case 'm':
		set_attribute(wf);
		break;

	case 'n':
		DeviceStatus(VSPCX(wf), wf);
		break;

	case 'q':
		set_ansi_led(wf);
		break;

	case 's':
		SaveCursor(wf);
		break;

	case 'u':
		RestoreCursor(wf);
		break;

	case 'x':		/* self test */
		ReqParam(wf);
		break;

	case 'y':		/* self test */
		break;

	case '?':
		return DoDecSpecial(c, wf);

	case 'h':
		set_dec_mode(c, wf);
		break;

	case 'l':
		set_dec_mode(c, wf);
		break;

	default:
		return ERROR;
	}

	return ESCEND;
}

static __inline int
CheckEsc(cw, wf)
	vsc_wchar_t cw;
	wf_t *wf;
{
	u_char cc;
	int cs;
	vsc_wchar_t c;

	if ((c = ISSET(cw, 0x7f)) < 0x20)
	{
		if (cw >= 0x80)
		{
			if (wf->iso.ctlset[1] == NOCTRL)
				goto state0;

			switch (c)
			{
			case SO:	/* SS2 */
				wf->iso.SS = 2;
				break;
			case SI:	/* SS3 */
				wf->iso.SS = 3;
				break;
			default:
				wf->cset = wf->iso.ctlset[1];
			}
			return EXT8CONTROLCHAR;
		}
		else
		{
			switch (c)
			{
			case ESC:
				ESCSTART
				return ESCSEQ;
			case CAN:
			case SUB:
				return ESCEND;
			case BEL:
				vscbeep(wf->vsp, 0, 0); /* default */
				break;
			case BS:
				BackSpace(wf);
				break;
			case LF:
			case VT:
			case FF:
				LineFeed(wf);
				break;
			case CR:
				wf->r.col = wf->o.col;
				break;
			case HT:
				ForwardTab(wf);
				break;
			case SI:	/* G0 */
				wf->iso.GL = 0;
				break;
			case SO:	/* G1 */
				wf->iso.GL = 1;
				break;
			default:
				wf->cset = wf->iso.ctlset[0];
			}
			return CONTROLCHAR;
		}
	}

	switch (wf->state)
	{
	case 0:
state0:
		if (wf->multibyte)
		{
			wf->multibyte = ((wf->multibyte << 8) | cw);
			return MULTIBYTEEND;
		}

		if (wf->iso.SS >= 0)
		{
			cs = wf->iso.charset[wf->iso.SS];
			wf->cset = wf->iso.g[wf->iso.SS];
			wf->iso.SS = -1;
		}
		else if (cw >= 0x80)
		{
			cs = wf->iso.charset[wf->iso.GR];
			wf->cset = wf->iso.g[wf->iso.GR];
		}
		else
		{
			cs = wf->iso.charset[wf->iso.GL];
			wf->cset = wf->iso.g[wf->iso.GL];
		}

		if (ISSET(cs, 0x1) == 0)
		{
			if (c == 0x20)
			{
				wf->multibyte = c;
				wf->cset = XASCII;
				return SINGLEBYTE;
			}
			if (c >= 0x7f)
				return CONTROLCHAR;
		}

		if (cs >= 2)
		{
			wf->multibyte = c;
			return SINGLEBYTE;
		}

		if (wf->cset == SJIS && (cw >= 0xa0 && cw < 0xe0))
		{
			wf->cset = XKANA;
			wf->multibyte = c;
			return SINGLEBYTE;
		}
		wf->multibyte = cw;
		return MULTIBYTEST;

	case 1:
		switch (c)
		{
		case '[':	/* Control Sequence Introducer */
		case '!':
		case '\"':
		case '#':
		case '$':
		case '(':
		case ')':
		case '*':
		case '+':
		case ',':
		case '-':
		case '.':
		case '/':
			STATECONTINUE
			return ESCSEQ;
		case '\\':
			return switch_normal_win(wf);
		case '_':
			return switch_status_win(wf);
		case '7':
			SaveCursor(wf);
			return ESCEND;
		case '8':
			RestoreCursor(wf);
			return ESCEND;
		case '=':
		case '>':
			return ESCEND;
		case 'D':
			LineFeed(wf);
			return ESCEND;
		case 'E':	/* next line */
			LineFeed(wf);
			wf->r.col = wf->o.col;
			return ESCEND;
		case 'H':	/* set tab */
			wf->tabs[wf->r.col] = 1;
			return ESCEND;
		case 'M':	/* Reverse Index */
			backward_line(wf);
			return ESCEND;
		case 'N':	/* sigle shift */
			wf->iso.SS = 2;
			return ESCEND;
		case 'O':
			wf->iso.SS = 3;
			return ESCEND;
		case 'Z':
			DeviceAt(wf);
			return ESCEND;
		case 'c':
			init_vc_device(wf->vsp, wf->vsp->scmode, 0);
			return ESCEND;
		case 'n':
			wf->iso.GL = 2;
			return ESCEND;
		case 'o':
			wf->iso.GL = 3;
			return ESCEND;
		case '~':
			wf->iso.GR = 1;
			return ESCEND;
		case '}':
			wf->iso.GR = 2;
			return ESCEND;
		case '|':
			wf->iso.GR = 3;
			return ESCEND;
		}
		break;

	case 2:
		cc = wf->EscSeq[1];
		switch (cc)
		{
		case '[':
			return ProcessAnsiEsc(c, wf);
		case '$':	/* 0x24 */
			if (JIS8 >= c && c >= JIS7)
			{
				BINDCHAR(0, CHARSET0)
				return ESCEND;
			}
			STATECONTINUE
			return ESCEND;
		case '!':
			BINDCTRL(0)
			return ESCEND;
		case '\"':
			BINDCTRL(1)
			return ESCEND;
		case '(':
		case ')':
		case '*':
		case '+':
			BINDCHAR(cc - 0x28, CHARSET2)
			return ESCEND;
		case ',':
		case '-':
		case '.':
		case '/':
			BINDCHAR(cc - 0x2c, CHARSET3)
			return ESCEND;
		case '#':
			if (c == '8')
				ScreenFillW(wf);
			return ESCEND;
		}
		break;

	case 3:
		cc = wf->EscSeq[2];
		switch (cc)
		{
		case '(':
		case ')':
		case '*':
		case '+':
			BINDCHAR(cc - 0x28, CHARSET0)
			return ESCEND;
			/* case ',': <0x2c is exception>	*/
		case '-':
		case '.':
		case '/':
			BINDCHAR(cc - 0x2c, CHARSET1)
			return ESCEND;
		}
		break;

	case DECSTATE:
		return DoDecSpecial(c, wf);

	case STRSTATE:
		return ESCSEQ;

	default:
		break;
	}

	return ERROR;
}

/* real out put */
void
vsput(buf, size, ka, wf)
	u_char *buf;
	u_int size;
	int ka;
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (ISSET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE) != 0)
		return;

	++ vsp->busycnt;

	for (; size > 0; size--)
	{
		switch (CheckEsc(((vsc_wchar_t) (*buf++)), wf))
		{
		case ESCSEQ:
		case CONTROLCHAR:
		case EXT8CONTROLCHAR:
			/* Do NOT Clear ESC State */
			continue;

		case CHGWINID:
			INITSTATE;
			wf = vsp->cwf;

		case ERROR:
		case ESCEND:
		default:
			INITSTATE;
			continue;

		case MULTIBYTEST:
		case EXT8MULTIBYTE:
			continue;

		case MULTIBYTEEND:
		case EXT8MULTIBYTEEND:
			vscwput(wf, (ka ? wf->attrp->sysattr : wf->attr));
			continue;

		case SINGLEBYTE:
			vscput(wf, (ka ? wf->attrp->sysattr : wf->attr));
			continue;
		}
	}

	if (vscp->cvsp == vsp)
	{
		if (ka)
			cursor_update(vscp->curcursor, CURSORUPABS);
		else
			cursor_update(vscp->curcursor, CURSORUPEXP);
	}

	-- vsp->busycnt;
}
