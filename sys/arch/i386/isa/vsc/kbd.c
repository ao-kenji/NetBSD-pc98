/*	$NecBSD: kbd.c,v 1.86.2.7 1999/09/05 07:17:41 honda Exp $	*/
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

#include "opt_ddb.h"
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/vscio.h>

#include <i386/isa/vsc/config.h>
#include <i386/isa/vsc/kbdreg.h>
#include <i386/isa/vsc/kbdvar.h>
#include <i386/isa/vsc/savervar.h>
#include <i386/isa/vsc/vsc_interface.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <machine/systmbusvar.h>

#define	KBC_MAX_RETRY	4
#define	IDX0	0
#define	STR	KBD_SUBT_STR
#define	FNC	KBD_SUBT_FNC

typedef struct kbd_ovlkey Ovlkey;
extern struct scan_code alt_del_def, alt_bs_def, default_key_code[];
extern int polling;

static struct kbd_softc ksctab;
static struct vsc_kbd_info kscvi;
static int kbdmatch __P((struct device *, struct cfdata *, void *));
static void kbdattach __P((struct device *, struct device *, void *));

struct cfattach kbdc_ca = {
	sizeof(struct device), kbdmatch, kbdattach
};

extern struct cfdriver kbdc_cd;

static u_char *vsc_process_key __P((struct kbd_softc *, u_char));
static __inline u_int8_t kbd_read_status __P((bus_space_tag_t, bus_space_handle_t));
static void kbd_wait __P((int));
static int kbd_wait_ready __P((bus_space_tag_t, bus_space_handle_t));
static void kbd_write_data_1 __P((bus_space_tag_t, bus_space_handle_t, u_char));
static void kbd_wait_ack __P((bus_space_tag_t, bus_space_handle_t));
#define	KBDC_LED_ERREXIT 0
#define	KBDC_LED_ERRIGN	1
#define	KBDC_LED_DRAIN	2
#define	WITH_RESULT_CODE	0
#define	NO_RESULT_CODE		1
static int kbd_control_led __P((struct kbd_softc *, int));
static int kbd_read_data __P((bus_space_tag_t, bus_space_handle_t, int, u_int8_t *));
static int kbd_write_cmd __P((bus_space_tag_t, bus_space_handle_t, int, u_char, int));
static void kbd_demonstrate __P((struct kbd_softc *));
static void kbd_reset __P((struct kbd_softc *));
static int kbd_special_hook __P((struct kbd_softc *, u_char));
static int kbd_tty_write __P((struct kbd_softc *, u_char *));
static int kbdintr __P((void *));
static int kbdc_systmmsg __P((struct device *, systm_event_t));

#ifdef	KBD_EXT
static void ovlinit __P((struct kbd_softc *));
static int getokeydef __P((struct kbd_softc *, u_int, Ovlkey *));
static int getckeydef __P((struct kbd_softc *, u_int, Ovlkey *));
static int rmkeydef __P((struct kbd_softc *, int));
static int setkeydef __P((struct kbd_softc *, Ovlkey *));
static void get_usl_keymap __P((struct kbd_softc *, keymap_t *));
#endif	/* KBD_EXT */

/* kbd connect functions */
static int vsc_kbd_console_establish __P((void));
static void *vsc_kbd_connect_establish __P((struct kbd_connect_args *));
/* kbd led control */
static int vsc_kbd_set_led __P((void *, int));
/* kbd input tty switch inform */
static int vsc_kbd_switch __P((void *, int, struct vsc_kbd_info *, struct tty *));
/* kbd polling */
static u_char *vsc_kbd_poll __P((void *, int));
/* kbd ioctl */
#ifdef	KBD_EXT
static int vsc_kbdioctl __P((void *, int, int, caddr_t, int));
#endif	/* KBD_EXT */
/* kbd mode setup */
static int vsc_kbd_setmode __P((void *, int, int));
static int vsc_kbd_connect_saver __P((void *, struct saver_softc *));

struct kbd_service_functions vsc_kbd_service_functions = {
	vsc_kbd_console_establish,
	vsc_kbd_poll,

	vsc_kbd_connect_establish,
	vsc_kbd_set_led,
	vsc_kbd_switch,
#ifdef	KBD_EXT
	vsc_kbdioctl,
#else	/* !KBD_EXT */
	NULL,
#endif	/* !KBD_EXT */
	vsc_kbd_setmode,
	vsc_kbd_connect_saver
};

/***********************************************
 * NetBSD System Interface
 ***********************************************/
static int
kbdc_systmmsg(dv, ev)
	struct device *dv;
	systm_event_t ev;
{
	struct kbd_softc *sc = (void *) dv;
	int s, error = 0;

	s = spltty();
	switch (ev)
	{
	case SYSTM_EVENT_RESUME:
		kbd_control_led(sc, KBDC_LED_DRAIN);
		break;

	default:
		break;
	}
	splx(s);

	return error;
}

static int
kbdmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct kbd_softc *sc = &ksctab;
	int error;

	ia->ia_msize = 0;
	ia->ia_iosize = 1;
	sc->sc_iot = ia->ia_iot;
	sc->sc_memt = ia->ia_memt;

	if ((error = vsc_kbd_console_establish()) != 0)
	{
		printf("kbdc: WARNING init failed or missing. code %x\n",
		       error);
		return 0;
	}
	return 1;
}

static void
kbdattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct kbd_softc *sc = &ksctab;

	printf(": ");
	if ((sc->sc_cap & KBD_CAP_CMDACCEPT) != 0)
		printf("software control support");
	printf("\n");

	systmmsg_bind((struct device *) sc, kbdc_systmmsg);

	sc->sc_iot = ia->ia_iot;
	sc->sc_memt = ia->ia_memt;

	config_found(self, ia, NULL);	/* XXX: must be self != sc */

	if (ia->ia_cfgflags & USE_PC98_BSDEL)
	{
		default_key_code[NO_BSKEY] = alt_bs_def;
		default_key_code[NO_DELKEY] = alt_del_def;
	}

	if (ia->ia_cfgflags & USE_ALT_SWITCH)
		sc->sc_hookkey = LED_ALT;

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
				       IPL_TTY, kbdintr, sc);
}

/***********************************************
 * VSC <=> KBD interface functions
 ***********************************************/
void *
vsc_kbd_connect_establish(ka)
	struct kbd_connect_args *ka;
{
	struct kbd_softc *sc = &ksctab;

	ka->ka_ks = &vsc_kbd_service_functions;
	if (sc->sc_vsc_control == NULL)
	{
		sc->sc_vsc_control = ka->ka_control;
		sc->sc_vsc_control_arg = ka->ka_control_arg;
		kbd_demonstrate(sc);
	}
	return sc;
}

