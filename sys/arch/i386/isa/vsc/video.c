/*	$NecBSD: video.c,v 1.71.2.7 1999/09/08 08:26:25 honda Exp $	*/
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
/*
 * THIS FILE TREATS MACHINE DEPENDENT PART OF VSC.
 * Only video.c video.h depend on PC-98 structure.
 */

#include "opt_kbd.h"
#include "opt_vsc.h"
#define	VSC_LOGO_ENABLE

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <locale/lc_ctype/ctype_jaJP.h>

#include <i386/isa/pc98spec.h>

#include <machine/vm86bios.h>
#include <machine/pc/display.h>

#include <i386/isa/vsc/30line.h>
#include <i386/isa/vsc/config.h>
#include <i386/isa/vsc/vsc.h>
#include <i386/isa/vsc/vt100.h>
#include <i386/isa/vsc/savervar.h>
#include <i386/isa/vsc/fontload.h>
#include <i386/isa/vsc/video.h>
#define	VSC_SCREENMODE_DATA
#include <i386/isa/vsc/vsc_sm.h>
#ifndef	NO_NETBSD_LOGO
#include <i386/isa/vsc/netbsd_logo.h>
#endif	/* !NO_NETBSD_LOGO */

/* colors */
u_char ibmpc_to_pc98[16] = {
	0x01, 0x21, 0x81, 0xa1, 0x41, 0x61, 0xc1, 0xe1, 0x01, 0x21, 0x81, 0xa1,
	0x41, 0x61, 0xc1, 0xe1
};

static int ansi_colors[8] = {0, 2, 4, 6, 1, 3, 5, 7};

static int alloc_vc_ring __P((struct video_state *, int));
static void vc_write_ring __P((struct video_state *, int, vram_t *));
static void vc_read_ring __P((int, vram_t *, struct ring_mem *));
static struct ring_mem *vc_get_ring __P((int, struct ring_mem *));
static void SaveCrtData __P((wf_t *));
static void RestoreCrtData __P((wf_t *));
static void VDisplay __P((struct video_state *));
static void VScrollBegin __P((struct video_state *));
static void VScrollEnd __P((struct video_state *));
static void InternalVScroll __P((int, struct video_state *));
static void InternalScreenSwitch __P((struct video_state *));
static void DoScreen __P((void *));
static void vsc_init_hardware __P((struct video_state *));

static __inline void copy_cpos __P((wf_t *, wf_t *));
void cursor_soft_intr __P((void *));
static void vsc_pegc_write_bank __P((struct vsc_softc *, u_int));
static void vsc_set_pallet __P((struct vsc_softc *, u_int, u_int, u_int, u_int));
void vsc_X_escape __P((void));
static int vm86_screen_mode __P((struct vsc_softc *, struct screenmode *, int));
extern int cold;
void vsc_pegc_draw_screen __P((struct vsc_softc *, 
			       struct pegc_draw_position *,
			       u_int8_t *, struct pegc_pallet_data *));

/**************************************************************
 * Ring Video Memory Operations
 **************************************************************/
static int
alloc_vc_ring(vsp, size)
	struct video_state *vsp;
	int size;
{
	struct ring_mem *sp;
	int len, i;

	if (size < sizeof(struct ring_mem) * 3 || size > 0x10000)
		return EINVAL;
	if (vsp->ring)
		return 0;
	if ((sp = malloc(size, M_DEVBUF, M_NOWAIT)) == NULL)
		return ENOSPC;
	memset(sp, 0, size);

	len = size / sizeof(struct ring_mem);
	for (i = 0; i < len - 1; i++)
	{
		sp[i].next = &sp[i + 1];
		sp[i + 1].prev = &sp[i];
	}
	sp[0].prev = &sp[i];
	sp[i].next = sp;
	vsp->vn.row += len;
	vsp->vn.col = MAX_COL;
	vsp->vstep = SKIPROW;

	/* must be last */
	vsp->ring = sp;
	return 0;
}

static void
vc_write_ring(vsp, len, src)
	struct video_state *vsp;
	int len;
	vram_t *src;
{
	struct ring_mem *sp;
	int s, error;

	if (vsp->ringsz == 0)
		return;

	while ((sp = vsp->ring) == NULL)
	{
		s = spltty();
		error = alloc_vc_ring(vsp, vsp->ringsz);
		splx(s);
		if (error)
			return;
	}

	while (len-- > 0)
	{
		bcopy(src, sp->mem, MAX_COL * sizeof(vram_t));
		src += MAX_COL;
		sp = sp->next;
	}
	vsp->ring = sp;
}

static void
vc_read_ring(len, dst, sp)
	int len;
	vram_t *dst;
	struct ring_mem *sp;
{

	if (sp == NULL)
		return;

	while (len-- > 0)
	{
		bcopy(sp->mem, dst, MAX_COL * sizeof(vram_t));
		dst += MAX_COL;
		sp = sp->next;
	}
}

static struct ring_mem *
vc_get_ring(len, sp)
	int len;
	struct ring_mem *sp;
{

	while (len-- > 0)
		sp = sp->prev;
	return sp;
}

/**************************************************************
 * Switch video memory pointers.
 **************************************************************/
void
attach_crt_mem(wf)
	wf_t *wf;
{

	wf->Ctab = wf->vsp->vs_vscp->scCtab;
	wf->Atab = wf->vsp->vs_vscp->scAtab;
}

void
attach_vcrt_mem(wf)
	wf_t *wf;
{

	wf->Ctab = wf->vsp->vscCtab;
	wf->Atab = wf->vsp->vscAtab;
}

/**************************************************************
 * Sub windows
 **************************************************************/
static __inline void
copy_cpos(src, dst)
	wf_t *src;
	wf_t *dst;
{

	dst->r = src->r;
}

int
close_swin(wf)
	wf_t *wf;
{
	int s;
	struct video_state *vsp = wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (ISSET(wf->flags, OPENF) == 0 || wf == vsp->lwf[MWINID])
		return 0;

	s = spltty();
	if (vsp->cwf == wf)
	{
		vsp->cwf = vsp->lwf[MWINID];
		copy_cpos(wf, vsp->lwf[MWINID]);
	}

	CLR(wf->flags, OPENF);
	init_vc_device(vsp, vsp->scmode, VSC_INIT_SIGWINCH);
	splx(s);

	cursor_update(vscp->curcursor, CURSORUPABS);
	return 0;
}

int
open_swin(flags, wf)
	int flags;
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;
	u_int sig;
	int s;

	s = spltty();
	if (ISSET(wf->flags, OPENF))
	{
		splx(s);
		return 0;
	}
	SET(wf->flags, OPENF);

	if (flags & VSC_SWIN_UPDATE)
	{
		copy_cpos(vsp->cwf, wf);
		vsp->cwf = wf;
	}

	sig = (flags & VSC_SWIN_SIGWINCH) ? VSC_INIT_SIGWINCH : 0;
	init_vc_device(vsp, vsp->scmode, sig);

	splx(s);

	if (flags & VSC_SWIN_UPDATE)
		cursor_update(vscp->curcursor, CURSORUPABS);

	return 0;
}

