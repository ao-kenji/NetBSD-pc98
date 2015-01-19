/*	$NecBSD: vsc.c,v 1.73.2.5 1999/09/03 06:31:39 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *
 *	@(#)pccons.c	5.11 (Berkeley) 5/21/91
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
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <machine/pc/display.h>

#include <i386/isa/timerreg.h>
#include <i386/isa/pc98spec.h>

#include <i386/isa/vsc/vsc.h>
#include <i386/isa/vsc/fontload.h>
#include <i386/isa/vsc/savervar.h>
#include <i386/isa/vsc/vsc_sm.h>

#define	PCVSCNO(dev)	(minor(dev))
#define	DEFAULT_BEEP_LEN (hz / 15)
#define	DEFAULT_DURATION (0x31b)

/* isa attach */
void pcstart __P((struct tty *));
int pcparam __P((struct tty *, struct termios *));

static int pc_xmode_on __P((struct video_state *));
static int pc_xmode_off __P((struct video_state *));

int pcopen __P((dev_t, int, int, struct proc *));
int pcclose __P((dev_t, int, int, struct proc *));
int pcread __P((dev_t, struct uio *, int));
int pcwrite __P((dev_t, struct uio *, int));
struct tty * pctty __P((dev_t));
int vsc_misc_ioctl __P((dev_t, int, caddr_t, int, struct proc *));
int pcioctl __P((dev_t, int, caddr_t, int, struct proc *));
void pcstop __P((struct tty *, int));
int pccnprobe __P((struct consdev *));
int pccninit __P((struct consdev *));
int pccnputc __P((dev_t, char));
int pccngetc __P((dev_t));
void pccnpollc __P((dev_t, int));
int pcmmap __P((dev_t, int, int));
static int usl_vt_ioctl __P((dev_t, int, caddr_t, int, struct proc *));

#ifdef	VSC_KMESG
static struct vsc_kmesg vsc_kmesg;
static u_char vsc_kmesg_buf[VSC_KMESG_SZ + 4];
#endif	/* VSC_KMESG */

/*************************************************
 * NetBSD cdev functions
 ************************************************/
int
pcopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct tty *tp;
	struct vsc_softc *vscp;
	struct vc_softc *vcsc;
	struct video_state *vsp;
	int unit = PCVSCNO(dev);

	if (unit >= vc_cd.cd_ndevs || (vcsc = vc_cd.cd_devs[unit]) == NULL ||
	    (vsp = vcsc->vsp) == NULL)
		return ENXIO;

	vscp = vsp->vs_vscp;
	if (vsp->tp == NULL)
	{
		tp = vsp->tp = ttymalloc();
		tty_attach(tp);
		if (vscp->cvsp == vsp)
		{
			/* inform our tty line to input methods */
			vscp->sc_ks->ks_switch(vscp->sc_kscp, vsp->id,
			       		       &vsp->vs_vi, tp);
		}
	}
	else
		tp = vsp->tp;

	tp->t_oproc = pcstart;
	tp->t_param = pcparam;
	tp->t_dev = dev;

	if (ISSET(tp->t_state, TS_ISOPEN) == 0)
	{
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		pcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	}
	else if (ISSET(tp->t_state, TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return EBUSY;

	SET(vsp->flags, VSC_OPEN);
	SET(tp->t_state, TS_CARR_ON);
	SET(tp->t_cflag, CLOCAL);

	return ((*linesw[tp->t_line].l_open) (dev, tp));
}

int
pcclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct video_state *vsp = GETVSP(PCVSCNO(dev));
	struct tty *tp = vsp->tp;

	(*linesw[tp->t_line].l_close) (tp, flag);
	ttyclose(tp);
	clear_process_terminal(vsp);
	return 0;
}

int
pcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp = GETVSP(PCVSCNO(dev))->tp;

	return ((*linesw[tp->t_line].l_read) (tp, uio, flag));
}

int
pcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp = GETVSP(PCVSCNO(dev))->tp;

	return ((*linesw[tp->t_line].l_write) (tp, uio, flag));
}

struct tty *
pctty(dev)
	dev_t dev;
{

	return (GETVSP(PCVSCNO(dev))->tp);
}

#if	!defined(FAKE_SYSCON) && !defined(FAKE_PCVT)
#define	FAKE_PCVT
#endif