static int
vsc_kbd_console_establish(void)
{
	struct kbd_softc *sc = &ksctab;
	struct vsc_kbd_info *vi = &kscvi;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int error = 0;

	if (sc->sc_ioh == NULL)
	{
		strcpy(sc->sc_dev.dv_xname, "kbd0");	/* XXX */
		sc->sc_vi = vi;
		sc->sc_hookkey = LED_CTL;
		sc->sc_keytab = &default_key_code[0];
#ifdef	VSC_META_ESC
		sc->sc_metachar[0] = 0x1b;
#endif	/* VSC_META_ESC */
		sc->sc_kmake_codeset = VSC_KBDMODE_CODESET0;
		sc->sc_krel_codeset = VSC_KBDMODE_CODESET1;
		sc->sc_iot = I386_BUS_SPACE_IO;
		sc->sc_memt = I386_BUS_SPACE_MEM;
		if (bus_space_map(sc->sc_iot, IO_KBD, 0, 0, &sc->sc_ioh) != 0)
			return EIO;
		if (bus_space_map_load(sc->sc_iot, sc->sc_ioh, 2,
				       BUS_SPACE_IAT_2,
				       BUS_SPACE_MAP_FAILFREE) != 0)
			return EIO;
	}

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	sc->sc_cap |= KBD_CAP_CMDACCEPT;
	kbd_reset(sc);
	error = kbd_write_cmd(iot, ioh, sc->sc_cap, KBC_CMD_LED,
			      WITH_RESULT_CODE);
	error |= kbd_write_cmd(iot , ioh, sc->sc_cap, 
			       CAPSLED | CANALED | BASELED,
			       WITH_RESULT_CODE);
	kbd_wait(1000);
	error |= kbd_write_cmd(iot, ioh, sc->sc_cap, KBC_CMD_LED,
			       WITH_RESULT_CODE);
	error |= kbd_write_cmd(iot, ioh, sc->sc_cap, BASELED,
			       WITH_RESULT_CODE);
#ifdef	KBD_PROBE_FAILURE
	if (error != 0)
	{
		sc->sc_cap &= ~KBD_CAP_CMDACCEPT;
		kbd_reset(sc);
		return 0;
	}
#endif	/* KBD_PROBE_FAILURE */
	return error;
}

static int
vsc_kbd_connect_saver(arg, svp)
	void *arg;
	struct saver_softc *svp;
{
	struct kbd_softc *sc = arg;

	sc->sc_svp = svp;
	return 0;
}

static int
vsc_kbd_setmode(arg, win, cmd)
	void *arg;
	int win;
	int cmd;
{
	struct kbd_softc *sc = arg;

	if (win != sc->sc_cwin)
		return EINVAL;

	return 0;
}

static u_char *
vsc_kbd_poll(arg, mode)
	void *arg;
	int mode;
{
	struct kbd_softc *sc = arg;
	u_char *cp = NULL;

	if ((mode & VSC_KBDPOLL_DIRECT) != 0)
	{
		bus_space_tag_t iot = sc->sc_iot;
		bus_space_handle_t ioh = sc->sc_ioh;
		u_int8_t res;

		if ((kbd_read_status(iot, ioh) & KBST_READY) == 0)
			return NULL;

		(void) kbd_read_data(iot, ioh, sc->sc_cap, &res);
		cp = vsc_process_key(sc, res);
	}
	else
	{
		int s = spltty();	

		kbdintr(sc);
		splx(s);
	}

	return cp;
}

static int
vsc_kbd_set_led(arg, win)
	void *arg;
	int win;
{
	struct kbd_softc *sc = arg;

	if (sc->sc_cwin == win)
	{
		kbd_control_led(sc, KBDC_LED_DRAIN);
	}
	return 0;
}

static int
vsc_kbd_switch(arg, win, vi, tp)
	void *arg;
	int win;
	struct vsc_kbd_info *vi;
	struct tty *tp;
{
	struct kbd_softc *sc = arg;
	int s;

	s = spltty();
	sc->sc_vi = vi;
	sc->sc_tp = tp;
	sc->sc_cwin = win;
	splx(s);

	return 0;
}

/***********************************************
 * Internal kbd functions (upper layer)
 ***********************************************/
#define	KBDINTR_MAXLOOP	256
static int
kbdintr(arg)
	void *arg;
{
	struct kbd_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int counter = 0;
	u_int8_t res;
	u_char *cp;

	if ((kbd_read_status(iot, ioh) & KBST_READY) == 0)
		return 0;

	if (sc->sc_svp != NULL)
		vsc_invoke_saver(sc->sc_svp);

	if (polling != 0)
		return 1;

	do
	{
		(void) kbd_read_data(iot, ioh, sc->sc_cap, &res);
		cp = vsc_process_key(sc, res);
		(void) kbd_tty_write(sc, cp);
	}
	while ((kbd_read_status(iot, ioh) & KBST_READY) != 0&&
		counter++ < KBDINTR_MAXLOOP);

	return 1;
}

static int
kbd_tty_write(sc, cp)
	struct kbd_softc *sc;
	u_char *cp;
{
	struct tty *tp;

	tp = sc->sc_tp;
	if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0)
		return EIO;

	if (cp != NULL)
	{
		do
		{
			(*linesw[tp->t_line].l_rint) (*cp++, tp);
		}
		while (*cp != 0);
	}
	return 0;
}

#define	MAX_HOOK	(LED_CTL | LED_ALT)
#define	ABORT_RETURN	{return 1;}
#define	IS_KEYPUSH(flag) (((flag) & KBDR_MAKEBIT) == 0)
#define	IS_KEYREL(flag)	 (((flag) & KBDR_MAKEBIT) != 0)