/**************************************************************
 * < MEMORY ACCESS >
 **************************************************************/
#define	CLEARMEM(CRTBASE, ATRBASE, COUNT)		\
{							\
	fillw(' ', (CRTBASE), (COUNT) );		\
	fillw(wf->attrp->attr, (ATRBASE), (COUNT));	\
}

void
ScreenFillW(wf)
	wf_t *wf;
{
	struct vsc_softc *vscp = wf->vsp->vs_vscp;

	fillw('E', wf->Ctab[wf->o.row], vscp->coltab[wf->rsz]);
	fillw(wf->attrp->attr, wf->Atab[wf->o.row], vscp->coltab[wf->rsz]);
}

void
ReverseVideo(wf, newattr)
	wf_t *wf;
	struct attribute *newattr;
{
	vram_t *pos;
	int size, i, s = spltty();

	if (wf->attrp != newattr)
	{
		wf->attrp = newattr;
		wf->attr ^= ATTR_RV;
		if (ISSET(wf->flags, OPENF))
		{
			splx(s);
			pos = wf->Atab[wf->o.row];
			size = wf->vsp->vs_vscp->coltab[wf->rsz];
			for (i = 0; i < size; i ++)
				pos[i] ^= ATTR_RV;
			return;
		}
	}
	splx(s);
}

void
ClearChar(flags, count, wf)
	int flags;
	int count;
	wf_t *wf;
{
	vram_t *crtat, *atrat;

	if (!count)
		return;

	switch (flags)
	{
	case FROMORG:
	default:
		crtat = wf->Ctab[wf->o.row];
		atrat = wf->Atab[wf->o.row];
		break;

	case FROMCUR:
		crtat = wf->Ctab[wf->r.row] + wf->r.col;
		atrat = wf->Atab[wf->r.row] + wf->r.col;
		break;

	case FROMLINE:
		crtat = wf->Ctab[wf->r.row];
		atrat = wf->Atab[wf->r.row];
		break;
	}
	CLEARMEM(crtat, atrat, count);
}

void
DeleteChar(flags, count, wf)
	int flags;
	int count;
	wf_t *wf;
{
	struct vsc_softc *vscp = wf->vsp->vs_vscp;
	vram_t *srcC, *srcA, *delC, *delA;
	int movecount;

	if (count <= 0)
		return;

	switch (flags)
	{
	case FROMCUR:
	default:
		srcC = wf->Ctab[wf->r.row] + wf->r.col;
		srcA = wf->Atab[wf->r.row] + wf->r.col;
		delC = wf->Ctab[wf->r.row + 1] - count;
		delA = wf->Atab[wf->r.row + 1] - count;
		movecount = (wf->n.col - wf->r.col - count) * sizeof(vram_t);
		break;

	case FROMLINE:
		srcC = wf->Ctab[wf->r.row];
		srcA = wf->Atab[wf->r.row];
		delC = wf->Ctab[wf->sc.hg - count];
		delA = wf->Atab[wf->sc.hg - count];
		movecount = (vscp->coltab[wf->sc.hg - wf->r.row - count]) * sizeof(vram_t);
		count = vscp->coltab[count];
		break;
	}

	if (movecount > 0)
	{
		bcopy(srcC + count, srcC, movecount);
		bcopy(srcA + count, srcA, movecount);
	}

	CLEARMEM(delC, delA, count);
}

void
InsertChar(flags, count, wf)
	int flags;
	int count;
	wf_t *wf;
{
	struct vsc_softc *vscp = wf->vsp->vs_vscp;
	vram_t *srcC, *srcA;
	int movecount;

	if (count <= 0)
		return;

	switch (flags)
	{
	case FROMCUR:
	default:
		srcC = wf->Ctab[wf->r.row] + wf->r.col;
		srcA = wf->Atab[wf->r.row] + wf->r.col;
		movecount = (wf->n.col - wf->r.col - count) * sizeof(vram_t);
		break;

	case FROMLINE:
		srcC = wf->Ctab[wf->r.row];
		srcA = wf->Atab[wf->r.row];
		movecount = (vscp->coltab[wf->sc.hg - wf->r.row - count]) * sizeof(vram_t);
		count = vscp->coltab[count];
		break;
	}

	if (movecount > 0)
	{
		bcopy(srcC, srcC + count, movecount);
		bcopy(srcA, srcA + count, movecount);
	}

	CLEARMEM(srcC, srcA, count);
}

void
scroll_screen(dir, step, wf)
	int dir;
	int step;
	wf_t *wf;
{
	int count, i;
	struct video_state *vsp = wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;
	vram_t *ScWcrtat = wf->Ctab[wf->sc.lw - 1];
	vram_t *ScAtrat = wf->Atab[wf->sc.lw - 1];

	if (dir == GO_FORWARD)
	{
		vc_write_ring(vsp, step, ScWcrtat);
#ifdef	VSC_FAST_SCROLL
		count = vscp->coltab[wf->sc.sz - step];
		bcopy(wf->Ctab[wf->sc.lw - 1 + step],
		      ScWcrtat, count * sizeof(vram_t));
		bcopy(wf->Atab[wf->sc.lw - 1 + step],
		      ScAtrat, count * sizeof(vram_t));
		ScWcrtat += count;
		ScAtrat += count;
#else	/* !VSC_FAST_SCROLL */
		count = vscp->coltab[1];
		for (i = 0; i < wf->sc.sz - step;
		     i++, ScWcrtat += count, ScAtrat += count)
		{
			bcopy(wf->Ctab[wf->sc.lw - 1 + step + i],
			      ScWcrtat, count * sizeof(vram_t));
			bcopy(wf->Atab[wf->sc.lw - 1 + step + i],
			      ScAtrat, count * sizeof(vram_t));
		}
#endif	/* !VSC_FAST_SCROLL */
	}
	else
	{
		count = vscp->coltab[wf->sc.sz - step];
		bcopy(ScWcrtat,
		    wf->Ctab[wf->sc.lw - 1 + step], count * sizeof(vram_t));
		bcopy(ScAtrat,
		    wf->Atab[wf->sc.lw - 1 + step], count * sizeof(vram_t));
	}

	CLEARMEM(ScWcrtat, ScAtrat, vscp->coltab[step]);
}

static void
SaveCrtData(wf)
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (ISSET(wf->flags, OPENF) == 0)
		return;
	bcopy(vscp->scCtab[wf->o.row], vsp->vscCtab[wf->o.row],
	      vscp->coltab[wf->rsz] * sizeof(vram_t));
	bcopy(vscp->scAtab[wf->o.row], vsp->vscAtab[wf->o.row],
	      vscp->coltab[wf->rsz] * sizeof(vram_t));
}