static int
usl_vt_ioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct vsc_softc *vscp;
	struct video_state *newvsp, *vsp;
	struct trapframe *fp;
	struct vt_mode newmode;
	int s, win, error;

	vsp = GETVSP(PCVSCNO(dev));
	vscp = vsp->vs_vscp;
	error = 0;

	switch (cmd)
	{
#ifdef	FAKE_SYSCON
	case CONS_GETVERS:
		*(int *) data = 0x100;	/* fake syscons 1.0 */
		break;
#endif	/* FAKE_SYSCON */

#ifdef	FAKE_PCVT
	case VGAPCVTID:
		{
			struct pcvtid *id = (struct pcvtid *) data;

			strcpy(id->name, PCVTIDNAME);
			id->rmajor = PCVTIDMAJOR;
			id->rminor = PCVTIDMINOR;
		}
		break;

	case VGAPCVTINFO:
		{
			struct pcvtinfo *info = (void *) data;

			info->opsys	= CONF_NETBSD;
			info->opsysrel	= PCVT_NETBSD;
			info->nscreens	= PCVT_NSCREENS;
			info->scanset	= PCVT_SCANSET;
			info->sysbeepf	= 1193182;
			info->pcburst	= 1;
			info->kbd_fifo_sz = 0;
			info->compile_opts = (CONF_VT220KEYB |
				CONF_SCREENSAVER | CONF_PRETTYSCRNS |
				CONF_CTRL_ALT_DEL | CONF_SIGWINCH |
				CONF_XSERVER);
		}
		break;
#endif	/* FAKE_PCVT */

	case KDENABIO:
		if (suser(p->p_ucred, &p->p_acflag) != 0)
		{
			error = EPERM;
			break;
		}

		fp = (struct trapframe *) p->p_md.md_regs;
		SET(fp->tf_eflags, PSL_IOPL);
		break;

	case KDDISABIO:
		fp = (struct trapframe *) p->p_md.md_regs;
		CLR(fp->tf_eflags, PSL_IOPL);
		break;

	case VT_SETMODE:
		newmode = * (struct vt_mode*) data;

		s = spltty();
		if (vsp->smode.mode == VT_PROCESS && vsp->proc != p)
		{
			error = EBUSY;
		}
		else
		{
			if (newmode.mode == VT_PROCESS)
			{
				vsp->smode = newmode;
				vsp->proc = p;
				vsp->pid = p->p_pid;
			}
			else
				clear_process_terminal(vsp);
		}
		splx(s);
		break;

	case VT_GETMODE:
		*(struct vt_mode *) data = vsp->smode;
		break;

	case VT_RELDISP:
		if (vsp->smode.mode != VT_PROCESS || vsp->proc != p)
		{
			error = EPERM;
			break;
		}

		s = spltty();

		switch (*(int *) data)
		{
		case VT_FALSE:
			if (vsp == vscp->cvsp && ISSET(vsp->flags, VSC_WAIT_REL))
			{
				CLR(vsp->flags, VSC_WAIT_REL);
				vscp->sw_pend = 0;
			}
			break;

		case VT_TRUE:
			if (vsp == vscp->cvsp && ISSET(vsp->flags, VSC_WAIT_REL))
			{
				CLR(vsp->flags, VSC_WAIT_REL);
				if (vscp->sw_pend)
				{
					newvsp = vscp->vsp[vscp->sw_pend - 1];
					RequestSwitchScreen(newvsp);
				}
			}
			else
				error = EINVAL;
			break;

		case VT_ACKACQ:
			if (ISSET(vsp->flags, VSC_WAIT_ACK))
			{
				CLR(vsp->flags, VSC_WAIT_ACK);
				vscp->sw_pend = 0;
			}
			break;

		default:
			error = EINVAL;
			break;
		}

		splx(s);
		break;

	case VT_OPENQRY:
		*(int *) data = vscp->cvsp->id + 1;
		break;

	case VT_GETACTIVE:
		*(int *) data = vscp->cvsp->id + 1;
		break;

	case VT_ACTIVATE:
		win = *(int *) data - 1;
		if (win < 0 || win >= NVSCS(vscp))
			error = EINVAL;
		else
			error = ExecSwitchScreen(vscp->vsp[win], 0);
		break;

	case VT_WAITACTIVE:
		win = *(int *) data - 1;

		s = spltty();
		if (win >= 0)
			newvsp = vscp->vsp[(win % (NVSCS(vscp)))];
		else
			newvsp = vscp->cvsp;

		if (newvsp != vscp->cvsp || vscp->sw_pend)
		{
			SET(newvsp->flags, VSC_WAIT_ACT);
			error = tsleep(&newvsp->smode, PZERO | PCATCH, "waitvc", 0);
			if (error == ERESTART)
				error = VSC_ERESTART;

			CLR(newvsp->flags, VSC_WAIT_ACT);
		}

		splx(s);
		break;

	case KDSETMODE:
		switch (*(int *) data)
		{
		case KD_GRAPHICS:
			error = pc_xmode_on(vsp);
			break;

		case KD_TEXT:
			error = pc_xmode_off(vsp);
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	case KDSETRAD:
		break;

	case KDSKBMODE:
		switch (*(int *) data)
		{
		case K_RAW:
			s = spltty();
			SET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_RAWKEY);
			splx(s);
			break;

		case K_XLATE:
			s = spltty();
			CLR(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_RAWKEY);
			splx(s);
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	case KDGKBMODE:
		*(int *) data = 
			ISSET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_RAWKEY) ?
			K_RAW : K_XLATE;
		break;

	case KDMKTONE:
		if (data)
		{
			u_int duration = *(u_int *) data >> 16;
			u_int pitch = *(u_int *) data & 0xffff;

			pitch = (pitch * 1193182) / TIMER_FREQ;
			duration = (duration * hz) / 1500;	/* 3000 ? */
			sysbeep(pitch, duration);
		}
		else
			sysbeep(DEFAULT_DURATION, DEFAULT_BEEP_LEN);
		break;

#ifdef	KBD_EXT
	case KDGETLED:
	case KBDGLOCK:
		*(int *) data = vsp->vs_vi.vi_leds;
		break;

	case KBDSLOCK:
		vsp->vs_vi.vi_leds = *(int *) data;
		vscp->sc_ks->ks_set_led(vscp->sc_kscp, vsp->id); /* update */
		break;

	case KDSETLED:
#define	KBDC_LED_MASK	(LED_KANA | LED_CAP | LED_NUM)
		{
			int leds;

			leds = *(int *)data;
			if ((leds & KBDC_LED_MASK) ==
			    (vsp->vs_vi.vi_leds & KBDC_LED_MASK))
				break;
			vsp->vs_vi.vi_leds = leds;
			vscp->sc_ks->ks_set_led(vscp->sc_kscp, vsp->id);
		}
		break;
#endif	/* KBD_EXT */

	default:
		return -1;
	}

	return error;
}