static int
kbd_special_hook(sc, keydata)
	struct kbd_softc *sc;
	u_char keydata;
{
	static int debugger_in;
	int force, cmd;
	u_int data;
	u_char dt;

	dt = keydata & KBDR_CODEMASK;
	force = ((sc->sc_shifts & MAX_HOOK) == MAX_HOOK) ? 1 : 0;

	switch (dt)
	{
	case 0:		/* esc */
		if (IS_KEYPUSH(keydata))
		{
			cmd = VSC_CONTROL_VSCROLL;
			data = VSC_SCREEN_SC_E;
			goto vsc_control_out;
		}
		ABORT_RETURN

	case 54:	/* up */
		if (IS_KEYPUSH(keydata))
		{
			cmd = VSC_CONTROL_VSCROLL;
			data = VSC_SCREEN_SC_U;
			goto vsc_control_out;
		}
		ABORT_RETURN

	case 55:	/* down */
		if (IS_KEYPUSH(keydata))
		{
			cmd = VSC_CONTROL_VSCROLL;
			data = VSC_SCREEN_SC_D;
			goto vsc_control_out;
		}
		ABORT_RETURN

	case 57:	/* del */
		if (force != 0)
		{
#ifdef	KBD_ENABLE_CPU_RESET
			/* ctl + alt + del: cpu reset */
			cpu_reset();
			return 1;
#else	/* !KBD_ENABLE_CPU_RESET */
			printf("%s: cpu reset disabled (see kernel options)\n",
				sc->sc_dev.dv_xname);
#endif	/* !KBD_ENABLE_CPU_RESET */
		}
		return 0;

	case 62:	/* home */
		if ((sc->sc_vi->vi_flags & VSC_KBDMODE_SET_XMODE) != 0)
			break;

		if (force == 0)
		{
			if (IS_KEYREL(keydata))
			{
				kbd_reset(sc);
				vsc_kbd_set_led(sc, sc->sc_cwin);
			}
		}
		else if (IS_KEYPUSH(keydata))
		{
			cmd = VSC_CONTROL_LCHG;
			data = 0;
			goto vsc_control_out;
		}
		ABORT_RETURN

	case 96:	/* stop */
		if (force == 0)
			break;

		if (IS_KEYPUSH(keydata) && debugger_in == 0)
		{
			debugger_in = 1;

#ifdef	DDB
			if ((sc->sc_vi->vi_flags & VSC_KBDMODE_SET_XMODE) == 0)
				Debugger();
#endif	/* DDB */
			debugger_in = 0;
		}
		ABORT_RETURN

	case 97:	/* copy */
		if (force == 0)
			break;

		sc->sc_hookkey = (sc->sc_hookkey == LED_ALT) ? 
				LED_CTL : LED_ALT;
		ABORT_RETURN

#if	DEFAULT_MAX_VSC > 10
	case 82:
	case 83:
	case 84:
	case 85:
	case 86:
		if (IS_KEYPUSH(keydata))
		{
			data = dt - 72;
			cmd = VSC_CONTROL_WCHG;
			goto vsc_control_out;
		}
		ABORT_RETURN
#endif	/* DEFAULT_MAX_VSC > 10 */

	case 98:
	case 99:
	case 100:
	case 101:
	case 102:
	case 103:
	case 104:
	case 105:
	case 106:
	case 107:
		if (IS_KEYPUSH(keydata))
		{
			data = dt - 98;
			cmd = VSC_CONTROL_WCHG;
			goto vsc_control_out;
		}
		ABORT_RETURN

	default:	/* others */
		break;
	}

	return 0;

vsc_control_out:
	if (force)
		cmd |= VSC_CONTROL_FORCE;
	sc->sc_vsc_control(sc->sc_vsc_control_arg, cmd, &data);
	ABORT_RETURN
}

#define	GETGET(ent)						\
{								\
	if (sc->sc_keytab[dt].ent.subtype == STR)		\
	{							\
		if (altnum >= 0)				\
			sc->sc_mc = sc->sc_ovltbl[altnum].ent;	\
		else						\
			sc->sc_mc = sc->sc_keytab[dt].ent.what.string;	\
	}							\
}

#if	VSC_META_ESC
#define	VSC_META_UP						\
	if ((sc->sc_shifts & LED_ALT) != 0)			\
	{							\
		sc->sc_metachar[1] = *sc->sc_mc;		\
		sc->sc_mc = sc->sc_metachar;			\
	}
#else	/* VSC_META_ESC */
#define	VSC_META_UP						\
	if ((sc->sc_shifts & LED_ALT) != 0)			\
	{							\
		sc->sc_metachar[0] = *sc->sc_mc | 0x80;		\
		sc->sc_mc = sc->sc_metachar;			\
	}
#endif	/* VSC_META_ESC */

#define	change_led_status(data)			\
{						\
	sc->sc_vi->vi_leds ^= (data);		\
}

#define	change_shift_status(data)		\
{						\
	if (IS_KEYREL(make))			\
		sc->sc_shifts &= ~(data);	\
	else					\
		sc->sc_shifts |= (data);	\
}

u_char *
vsc_process_key(sc, keydata)
	struct kbd_softc *sc;
	u_char keydata;
{
	u_int data;
	u_char dt, make;
	u_short type;
	short altnum;

	sc->sc_mc = NULL;
	sc->sc_Xchar[0] = keydata;
	make = (sc->sc_Xchar[0] & KBDR_MAKEBIT);
	dt = (sc->sc_Xchar[0] & KBDR_CODEMASK);
	type = sc->sc_keytab[dt].type;

	if ((type & KBD_OVERLOAD) != 0)
	{
		altnum = sc->sc_keytab[dt].ovlindex;
		type = ((sc->sc_ovltbl[altnum].type) & KBD_MASK);
	}
	else
		altnum = -1;

	switch (type)
	{
	case KBD_SHIFT:
		change_shift_status(LED_SHIFT);
		goto out;

	case KBD_ALTGR:
	case KBD_META:
		change_shift_status(LED_ALT);
		goto out;

	case KBD_CTL:
		change_shift_status(LED_CTL);
		goto out;

	case KBD_CAPS:
		change_led_status(LED_CAP)
		goto out;

	case KBD_NUM:
		change_led_status(LED_NUM)
		goto out;

	case KBD_KANA:
		data = IS_KEYREL(make) ? 
			sc->sc_krel_codeset : sc->sc_kmake_codeset;
		sc->sc_vsc_control(sc->sc_vsc_control_arg, 
				   VSC_CONTROL_SETMODE, &data);
		change_led_status(LED_KANA)
		goto out;

	case KBD_SCROLL:
		change_led_status(LED_SCR);
		goto out;
	}

	if ((sc->sc_shifts & sc->sc_hookkey) != 0 &&
	    kbd_special_hook(sc, keydata) != 0)
		return NULL;

	if ((sc->sc_vi->vi_flags & VSC_KBDMODE_SET_RAWKEY) != 0)
	{
		return sc->sc_Xchar;
	}

	/* normal */
	if (IS_KEYREL(make))
		return sc->sc_mc;

	switch (type)
	{
	case KBD_BREAK:
	case KBD_ASCII:
	case KBD_FUNC:
	case KBD_KP:
	case KBD_CURSOR:
		if ((sc->sc_shifts & LED_CTL) != 0)
			GETGET(ctrl)
		else if ((sc->sc_shifts & LED_SHIFT) != 0)
			GETGET(shift)
		else
			GETGET(unshift)

		if (sc->sc_mc && sc->sc_mc[1] == 0)
		{
			if ((sc->sc_vi->vi_leds & LED_CAP) != 0)
			{
#define	SHIFTCHAR	('a' - 'A')
				if (*sc->sc_mc >= 'a' && *sc->sc_mc <= 'z')
				{
					sc->sc_capchar[0] =
						*sc->sc_mc - SHIFTCHAR;
					sc->sc_mc = sc->sc_capchar;
				}
				else if (*sc->sc_mc >= 'A' && *sc->sc_mc <= 'Z')
				{
					sc->sc_capchar[0] = 
						*sc->sc_mc + SHIFTCHAR;
					sc->sc_mc = sc->sc_capchar;
				}
			}

			VSC_META_UP
		}
		break;

	case KBD_RETURN:
		GETGET(unshift)
		if ((sc->sc_vi->vi_flags & VSC_KBDMODE_SET_KEYLNM) != 0 &&
		    (*sc->sc_mc == '\r'))
			sc->sc_mc = "\r\n";

		VSC_META_UP
		break;

	case KBD_META:
	case KBD_ALTGR:
	case KBD_SCROLL:
	case KBD_CAPS:
	case KBD_SHFTLOCK:
	case KBD_CTL:
	case KBD_NONE:
	default:
		break;
	}

	return sc->sc_mc;

out:
	return ((sc->sc_vi->vi_flags & VSC_KBDMODE_SET_RAWKEY) != 0) ?
		sc->sc_Xchar : NULL;
}

