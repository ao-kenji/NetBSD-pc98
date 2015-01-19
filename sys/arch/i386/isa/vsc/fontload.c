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

#include "opt_kbd.h"
#include "opt_vsc.h"

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

#include <i386/isa/vsc/config.h>
#include <i386/isa/vsc/vsc.h>
#include <i386/isa/vsc/vt100.h>
#include <i386/isa/vsc/fontload.h>
#include <i386/isa/vsc/video.h>
#include <i386/isa/vsc/kanji.h>

static int vsc_add_gfont __P((struct vsc_softc *, FontCode, u_short *));
static FontCode get_gaiji_slot __P((struct vsc_softc *));
static int free_gaiji_slot __P((struct vsc_softc *, FontCode));
static int enter_code_tbl __P((struct vsc_softc *, struct FontBlockList *, FontCode, FontCode));
static int check_range __P((FontCode));
static int modify_swap_code __P((struct vsc_softc *, struct FontList *, struct FontBlockList *));
static void load_periodic_jis83 __P((void *));

/* def */
#define	KANJI_TABLE_SIZE	(sizeof(kanjiset) / sizeof(struct kanji))
#define	JISCONV_TABLE_SIZE	(sizeof(jis8tbl) / sizeof(struct ConvTbl))

/* extra fonts */
static vsc_wchar_t gchar[] = {
	'[', 0xfc, ']', '^', 0x20,
	0xea, 0x8e, 0x09, 0x0c, 0x0d, 0x0a, 0xdf, 0xf0,
	0x9c, 0x0b, 0x9b, 0x99, 0x98, 0x9a, 0x8f, 0x94,
	0x9d, 0x95, 0x9e, 0x5f, 0x93, 0x92, 0x90, 0x91,
	0x96, 0xe0, 0xe1, 0xe2, 0xe3, 0x9f, 0xa5, 0x20
};

int
SearchCodeSwap(c, fb)
	FontCode *c;
	struct FontBlockList *fb;
{
	int low, high, i;
	struct ConvTbl *fc = fb->tbl;

	if ((high = fb->cc - 1) < 0)
		return -1;

	for (low = 0; ; )
	{
		i = (low + high) / 2;
		if (fc[i].org == (*c))
		{
			(*c) = fc[i].alt;
			return i;
		}

		if (low >= high)
			return -1;

		if (fc[i].org < (*c))
			low = i + 1;
		else
			high = i - 1;
	};
}

static FontCode
get_gaiji_slot(vscp)
	struct vsc_softc *vscp;
{
	struct Gaiji *gp = vscp->sc_gaijip;
	FontCode code;

again:
	if (gp->cc >= GAIJISIZE)
		return 0;

	for (code = gp->hint; gp->gaiji[code] && code < GAIJISIZE; code++)
		;

	if (code < GAIJISIZE)
	{
		gp->gaiji[code] = 1;
		gp->cc++;
		gp->hint = code + 1;
	}
	else
	{
		gp->hint = 0;
		goto again;
	}

	if (code >= USERGAIJI1MAX - USERGAIJIBASE)
		code = code + USERGAIJI2BASE - (USERGAIJI1MAX - USERGAIJIBASE);
	else
		code = code + USERGAIJIBASE;

	return code;
}

static int
free_gaiji_slot(vscp, code)
	struct vsc_softc *vscp;
	FontCode code;
{
	struct Gaiji *gp = vscp->sc_gaijip;

	if (code < USERGAIJIBASE || code >= USERGAIJIMAX)
		return -1;
	if (code >= USERGAIJI1MAX && code < USERGAIJI2BASE)
		return -1;
	if (code >= USERGAIJI2BASE)
		code = code - USERGAIJI2BASE + (USERGAIJI1MAX - USERGAIJIBASE);
	else
		code = code - USERGAIJIBASE;

	if (gp->gaiji[code])
	{
		gp->gaiji[code] = 0;
		gp->cc --;
	}

	gp->hint = code;

	return 0;
}