int
vsc_misc_ioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	extern dev_t rootdev;

	switch (cmd)
	{
	case CYRIX_CACHE_ON:
		return cpu_cc(curcpu, CPU_CC_ON);

	case CYRIX_CACHE_OFF:
		return cpu_cc(curcpu, CPU_CC_OFF);

	case VSC_GET_ROOTDEV:
		/* XXX:
		 * Should move to i386_sysctl etc...
	 	 * There is no way to get rootdev (at boot time),
		 * since kernfs rootdev and kmem sysmbol read does not work.
		 */
		*(dev_t *) data = rootdev;
		return 0;
	}
	return -1;
}

int
pcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct video_state *vsp = GETVSP(PCVSCNO(dev));
	struct vsc_softc *vscp = vsp->vs_vscp;
	struct tty *tp = vsp->tp;
	struct status_line *stp;
	int error, mode, s;
	wf_t *wf;

	error = (*linesw[tp->t_line].l_ioctl) (tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	error = 0;
	switch (cmd)
	{
	case CONSOLE_X_MODE:
		if (*(int *) data == X_MODE_ON)
			return pc_xmode_on(vsp);
		else if (*(int *) data == X_MODE_OFF)
			return pc_xmode_off(vsp);
		else
			return EINVAL;

	case CONSOLE_X_MODE_ON:
		return pc_xmode_on(vsp);

	case CONSOLE_X_MODE_OFF:
		return pc_xmode_off(vsp);

	case CONSOLE_X_BELL:
		sysbeep(DEFAULT_DURATION, DEFAULT_BEEP_LEN);
		return error;

#ifdef	SCREEN_SAVER
	case VSC_SET_SAVERTIME:
		vscp->cursaver->saver_time = *(int *) data;
		saver_update(vscp->cursaver, SAVER_START);
		return error;
#endif	/* SCREEN_SAVER */

	case VSC_SET_CRTMODE:
		if (ISSET(vsp->flags, VSC_RCRT) != ISSET(*(int *) data, VSC_RCRT))
		{
			struct attribute *atp;
			int i;

			s = spltty();
			CLR(vsp->flags, VSC_RCRT);
			SET(vsp->flags, ISSET((*(int *) data), VSC_RCRT));

			for (i = 0; i < NSWIN; i++)
			{
				wf = vsp->lwf[i];
				atp = (wf->attrp == &vscp->attr) ?
				      &vscp->rattr : &vscp->attr;
				ReverseVideo(wf, atp);
			}
			splx(s);
		}
		return error;

	case VSC_SET_CRTLINES:
		mode = *(int *) data;
		if (mode < 0 || mode >= VSC_MAX_SM)
			return EINVAL;

		vsc_change_crt_lines(vscp, &screenmodes[mode], VSC_SETSCM_NULL);
		return error;

	case VSC_SET_KANJIMODE:
		return set_kanjimode(vsp, *(int *) data);

	case VSC_GET_KANJIMODE:
		*(int *) data = vsp->kanjicode;
		return error;

	case VSC_SET_ISO2022TABLE:
		kanjiset[ISO2022_CODESET_AUX].iso = *(struct iso2022set *) data;
		return set_kanjimode(vsp, ISO2022_CODESET_AUX);

	case VSC_GET_ISO2022TABLE:
		*(struct iso2022set *) data = vsp->cwf->iso;
		return error;

	case VSC_LOAD_FONT:
		return enter_fonts(vscp, (struct FontList *) data);

	case VSC_RESET_FONT:
		return clear_all_fonts(vscp);

	case VSC_LOAD_JIS83:
		return load_jis83_fonts(vscp);

	case VSC_FORCE_JIS83:
		if (*(int *) data)
			SET(vsp->flags, VSC_FJIS83);
		else
			CLR(vsp->flags, VSC_FJIS83);
		return error;

	case VSC_USE_VRAM:
		if (*(int *) data)
			SET(vsp->flags, VSC_GRAPH);
		else
			CLR(vsp->flags, VSC_GRAPH);

		vsc_start_display(vsp);
		return error;

	case VSC_SPLIT_SC:
		if (vsp->vr.row)
			return EBUSY;
		if (ISSET(*(int *) data, VSC_SPLIT))
			SET(vsp->flags, VSC_SPLIT);
		else
			CLR(vsp->flags, VSC_SPLIT);
		return error;

	case VSC_WRITE_ST:
		if (ISSET(vsp->flags, VSC_HAS_STATUS) == 0)
			return EINVAL;

		stp = (struct status_line *) data;
		if (stp->length < 0 || stp->length > MAX_ST_BYTES)
			return EINVAL;

		wf = vsp->lwf[SWINID];
		wf->r = wf->o;

		vsput(stp->msg, stp->length, 0, wf);
		return error;

	case VSC_STATUS_LINE:
		if (ISSET(*(int *) data, VSC_HAS_STATUS))
		{
			SET(vsp->flags, VSC_HAS_STATUS);
			error = open_swin(VSC_SWIN_SIGWINCH, vsp->lwf[SWINID]);
		}
		else
		{
			CLR(vsp->flags, VSC_HAS_STATUS);
			error = close_swin(vsp->lwf[SWINID]);
		}
		return error;

#ifdef	VSC_KMESG
	case VSC_GET_KMESG:
		{
			struct vsc_kmesg *kp = (void *) data;

			kp->pos = vsc_kmesg.pos;
			return copyout(vsc_kmesg.data, kp->data, VSC_KMESG_SZ);
		}
#endif	/* VSC_KMESG */

	case VSC_DEBUG:
		{
			struct vc_debug *dinfo;
			int i;

			dinfo = (struct vc_debug *) data;
			dinfo->vs = *vsp;
			for (i = 0; i < NSWIN; i++)
				dinfo->lwf[i] = *(vsp->lwf[i]);
		}
		return error;

	default:
		break;
	}

	error = usl_vt_ioctl(dev, cmd, data, flag, p);
	if (error >= 0)
		return (error == VSC_ERESTART) ? ERESTART : error;

	if (vscp->sc_ks->ks_kbdioctl != NULL)
	{
		error = vscp->sc_ks->ks_kbdioctl(vscp->sc_kscp, vsp->id,
						 cmd, data, flag);
		if (error >= 0)
			return error;
	}

	error = vsc_misc_ioctl(dev, cmd, data, flag, p);
	if (error >= 0)
		return error;

	return ENOTTY;
}