/*****************************************************************
 * PCVT & SYSCON COMPAT ROUTINE.
 *****************************************************************/
#ifdef	KBD_EXT

static void
ovlinit(sc)
	struct kbd_softc *sc;
{
	int i;

	memset(sc->sc_ovltbl, 0, sizeof(Ovlkey) * OVLTBL_SIZE);
	for (i = 0; i < OVLTBL_SIZE; i++)
		sc->sc_ovltbl[i].suba = KBD_SUBT_STR;
	for (i = 0; i < KBD_TABLE_SIZE; i++)
		sc->sc_keytab[i].type &= KBD_MASK;
}

static int
getokeydef(sc, key, thisdef)
	struct kbd_softc *sc;
	u_int key;
	Ovlkey *thisdef;
{
	struct scan_code *sym;

	if (key >= KBD_TABLE_SIZE)
		return EINVAL;

	memset(thisdef, 0, sizeof(*thisdef));

	sym = &sc->sc_keytab[key];
	thisdef->keynum = key;
	thisdef->type = sym->type;

	if (sym->unshift.subtype == STR)
	{
		strncpy(thisdef->unshift, sym->unshift.what.string,
		        CODE_SIZE);
		thisdef->subu = KBD_SUBT_STR;
	}
	else
		thisdef->subu = KBD_SUBT_FNC;

	if (sym->shift.subtype == STR)
	{
		strncpy(thisdef->shift, sym->shift.what.string,
		        CODE_SIZE);
		thisdef->subs = KBD_SUBT_STR;
	}
	else
		thisdef->subs = KBD_SUBT_FNC;

	if (sym->ctrl.subtype == STR)
	{
		strncpy(thisdef->ctrl, sym->ctrl.what.string,
		        CODE_SIZE);
		thisdef->subc = KBD_SUBT_STR;
	}
	else
		thisdef->subc = KBD_SUBT_FNC;

	if (sym->unshift.subtype == STR)
	{
		strncpy(thisdef->altgr, sym->unshift.what.string,
		        CODE_SIZE);
		thisdef->suba = KBD_SUBT_STR;
	}
	else
		thisdef->suba = KBD_SUBT_FNC;

	return 0;
}

static int
getckeydef(sc, key, thisdef)
	struct kbd_softc *sc;
	u_int key;
	Ovlkey *thisdef;
{
	struct scan_code *sym;

	if (key >= KBD_TABLE_SIZE)
		return EINVAL;

	sym = &sc->sc_keytab[key];

	if (sym->type & KBD_OVERLOAD)
		*thisdef = sc->sc_ovltbl[sym->ovlindex];
	else
		getokeydef(sc, key, thisdef);

	return 0;
}

static int
rmkeydef(sc, key)
	struct kbd_softc *sc;
	int key;
{
	Ovlkey *ref;
	struct scan_code *sym;

	if (key == 0 || key >= KBD_TABLE_SIZE)
		return EINVAL;

	sym = &sc->sc_keytab[key];
	if (sym->type & KBD_OVERLOAD)
	{
		ref = &sc->sc_ovltbl[sym->ovlindex];
		ref->keynum = 0;
		ref->type = 0;
		ref->unshift[0] =
		ref->shift[0] =
		ref->ctrl[0] =
		ref->altgr[0] = 0;
		sym->type &= KBD_MASK;
	}

	return 0;
}

static int
setkeydef(sc, data)
	struct kbd_softc *sc;
	Ovlkey *data;
{
	register int i;

	if (data->keynum == 0 ||
	    data->keynum >= KBD_TABLE_SIZE ||
	    (data->type & KBD_MASK) == KBD_BREAK ||
	    (data->type & KBD_MASK) > KBD_SHFTLOCK)
		return EINVAL;

	data->unshift[KBDMAXOVLKEYSIZE] =
	data->shift[KBDMAXOVLKEYSIZE] =
	data->ctrl[KBDMAXOVLKEYSIZE] =
	data->altgr[KBDMAXOVLKEYSIZE] = 0;

	data->subu =
	data->subs =
	data->subc =
	data->suba = KBD_SUBT_STR;	/* just strings .. */

	data->type |= KBD_OVERLOAD;	/* mark overloaded */
	if (sc->sc_keytab[data->keynum].type & KBD_OVERLOAD)
	{
		i = sc->sc_keytab[data->keynum].ovlindex;
	}
	else
	{
		for (i = 0; i < OVLTBL_SIZE; i++)
			if (sc->sc_ovltbl[i].keynum == 0)
				break;
		if (i == OVLTBL_SIZE)
			return ENOSPC;	/* no space, abuse of ENOSPC(!) */
	}

	sc->sc_ovltbl[i] = *data;	/* copy new data string */
	sc->sc_keytab[data->keynum].type |= KBD_OVERLOAD;	/* mark key */
	sc->sc_keytab[data->keynum].ovlindex = i;
	return 0;
}

