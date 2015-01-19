/*	$NecBSD: vscdev.c,v 1.51.2.6 1999/09/03 06:31:40 honda Exp $	*/
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

#include <i386/isa/vsc/config.h>

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/cons.h>

#include <machine/pc/display.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <i386/isa/timerreg.h>
#include <i386/isa/pc98spec.h>

#include <i386/isa/vsc/vsc.h>
#include <i386/isa/vsc/video.h>
#include <i386/isa/vsc/savervar.h>
#include <i386/isa/vsc/fontload.h>
#include <i386/isa/vsc/vsc_sm.h>

/* global data */
struct vsc_softc *vsc_gp;

struct vc_mem {
	vram_t c[MAX_ROW * MAX_COL];
	vram_t a[MAX_ROW * MAX_COL];
};

struct cons_mem {
	struct video_state vs;
	wf_t lwf[NSWIN];
	u_char tabs0[MAX_COL + 4];
	u_char tabs1[MAX_COL + 4];
};

/* cursor & screen saver default softc */
struct cursor_softc cursortab;
#ifdef	SCREEN_SAVER
struct saver_softc savertab;
#endif	/* SCREEN_SAVER */

/* the following memory statically allocated for the cons dev */
static struct cons_mem startup_cons;
static struct vsc_softc startup_vsc;

static int vscmatch __P((struct device *, struct cfdata *, void *));
static void vscattach __P((struct device *, struct device *, void *));
static int vscsubmatch __P((struct device *, struct cfdata *, void *));
static void vcattach __P((struct device *, struct device *, void *));
static int vsc_control __P((void *, u_int, u_int *));
static int vsc_systmmsg __P((struct device *, systm_event_t));
static void issue_sigwinch __P((struct tty *, wf_t *));

struct cfattach vsc_ca = {
	sizeof(struct device), vscmatch, vscattach
};

extern struct cfdriver vsc_cd;

struct cfattach vc_ca = {
	sizeof(struct vc_softc), vscsubmatch, vcattach
};

extern struct cfdriver vc_cd;

/* static declare */
static void setup_Ctab __P((vram_t **, vram_t *, vram_t **, vram_t *));
static struct video_state *attach_vc_device __P((struct vsc_softc *, int));
static int alloc_vc_pmem __P((struct video_state *));
static void
init_win_frame __P((struct window *, struct window *, wf_t *, struct video_state *));
static struct video_state *alloc_vc_device __P((struct vsc_softc *, int));

/*************************************************
 * NetBSD Attach Probe
 ************************************************/
static int
vscmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = (struct isa_attach_args *) aux;
	struct vsc_softc *vscp;

	vscp = attach_vsc_consdev();
	if (ISSET(vscp->sc_flags, VSC_DEVICE_ATTACHED))
		return 0;

	vscp->sc_iot = ia->ia_iot;
	vscp->sc_memt = ia->ia_memt;
	return 1;
}

struct vsc_attach_args {
	u_int id;
};