static void
RestoreCrtData(wf)
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (ISSET(wf->flags, OPENF) == 0)
		return;
	bcopy(vsp->vscCtab[wf->o.row], vscp->scCtab[wf->o.row],
	      vscp->coltab[wf->rsz] * sizeof(vram_t));
	bcopy(vsp->vscAtab[wf->o.row], vscp->scAtab[wf->o.row],
	      vscp->coltab[wf->rsz] * sizeof(vram_t));
}

void
clear_process_terminal(vsp)
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;

	/* XXX */
	if (ISSET(vsp->flags, VSC_WAIT_REL | VSC_WAIT_ACK))
		vscp->sw_pend = 0;

	vsp->proc = NULL;
	vsp->smode.mode = VT_AUTO;
	CLR(vsp->flags, VSC_WAIT_REL | VSC_WAIT_ACT | VSC_WAIT_ACK);
}

static void
VDisplay(vsp)
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;
	wf_t *wf = vsp->lwf[MWINID];
	struct ring_mem *rp;

	if (ISSET(vsp->flags, VSC_SPLIT))
	{
		rp = vc_get_ring(vsp->vr.row + wf->rsz - 1, vsp->ring);
		vc_read_ring(wf->rsz, wf->Ctab[wf->o.row], rp);
	}
	else
	{
		int i;

		fillw(wf->attrp->vattr, vscp->scAtab[wf->o.row],
		      vscp->coltab[wf->rsz]);
		i = ((vsp->vr.row >= wf->rsz) ? wf->rsz : vsp->vr.row);
		rp = vc_get_ring(vsp->vr.row, vsp->ring);
		vc_read_ring(i, vscp->scCtab[wf->o.row], rp);
		if (vsp->vr.row >= wf->rsz)
			return;
		bcopy(wf->Ctab[wf->o.row], vscp->scCtab[i + wf->o.row],
		      vscp->coltab[wf->rsz - i] * sizeof(vram_t));
	}
}

static void
VScrollBegin(vsp)
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;
	wf_t *mwf = vsp->lwf[MWINID];
	wf_t *vwf = vsp->lwf[VWINID];

	if (ISSET(vsp->flags, VSC_SPLIT))
	{
		open_swin(VSC_SWIN_SIGWINCH | VSC_SWIN_UPDATE, vwf);
		vc_write_ring(vsp, mwf->rsz, mwf->Ctab[mwf->o.row]);
		fillw(mwf->attrp->vattr, mwf->Atab[mwf->o.row], vscp->coltab[mwf->rsz]);
	}
	else
	{
		CLR(vsp->flags, VSC_CURSOR);
		cursor_update(vscp->curcursor, CURSOROFF);
		SaveCrtData(mwf);
		fillw(mwf->attrp->vattr, mwf->Atab[mwf->o.row],
		      vscp->coltab[mwf->rsz]);
		attach_vcrt_mem(mwf);
	}

	vsp->vr.row = 1;
	VDisplay(vsp);
}

static void
VScrollEnd(vsp)
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;
	wf_t *mwf = vsp->lwf[MWINID];

	if (ISSET(vsp->flags, VSC_SPLIT))
	{
		vsp->vr.row = 1;
		VDisplay(vsp);
		fillw(mwf->attrp->attr, mwf->Atab[mwf->o.row],
		      vscp->coltab[mwf->rsz]);
		close_swin(vsp->lwf[VWINID]);
	}
	else
	{
		RestoreCrtData(mwf);
		attach_crt_mem(mwf);
		SET(vsp->flags, VSC_CURSOR);
		cursor_update(vscp->curcursor, CURSORON);
		cursor_update(vscp->curcursor, CURSORUPABS);
	}

	vsp->vr.row = 0;
}

/*
 * Remark: There are 3 processes which access the real crt mem
 *	at the same time. We must avoid it!
 *
 *       VScroll                   ExecSwitchScreen
 *         |      \            /       |
 *	   |(F)      DoScreen          |(F)
 *         |      /            \       |
 *       InternalVScroll           InternalScreenSwitch
 *                    \     /
 *                       | <--- svputc
 *                    Real Crt Mem.
 */
static void
DoScreen(arg)
	void *arg;
{
	struct DoScreenData *data = arg;
	struct video_state *tvsp, *cvsp;
	struct vsc_softc *vscp;

	switch (data->flags)
	{
	case VSC_SCREEN_CHG:
		tvsp = data->tvsp;
		vscp = tvsp->vs_vscp;
		cvsp = vscp->cvsp;
		if (cvsp->busycnt || tvsp->busycnt)
			goto retry;

		InternalScreenSwitch(tvsp);
		if (tvsp->proc &&
		    tvsp->proc != (struct proc *) pfind(tvsp->pid))
			clear_process_terminal(tvsp);

		if (ISSET(tvsp->flags, VSC_WAIT_ACT))
		{
			wakeup(&tvsp->smode);
			CLR(tvsp->flags, VSC_WAIT_ACT);
		}

		if (tvsp->smode.mode == VT_PROCESS)
		{
			SET(tvsp->flags, VSC_WAIT_ACK);
			if (tvsp->smode.acqsig)
				psignal(tvsp->proc, tvsp->smode.acqsig);
		}
		else
			vscp->sw_pend = 0;
		break;

	case VSC_SCREEN_SC_D:
	case VSC_SCREEN_SC_U:
	case VSC_SCREEN_SC_E:
		tvsp = data->vsp;
		if (tvsp->busycnt)
			goto retry;

		InternalVScroll(data->flags, tvsp);
		break;
	}

	data->win_event = 0;
	return;

retry:
	data->win_event = 1;
	timeout(DoScreen, arg, 1);
	return;
}

static void
InternalVScroll(dir, vsp)
	int dir;
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (vsp != vscp->cvsp || vsp->ring == NULL)
		return;

	switch (dir)
	{
	case VSC_SCREEN_SC_D:
		if (vsp->vr.row == 0)
			VScrollBegin(vsp);
		else
		{
			vsp->vr.row += vsp->vstep;
			if (vsp->vr.row > vsp->vn.row)
				vsp->vr.row = vsp->vn.row;
			VDisplay(vsp);
		}
		break;

	case VSC_SCREEN_SC_U:
		if (vsp->vr.row == 0)
			break;
		vsp->vr.row -= vsp->vstep;
		if (vsp->vr.row >= 1)
		{
			VDisplay(vsp);
			break;
		}
		else
		{
			vsp->vr.row = 0;
			VScrollEnd(vsp);
		}
		break;

	case VSC_SCREEN_SC_E:
		if (vsp->vr.row == 0)
			break;
		VScrollEnd(vsp);
		break;
	}
}