static void
get_usl_keymap(sc, map)
	struct kbd_softc *sc;
	keymap_t *map;
{
	int i;

	memset((caddr_t) map, 0, sizeof(keymap_t));
	map->n_keys = 0x59;	/* that many keys we know about */

	for (i = 0; i < KBD_TABLE_SIZE; i++)
	{
		Ovlkey kdef;
		u_char c;
		int j;

		getckeydef(sc, i, &kdef);
		kdef.type &= KBD_MASK;

		switch (kdef.type)
		{
		case KBD_ASCII:
		case KBD_RETURN:
			map->key[i].map[0] = kdef.unshift[0];
			map->key[i].map[1] = kdef.shift[0];
			map->key[i].map[2] = map->key[i].map[3] =
				kdef.ctrl[0];
			map->key[i].map[4] = map->key[i].map[5] =
				c = kdef.altgr[0];
			/*
			 * XXX this is a hack
			 * since we currently do not map strings to AltGr +
			 * shift, we attempt to use the unshifted AltGr
			 * definition here and try to toggle the case
			 * this should at least work for ISO8859 letters,
			 * but also for (e.g.) russian KOI-8 style
			 */
			if ((c & 0x7f) >= 0x40)
				map->key[i].map[5] = (c ^ 0x20);
			break;

		case KBD_FUNC:
			/* we are only interested in F1 thru F10 here */
			if (i >= 98 && i <= 107)
			{
				map->key[i].map[0] = i - 98 + 27;
				map->key[i].spcl = 0x80;
			}
			else if (i >= 82 && i <= 86)
			{
				/*
				 * we are only interested in F11 thru F20
				 * here
				 */
				map->key[i].map[0] = i - 98 + 27 + 10;
				map->key[i].spcl = 0x80;
			}
			break;

		case KBD_SHIFT:
			c = 2;
			goto special;
		case KBD_CAPS:
			c = 4;
			goto special;
		case KBD_NUM:
			c = 5;
			goto special;
		case KBD_SCROLL:
			c = 6;
			goto special;
		case KBD_META:
			c = 7;
			goto special;
		case KBD_CTL:
			c = 9;

special:
			for (j = 0; j < NUM_STATES; j++)
				map->key[i].map[j] = c;
			map->key[i].spcl = 0xff;
			break;

		default:
			break;
		}
	}
}

static int
vsc_kbdioctl(arg, win, cmd, data, flag)
	void *arg;
	int win;
	int cmd;
	caddr_t data;
	int flag;
{
	struct kbd_softc *sc = arg;
	int key;

	if (sc->sc_ovltbl == NULL)
	{
		sc->sc_ovltbl = malloc(sizeof(Ovlkey) * OVLTBL_SIZE,
					M_DEVBUF, M_WAITOK);
		if (sc->sc_ovltbl == NULL)
			return ENOTTY;
		ovlinit(sc);
	}

	switch (cmd)
	{
	case KBDRESET:
		kbd_reset(sc);
		vsc_kbd_set_led(sc, win);
		break;

	case KBDGTPMAT:
		*(int *) data = sc->sc_tpmrate;
		break;

	case KBDSTPMAT:
		sc->sc_tpmrate = *(int *) data;
		break;

	case KBDGREPSW:
		*(int *) data = sc->sc_repflags;
		break;

	case KBDSREPSW:
		sc->sc_repflags = (*(int *) data) & 1;
		break;

	case KBDGCKEY:
		key = ((Ovlkey *) data)->keynum;
		return getckeydef(sc, key, (Ovlkey *) data);

	case KBDSCKEY:
		key = ((Ovlkey *) data)->keynum;
		return setkeydef(sc, (Ovlkey *) data);

	case KBDGOKEY:
		key = ((Ovlkey *) data)->keynum;
		return getokeydef(sc, key, (Ovlkey *) data);

	case KBDRMKEY:
		key = *(int *) data;
		return rmkeydef(sc, key);

	case KBDDEFAULT:
		ovlinit(sc);
		break;

	case GIO_KEYMAP:
		get_usl_keymap(sc, (keymap_t *) data);
		break;

	default:
		return -1;
	}
	return 0;
}
#endif	/* KBD_EXT */

/***********************************************
 * Hardware Access Methods
 ***********************************************/
static __inline u_int8_t
kbd_read_status(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{

	return bus_space_read_1(iot, ioh, kbd_status);
}

/* recovery time: 20 micro second */
#define	KBD_RECOVERY	20

static void
kbd_wait(mu)
	int mu;
{
	extern int cold;

	if (cold)
	{
		mu = (mu * 5) / 3 + 1;
		while (mu-- > 0)
			outb(0x5f, 0);
	}
	else
		delay(mu);
}

static int
kbd_wait_ready(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	int count = 1000;

	while ((kbd_read_status(iot, ioh) & KBST_READY) == 0 && --count > 0)
		kbd_wait(1);

	return (count > 0) ? 0 : EIO;
}

static void
kbd_write_data_1(iot, ioh, data)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_char data;
{

	bus_space_write_1(iot, ioh, kbd_ctrl,
		          KBCR_TEN | KBCR_DTR | KBCR_REN | KBCR_ECL);
	kbd_wait(KBD_RECOVERY);
	bus_space_write_1(iot, ioh, kbd_data, data);
	kbd_wait(KBD_RECOVERY);
	bus_space_write_1(iot, ioh, kbd_ctrl,
			  KBCR_DTR | KBCR_REN | KBCR_ECL);
	kbd_wait(KBD_RECOVERY);
}

static void
kbd_wait_ack(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{

	/* wait for ack: 37 micro sec */
	kbd_wait(37);
}

static int
kbd_read_data(iot, ioh, cap, datap)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int cap;
	u_int8_t *datap;
{

	kbd_wait_ack(iot, ioh);

	if ((cap & KBD_CAP_CMDACCEPT) != 0 &&
	    (kbd_read_status(iot, ioh) & KBST_ERRMASK) != 0)
	{
		bus_space_write_1(iot, ioh, kbd_ctrl,
				  KBCR_DTR | KBCR_REN | KBCR_ECL);
		kbd_wait(KBD_RECOVERY);

		*datap = bus_space_read_1(iot, ioh, kbd_data);

		bus_space_write_1(iot, ioh, kbd_ctrl,
				  KBCR_DTR | KBCR_REN | KBCR_ECL);
		kbd_wait(KBD_RECOVERY);
		return EIO;
	}
	else
	{
		*datap = bus_space_read_1(iot, ioh, kbd_data);
		return 0;
	}
}

static int
kbd_write_cmd(iot, ioh, cap, cmd, flags)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int cap;
	u_char cmd;
	int flags;
{
	u_int8_t res;
	int i, rc;

	/* dummy read */
	(void) kbd_read_data(iot, ioh, cap, &res);

	/* go */
	for (i = 0; i < KBC_MAX_RETRY; i++)
	{
		kbd_write_data_1(iot, ioh, cmd);
		if (flags == NO_RESULT_CODE)
			return 0;

		rc = 0;
		do
		{
			if (kbd_wait_ready(iot, ioh) || rc++ >= KBC_MAX_RETRY)
				return EIO;

			if (kbd_read_data(iot, ioh, cap, &res) != 0)
			{
				(void) kbd_read_data(iot, ioh, cap, &res);
				break;
			}

			if (res == KBC_ANS_ACK)
				return 0;
		}
		while (res != KBC_ANS_NACK);
	}

	return EIO;
}

/* reset kbd */
#define	CTRLSET(XXX) \
	{ bus_space_write_1(iot, ioh, kbd_ctrl, (XXX)); kbd_wait(20); }

static void
kbd_reset(sc)
	struct kbd_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	CTRLSET(0);
	CTRLSET(0);
	CTRLSET(0);
	CTRLSET(0x40);
	CTRLSET(0x5e);
	CTRLSET(0x3a);
	CTRLSET(0x32);

	bus_space_write_1(iot, ioh, kbd_ctrl, KBCR_DTR | KBCR_REN | KBCR_ECL);
	kbd_wait(5000); /* must be in 3 msec <-> 10 msec */
	kbd_write_cmd(iot, ioh, sc->sc_cap | KBD_CAP_CMDACCEPT,
		      0, NO_RESULT_CODE);
	kbd_wait(50000); /* must wait for the ready status: 50 m sec */

	/* status update */
	sc->sc_vi->vi_leds = sc->sc_shifts = 0;
#ifdef	KBD_EXT
	if (sc->sc_ovltbl != NULL)
		ovlinit(sc);
#endif	/* KBD_EXT */
}

static int
kbd_control_led(sc, howto)
	struct kbd_softc *sc;
	int howto;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int cmd;
	u_int8_t res;
	int error = 0;

	if ((sc->sc_cap & KBD_CAP_CMDACCEPT) == 0)
		return 0;

	error = kbd_write_cmd(iot, ioh, sc->sc_cap, KBC_CMD_LED,
			      WITH_RESULT_CODE);
	cmd = BASELED;
	if (sc->sc_vi->vi_leds & LED_KANA)
		cmd |= CANALED;
	if (sc->sc_vi->vi_leds & LED_CAP)
		cmd |= CAPSLED;
	if (sc->sc_vi->vi_leds & LED_NUM)
		cmd |= NUMLED;

	if (error && howto == KBDC_LED_ERREXIT)
		return error;

	error = kbd_write_cmd(iot, ioh, sc->sc_cap, cmd, WITH_RESULT_CODE);
	if (howto != KBDC_LED_DRAIN)
		return error;

	(void) kbd_read_status(iot, ioh);
	kbd_wait_ack(iot, ioh);	
	(void) kbd_read_data(iot, ioh, sc->sc_cap, &res);
	return error;
}