static void
vscattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = (struct isa_attach_args *) aux;
	struct vsc_softc *vscp;
	struct vsc_attach_args va;
	sysinfo_info_t info_crt;
	struct cfdata *pcf;
	u_char *s;
	paddr_t start;
	int i, error;

	printf("\n");

	vscp = attach_vsc_consdev();
	info_crt = lookup_sysinfo(SYSINFO_CRT);

	vscp->sc_pscid = -1;
	systmmsg_bind((struct device *)vscp, vsc_systmmsg);

	vscp->sc_iot = ia->ia_iot;
	vscp->sc_memt = ia->ia_memt;
	
	/* XXX:
	 * Console device could not use memh 
	 * becuase our vm system not running at the first stage of boot.
	 * Here we should register all memory regions to protect them.
	 */
	start = CBUSHOLE_START;
	error = bus_space_map(vscp->sc_memt, start + CBUSHOLE_TRAM_OFFSET,
		              CBUSHOLE_TRAM_SIZE, 0, &vscp->sc_tramh);
	if (error != 0)
		goto out;
	error = bus_space_map(vscp->sc_memt, start + CBUSHOLE_TARAM_OFFSET,
		             CBUSHOLE_TRAM_SIZE, 0, &vscp->sc_taramh);
	if (error != 0)
		goto out;
	error = bus_space_map(vscp->sc_memt, start + CBUSHOLE_GRAM0_OFFSET,
		             CBUSHOLE_GRAM_SIZE * 2, 0, &vscp->sc_gram0h);
	if (error != 0)
		goto out;
	error = bus_space_subregion(vscp->sc_memt, vscp->sc_gram0h,
				    CBUSHOLE_GRAM_SIZE, CBUSHOLE_GRAM_SIZE,
				    &vscp->sc_gram1h);	
	if (error != 0)
		goto out;
	vscp->sc_pramh = vscp->sc_gram0h;

	error = bus_space_map(vscp->sc_memt, start + CBUSHOLE_GRAM2_OFFSET,
		             CBUSHOLE_GRAM_SIZE, 0, &vscp->sc_gram2h);
	if (error != 0)
		goto out;
	error = bus_space_map(vscp->sc_memt, start + CBUSHOLE_GRAM3_OFFSET,
		             CBUSHOLE_GRAM_SIZE, 0, &vscp->sc_gram3h);
	if (error != 0)
		goto out;
	vscp->sc_pregh = vscp->sc_gram3h;

	/* 
	 * Initialize each virtual console.
	 */
	for (i = 0; i < DEFAULT_MAX_VSC; i++)
	{
		va.id = i;
		if ((pcf = config_search(vscsubmatch, self, &va)) != NULL)
			config_attach(self, pcf, &va, NULL);
	}

	SET(vscp->sc_flags, VSC_DEVICE_ATTACHED);

	init_fontload_system(vscp);

	printf("%s: ", self->dv_xname);
	s = (info_crt & CRT_PEGC_ENABLED) ? "pegc" : "egc";
	printf("graphic mode(%s). ", s);

	if (load_jis83_fonts(vscp) == 0)
		printf("loading fonts(jis83+jis90). ");

#ifdef	SCREEN_SAVER
	saver_update(vscp->cursaver, SAVER_START);
	printf("screen saver(%d sec). ", vscp->cursaver->saver_time);
#endif	/* SCREEN_SAVER */
	printf("\n");

	ExecSwitchScreen(vscp->consvsp, 1);	/* force screen switch now! */
	return;

out:
	panic("%s: can not map VIDEO ram", vscp->sc_dev.dv_xname);
}

static int
vscsubmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct vsc_attach_args *va = (struct vsc_attach_args *) aux;

	if (cf->cf_loc[VSCCF_DRIVE] != VSCCF_DRIVE_DEFAULT &&
	    cf->cf_loc[VSCCF_DRIVE] != va->id)
		return 0;
	return 1;
}