void
VScroll(vsp, dir, force)
	struct video_state *vsp;
	int dir;
	int force;
{
	struct vsc_softc *vscp = vsp->vs_vscp;
	struct DoScreenData *data = &vscp->sd;
	int s;

	if (force)
	{
		s = spltty();
		InternalVScroll(dir, vsp);
		splx(s);
		return;
	}

	s = spltty();
	 if (data->win_event)
	{
		splx(s);
		return;
	}
	data->win_event = 1;
	splx(s);

	data->vsp = vsp;
	data->flags = dir;
#ifdef AVOID_LONG_SPLTTY
	timeout(DoScreen, (void *) data, 1);
#else	/* !AVOID_LONG_SPLTTY */
	DoScreen(data);
#endif	/* !AVOID_LONG_SPLTTY */
}

void
EnterLeaveVcrt(vsp, vcrt)
	struct video_state *vsp;
	int vcrt;
{
	wf_t *wf;
	int wno;

	for (wno = 0; wno < NSWIN; wno++)
	{
		wf = vsp->lwf[wno];
		if (vsp->vr.row > 0 && wno == MWINID &&
		    ISSET(vsp->flags, VSC_SPLIT) == 0)
			continue;

		if (vcrt != 0)
		{
			if (wf->Ctab != vsp->vscCtab)
				SaveCrtData(wf);
			attach_vcrt_mem(wf);
		}
		else
		{
			RestoreCrtData(wf);
			attach_crt_mem(wf);
		}
	}
}

void
InternalScreenSwitch(tmpvs)
	struct video_state *tmpvs;
{
	struct vsc_softc *vscp = tmpvs->vs_vscp;
	struct video_state *oldvs = vscp->cvsp;

	if (tmpvs != oldvs)
	{
		EnterLeaveVcrt(oldvs, 1);
		EnterLeaveVcrt(tmpvs, 0);
	}

	/* switch new window */
	vscp->cvsp = tmpvs;
	vscp->sc_ks->ks_switch(vscp->sc_kscp, tmpvs->id,
			       &tmpvs->vs_vi, tmpvs->tp);
	if (oldvs->vs_vi.vi_leds != tmpvs->vs_vi.vi_leds)
		vscp->sc_ks->ks_set_led(vscp->sc_kscp, tmpvs->id);
	if (tmpvs->vr.row != 0 && ISSET(tmpvs->flags, VSC_SPLIT) == 0)
		VDisplay(tmpvs);

	cursor_switch(vscp->curcursor, tmpvs);
#ifdef	SCREEN_SAVER
	saver_switch(vscp->cursaver, tmpvs);
#endif	/* SCREEN_SAVER */

	vsc_start_display(tmpvs);

#ifdef	VSC_RSTAFX
	if (!ISSET(tmpvs->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE) != 0 &&
	    ISSET(oldvs->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE) != 0)
		vsc_set_scmode(vscp, NULL, VSC_SETSCM_FORCE, VSC_SETSCM_ASYNC);
#endif	/* VSC_RSTAFX */
}

int
RequestSwitchScreen(vsp)
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;
	struct DoScreenData *data = &vscp->sd;
	int s;

	s = spltty();
	if (data->win_event)
	{
		splx(s);
		return 0;
	}
	data->win_event = 1;
	splx(s);

	data->flags = VSC_SCREEN_CHG;
	data->tvsp = vsp;

#ifdef AVOID_LONG_SPLTTY
	timeout(DoScreen, (void *) data, 1);
#else	/* AVOID_LONG_SPLTTY */
	DoScreen(data);
#endif	/* AVOID_LONG_SPLTTY */
	return 0;
}

int
ExecSwitchScreen(newvsp, force)
	struct video_state *newvsp;
	int force;
{
	struct video_state *vsp;
	struct vsc_softc *vscp;
	int s;

	if (newvsp == NULL || ISSET(newvsp->flags, VSC_OPEN) == 0)
		return EINVAL;

	vscp = newvsp->vs_vscp;
 	vsp = vscp->cvsp;

	s = spltty();
	if (force != 0)
	{
		vscp->sw_pend = 0;
		InternalScreenSwitch(newvsp);
		splx(s);
		return 0;
	}

	if (vsp->proc && vsp->proc != (struct proc *) pfind(vsp->pid))
		clear_process_terminal(vsp);

	if (newvsp != vsp && newvsp->proc &&
	    newvsp->proc != (struct proc *) pfind(newvsp->pid))
		clear_process_terminal(newvsp);

	if (vscp->sw_pend)
	{
		splx(s);
		return EAGAIN;
	}

	if (newvsp != vsp)
	{
		vscp->sw_pend = newvsp->id + 1;

		if (ISSET(vsp->flags, VSC_WAIT_ACT))
		{
			wakeup(&vsp->smode);
			CLR(vsp->flags, VSC_WAIT_ACT);
		}

		if (vsp->smode.mode == VT_PROCESS)
		{
			SET(vsp->flags, VSC_WAIT_REL);
			if (vsp->smode.relsig)
				psignal(vsp->proc, vsp->smode.relsig);
		}
		else
			RequestSwitchScreen(newvsp);
	}

	splx(s);
	return 0;
}
/*********************************************************
 * OUTPUT
 *********************************************************/
static __inline void put_vram __P((FontCode, FontCode, wf_t *));
static __inline void fontto98font __P((wf_t *));
static __inline void fontwto98font __P((wf_t *));

static __inline void
fontwto98font(wf)
	wf_t *wf;
{
	struct video_state *vsp= wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;

	switch (wf->cset)
	{
	case SJIS:
		wf->multibyte = (vsc_wchar_t) sjis_jis((u_short) wf->multibyte);
		wf->cset = JIS7;

	case JIS7:
		if (ISSET(vsp->flags, VSC_FJIS83) == 0)
		{
			CLR(wf->multibyte, 0x8080);
			break;
		}
		wf->cset = JIS8;

	case JIS8:
		CLR(wf->multibyte, 0x8080);
		if (vscp->sc_wcharfb != NULL)
			SearchCodeSwap(&wf->multibyte, vscp->sc_wcharfb);
		break;

	default:
		wf->multibyte = 0x2020;
		break;
	}

	wf->multibyte = (__byte_swap_word(wf->multibyte)) - 0x20;
}