/***********************************************
 * Kbd demo
 ***********************************************/
#define	KBD_DEMONSTRATE_WAITCYCLE	(80000)
#define	KBD_DEMONSTRATE_NTIMES		(3)

static void
kbd_demonstrate(sc)
	struct kbd_softc *sc;
{
	int win = sc->sc_cwin;
	int i;

	for (i = 0; i < KBD_DEMONSTRATE_NTIMES; i++)
	{
		sc->sc_vi->vi_leds = LED_CAP;
		vsc_kbd_set_led(sc, win);
		kbd_wait(KBD_DEMONSTRATE_WAITCYCLE);
		sc->sc_vi->vi_leds = LED_KANA;
		vsc_kbd_set_led(sc, win);
		kbd_wait(KBD_DEMONSTRATE_WAITCYCLE);
		sc->sc_vi->vi_leds = LED_NUM;
		vsc_kbd_set_led(sc, win);
		kbd_wait(KBD_DEMONSTRATE_WAITCYCLE);
		sc->sc_vi->vi_leds = 0;
		vsc_kbd_set_led(sc, win);
		kbd_wait(KBD_DEMONSTRATE_WAITCYCLE);
	}
	kbd_reset(sc);
}

/***********************************************
 * Kbd Code Table
 ***********************************************/
#define	C	(u_char *)
#define	V	(void *)
#define	S	STR
#define	F	FNC
#define	I	IDX0

static struct scan_code alt_del_def = {
	KBD_KP, I, {S, {C "\177"}}, {S, {C "\177"}}, {S, {C "\177"}}
};

static struct scan_code alt_bs_def = {
	KBD_ASCII, I, {S, {C "\010"}}, {S, {C "\010"}}, {S, {C "\010"}}
};