static void
vcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cfdata *cf = self->dv_cfdata;
	struct vsc_softc *vscp;
	struct vc_softc *vcsc = (struct vc_softc *) self;
	struct vsc_attach_args *va = (struct vsc_attach_args *) aux;
	struct video_state *vsp;
	wf_t *wf;
	u_int len;

	/*
	 * Console device which was already allocated in pccninit() is
	 * attached at the first slot.
	 */
	vscp = attach_vsc_consdev();
	if (va->id == 0)
		vsp = vscp->consvsp;
	else
		vsp = attach_vc_device(vscp, 0);

	if (alloc_vc_pmem(vsp))
		panic("vsc: short of virtual screen memory\n");

	/* assign */
	vcsc->vsp = vsp;

	len = (u_int) cf->cf_loc[VSCCF_IOSIZ];
	if (len > 0x10000 || len < sizeof(struct ring_mem) * 3)
		len = 0;
	vsp->ringsz = len;

	wf = vsp->lwf[MWINID];
	printf(" win id %d (size %dx%d+%3d) attached.",
	       vsp->id, wf->n.col, wf->n.row, len / sizeof(struct ring_mem));

	/* cons */
	if (ISSET(vsp->flags, VSC_I_AM_CONS))
		printf(" <console>");
	if (ISSET(cf->cf_flags, VSC_GRAPHIC_ON))
		SET(vsp->flags, VSC_GRAPH);
	if (ISSET(cf->cf_flags, VSC_DEC_AWM))
		SET(wf->decmode, DECAWM);
	if (ISSET(cf->cf_flags, VSC_DEC_INS))
		SET(wf->decmode, DECINS);
	if (ISSET(cf->cf_flags, VSC_WIN_SPLIT))
		SET(vsp->flags, VSC_SPLIT);
	if (ISSET(cf->cf_flags, VSC_KEY_LNM))
		SET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_KEYLNM);

	printf(" flags %b", cf->cf_flags, VSC_FLAGS_BITS);

	/* kanji mode */
	vsp->startcode = VSC_KANJI_MODE(cf->cf_flags);
	set_kanjimode(vsp, vsp->startcode);
	printf(" %s", kanjiset[vsp->kanjicode].name);
	printf(".\n");

	/* graphic on */
	CLR(vsp->flags, VSC_GRAPH);
	vsc_start_display(vsp);

	/* status window open */
	if (ISSET(cf->cf_flags, VSC_SWIN_OPEN))
	{
		SET(vsp->flags, VSC_HAS_STATUS);
		open_swin(0, vsp->lwf[SWINID]);
	}
}

/*************************************************
 * Attach
 ************************************************/
static void
setup_Ctab(Ctab, caddr, Atab, aaddr)
	vram_t **Ctab;
	vram_t *caddr;
	vram_t **Atab;
	vram_t *aaddr;
{
	int i;

	for (i = 0; i < MAX_ROW; i++)
	{
		Ctab[i] = caddr + MAX_COL * i;
		Atab[i] = aaddr + MAX_COL * i;
	}
}

static int
alloc_vc_pmem(vsp)
	struct video_state *vsp;
{
	struct vc_mem *cp;
	struct vsc_softc *vscp = vsp->vs_vscp;
	u_int attr, size = sizeof(struct vc_mem);
	int s;

	if ((cp = malloc(size, M_DEVBUF, M_NOWAIT)) == NULL)
		return ENOSPC;
	memset((u_char *) cp, 0, size);
	s = spltty();
	setup_Ctab(vsp->vscCtab, cp->c, vsp->vscAtab, cp->a);
	splx(s);

	fillw(' ', vsp->vscCtab[0], MAX_ROW * MAX_COL);
	attr = ISSET(vsp->flags, VSC_RCRT) ? vscp->rattr.attr: vscp->attr.attr;
	fillw((vram_t) attr, vsp->vscAtab[0], MAX_ROW * MAX_COL);
	return 0;
}

static struct video_state *
alloc_vc_device(vscp, flags)
	struct vsc_softc *vscp;
	int flags;
{
	struct video_state *vsp;
	struct cons_mem *cp;
	int i;

	if (ISSET(flags, VSC_I_AM_CONS))
		cp = &startup_cons;
	else
		cp = (struct cons_mem *) malloc(sizeof(startup_cons),
						M_DEVBUF, M_WAITOK);
	if (cp == NULL)
		panic("vsc: no memory for video");

	vsp = &cp->vs;
	memset(cp, 0, sizeof(startup_cons));
	vsp->vs_vscp = vscp;
	setup_Ctab(vsp->vscCtab, TEXT_RAM_ADDR(vscp),
		   vsp->vscAtab, ATTR_RAM_ADDR(vscp));

	for (i = 0; i < NSWIN; i++)
	{
		vsp->lwf[i] = &cp->lwf[i];
		vsp->lwf[i]->vsp = vsp;
		attach_vcrt_mem(vsp->lwf[i]);
	}

	vsp->lwf[MWINID]->tabs = cp->tabs0;
	SET(vsp->lwf[MWINID]->flags, OPENF);
	vsp->lwf[MWINID]->rsz = 0;	/* MAXSIZE */
	vsp->lwf[VWINID]->tabs = cp->tabs0;
	vsp->lwf[VWINID]->rsz = VSC_ROW;
	vsp->lwf[SWINID]->tabs = cp->tabs1;
	vsp->lwf[SWINID]->rsz = STATUS_ROW;
	vsp->cwf = vsp->lwf[MWINID];
	SET(vsp->flags, flags);

	return vsp;
}