static __inline void
fontto98font(wf)
	wf_t *wf;
{
	struct video_state *vsp = wf->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;
	FontCode code;

	switch (wf->cset)
	{
	case XASCII:
	case LATIN1:
		if (vscp->sc_asciifp != NULL &&
		    wf->multibyte < MAXASCIICODE &&
		    (code = vscp->sc_asciifp[wf->multibyte]) != 0)
		{
			wf->multibyte = __byte_swap_word(code) - 0x20;
		}
		else
		{
			switch (wf->multibyte)
			{
			case 0x5c:
				wf->multibyte = 0xfc;
				break;

			case 0x7c:
				wf->multibyte = 0x96;
				break;
			}
		}
		break;

	case ROMAJI:
		if (vscp->sc_asciifp != NULL &&
		    wf->multibyte < MAXASCIICODE &&
		    (code = vscp->sc_asciifp[wf->multibyte]) != 0)
		{
			wf->multibyte = __byte_swap_word(code) - 0x20;
		}
		break;

	case ROMSTAND:
		break;

	case XKANA:
	case ROMSPECIAL:
		if (wf->multibyte > 0x20 && wf->multibyte < 0x60)
			SET(wf->multibyte, 0x80);
		else
			wf->multibyte = (vsc_wchar_t) ' ';
		break;

	case SUPGRAPH:
	case SPEGRAPH:
	case TECH:
		if (vscp->sc_gchar != NULL &&
		    wf->multibyte >= 0x5b && wf->multibyte < 0x7f)
			wf->multibyte = vscp->sc_gchar[wf->multibyte - 0x5b];
		break;

	default:
		wf->multibyte = (vsc_wchar_t) ' ';
	}
}

static __inline void
put_vram(code, attr, wf)
	FontCode code;
	FontCode attr;
	wf_t *wf;
{

	*(wf->Ctab[wf->r.row] + wf->r.col) = (vram_t) code;
	*(wf->Atab[wf->r.row] + (wf->r.col++)) = (vram_t) attr;
}

void
vscwput(wf, attr)
	wf_t *wf;
	int attr;
{

	fontwto98font(wf);
	if (wf->r.col > wf->n.col - sizeof(vsc_wchar_t))
	{
		if (ISSET(wf->decmode, DECAWM))
		{
			wf->r.col = wf->n.col - sizeof(vsc_wchar_t) + 1;
			ClearChar(FROMCUR, sizeof(vsc_wchar_t) - 1, wf);
			if (wf->r.row == wf->sc.hg - 1)
				scroll_screen(GO_FORWARD, 1, wf);
			else if (wf->r.row < wf->n.row - 1)
				wf->r.row++;
			else
				wf->r.row = wf->n.row - 1;
			wf->r.col = wf->o.col;
		}
		else
			wf->r.col = wf->n.col - sizeof(vsc_wchar_t);
	}

	if (ISSET(wf->decmode, DECINS))
		InsertChar(FROMCUR, sizeof(vsc_wchar_t), wf);
	put_vram((FontCode) wf->multibyte, (FontCode) attr, wf);
	put_vram((FontCode) (wf->multibyte | 0x80), (FontCode) attr, wf);
	wf->multibyte = 0;
}


void
vscput(wf, attr)
	wf_t *wf;
	int attr;
{

	fontto98font(wf);
	if (wf->r.col >= wf->n.col)
	{
		if (ISSET(wf->decmode, DECAWM))
		{
			if (wf->r.row == wf->sc.hg - 1)
				scroll_screen(GO_FORWARD, 1, wf);
			else if (wf->r.row < wf->n.row - 1)
				wf->r.row++;
			else
				wf->r.row = wf->n.row - 1;
			wf->r.col = wf->o.col;
		}
		else
			wf->r.col = wf->n.col - 1;
	}

	if (ISSET(wf->decmode, DECINS))
		InsertChar(FROMCUR, sizeof(u_char), wf);
	put_vram((FontCode) wf->multibyte, (FontCode) attr, wf);
	wf->multibyte = 0;
}

void
set_attribute(wf)
	wf_t *wf;
{
	vsc_wchar_t attr;
	vsc_wchar_t ap, rev;
	u_int i;

	for (i = 0; i <= wf->nparm && i < MAXPARM; i++)
	{
		switch (ap = wf->parm[i])
		{
		case 1:
			SET(wf->attrflags, VSC_ATTR_BOLD);
			break;

		case 4:	/* UNDERLINE */
			SET(wf->attrflags, VSC_ATTR_UL);
			break;

		case 5:	/* BLINKING */
			SET(wf->attrflags, VSC_ATTR_BL);
			break;

		case 7:	/* REVERSE (black on light grey) */
			SET(wf->attrflags, VSC_ATTR_RV);
			break;

		case 22:
			wf->fgc = wf->bgc = 0;
			break;

		case 24:
			CLR(wf->attrflags, VSC_ATTR_UL);
			break;

		case 25:
			CLR(wf->attrflags, VSC_ATTR_BL);
			break;

		case 27:
			CLR(wf->attrflags, VSC_ATTR_RV);
			break;

		default:
			if (ap >= 30 && ap <= 37)
			{
				wf->fgc = ap;
			}
			else if (ap >= 40 && ap <= 47)
			{
				wf->bgc = ap;
			}
			else
			{
				wf->fgc = wf->bgc = 0;
				CLR(wf->attrflags, VSC_ATTR_RV | VSC_ATTR_BL | VSC_ATTR_UL | VSC_ATTR_BOLD);
			}
			break;
		}
	}

	attr = (wf->subid == SWINID) ? wf->attrp->sattr : wf->attrp->attr;
	rev = 30;	/* XXX */

	if (wf->fgc)
	{
		attr = ISSET(attr, ~ATTR_CMASK) |
		       (ansi_colors[(wf->fgc - 30)] << ATTR_CSHIFT);
	}

	if (wf->bgc && wf->fgc == rev)
	{
		attr = ISSET(attr, ~ATTR_CMASK) |
			(ansi_colors[(wf->bgc - 40)] << ATTR_CSHIFT);
		SET(wf->attrflags, VSC_ATTR_BGON);
	}
	else
		CLR(wf->attrflags, VSC_ATTR_BGON);

	if (ISSET(wf->attrflags, VSC_ATTR_BOLD))
	{
		CLR(attr, ATTR_CMASK);
		SET(attr, ansi_colors[3] << ATTR_CSHIFT);
	}

	if (ISSET(wf->attrflags, VSC_ATTR_UL))
		SET(attr, ATTR_UL);

	if (ISSET(wf->attrflags, VSC_ATTR_BL))
		SET(attr, ATTR_BL);

	if (ISSET(wf->attrflags, VSC_ATTR_RV | VSC_ATTR_BGON))
	{
		if (wf->attrp == &wf->vsp->vs_vscp->rattr)
			CLR(attr, ATTR_RV);
		else
			SET(attr, ATTR_RV);
	}

	wf->attr = attr;
}


/**************************************************************
 * Cursor
 **************************************************************/
u_int vsc_ctl_cursor __P((int, struct cursor_softc *));
u_int vsc_display_cursor __P((int, struct cursor_softc *));

int
cursor_init(sc, vsp)
	struct cursor_softc *sc;
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;
	int s = spltty();

	sc->func = vsc_display_cursor;
	sc->ctrl = vsc_ctl_cursor;
	sc->vsp = vsp;
	sc->timeout_active = 0;
	vscp->curcursor = sc;
	(*(sc->ctrl)) (1, sc);	/* cursor on */
	splx(s);
	return 0;
}