static void pcrestart __P((void *));

static void
pcrestart(arg)
	void *arg;
{
	struct tty *tp = arg;
	int s = spltty();

	CLR(tp->t_state, TS_TIMEOUT);
	splx(s);

	/* out of spltty! */
	pcstart(tp);
}

void
pcstart(tp)
	register struct tty *tp;
{
	register struct clist *rbp;
	struct video_state *vsp;
	struct vsc_softc *vscp;
	int s, len;

	s = spltty();
	vsp = GETVSP(PCVSCNO(tp->t_dev));
	vscp = vsp->vs_vscp;

	if (ISSET(tp->t_state, TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto out;

	if (vscp->sd.win_event)
	{
		SET(tp->t_state, TS_TIMEOUT);
		timeout(pcrestart, (void *) tp, 1);
		goto out;
	}

	SET(tp->t_state, TS_BUSY);
	vsc_invoke_saver(vscp->cursaver);
	splx(s);

	/*
	 * We need to do this outside spl since it could be fairly
	 * expensive and we don't want our serial ports to overflow.
	 */
	rbp = &tp->t_outq;
	len = ndqb(rbp, 0);
	if (len > PCBURST)
		len = PCBURST;
	vsput(rbp->c_cf, len, 0, vsp->cwf);
	ndflush(rbp, len);

	s = spltty();
	CLR(tp->t_state, TS_BUSY);

	if (rbp->c_cc)
	{
		SET(tp->t_state, TS_TIMEOUT);
		timeout(pcrestart, (void *) tp, 1);
	}

	if (rbp->c_cc <= tp->t_lowat)
	{
		if (ISSET(tp->t_state, TS_ASLEEP))
		{
			CLR(tp->t_state, TS_ASLEEP);
			wakeup(rbp);
		}

		selwakeup(&tp->t_wsel);
	}
out:
	splx(s);
}

void
pcstop(tp, flag)
	struct tty *tp;
	int flag;
{
}

int
pcparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	struct video_state *vsp = GETVSP(PCVSCNO(tp->t_dev));

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	tp->t_winsize.ws_row = vsp->cwf->rsz;
	tp->t_winsize.ws_col = vsp->cwf->n.col;
	return 0;
}

/*************************************************
 * Cons dev
 ************************************************/
int polling;

struct kbd_service_functions *
vsc_input_service_select(sel)
	int sel;
{
	extern struct kbd_service_functions vsc_kbd_service_functions; /* XXX */

	return &vsc_kbd_service_functions;
};

int
pccnprobe(cp)
	struct consdev *cp;
{
	struct kbd_service_functions *ks;
	int maj;

 	ks = vsc_input_service_select(VSC_INPUT_SERVICE_DEFAULT);
	if (ks->ks_console_establish() != 0)
	{
		cp->cn_pri = CN_DEAD;
		return 0;
	}

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == pcopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_INTERNAL;
	return 1;
}

int
pccninit(cp)
	struct consdev *cp;
{

#ifdef	VSC_KMESG
	vsc_kmesg.data = vsc_kmesg_buf;
#endif	/* VSC_KMESG */

	attach_vsc_consdev();
	return 0;
}

int
pccnputc(dev, c)
	dev_t dev;
	char c;
{
	struct vsc_softc *vscp = vsc_gp; 	/* XXX */
	struct video_state *vsp;
	int s = spltty();

	vsp =
#ifdef	CONSDEV_KERNEL_MSG
		vscp->consvsp;
#else	/* !CONSDEV_KERNEL_MSG */
		vscp->cvsp;
#endif	/* !CONSDEV_KERNEL_MSG */

	vsc_invoke_saver(vscp->cursaver);
	splx(s);

	if (c == '\n')
		vsput("\r\n", 2, 1, vsp->cwf);
	else
		vsput((u_char *) &c, 1, 1, vsp->cwf);

#ifdef	VSC_KMESG
	s = spltty();
	if (c != '\b')
		vsc_kmesg.data[(vsc_kmesg.pos ++) % VSC_KMESG_SZ] = c;
	else
		vsc_kmesg.pos --;
	splx(s);
#endif	/* VSC_KMESG */
	return 0;
}

int
pccngetc(dev)
	dev_t dev;
{
	struct vsc_softc *vscp = vsc_gp;
	struct video_state *vsp = vscp->cvsp;
	u_int mode;
	int s;
	char *cp;

	if (ISSET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE))
		return 0;
	mode = lookup_sysinfo(SYSINFO_CONSMODE);

	do
	{
		s = spltty();	/* block pcrint while we poll */
		cp = vscp->sc_ks->ks_poll(vscp->sc_kscp, VSC_KBDPOLL_DIRECT);
		splx(s);
		if ((mode & CONSM_NDELAY) && cp == NULL)
			return 0;
	}
	while (!cp);

	if (*cp == '\r')
		return '\n';
	else
		return *cp;
}