/* special attach for the pc cons dev */
struct vsc_softc *
attach_vsc_consdev(void)
{
	struct video_state *vsp;
	struct vsc_softc *vscp;
	struct kbd_connect_args ka;
	int i;

	if (vsc_gp != NULL)
		return vsc_gp;

	vscp = vsc_gp = &startup_vsc;
	vscp->sc_crtbase = (u_int16_t *) ISA_HOLE_VADDR(CBUSHOLE_START);

	vscp->sc_iot = I386_BUS_SPACE_IO;		/* XXX */
	vscp->sc_memt = I386_BUS_SPACE_MEM;		/* XXX */

	bus_space_map(vscp->sc_iot, IO_GDC1, 0, 0, &vscp->sc_GDC1ioh);
	bus_space_map_load(vscp->sc_iot, vscp->sc_GDC1ioh, 
			   8, BUS_SPACE_IAT_2, 0);

	bus_space_map(vscp->sc_iot, IO_GDC2, 0, 0, &vscp->sc_GDC2ioh);
	bus_space_map_load(vscp->sc_iot, vscp->sc_GDC2ioh, 
			   16, BUS_SPACE_IAT_1, 0);

	setup_Ctab(vscp->scCtab, TEXT_RAM_ADDR(vscp),
		   vscp->scAtab, ATTR_RAM_ADDR(vscp));
	for (i = 0; i < MAX_ROW; i++)
		vscp->coltab[i] = MAX_COL * i;

	/* setup attribute */
	vscp->rattr.attr = VSC_ATTRIBRO;
	vscp->rattr.sattr = VSC_SATTRIBRO;
	vscp->rattr.vattr = VSC_VATTRIBRO;
	vscp->rattr.sysattr = VSC_SYS_ATTRIBRO;

	vscp->attr.attr = VSC_ATTRIBO;
	vscp->attr.sattr = VSC_SATTRIBO;
	vscp->attr.vattr = VSC_VATTRIBO;
	vscp->attr.sysattr = VSC_SYS_ATTRIBO;

	/* alloc top virtual console */
	vsp = vscp->consvsp = attach_vc_device(vscp, VSC_I_AM_CONS);
	vscp->cvsp = vsp;

	/* attach kbd */
	vscp->sc_ks = vsc_input_service_select(VSC_INPUT_SERVICE_DEFAULT);
	ka.ka_control = vsc_control;
	ka.ka_control_arg = vscp;
	vscp->sc_kscp = vscp->sc_ks->ks_connect_establish(&ka);
	vscp->sc_ks = ka.ka_ks;
	vscp->sc_ks->ks_switch(vscp->sc_kscp, vsp->id, &vsp->vs_vi, NULL);

	/* attach physical memory */
	for (i = 0; i < NSWIN; i++)
		attach_crt_mem(vsp->lwf[i]);

	/* init hardware */
	cursor_init(&cursortab, vsp);
#ifdef	SCREEN_SAVER
	saver_init(&savertab, vsp);
	vscp->sc_ks->ks_connect_saver(vscp->sc_kscp, &savertab); 
#endif	/* SCREEN_SAVER */
	vsc_set_scmode(vscp, vsp->scmode, VSC_SETSCM_FORCE, VSC_SETSCM_NULL);
	vsc_start_display(vsp);

	return vscp;
}