void
cursor_soft_intr(arg)
	void *arg;
{
	struct cursor_softc *sc = arg;
	struct video_state *vsp = sc->vsp;
	wf_t *wf;
	int pos;

	wf = vsp->cwf;
	pos = vsp->vs_vscp->coltab[wf->r.row] + VSPCOL(wf);

	if (pos != sc->pos)
	{
		sc->pos = pos;
		sc->timeout_active = 1;
		timeout(cursor_soft_intr, (void *) sc, hz / 15);
		(*(sc->func)) (pos, sc);
	}
	else
		sc->timeout_active = 0;
}

int
cursor_update(sc, flags)
	struct cursor_softc *sc;
	int flags;
{
	struct video_state *vsp = sc->vsp;
	struct vsc_softc *vscp = vsp->vs_vscp;

	switch (flags)
	{
	case CURSOROFF:
		if (sc->state == 1)
			(*(sc->ctrl)) (0, sc);
		untimeout(cursor_soft_intr, (void *) sc);
		sc->timeout_active = 0;
		break;

	case CURSORON:
		if (sc->state == 0)
			(*(sc->ctrl)) (1, sc);
		break;

	case CURSORUPABS:
		{
			wf_t *wf = vsp->cwf;

			sc->pos = vscp->coltab[wf->r.row] + VSPCOL(wf);
			((*sc->func)) (sc->pos, sc);

			break;
		}

	case CURSORUPEXP:
		if (sc->timeout_active == 0)
			cursor_soft_intr(sc);
		break;
	}
	return 0;
}

int
cursor_switch(sc, vsp)
	struct cursor_softc *sc;
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (vsp != vscp->cvsp)
		return EBUSY;

	cursor_update(vscp->curcursor, CURSOROFF);
	vscp->curcursor = sc;
	sc->vsp = vsp;

	if (ISSET(vsp->flags, VSC_CURSOR))
	{
		cursor_update(sc, CURSORON);
		cursor_update(sc, CURSORUPABS);
	}
	return 0;
}

/**********************************************************
 * PORT ACCESS.
 *********************************************************/
u_int
vsc_ctl_cursor(flag, sc)
	int flag;
	struct cursor_softc *sc;
{
	struct vsc_softc *vscp = sc->vsp->vs_vscp;
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC1ioh;

	bus_space_write_1(iot, ioh, gdc1_cmd, 0x4b);
	bus_space_write_1(iot, ioh, gdc1_params, flag ? 0x8f : 0x0f);
	bus_space_write_1(iot, ioh, gdc1_params, 0);
	bus_space_write_1(iot, ioh, gdc1_params, 0x7b);

	sc->state = flag;
	return 0;
}

u_int
vsc_display_cursor(pos, sc)
	int pos;
	struct cursor_softc *sc;
{
	struct vsc_softc *vscp = sc->vsp->vs_vscp;

	if (sc->state != 0)
	{
		bus_space_tag_t iot = vscp->sc_iot;
		bus_space_handle_t ioh = vscp->sc_GDC1ioh;

		bus_space_write_1(iot, ioh, gdc1_cmd, 0x49);
		bus_space_write_1(iot, ioh, gdc1_params, (u_int8_t) pos);
		bus_space_write_1(iot, ioh, gdc1_params, (u_int8_t) (pos >> NBBY));
	}
	return 0;
}

static void
vsc_pegc_write_bank(vscp, num)
	struct vsc_softc *vscp;
	u_int num;
{
	u_int8_t *vreg = (u_int8_t *) PEGC_BANK_OFFSET(vscp);

	vreg[4] = (u_char) (num % 0x10);
	vreg[6] = (u_char) ((num + 1) % 0x10);
}

static void
vsc_set_pallet(vscp, color, red, green, blue)
	struct vsc_softc *vscp;
	u_int color;
	u_int red, green, blue;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC2ioh;

	bus_space_write_1(iot, ioh, gdc2_color, color);
	bus_space_write_1(iot, ioh, gdc2_red, red);
	bus_space_write_1(iot, ioh, gdc2_green, green);
	bus_space_write_1(iot, ioh, gdc2_blue, blue);
}

static void
vsc_init_hardware(vsp)
	struct video_state *vsp;
{
	u_int attr;
	struct vsc_softc *vscp = vsp->vs_vscp;
	sysinfo_info_t info_crt;

	info_crt = lookup_sysinfo(SYSINFO_CRT);

	fillw(' ', TEXT_RAM_ADDR(vscp), MAX_ROW * MAX_COL);
	attr = ISSET(vsp->flags, VSC_RCRT) ? vscp->rattr.attr : vscp->attr.attr;
	fillw(attr, ATTR_RAM_ADDR(vscp), MAX_ROW * MAX_COL);

	memset(G_RAM0_ADDR(vscp), 0, G_RAM_SIZE);
	memset(G_RAM1_ADDR(vscp), 0, G_RAM_SIZE);
	memset(G_RAM2_ADDR(vscp), 0, G_RAM_SIZE);

	if ((info_crt & CRT_PEGC_ENABLED) == 0)
	{
		memset(G_RAM3_ADDR(vscp), 0, G_RAM_SIZE);
		bus_space_write_1(vscp->sc_iot, vscp->sc_GDC1ioh, gdc1_mode2, 0);
		vsc_set_pallet(vscp, 0x37, 0x26, 0x15, 0x04);
	}
	else
	{
#ifdef	NO_NETBSD_LOGO
		vsc_pegc_draw_screen(vscp, NULL, NULL, NULL);
#else	/* !NO_NETBSD_LOGO */
		/*
		 * XXX: we need picture data!
		 */
		if (ISSET(vscp->sc_flags, VSC_DEVICE_ATTACHED) == 0)
		{
			SET(vsp->flags, VSC_GRAPH);
			vsc_pegc_draw_screen(vscp, &netbsd_logo_position,
				netbsd_logo_data, &netbsd_logo_pallet[0]);
		}
#endif	/* !NO_NETBSD_LOGO */
	}
}

void
vsc_start_display(vsp)
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (vsp != vscp->cvsp)
		return;

	vsc_vsync_wait(vscp);
	if (ISSET(vsp->flags, VSC_GRAPH))
		vsc_graphic_on(vscp);
	else
		vsc_graphic_off(vscp);

	vsc_vsync_wait(vscp);
	if (ISSET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE) != 0)
		vsc_text_off(vscp);
	else
		vsc_text_on(vscp);
}

/******************************************************************
 * All interface for "the 30 lines".
 * In the real sense, we need the modification only in the
 * following part below if you will change 30 lines routines.
 *****************************************************************/
#define	ROWUP(ROW)	((ROW) > 25 ? 30 : 25)