static int
enter_code_tbl(vscp, fb, org, alt)
	struct vsc_softc *vscp;
	struct FontBlockList *fb;
	FontCode org;
	FontCode alt;
{
	FontCode code = org;
	int i, s;
	struct ConvTbl *tbl = fb->tbl;

	i = SearchCodeSwap(&code, fb);
	if (i >= 0)
	{
		s = spltty();
		tbl[i].alt = alt;
		splx(s);
		return 0;
	}

	/* new element */
	if (fb->cc >= fb->size)
		return ENOSPC;

	for (i = 0; tbl[i].org < org && i < fb->cc; i++)
		;

	/* overlap copy */
	s = spltty();
	bcopy((char *) &tbl[i], (char *) &tbl[i + 1],
	      sizeof(struct ConvTbl) * (fb->size - i - 1));
	tbl[i].org = org;
	tbl[i].alt = alt;
	fb->cc ++;
	splx(s);
	return 0;
}

static int
check_range(code)
	FontCode code;
{

	if ((code & 0xff) < 0x20 || (code & 0xff) >= 0x80 ||
	    (code & 0xff00) < 0x2000 || (code & 0xff00) >= 0x8000)
		return EINVAL;
	return 0;
}

static int
modify_swap_code(vscp, fl, fb)
	struct vsc_softc *vscp;
	struct FontList *fl;
	struct FontBlockList *fb;
{
	FontCode code = fl->code;
	int s, error = 0;

	if (fl->type == FONTASCII)
	{
		if (fl->code >= MAXASCIICODE)
		{
			error = EINVAL;
			goto done;
		}

		if (vscp->sc_asciifp[fl->code])
		{
			free_gaiji_slot(vscp, vscp->sc_asciifp[fl->code]);
			vscp->sc_asciifp[fl->code] = 0;
		}

		if ((code = get_gaiji_slot(vscp)) == 0)
		{
			error = ENOSPC;
			goto done;
		}

		vsc_add_gfont(vscp, code, fl->font);
		vscp->sc_asciifp[fl->code] = code;
		fl->result = code;
		goto done;
	}

	if ((error = check_range(code)) != 0)
		goto done;

	if (SearchCodeSwap(&code, fb) >= 0)
		free_gaiji_slot(vscp, code);

	switch (fl->type)
	{
	case FONTROM:
		if ((error = check_range(fl->romcode)) != 0)
			break;
		error = enter_code_tbl(vscp, fb, fl->code, fl->romcode);
		break;

	case FONTGAIJI:
		if ((code = get_gaiji_slot(vscp)) == 0)
		{
			error = ENOSPC;
			break;
		}

		if ((error = enter_code_tbl(vscp, fb, fl->code, code)) != 0)
			break;
		vsc_add_gfont(vscp, code, fl->font);
		fl->result = code;
		break;

	default:
		error = EINVAL;
		break;
	}

done:
	s = spltty();
	CLR(vscp->sc_gaijip->stat, GAIJIBUSY);
	wakeup(&vscp->sc_gaijip->stat);
	splx(s);
	return error;
}

static void
load_periodic_jis83(arg)
	void *arg;
{
	struct vsc_softc *vscp = arg;
	struct Gaiji *gp = vscp->sc_gaijip;
	int s;

	if (fontjis8list[vscp->sc_j83ind].code == 0)
		return;

	s = spltty();
	if (ISSET(gp->stat, GAIJIBUSY) == 0)
	{
		SET(gp->stat, GAIJIBUSY);
		splx(s);

		modify_swap_code(vscp, &fontjis8list[vscp->sc_j83ind],
				 vscp->sc_wcharfb);
		vscp->sc_j83ind ++;
	}
	else
		splx(s);

	timeout(load_periodic_jis83, vscp, hz / 10);
}

/* user interface */
int
enter_fonts(vscp, fl)
	struct vsc_softc *vscp;
	struct FontList *fl;
{
	struct FontBlockList *fb = vscp->sc_wcharfb;
	struct Gaiji *gp = vscp->sc_gaijip;
	int error, s;

	if (fb == NULL)
		return EINVAL;

	s = spltty();
	while (ISSET(gp->stat, GAIJIBUSY))
	{
		error = tsleep(&gp->stat, PWAIT | PCATCH, "vsc_gaiji", 0);
		if (error != 0)
		{
			splx(s);
			return error;
		}
	}
	SET(gp->stat, GAIJIBUSY);
	splx(s);

	return modify_swap_code(vscp, fl, fb);
}

int
clear_all_fonts(vscp)
	struct vsc_softc *vscp;
{
	struct FontBlockList *fb = vscp->sc_wcharfb;
	int s;

	if (fb == NULL)
		return EINVAL;

	s = splhigh();
	untimeout(load_periodic_jis83, vscp);
	init_fontload_system(vscp);
	splx(s);

	return 0;
}