/* top function to attach virtual screens */
static struct video_state *
attach_vc_device(vscp, flags)
	struct vsc_softc *vscp;
	int flags;
{
	struct video_state *vsp;
	sysinfo_info_t info_crt;
#ifdef	LINEIS25
	int s, row = 25;
#else	/* LINEIS25 */
	int s, row = 30;
#endif	/* LINEIS25 */

	info_crt = lookup_sysinfo(SYSINFO_CRT);
	if (info_crt & CRT_PEGC_ENABLED)
		row = 30;
	if (info_crt & CRT_FORCE_25L)
		row = 25;

	s = spltty();

#ifdef	REVERSE_CRT
	SET(flags, VSC_RCRT);
#endif	/* REVERSE_CRT */

	vsp = alloc_vc_device(vscp, flags | VSC_CURSOR | VSC_SAVER);
	init_vc_device(vsp, row > 25 ? VSC_30_SM : VSC_25_SM, VSC_INIT_KANJI);

	vscp->vsp[vscp->nvscs] = vsp;
	vsp->id = (vscp->nvscs ++);
	splx(s);
	return vsp;
}

/*******************************************************
 * Initialize
 ******************************************************/
static void
init_win_frame(wo, wn, wf, vsp)
	struct window *wo;
	struct window *wn;
	wf_t *wf;
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;
	int i;

	wf->n = *wn;
	wf->o = *wo;
	wf->rsz = wf->n.row - wf->o.row;

	/* cursor pos */
	wf->r.col = (wf->r.col < wf->o.col ? wf->o.col :
		     (wf->r.col >= wf->n.col ? wf->n.col - 1 : wf->r.col));
	wf->r.row = (wf->r.row < wf->o.row ? wf->o.row :
		     (wf->r.row >= wf->n.row ? wf->n.row - 1 : wf->r.row));

	/* scroll region */
	wf->sc.lw = wf->o.row + 1;
	wf->sc.hg = wf->n.row;
	wf->sc.sz = wf->sc.hg - wf->sc.lw + 1;

	/* vt100 */
	wf->nparm = 0;
	wf->parm[0] = 0;
	wf->state = 0;
	wf->multibyte = 0;

	wf->attrp = ISSET(vsp->flags, VSC_RCRT) ? &vscp->rattr : &vscp->attr;
	wf->attrflags = 0;
	wf->fgc = wf->bgc = 0;
	set_attribute(wf);

	/* flags */
#ifdef	VSC_USE_AWM
	wf->decmode = DECAWM;
#else	/* VSC_USE_AWM */
	wf->decmode = 0;
#endif	/* VSC_USE_AWM */

	/* tab setup */
	wf->tabs[0] = 0;
	for (i = 1; i < wf->n.col; i++)
		wf->tabs[i] = (i % DEFAULT_TAB_STEP) ? 0 : 1;

	/* kanji */
	InitISO2022(&kanjiset[vsp->kanjicode].iso, wf);
	SaveCursor(wf);
}

void
init_vc_device(vsp, smp, flags)
	struct video_state *vsp;
	struct screenmode *smp;
	u_int flags;
{
	int i, row;
	struct window wo, wn;
	wf_t *wf;
	int s = spltty();

	vsp->scmode = smp;
	row = smp->sm_lines;

	wn.row = row;
	wn.col = NORMAL_COL;

	if (flags & VSC_INIT_KANJI)
		vsp->kanjicode = vsp->startcode;

	memset(&wo, 0, sizeof(wo));
	for (i = NSWIN - 1; i >= 0; i--)
	{
		wf = vsp->lwf[i];
		wf->subid = i;

		if (wn.row < 1)
			panic("vc: fatal error\n");
		if (i == MWINID)
			wf->rsz = wn.row;

		wo.row = wn.row - wf->rsz;
		init_win_frame(&wo, &wn, wf, vsp);

		if (ISSET(wf->flags, OPENF))
			wn.row -= wf->rsz;
	}

	if (flags & VSC_INIT_SIGWINCH)
		issue_sigwinch(vsp->tp, vsp->cwf);

	splx(s);
}