void
vsc_change_crt_lines(vscp, smp, async)
	struct vsc_softc *vscp;
	struct screenmode *smp;
	int async;
{
	struct video_state *vsp;
	int win;

	if (ISSET(vscp->sc_flags, VSC_DEVICE_ATTACHED) == 0)
		return;

	if (smp == NULL)
		smp = (vscp->sc_csm->sm_lines <= 25 ? VSC_30_SM : VSC_25_SM);

	for (win = 0; win < NVSCS(vscp); win ++)
	{
		vsp = vscp->vsp[win];
		if (ISSET(vsp->flags, VSC_OPEN))
			init_vc_device(vsp, smp, VSC_INIT_SIGWINCH);
	}

	vsc_set_scmode(vscp, smp, VSC_SETSCM_NULL, async);
}

int
vsc_set_scmode(vscp, smp, force, async)
	struct vsc_softc *vscp;
	struct screenmode *smp;
	int force, async;
{
	sysinfo_info_t info_crt;

	if ((vscp->sc_flags & VSC_HARDWARE_INITIALIZED) == 0)
	{
		vsc_init_hardware(vscp->cvsp);
		vscp->sc_flags |= VSC_HARDWARE_INITIALIZED;
	}

	if (smp == vscp->sc_csm && force == VSC_SETSCM_NULL)
		return 0;

	if (smp == NULL)
		smp = vscp->sc_csm;

	vscp->sc_csm = smp;
	info_crt = lookup_sysinfo(SYSINFO_CRT);
	if ((info_crt & CRT_PEGC_ENABLED) == 0)
	{
		initialize_gdc(vscp, smp->sm_lines);
	}
	else if (cold == 0)
	{
		vm86_screen_mode(vscp, smp, async);
	}
	return 1;
}

/*****************************************************
 * SCREEN SETUP: VM86 BIOS CALL
 *****************************************************/
static void vm86_screen_done __P((void *));

static void
vm86_screen_done(arg)
	void *arg;
{
	struct vsc_softc *vscp = arg;
	int s;

	vsc_start_display(vscp->cvsp);

	s = spltty();
	cursor_switch(vscp->curcursor, vscp->cvsp);
	splx(s);
}

static int
vm86_screen_mode(vscp, smp, async)
	struct vsc_softc *vscp;
	struct screenmode *smp;
	int async;
{
	struct vm86_call vm86_arg;
	struct sigcontext *scp = &vm86_arg.vc_reg;
	int s;

	/* XXX */
	cursor_update(vscp->curcursor, CURSORUPABS);
	cursor_update(vscp->curcursor, CURSOROFF);

	s = spltty();
	vm86_arg.vc_intno = 0x18;
	vm86_arg.vc_flags = (async ? VM86BIOS_ASYNC : 0);

	scp->sc_eax = smp->sm_reg_eax;
	scp->sc_ebx = smp->sm_reg_ebx;
	vm86biosstart(&vm86_arg, NULL, NULL, 0);

	scp->sc_eax = 0x4d00;
	scp->sc_ecx = smp->sm_reg_ecx;
	vm86biosstart(&vm86_arg, vm86_screen_done, vscp, 0);

	splx(s);
	return 0;
}

/*****************************************************
 * SCREEN SETUP: 30 LINES. Copyright by <XXX>
 * TODO:
 * 1) All wait_sync routines could fall into infinite loops,
 *    Fix them.
 * 2) gdc_clock uses the port outside of our io map.
 *****************************************************/
void
master_gdc_cmd(vscp, cmd)
	struct vsc_softc *vscp;
	u_int cmd;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC1ioh;

	while ((bus_space_read_1(iot, ioh, gdc1_stat) & 2) != 0)
		;
	bus_space_write_1(iot, ioh, gdc1_cmd, cmd);
}

void
master_gdc_prm(vscp, pmtr)
	struct vsc_softc *vscp;
	u_int pmtr;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC1ioh;

	while ((bus_space_read_1(iot, ioh, gdc1_stat) & 2) != 0)
		;
	bus_space_write_1(iot, ioh, gdc1_params, pmtr);
}

void
master_gdc_word_prm(vscp, wpmtr)
	struct vsc_softc *vscp;
	u_int wpmtr;
{
	u_int p1, p2;

	p1 = wpmtr & 0xff;
	p2 = wpmtr >> 8;

	master_gdc_prm(vscp, p1);
	master_gdc_prm(vscp, p2);
}

void
master_gdc_fifo_empty(vscp)
	struct vsc_softc *vscp;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC1ioh;

	while ((bus_space_read_1(iot, ioh, gdc1_stat) & 0x04) == 0)
		;
}

void
master_gdc_wait_vsync(vscp)
	struct vsc_softc *vscp;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC1ioh;

	while ((bus_space_read_1(iot, ioh, gdc1_stat) & 0x20) != 0)
		;
	while ((bus_space_read_1(iot, ioh, gdc1_stat) & 0x20) == 0)
		;
}

void
gdc_cmd(vscp, cmd)
	struct vsc_softc *vscp;
	u_int cmd;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC2ioh;

	while ((bus_space_read_1(iot, ioh, gdc2_stat) & 2) != 0)
		;
	bus_space_write_1(iot, ioh, gdc2_cmd, cmd);
}

void
gdc_prm(vscp, pmtr)
	struct vsc_softc *vscp;
	u_int pmtr;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC2ioh;

	while ((bus_space_read_1(iot, ioh, gdc2_stat) & 2) != 0)
		;
	bus_space_write_1(iot, ioh, gdc2_params, pmtr);
}

void
gdc_word_prm(vscp, wpmtr)
	struct vsc_softc *vscp;
	u_int wpmtr;
{
	int p1, p2;

	p1 = wpmtr & 0xff;
	p2 = wpmtr >> 8;

	gdc_prm(vscp, p1);
	gdc_prm(vscp, p2);
}

void
gdc_fifo_empty(vscp)
	struct vsc_softc *vscp;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC2ioh;

	while ((bus_space_read_1(iot, ioh, gdc2_stat) & 0x04) == 0)
		;
}

void
gdc_wait_vsync(vscp)
	struct vsc_softc *vscp;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh = vscp->sc_GDC2ioh;

	while ((bus_space_read_1(iot, ioh, gdc2_stat) & 0x20) != 0)
		;
	while ((bus_space_read_1(iot, ioh, gdc2_stat) & 0x20) == 0)
		;
}

int
check_gdc_clock(vscp)
	struct vsc_softc *vscp;
{

	if ((inb(0x31) & 0x80) == 0) {
		return _5MHZ;
	} else {
		return _2_5MHZ;
	}
}