void
init_fontload_system(vscp)
	struct vsc_softc *vscp;
{
	struct FontBlockList *fb;
	struct Gaiji *gp;

	vscp->sc_gchar = gchar;
	if ((gp = vscp->sc_gaijip) == NULL)
	{
		gp = malloc(sizeof(struct Gaiji), M_DEVBUF, M_WAITOK);
		if (gp == NULL)
			panic("vc: no memory for gaiji");
		vscp->sc_gaijip = gp;
	}

	if ((fb = vscp->sc_wcharfb) == NULL)
	{
		fb = malloc(sizeof(struct FontBlockList), M_DEVBUF, M_WAITOK);
		if (fb == NULL)
			panic("vc: no memory for jis8 fb");
		memset(fb, 0, sizeof(struct FontBlockList));
		fb->size = SWAPCODE_TBL_SIZE;
		vscp->sc_wcharfb = fb;
	}

	if (fb->tbl == NULL)
	{
		fb->tbl = malloc(fb->size * sizeof(struct ConvTbl),
				 M_DEVBUF, M_WAITOK);
		if (fb->tbl == NULL)
			panic("vc: no memory for fonts");
	}

	if (vscp->sc_asciifp == NULL)
	{
		vscp->sc_asciifp = malloc(MAXASCIICODE * sizeof(FontCode),
					  M_DEVBUF, M_WAITOK);
		if (vscp->sc_asciifp == NULL)
			panic("vc: no memory for ascii fonts");
	}

	fb->cc = 0;
	gp->stat = 0;
	gp->cc = 0;
	gp->hint = 0;
	memset(fb->tbl, 0, fb->size * sizeof(struct ConvTbl));
	memset(gp->gaiji, 0, GAIJISIZE * sizeof(u_char));
	memset(vscp->sc_asciifp, 0, MAXASCIICODE * sizeof(FontCode));
}

int
load_jis83_fonts(vscp)
	struct vsc_softc *vscp;
{
	struct FontBlockList *fb = vscp->sc_wcharfb;
	int s, offset;

	if (fb == NULL || fb->cc != 0 ||
	    ISSET(vscp->sc_gaijip->stat, JIS83LOAD) != 0)
		return EINVAL;

	offset = JISCONV_TABLE_SIZE;
	if (offset > fb->size)
		offset = fb->size;
	if (offset == 0)
		return ENOENT;

	s = spltty();
	fb->cc = offset;
	bcopy(jis8tbl, fb->tbl, offset * sizeof(struct ConvTbl));
	vscp->sc_j83ind = 0;
	SET(vscp->sc_gaijip->stat, JIS83LOAD);
	splx(s);

	timeout(load_periodic_jis83, vscp, hz / 10);
	return 0;
}

int
set_kanjimode(vsp, mode)
	struct video_state *vsp;
	int mode;
{
	int i;
	wf_t *wf;

	if (mode < 0 || mode >= KANJI_TABLE_SIZE)
		return EINVAL;

	vsp->kanjicode = mode;

	for (i = 0; i < NSWIN; i++)
	{
		wf = vsp->lwf[i];
		wf->multibyte = 0;
		InitISO2022(&kanjiset[mode].iso, wf);
	}

	return 0;
}

static int
vsc_add_gfont(vscp, code, pt)
	struct vsc_softc *vscp;
	FontCode code;
	u_short *pt;
{
	bus_space_tag_t iot = vscp->sc_iot;
	bus_space_handle_t ioh1 = vscp->sc_GDC1ioh;
	register bus_space_handle_t ioh2 = vscp->sc_GDC2ioh;
	int i;

	vsc_vsync_wait(vscp);
	bus_space_write_1(iot, ioh1, gdc1_mode , 0xb);
	bus_space_write_1(iot, ioh2, cg_data1, (u_char) code);
	bus_space_write_1(iot, ioh2, cg_data2, ((code & 0xff00) >> 8) - 0x20);
	for (i = 0; i < 16; i++)
	{
		bus_space_write_1(iot, ioh2, cg_line, i | 0x20);
		bus_space_write_1(iot, ioh2, cg_pattern, (pt[i] & 0xff00) >> 8);
		bus_space_write_1(iot, ioh2, cg_line, i);
		bus_space_write_1(iot, ioh2, cg_pattern, pt[i] & 0xff);
	}

	bus_space_write_1(iot, ioh1, gdc1_mode, 0xa);
	return 1;
}
