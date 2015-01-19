/*	$NecBSD: vsc.h,v 1.49.10.8 1999/09/03 06:31:39 honda Exp $	*/
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
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef	_I386_VSC_VSC_H_
#define	_I386_VSC_VSC_H_

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/vscio.h>
#include <i386/isa/vsc/vsc_interface.h>

#define	SET(flags, val)	((flags) |= (val))
#define	CLR(flags, val) ((flags) &= ~(val))
#define	ISSET(flags, val) ((flags) & (val))
#define	VSC_ERESTART	(ELAST + 1)
#define	MAX_ROW		32
#define	MAX_COL		80

/* cf_flags values */
#define	VSC_CFFLAGS_MASK	0xffff
#define	VSC_DEC_AWM		0x0001
#define	VSC_DEC_INS		0x0002
#define	VSC_GRAPHIC_ON		0x0004
#define	VSC_WIN_SPLIT		0x0008
#define	VSC_KEY_LNM		0x0010
#define	VSC_SWIN_OPEN		0x0020
#define	VSC_KANJI_MODE(flags)	(((flags) & 0xff0000) >> 16)
#define	VSC_FLAGS_BITS	"\020\006stl\005klnm\004split\003graph\002ins\001awm"

typedef u_int16_t vsc_wchar_t;
typedef u_int16_t vram_t;

struct window {
	int row;
	int col;
};

struct scroll_region {
	int lw;
	int hg;
	int sz;
};

struct ring_mem {
	struct ring_mem *next;
	struct ring_mem *prev;
	vram_t mem[MAX_COL];
};

#define	CURSTATESIZE \
	(sizeof(struct iso2022set) + sizeof(struct window) + 4 * sizeof(int))

struct attribute {
	int attr;
	int sattr;
	int vattr;
	int sysattr;
};

struct win_frame {
#define	NSWIN	3
#define	MWINID	0
#define	VWINID	1
#define	SWINID	2
	int subid;

	/**/
	struct video_state *vsp;

	/**/
	vram_t **Ctab;
	vram_t **Atab;

	/**/
	struct window o;
	struct window n;
	int rsz;

	/**/
#define	OPENF	1
	u_int flags;

	/**/
	struct scroll_region sc;

	/**/
#define	DECOM	(0x1 << 6)
#define	DECAWM	(0x1 << 7)
#define	DECINS	(0x1 << 16)
	u_int decmode;

	/**/
	u_char *tabs;

	/**/
	struct attribute *attrp;

	/**/
	int attr;
	u_int fgc, bgc;
#define	VSC_ATTR_BOLD	0x01
#define	VSC_ATTR_UL	0x02
#define	VSC_ATTR_BL	0x04
#define	VSC_ATTR_RV	0x08
#define	VSC_ATTR_BGON	0x10
	u_int attrflags;

	struct window r;
	struct iso2022set iso;
	u_char bs[CURSTATESIZE];

	/**/
	u_int nparm;
#define	MAXPARM	8
#define	VSPCX(wf)  ((wf)->parm[0])
#define	VSPCY(wf)  ((wf)->parm[1])
#define	VSPCOL(wf) ((wf)->r.col >= (wf)->n.col ? ((wf)->n.col-1) : (wf)->r.col)
	vsc_wchar_t parm[MAXPARM];

#define	NSTATE	4
	int state;
	vsc_wchar_t EscSeq[NSTATE];
	int cset;
	vsc_wchar_t multibyte;
};

typedef struct win_frame wf_t;
struct video_state {
	struct vsc_softc *vs_vscp;	/* my parents vsc */

	struct tty *tp;
	int id;

	/**/
#define	VSC_FJIS83	0x01
#define	VSC_OPEN	0x04
#define	VSC_GRAPH	0x08
#define	VSC_CURSOR	0x20
#define	VSC_SAVER	0x40
#define	VSC_I_AM_CONS	0x80
#define	VSC_INIT_DONE	0x100
#define	VSC_HAS_STATUS	0x200
#define	VSC_SPLIT	0x400
#define	VSC_RCRT	0x800
#define	VSC_WAIT_ACK	0x1000
#define	VSC_WAIT_REL	0x2000
#define	VSC_WAIT_ACT	0x4000
	u_int flags;
	struct vsc_kbd_info vs_vi;

	/**/
	int vstep;
	struct window vn;
	struct window vr;
	struct ring_mem *ring;
	u_int ringsz;

	/* screen memory */
	vram_t *vscCtab[MAX_ROW];
	vram_t *vscAtab[MAX_ROW];

	/* kanji */
	int startcode;
	int kanjicode;

	/**/
	wf_t *cwf;
	wf_t *lwf[NSWIN];
	wf_t *backupwf;

	/* misc */
	pid_t pid;
	struct proc *proc;
	struct vt_mode smode;
	struct screenmode *scmode;

	/* busy count */
	volatile int busycnt;

#define	PCBURST	256
};

struct vc_softc {
	struct device sc_dev;

	struct video_state *vsp;
};

struct Gaiji;
struct screenmode;
struct FontBlockList;
struct kbd_service_functions;
struct saver_softc;
struct vsc_softc {
	struct device sc_dev;
	void *sc_kscp;
	struct kbd_service_functions *sc_ks;

	u_int16_t *sc_crtbase;
	bus_space_tag_t sc_iot;
	bus_space_tag_t sc_memt;

	bus_space_handle_t sc_GDC1ioh;
	bus_space_handle_t sc_GDC2ioh;