/* start 30line initialize */
void
initialize_gdc(vscp, mode)
	struct vsc_softc *vscp;
	u_int mode;
{
	int m_mode, s_mode, gdc_clock;

	mode = (mode > 25 ? _30L : _25L);

	gdc_clock = check_gdc_clock(vscp);

	if (mode == 0) {
		m_mode = _25L;
	} else {
		m_mode = _30L;
	}

	s_mode = 2 * mode + gdc_clock;

	master_gdc_cmd(vscp, _GDC_RESET);
	master_gdc_cmd(vscp, _GDC_MASTER);
	gdc_cmd(vscp, _GDC_RESET);
	gdc_cmd(vscp, _GDC_SLAVE);

	/* GDC Master */
	master_gdc_cmd(vscp, _GDC_SYNC);
	master_gdc_prm(vscp, 0x04);	/* flush less *//* text & graph */
	master_gdc_prm(vscp, master_param[m_mode][GDC_CR]);
	master_gdc_word_prm(vscp, ((master_param[m_mode][GDC_HFP] << 10)
			     + (master_param[m_mode][GDC_VS] << 5)
			     + master_param[m_mode][GDC_HS]));
	master_gdc_prm(vscp, master_param[m_mode][GDC_HBP]);
	master_gdc_prm(vscp, master_param[m_mode][GDC_VFP]);
	master_gdc_word_prm(vscp, ((master_param[m_mode][GDC_VBP] << 10)
			     + (master_param[m_mode][GDC_LF])));
	master_gdc_fifo_empty(vscp);
	master_gdc_cmd(vscp, _GDC_PITCH);
	master_gdc_prm(vscp, MasterPCH);
	master_gdc_fifo_empty(vscp);

	/* GDC slave */
	gdc_cmd(vscp, _GDC_SYNC);
	gdc_prm(vscp, 0x06);
	gdc_prm(vscp, slave_param[s_mode][GDC_CR]);
	gdc_word_prm(vscp, (slave_param[s_mode][GDC_HFP] << 10)
		     + (slave_param[s_mode][GDC_VS] << 5)
		     + (slave_param[s_mode][GDC_HS]));
	gdc_prm(vscp, slave_param[s_mode][GDC_HBP]);
	gdc_prm(vscp, slave_param[s_mode][GDC_VFP]);
	gdc_word_prm(vscp, (slave_param[s_mode][GDC_VBP] << 10)
		     + (slave_param[s_mode][GDC_LF]));
	gdc_fifo_empty(vscp);
	gdc_cmd(vscp, _GDC_PITCH);
	gdc_prm(vscp, SlavePCH[gdc_clock]);
	gdc_fifo_empty(vscp);

	/* set Master GDC scroll param */
	master_gdc_wait_vsync(vscp);
	master_gdc_wait_vsync(vscp);
	master_gdc_wait_vsync(vscp);
	master_gdc_cmd(vscp, _GDC_SCROLL);
	master_gdc_word_prm(vscp, 0);
	master_gdc_word_prm(vscp, (master_param[m_mode][GDC_LF] << 4) | 0x0000);
	master_gdc_fifo_empty(vscp);

	/* set Slave GDC scroll param */
	gdc_wait_vsync(vscp);
	gdc_cmd(vscp, _GDC_SCROLL);
	gdc_word_prm(vscp, 0);
	if (gdc_clock == _5MHZ) {
		gdc_word_prm(vscp, (SlaveScrlLF[mode] << 4) | 0x4000);
	} else {
		gdc_word_prm(vscp, SlaveScrlLF[mode] << 4);
	}
	gdc_fifo_empty(vscp);

	gdc_word_prm(vscp, 0);
	if (gdc_clock == _5MHZ) {
		gdc_word_prm(vscp, (SlaveScrlLF[mode] << 4) | 0x4000);
	} else {
		gdc_word_prm(vscp, SlaveScrlLF[mode] << 4);
	}
	gdc_fifo_empty(vscp);

	/* sync start */
	gdc_cmd(vscp, _GDC_STOP);

	gdc_wait_vsync(vscp);
	gdc_wait_vsync(vscp);
	gdc_wait_vsync(vscp);

	master_gdc_cmd(vscp, _GDC_START);
}

/************************************************************************
 * Draw region
 ************************************************************************/
#define	VSC_PEGC_X_SIZE	640
#define	VSC_PEGC_Y_SIZE	480

void
vsc_pegc_draw_screen(vscp, pp, pdp, pldp)
	struct vsc_softc *vscp;
	struct pegc_draw_position *pp;
	u_int8_t *pdp;
	struct pegc_pallet_data *pldp;
{
	int x, y, xsz, ysz;
	int cx, cy, pos, offset;
	u_int8_t *vmem = PEGC_WINDOW_OFFSET(vscp);

	x = y = xsz = ysz = 0;
	if (pp != NULL)
	{
		xsz = pp->pd_xsz;
		ysz = pp->pd_ysz;
		switch (pp->pd_flags & PEGC_DRAW_POSITION_MASK)	
		{
		case PEGC_DRAW_POSITION_UL:
			x = y = 0;
			break;
		case PEGC_DRAW_POSITION_UR:
			y = 0;
			x = VSC_PEGC_X_SIZE - xsz;
			break;
		case PEGC_DRAW_POSITION_DL:
			y = VSC_PEGC_Y_SIZE - ysz;
			x = 0;
			break;
		case PEGC_DRAW_POSITION_DR:
			y = VSC_PEGC_Y_SIZE - ysz;
			x = VSC_PEGC_X_SIZE - xsz;
			break;
		case PEGC_DRAW_POSITION_CENTER:
			y = (VSC_PEGC_Y_SIZE - ysz) / 2;
			x = (VSC_PEGC_X_SIZE - xsz) / 2;
			break;
		case PEGC_DRAW_POSITION_FIXED:
			y = pp->pd_y;
			x = pp->pd_x;
			break;
		}
	}
		
	vsc_set_pallet(vscp, 0, 0, 0, 0);	/* 0 slot */
	if (pldp != NULL)
	{
		for (pos = 0; pldp->pd_num >= 0; pos ++, pldp ++)
		{
			vsc_set_pallet(vscp, pldp->pd_num + 1,
			       pldp->pd_red, pldp->pd_green, pldp->pd_blue);
		}
	}
	
	for (pos = 0; pos < VSC_PEGC_X_SIZE * VSC_PEGC_Y_SIZE; pos ++)
	{
		if ((pos % PEGC_WINDOW_SIZE) == 0)
		{
			vsc_pegc_write_bank(vscp,
					    (pos / PEGC_WINDOW_SIZE) * 2);
		}

		cx = pos % VSC_PEGC_X_SIZE;
		cy = pos / VSC_PEGC_X_SIZE;	
		if (cx >= x && cx < x + xsz && cy >= y && cy < y + ysz)
		{
			offset = (cy - y) * xsz + (cx - x);
			vmem[pos % PEGC_WINDOW_SIZE] = pdp[offset] + 1;
		}
		else
			vmem[pos % PEGC_WINDOW_SIZE] = 0;
	}
}