void
pccnpollc(dev, on)
	dev_t dev;
	int on;
{
	struct vsc_softc *vscp = vsc_gp;

	polling = on;
	if (!on)
	{
		int s;

		/*
		 * If disabling polling, make sure there are no bytes left in
		 * the FIFO, holding up the interrupt line. Otherwise we
		 * won't get any further interrupts.
		 */
		s = spltty();
		vscp->sc_ks->ks_poll(vscp->sc_kscp, 0);
		splx(s);
	}
}

int
pcmmap(dev, offset, nprot)
	dev_t dev;
	int offset;
	int nprot;
{

	if ((u_int) offset >= CBUSHOLE_SIZE)
		return -1;

	return i386_btop(CBUSHOLE_START + offset);
}

#include "machine/psl.h"

static int
pc_xmode_on(vsp)
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;

	if (vsp != vscp->cvsp)
		return EINVAL;

	SET(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE | VSC_KBDMODE_SET_RAWKEY);
	CLR(vsp->flags, VSC_CURSOR | VSC_SAVER);

#ifdef	COMPAT_10
	{
		register struct trapframe *fp;

		fp = (struct trapframe *) curproc->p_md.md_regs;
		SET(fp->tf_eflags, PSL_IOPL);
	}
#endif	/* COMPAT_10 */

	EnterLeaveVcrt(vsp, 1);
	cursor_switch(vscp->curcursor, vsp);