	bus_space_handle_t sc_tramh;
	bus_space_handle_t sc_taramh;
	bus_space_handle_t sc_gram0h;
	bus_space_handle_t sc_gram1h;
	bus_space_handle_t sc_gram2h;
	bus_space_handle_t sc_gram3h;
	bus_space_handle_t sc_pramh;
	bus_space_handle_t sc_pregh;

#define	VSC_DEVICE_ATTACHED		0x0001
#define	VSC_HARDWARE_INITIALIZED	0x0002
	u_int sc_flags;

	/**/
	u_short coltab[MAX_ROW];
	vram_t *scCtab[MAX_ROW];
	vram_t *scAtab[MAX_ROW];

	/**/
	int sw_pend;
	struct DoScreenData {
		u_int flags;
		struct video_state *tvsp;
		struct video_state *vsp;
		int win_event;
	} sd;

	/**/
	int nvscs;
	int sc_pscid;
	struct video_state *cvsp;
	struct video_state *consvsp;
	struct video_state *vsp[DEFAULT_MAX_VSC];

	/* attribute */
	struct attribute rattr;
	struct attribute attr;

	/* cursor & screen saver */
	struct cursor_softc *curcursor;
	struct saver_softc *cursaver;

	/* font/gaiji structures */
	int sc_j83ind;
	vsc_wchar_t *sc_gchar;
	struct FontBlockList *sc_wcharfb;
	FontCode *sc_asciifp;
	struct Gaiji *sc_gaijip;

	/* screen status */
	struct screenmode *sc_csm;
	int sc_gdc_lines;
};

/* CURSOR */
#define	CURSORUPABS	0
#define	CURSORUPEXP	1
#define	CURSORON	2
#define	CURSOROFF	3

struct cursor_softc {
	u_int timeout_active;

	int pos;
	u_int state;
	struct video_state *vsp;

	u_int (*ctrl) (int, struct cursor_softc *);
	u_int (*func) (int, struct cursor_softc *);
};

/*******************************************************
 * prototype & inlines
 ******************************************************/
/* others */
extern struct cfdriver vc_cd;
extern struct vsc_softc *vsc_gp;

/* vscdev */
#define	VSC_INPUT_SERVICE_DEFAULT	0
#define	VSC_INPUT_SERVICE_KEYBOARD	1
#define	VSC_INPUT_SERVICE_USB		2
#define	VSC_INPUT_SERVICE_SERIAL	3
struct kbd_service_functions *vsc_input_service_select __P((int));

#define	VSC_INIT_SIGWINCH	0x01
#define	VSC_INIT_KANJI		0x02
void init_vc_device __P((struct video_state *, struct screenmode *, u_int));

/* vram op in video.c */
void attach_crt_mem __P((wf_t *));
void attach_vcrt_mem __P((wf_t *));

#define	VSC_SWIN_SIGWINCH	0x01
#define	VSC_SWIN_UPDATE		0x02
int open_swin __P((int, wf_t *));
int close_swin __P((wf_t *));

#define	GO_FORWARD	0
#define	GO_BACKWARD	1
#define	FROMORG		0
#define	FROMCUR		1
#define	FROMLINE	2
void scroll_screen __P((int, int, wf_t *));
void ClearChar __P((int, int, wf_t *));
void DeleteChar __P((int, int, wf_t *));
void InsertChar __P((int, int, wf_t *));
void SaveCursor __P((wf_t *));	/* vt100.c */

/* attribute */
void set_attribute __P((wf_t *));

/* input */
void vscput __P((wf_t *, int));
void vscwput __P((wf_t *, int));
void vsput __P((u_char *, u_int, int, wf_t *));	/* vt100.c */

/* switch screen */	/* video.c */
void clear_process_terminal __P((struct video_state *));
int ExecSwitchScreen __P((struct video_state *, int));
int RequestSwitchScreen __P((struct video_state *));
void EnterLeaveVcrt __P((struct video_state *, int));
void VScroll __P((struct video_state *, int, int));

/* init */
void ScreenFillW __P((wf_t *));	/* video.c */
void ReverseVideo __P((wf_t *, struct attribute *));	/* video.c */

/* cursor */
struct cursor_softc;
int cursor_init __P((struct cursor_softc *, struct video_state *));
int cursor_update __P((struct cursor_softc *, int));
int cursor_switch __P((struct cursor_softc *, struct video_state *));

/* video mode */	/* video.c */
void vsc_start_display __P((struct video_state *));
#define	VSC_SETSCM_NULL 	0x00
#define	VSC_SETSCM_FORCE	0x01
#define	VSC_SETSCM_ASYNC	0x02
int vsc_set_scmode __P((struct vsc_softc *, struct screenmode *, int, int));
void vsc_change_crt_lines __P((struct vsc_softc *, struct screenmode *, int));

/* kanji */	
void InitISO2022 __P((struct iso2022set *, wf_t *));

/* console kbd */
struct vsc_softc *attach_vsc_consdev __P((void));
int vscbeep __P((struct video_state *, int, int));

/* some macros */
#define	NVSCS(vscp)	((vscp)->nvscs)
#define	GETVCSOFTC(win)	((struct vc_softc *)(vc_cd.cd_devs[(win)]))
#define	GETVSP(win)	(GETVCSOFTC(win)->vsp)

/* Debug */
struct vc_debug {
	struct video_state vs;
	wf_t mwf;
	wf_t lwf[NSWIN];
};

struct vsc_kmesg {
#define VSC_KMESG_SZ	0x20000
	u_int pos;
	u_char *data;
};

#define	VSC_DEBUG	_IOR('P', 50, struct vc_debug)
#define	VSC_GET_KMESG	_IOWR('P', 50, struct vsc_kmesg)
#endif	/* !_I386_VSC_VSC_H_ */