struct scan_code default_key_code[KBD_TABLE_SIZE] = {
{KBD_ASCII, I, {S, {C "\033"}}, {S, {C "\033"}}, {S, {C "\033"}}}, /* 0 ESC */
{KBD_ASCII, I, {S, {C "1"}}, {S, {C "!"}}, {S, {C "!"}}},	/* 1 1 */
{KBD_ASCII, I, {S, {C "2"}}, {S, {C "\""}}, {S, {C "\032"}}},	/* 2 2 */
{KBD_ASCII, I, {S, {C "3"}}, {S, {C "#"}}, {S, {C "\033"}}},	/* 3 3 */
{KBD_ASCII, I, {S, {C "4"}}, {S, {C "$"}}, {S, {C "\034"}}},	/* 4 4 */
{KBD_ASCII, I, {S, {C "5"}}, {S, {C "%"}}, {S, {C "\035"}}},	/* 5 5 */
{KBD_ASCII, I, {S, {C "6"}}, {S, {C "&"}}, {S, {C "\036"}}},	/* 6 6 */
{KBD_ASCII, I, {S, {C "7"}}, {S, {C "'"}}, {S, {C "\037"}}},	/* 7 7 */
{KBD_ASCII, I, {S, {C "8"}}, {S, {C "("}}, {S, {C "\177"}}},	/* 8 8 */
{KBD_ASCII, I, {S, {C "9"}}, {S, {C ")"}}, {S, {C "9"}}},	/* 9 9 */
{KBD_ASCII, I, {S, {C "0"}}, {S, {C "\000"}}, {S, {C "0"}}},	/* 10 0 */
{KBD_ASCII, I, {S, {C "-"}}, {S, {C "="}}, {S, {C "-"}}},	/* 11 - */
{KBD_ASCII, I, {S, {C "^"}}, {S, {C "`"}}, {S, {C "\036"}}},	/* 12 ^ */
{KBD_ASCII, I, {S, {C "\\"}}, {S, {C "|"}}, {S, {C "\034"}}},	/* 13 \ */
{KBD_ASCII, I, {S, {C "\177"}}, {S, {C "\010"}}, {S, {C "\177"}}},/* 14 bs */
{KBD_ASCII, I, {S, {C "\t"}}, {S, {C "\t"}}, {S, {C "\t"}}},	/* 15 tab */
{KBD_ASCII, I, {S, {C "q"}}, {S, {C "Q"}}, {S, {C "\021"}}},	/* 16 q */
{KBD_ASCII, I, {S, {C "w"}}, {S, {C "W"}}, {S, {C "\027"}}},	/* 17 w */
{KBD_ASCII, I, {S, {C "e"}}, {S, {C "E"}}, {S, {C "\005"}}},	/* 18 e */
{KBD_ASCII, I, {S, {C "r"}}, {S, {C "R"}}, {S, {C "\022"}}},	/* 19 r */
{KBD_ASCII, I, {S, {C "t"}}, {S, {C "T"}}, {S, {C "\024"}}},	/* 20 t */
{KBD_ASCII, I, {S, {C "y"}}, {S, {C "Y"}}, {S, {C "\031"}}},	/* 21 y */
{KBD_ASCII, I, {S, {C "u"}}, {S, {C "U"}}, {S, {C "\025"}}},	/* 22 u */
{KBD_ASCII, I, {S, {C "i"}}, {S, {C "I"}}, {S, {C "\011"}}},	/* 23 i */
{KBD_ASCII, I, {S, {C "o"}}, {S, {C "O"}}, {S, {C "\017"}}},	/* 24 o */
{KBD_ASCII, I, {S, {C "p"}}, {S, {C "P"}}, {S, {C "\020"}}},	/* 25 p */
{KBD_ASCII, I, {S, {C "@"}}, {S, {C "~"}}, {S, {C "\000"}}},	/* 26 @ */
{KBD_ASCII, I, {S, {C "["}}, {S, {C "{"}}, {S, {C "\033"}}},	/* 27 [ */
{KBD_RETURN, I, {S, {C "\r"}}, {S, {C "\r"}}, {S, {C "\n"}}},	/* 28 return */
{KBD_ASCII, I, {S, {C "a"}}, {S, {C "A"}}, {S, {C "\001"}}},	/* 29 a */
{KBD_ASCII, I, {S, {C "s"}}, {S, {C "S"}}, {S, {C "\023"}}},	/* 30 s */
{KBD_ASCII, I, {S, {C "d"}}, {S, {C "D"}}, {S, {C "\004"}}},	/* 31 d */
{KBD_ASCII, I, {S, {C "f"}}, {S, {C "F"}}, {S, {C "\006"}}},	/* 32 f */
{KBD_ASCII, I, {S, {C "g"}}, {S, {C "G"}}, {S, {C "\007"}}},	/* 33 g */
{KBD_ASCII, I, {S, {C "h"}}, {S, {C "H"}}, {S, {C "\010"}}},	/* 34 h */
{KBD_ASCII, I, {S, {C "j"}}, {S, {C "J"}}, {S, {C "\n"}}},	/* 35 j */
{KBD_ASCII, I, {S, {C "k"}}, {S, {C "K"}}, {S, {C "\013"}}},	/* 36 k */
{KBD_ASCII, I, {S, {C "l"}}, {S, {C "L"}}, {S, {C "\014"}}},	/* 37 l */
{KBD_ASCII, I, {S, {C ";"}}, {S, {C "+"}}, {S, {C ";"}}},	/* 38 ; */
{KBD_ASCII, I, {S, {C ":"}}, {S, {C "*"}}, {S, {C ":"}}},	/* 39 : */
{KBD_ASCII, I, {S, {C "]"}}, {S, {C "}"}}, {S, {C "\035"}}},	/* 40 ' */
{KBD_ASCII, I, {S, {C "z"}}, {S, {C "Z"}}, {S, {C "\032"}}},	/* 41 z */
{KBD_ASCII, I, {S, {C "x"}}, {S, {C "X"}}, {S, {C "\030"}}},	/* 42 x */
{KBD_ASCII, I, {S, {C "c"}}, {S, {C "C"}}, {S, {C "\003"}}},	/* 43 c */
{KBD_ASCII, I, {S, {C "v"}}, {S, {C "V"}}, {S, {C "\026"}}},	/* 44 v */
{KBD_ASCII, I, {S, {C "b"}}, {S, {C "B"}}, {S, {C "\002"}}},	/* 45 b */
{KBD_ASCII, I, {S, {C "n"}}, {S, {C "N"}}, {S, {C "\016"}}},	/* 46 n */
{KBD_ASCII, I, {S, {C "m"}}, {S, {C "M"}}, {S, {C "\r"}}},	/* 47 m */
{KBD_ASCII, I, {S, {C ","}}, {S, {C "<"}}, {S, {C "<"}}},	/* 48 , */
{KBD_ASCII, I, {S, {C "."}}, {S, {C ">"}}, {S, {C ">"}}},	/* 49 . */
{KBD_ASCII, I, {S, {C "/"}}, {S, {C "?"}}, {S, {C "\177"}}},	/* 50 / */
{KBD_ASCII, I, {S, {C "\000"}}, {S, {C "_"}}, {S, {C "\037"}}},	/* 51 _ */
{KBD_ASCII, I, {S, {C " "}}, {S, {C " "}}, {S, {C "\000"}}},	/* 52 space */
{KBD_KP, I, {S, {C "\033[8~"}}, {S, {C "\033O8~"}}, {S, {C "\033O8~"}}},	/* 53 xfer */
{KBD_CURSOR, I, {S, {C "\033[5~"}}, {S, {C "\033[5~"}}, {S, {C "\033[5~"}}},	/* 54 R Up */
{KBD_CURSOR, I, {S, {C "\033[6~"}}, {S, {C "\033[6~"}}, {S, {C "\033[6~"}}},	/* 55 R Down */
{KBD_KP, I, {S, {C "\033[2~"}}, {S, {C "\033[2~"}}, {S, {C "\033[2~"}}},	/* 56 INS */
{KBD_KP, I, {S, {C "\033[3~"}}, {S, {C "\033[3~"}}, {S, {C "\033[3~"}}},	/* 57 DEL */
{KBD_CURSOR, I, {S, {C "\033[A"}}, {S, {C "\033OA"}}, {S, {C "\033[A"}}},	/* 58 u-arrow */
{KBD_CURSOR, I, {S, {C "\033[D"}}, {S, {C "\033OD"}}, {S, {C "\033[D"}}},	/* 59 l-arrow */
{KBD_CURSOR, I, {S, {C "\033[C"}}, {S, {C "\033OC"}}, {S, {C "\033[C"}}},	/* 60 r-arrow */
{KBD_CURSOR, I, {S, {C "\033[B"}}, {S, {C "\033OB"}}, {S, {C "\033[B"}}},	/* 61 d-arrow */
{KBD_KP, I, {S, {C "\033[1~"}}, {S, {C "\033[1~"}}, {S, {C "\033[1~"}}},	/* 62 home/clr */
{KBD_KP, I, {S, {C "\033[7~"}}, {S, {C "\033[7~"}}, {S, {C "\033[7~"}}},	/* 63 help */
{KBD_KP, I, {S, {C "-"}}, {S, {C "\033Om"}}, {S, {C "-"}}},	/* 64 - */
{KBD_KP, I, {S, {C "/"}}, {S, {C "/"}}, {S, {C "/"}}},		/* 65 / */
{KBD_KP, I, {S, {C "7"}}, {S, {C "\033Ow"}}, {S, {C "7"}}},	/* 66 7 */
{KBD_KP, I, {S, {C "8"}}, {S, {C "\033Ox"}}, {S, {C "8"}}},	/* 67 8 */
{KBD_KP, I, {S, {C "9"}}, {S, {C "\033Oy"}}, {S, {C "9"}}},	/* 68 9 */
{KBD_KP, I, {S, {C "*"}}, {S, {C "*"}}, {S, {C "*"}}},		/* 69 * */
{KBD_KP, I, {S, {C "4"}}, {S, {C "\033Ot"}}, {S, {C "4"}}},	/* 70 4 */
{KBD_KP, I, {S, {C "5"}}, {S, {C "\033Ou"}}, {S, {C "5"}}},	/* 71 5 */
{KBD_KP, I, {S, {C "6"}}, {S, {C "\033Ov"}}, {S, {C "6"}}},	/* 72 6 */
{KBD_KP, I, {S, {C "+"}}, {S, {C "+"}}, {S, {C "+"}}},		/* 73 + */
{KBD_KP, I, {S, {C "1"}}, {S, {C "\033Oq"}}, {S, {C "1"}}},	/* 74 1 */
{KBD_KP, I, {S, {C "2"}}, {S, {C "\033Or"}}, {S, {C "2"}}},	/* 75 2 */
{KBD_KP, I, {S, {C "3"}}, {S, {C "\033Os"}}, {S, {C "3"}}},	/* 76 3 */
{KBD_KP, I, {S, {C "="}}, {S, {C "="}}, {S, {C "="}}},		/* 77 = */
{KBD_KP, I, {S, {C "0"}}, {S, {C "\033Op"}}, {S, {C "0"}}},	/* 78 0 */
{KBD_KP, I, {S, {C ","}}, {S, {C ","}}, {S, {C ","}}},		/* 79 , */
{KBD_KP, I, {S, {C "."}}, {S, {C "\033On"}}, {S, {C "."}}},	/* 80 . */
{KBD_KP, I, {S, {C "\033[9~"}}, {S, {C "\033O9~"}}, {S, {C "\033O9~"}}},	/* 81 nfer */
{KBD_FUNC, I, {S, {C "\033[W"}}, {S, {C "\033OW"}}, {S, {C "\033[W"}}},	/* 82 f11 */
{KBD_FUNC, I, {S, {C "\033[X"}}, {S, {C "\033OX"}}, {S, {C "\033[X"}}},	/* 83 f12 */
{KBD_FUNC, I, {S, {C "\033[Y"}}, {S, {C "\033OY"}}, {S, {C "\033[Y"}}},	/* 84 f13 */
{KBD_FUNC, I, {S, {C "\033[Z"}}, {S, {C "\033OZ"}}, {S, {C "\033[Z"}}},	/* 85 f14 */
{KBD_FUNC, I, {S, {C "\033[a"}}, {S, {C "\033Oa"}}, {S, {C "\033[a"}}},	/* 86 f15 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 87  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 88  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 89  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 90  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 91  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 92  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 93  */
{KBD_KP, I, {S, {C "\033[1~"}}, {S, {C "\033[1~"}}, {S, {C "\033[1~"}}},	/* 94 home */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 95  */
{KBD_KP, I, {S, {C "\033[0~"}}, {S, {C "\033[0~"}}, {S, {C "\033[0~"}}},	/* 96 stop */
{KBD_KP, I, {S, {C "\033[4~"}}, {S, {C "\033[4~"}}, {S, {C "\033[4~"}}},	/* 97 copy */
{KBD_FUNC, I, {S, {C "\033[M"}}, {S, {C "\033OM"}}, {S, {C "\033[M"}}},	/* 98 f1 */
{KBD_FUNC, I, {S, {C "\033[N"}}, {S, {C "\033ON"}}, {S, {C "\033[N"}}},	/* 99 f2 */
{KBD_FUNC, I, {S, {C "\033[O"}}, {S, {C "\033OO"}}, {S, {C "\033[O"}}},	/* 100 f3 */
{KBD_FUNC, I, {S, {C "\033[P"}}, {S, {C "\033OP"}}, {S, {C "\033[P"}}},	/* 101 f4 */
{KBD_FUNC, I, {S, {C "\033[Q"}}, {S, {C "\033OQ"}}, {S, {C "\033[Q"}}},	/* 102 f5 */
{KBD_FUNC, I, {S, {C "\033[R"}}, {S, {C "\033OR"}}, {S, {C "\033[R"}}},	/* 103 f6 */
{KBD_FUNC, I, {S, {C "\033[S"}}, {S, {C "\033OS"}}, {S, {C "\033[S"}}},	/* 104 f7 */
{KBD_FUNC, I, {S, {C "\033[T"}}, {S, {C "\033OT"}}, {S, {C "\033[T"}}},	/* 105 f8 */
{KBD_FUNC, I, {S, {C "\033[U"}}, {S, {C "\033OU"}}, {S, {C "\033[U"}}},	/* 106 f9 */
{KBD_FUNC, I, {S, {C "\033[V"}}, {S, {C "\033OV"}}, {S, {C "\033[V"}}},	/* 107 f10 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 108  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 109  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 110  */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 111  */
{KBD_SHIFT, I, {S, {C " "}}, {S, {C " "}}, {S, {C " "}}},	/* 112 shift */
{KBD_CAPS, I, {S, {C " "}}, {S, {C " "}}, {S, {C " "}}},	/* 113 caps */
{KBD_KANA, I, {S, {C " "}}, {S, {C " "}}, {S, {C " "}}},	/* 114 kana */
{KBD_META, I, {S, {C " "}}, {S, {C " "}}, {S, {C " "}}},	/* 115 graph */
{KBD_CTL, I, {S, {C " "}}, {S, {C " "}}, {S, {C " "}}},	/* 116 control */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 117 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 118 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 119 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 120 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 121 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 122 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 123 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 124 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 125 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 126 */
{KBD_NONE, I, {S, {C ""}}, {S, {C ""}}, {S, {C ""}}},	/* 127 */
};
#undef	C
#undef	V
#undef	S
#undef	F
#undef	I