#ifdef	SCREEN_SAVER
	saver_switch(vscp->cursaver, vsp);
#endif	/* SCREEN_SAVER */
	vsc_start_display(vsp);

	return 0;
}

static int
pc_xmode_off(vsp)
	struct video_state *vsp;
{
	struct vsc_softc *vscp = vsp->vs_vscp;

	CLR(vsp->vs_vi.vi_flags, VSC_KBDMODE_SET_XMODE | VSC_KBDMODE_SET_RAWKEY);
	SET(vsp->flags, VSC_CURSOR | VSC_SAVER);

#ifdef	COMPAT_10
	{
		register struct trapframe *fp;

		fp = (struct trapframe *) curproc->p_md.md_regs;
		CLR(fp->tf_eflags, PSL_IOPL);
	}
#endif	/* COMPAT_10 */

	EnterLeaveVcrt(vsp, 0);
	cursor_switch(vscp->curcursor, vsp);
#ifdef	SCREEN_SAVER
	saver_switch(vscp->cursaver, vsp);
#endif	/* SCREEN_SAVER */
	vsc_start_display(vsp);

#ifdef	VSC_RSTAFX
	vsc_set_scmode(vscp, NULL, VSC_SETSCM_FORCE, VSC_SETSCM_ASYNC);
#endif	/* VSC_RSTAFX */

	return 0;
}

int
vscbeep(vsp, tone, len)
	struct video_state *vsp;
	int tone, len;
{

	/* XXX */
	if (tone == 0)
	{
		tone = DEFAULT_DURATION;
		len = DEFAULT_BEEP_LEN;
	}

	sysbeep(tone, len);
	return 0;
}