/*************************************************
 * Misc util
 ************************************************/
static void
issue_sigwinch(tp, wf)
	struct tty *tp;
	wf_t *wf;
{

	if (tp == NULL)
		return;

	tp->t_winsize.ws_row = wf->rsz;
	tp->t_winsize.ws_col = wf->n.col;

	pgsignal(tp->t_pgrp, SIGWINCH, 1);
}

static int
vsc_control(arg, rcmd, dp)
	void *arg;
	u_int rcmd;
	u_int *dp;
{
	struct vsc_softc *vscp = arg;
	struct video_state *vsp = vscp->cvsp;
	u_int cmd = (rcmd & VSC_CONTROL_CMDMASK);
	u_int force = (rcmd & VSC_CONTROL_FORCE);
	int code;

	switch (cmd)
	{
	case VSC_CONTROL_WCHG:
		if ((vscp->sc_flags & VSC_DEVICE_ATTACHED) == 0)
			return EINVAL;
		if (*dp >= vc_cd.cd_ndevs || GETVCSOFTC(*dp) == NULL)
			return EINVAL;

		(void) ExecSwitchScreen(GETVSP(*dp), force);
		break;

	case VSC_CONTROL_LCHG:
		if (ISSET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE) != 0)
			break;
		vsc_change_crt_lines(vscp, NULL, VSC_SETSCM_ASYNC);
		break;

	case VSC_CONTROL_VSCROLL:
		if (ISSET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE) != 0)
			break;

		VScroll(vsp, *dp, force);
		break;

	case VSC_CONTROL_SETMODE:
		switch (*dp)
		{
		case VSC_KBDMODE_CODESET0:
			code = ISO2022_CODESET_SJIS;
			break;

		case VSC_KBDMODE_CODESET1:
		default:
			code = ISO2022_CODESET_EUC;
			break;
		}
		set_kanjimode(vsp, code);
		break;

	default:
		return EINVAL;
	}

	return 0;
}

static int
vsc_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct vsc_softc *vscp = (void *) dv;
	struct video_state *vsp = vscp->cvsp;
	u_int win;
	int s, error = 0;

	s = spltty();
	switch (ev)
	{
	case SYSTM_EVENT_RESUME:
		break;

	case SYSTM_EVENT_QUERY_SUSPEND:
		if (vsp != NULL &&
		    ISSET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE) != 0)
			error = SYSTM_EVENT_STATUS_BUSY;
		break;

	case SYSTM_EVENT_NOTIFY_SUSPEND:
		if (vscp->sc_pscid >= 0)
			break;
		if (ISSET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE) == 0)
			break;

		vscp->sc_pscid = vsp->id;

		/* find a free slot */
		for (win = 0; win < vc_cd.cd_ndevs; win ++)
		{
	    		if (GETVCSOFTC(win) == NULL)
				continue;
			if (ISSET(GETVSP(win)->vs_vi.vi_flags,\
				  VSC_KBDMODE_SET_XMODE) == 0)
				break;
		}

		if (win < vc_cd.cd_ndevs)
		{ 
			if (vsc_control(vscp, VSC_CONTROL_WCHG, &win) == 0)
				break;
		}
		vscp->sc_pscid = -1;
		error = SYSTM_EVENT_STATUS_REJECT;
		break;

	case SYSTM_EVENT_NOTIFY_RESUME:
		if (vscp->sc_pscid < 0)
			break;

		win = vscp->sc_pscid;
		vscp->sc_pscid = -1;
		vsc_control(vscp, VSC_CONTROL_WCHG, &win);
		break;

	case SYSTM_EVENT_ABORT_REQUEST:
		vscp->sc_pscid = -1;
		break;

	default:
		break;
	}
	splx(s);

	return error;
}
