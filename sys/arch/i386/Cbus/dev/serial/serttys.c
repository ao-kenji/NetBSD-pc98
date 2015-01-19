/*	$NecBSD: serttys.c,v 1.4 1999/07/23 20:57:31 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff.  All rights reserved.
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
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/vnode.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/servar.h>
#include <i386/Cbus/dev/serial/sersubr.h>
#include <i386/Cbus/dev/serial/ser_console.h>
#include <i386/Cbus/dev/serial/serio.h>

static int serparam __P((struct tty *, struct termios *));
static void serstart __P((struct tty *));
static void serpoll __P((void *));

/* followings are kernel interface compatiblity names */
int comopen __P((dev_t, int, int, struct proc *));
int comclose __P((dev_t, int, int, struct proc *));
int comread __P((dev_t, struct uio *, int));
int comwrite __P((dev_t, struct uio *, int));
struct tty *comtty __P((dev_t));
int comioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
void comstop __P((struct tty *, int));
void comsoft __P((void *));

/* attach probe interfaces */
static int ttycuamatch __P((struct device *, struct cfdata *, void *));
static int ttycommatch __P((struct device *, struct cfdata *, void *));
static void ttyattachsubr __P((struct device *, struct device *, void *));
static void ttyinitsubr __P((struct com_softc *, struct commulti_attach_args *));

static int serttymatch __P((struct device *, struct cfdata *, void *));
void serttyattach __P((struct device *, struct device *, void *));
static int sertty_activate __P((struct com_softc *));
static int sertty_deactivate __P((struct com_softc *));

struct cfattach sertty_ca = {
	sizeof(struct device), serttymatch, serttyattach
};

extern struct cfdriver sertty_cd;

struct cfattach ttycua_ca = {
	sizeof(struct sertty_softc), ttycuamatch, ttyattachsubr
};

extern struct cfdriver ttycua_cd;

struct cfattach ttycom_ca = {
	sizeof(struct sertty_softc), ttycommatch, ttyattachsubr
};

extern struct cfdriver ttycom_cd;

TAILQ_HEAD(ser_actque, com_softc);
static struct ser_actque sertab;

/****************************************************
 * sertty probe attach
 ****************************************************/
static int
serttymatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{

	return 1;
}

void
serttyattach(parent, match, aux)
	struct device *parent, *match;
	void *aux;
{
	static int inited;
	struct commulti_attach_args *ca = aux;
	struct com_softc *sc = (struct com_softc *) ca->ca_m;
	struct sertty_attach_args cta;
	int id;

	if (inited == 0)
	{
		int s = spltty();
		TAILQ_INIT(&sertab);
		inited = 1;
		splx(s);
	}

	ttyinitsubr(sc, ca);
	for (id = 0; id < COM_MAXTTY; id ++)
	{
		cta.ca_sc = sc;
		cta.ca_id = id;
		config_found((void *) match, &cta, NULL);
	}

	sc->sc_enable = 1;
}

/****************************************************
 * tty lines probe attach
 ****************************************************/
static int
ttycommatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct sertty_attach_args *ca = aux;

	return  (ca->ca_id == 0);
}

static int
ttycuamatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct sertty_attach_args *ca = aux;

	return (ca->ca_id == 1);
}

void
ttyattachsubr(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sertty_softc *ts = (struct sertty_softc *) self;
	struct sertty_attach_args *ca = aux;
	struct com_softc *sc;
	int s;

	sc = ca->ca_sc;
	ts->sc_com = sc;
	ts->sc_termios = sc->sc_termios;
	ts->sc_swflags = sc->sc_swflags;
	ts->sc_tty = ttymalloc();
	tty_attach(ts->sc_tty);

	s = spltty();
	if (sc->sc_cts == NULL)
	{
		sc->sc_cts = ts;
		sc->sc_ctty = ts->sc_tty;
	}
	sc->sc_nts ++;
	sc->sc_ts[ca->ca_id] = ts;
	splx(s);

	printf(": tty line (id %d:0x%lx)\n", ca->ca_id,
		(u_long) ts->sc_tty);
}

static void
ttyinitsubr(sc, ca)
	struct com_softc *sc;
	struct commulti_attach_args *ca;
{
	struct com_switch *cswp;

	cswp = sc->sc_cswp;
	sc->sc_swflags = 0;
	sc->sc_termios.c_iflag = TTYDEF_IFLAG;
	sc->sc_termios.c_oflag = TTYDEF_OFLAG;
	sc->sc_termios.c_cflag = TTYDEF_CFLAG;
	sc->sc_termios.c_lflag = TTYDEF_LFLAG;
	sc->sc_termios.c_ospeed = cswp->cswt_speed;
	sc->sc_termios.c_ispeed = sc->sc_termios.c_ospeed;

	sc->sc_sertty_activate = sertty_activate;
	sc->sc_sertty_deactivate = sertty_deactivate;

	sc->sc_rdata = malloc(COM_IBUFSIZE, M_DEVBUF, M_NOWAIT);
	sc->sc_rstat = malloc(COM_IBUFSIZE, M_DEVBUF, M_NOWAIT);
	if (sc->sc_rdata == NULL || sc->sc_rstat == NULL)
		panic("%s: no data buffer", sc->sc_dev.dv_xname);

	rb_init(&sc->sc_invrb, COM_IBUFSIZE);
	sc->ob_cc = -1;

	if (ca->ca_console != 0) 
		ser_connect_comconsole(sc);

	(*cswp->cswt_hwattach) (sc);

	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
		delay(1000);
}

/****************************************************
 * device number inline
 ****************************************************/
static __inline struct sertty_softc *serttysoftc __P((dev_t));
static __inline int serttyvalid __P((dev_t));

static __inline int
serttyvalid(dev)
	dev_t dev;
{

	if (dev & 0x80)
		return ((dev & 0x7f) < ttycua_cd.cd_ndevs);
	else
		return ((dev & 0x7f) < ttycom_cd.cd_ndevs);
}

static __inline struct sertty_softc *
serttysoftc(dev)
	dev_t dev;
{
	if (dev & 0x80)
		return (struct sertty_softc *) (ttycua_cd.cd_devs[dev & 0x7f]);
	else
		return (struct sertty_softc *) (ttycom_cd.cd_devs[dev & 0x7f]);
}

/****************************************************
 * declare
 ****************************************************/
static void sertty_ins_actq __P((struct com_softc *));
static void sertty_rem_actq __P((struct com_softc *));
static void sertty_reopen __P((struct com_softc *, struct tty *));
static int sertty_busy __P((struct com_softc *));
static int sertty_switch __P((struct com_softc *, int));
static void sertty_enter_leave __P((struct com_softc *, struct sertty_softc *, struct sertty_softc *));
static void sertty_close __P((struct com_softc *, struct sertty_softc *, int));

static void serdrain __P((struct com_softc *, int));
static int serhw_activate __P((struct com_softc *));
static int serhw_deactivate __P((struct com_softc *));
static int serhw_wait_carrier __P((struct com_softc *, struct sertty_softc *));
static __inline void serhw_buffer_init __P((struct com_softc *, struct tty *));
static int serhw_flow __P((struct tty *, int));

/****************************************************
 * activate, deactivate
 ****************************************************/
#define ttyline_active(ts) ((ts)->sc_openf || (ts)->sc_wopen)
#define	ttyline_open(ts) ((ts)->sc_openf)

static int
sertty_deactivate(sc)
	struct com_softc *sc;
{
	struct tty *tp;

	sc->sc_enable = 0;
	if ((tp = sc->sc_ctty) == NULL || 
	    (tp->t_wopen == 0 && ISSET(tp->t_state, TS_ISOPEN) == 0))
		return 0;

	serhw_deactivate(sc);
	return 0;
}

static int
sertty_activate(sc)
	struct com_softc *sc;
{
	struct tty *tp;

	sc->sc_enable = 1;
	if ((tp = sc->sc_ctty) == NULL || 
	    (tp->t_wopen == 0 && ISSET(tp->t_state, TS_ISOPEN) == 0))
		return 0;

	sertty_reopen(sc, tp);
	return 0;
}

static int
sertty_busy(sc)
	struct com_softc *sc;
{
	register int i;
	register struct sertty_softc *ts;

	for (i = 0; i < COM_MAXTTY; i ++)
	{
		ts = sc->sc_ts[i];
		if (ts != NULL && ttyline_active(ts))
			return 1;
	}
	return 0;
}

/****************************************************
 * sertty switch
 ****************************************************/
static void
sertty_enter_leave(sc, ots, nts)
	struct com_softc *sc;
	struct sertty_softc *ots, *nts;
{
	int s;

	if (ots == nts)
		return;

	ots->sc_swflags = sc->sc_swflags;
	sc->sc_swflags = nts->sc_swflags;

	s = splhigh();
	sc->sc_ctty = nts->sc_tty;
	sc->sc_cts = nts;
	splx(s);
}

static int
sertty_switch(sc, tick)
	struct com_softc *sc;
	int tick;
{
	register int i;
	register struct sertty_softc *ts = NULL;
	int s;

	if (tick > 0)
		tsleep((caddr_t) sc, TTIPRI, "com_sw", tick);

	if (ISSET(sc->sc_cflags, COM_TTY_WANTED))
	{
		CLR(sc->sc_cflags, COM_TTY_WANTED);
		wakeup((caddr_t) &sc->sc_ctty);
	}

	for (i = 0; i < COM_MAXTTY; i ++)
	{
		ts = sc->sc_ts[i];
		if (ts != NULL && ts->sc_com == sc && ttyline_open(ts))
			goto found;
	}

	for (i = 0; i < COM_MAXTTY; i ++)
	{
		ts = sc->sc_ts[i];
		if (ts != NULL && ts->sc_com == sc && ttyline_active(ts))
			goto found;
	}
	return 0;

found:
	s = spltty();
	sertty_enter_leave(sc, sc->sc_cts, ts);
	sertty_reopen(sc, ts->sc_tty);
	splx(s);
	return 1;
}

static void
sertty_reopen(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{

	/* reopen hardware */
	(*tp->t_param) (tp, &tp->t_termios);
	serhw_activate(sc);

	/* wakeup a waitor if carrier on */
	if (ISSET(tp->t_state, TS_CARR_ON) && tp->t_wopen > 0)
		ttwakeup(tp);

	/* restart output */
	if (ISSET(tp->t_state, TS_ISOPEN))
		setsofttty();
}

/****************************************************
 * open
 ****************************************************/
int
comopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct sertty_softc *ts;
	struct com_softc *sc;
	struct tty *tp;
	int s, error = 0;

	if (serttyvalid(dev) == 0)
		return ENXIO;
	ts = serttysoftc(dev);
	if (ts == NULL)
		return ENXIO;

	sc = ts->sc_com;
	if (sc->sc_enable == 0)
		return ENXIO;
	tp = ts->sc_tty;

#ifdef	KGDB
	if (ISSET(sc->sc_hwflags, COM_HW_KGDB))
		return EBUSY;
#endif	/* KGDB */

	s = spltty();
	while (sc->sc_cts != ts && ttyline_open(sc->sc_cts))
	{
		/* XXX */
		if (ISSET(flag, O_NONBLOCK))
		{
			splx(s);
			return EBUSY;
		}

		SET(sc->sc_cflags, COM_TTY_WANTED);
		error = tsleep((caddr_t) &sc->sc_ctty,
			       TTIPRI | PCATCH, "comopen", 0);
		if (error)
		{
			splx(s);
			return error;
		}
	}

	sertty_enter_leave(sc, sc->sc_cts, ts);

	tp->t_oproc = serstart;
	tp->t_param = serparam;
	tp->t_hwiflow = serhw_flow;
	tp->t_dev = dev;

	if (!ttyline_open(ts))
	{
		ttychars(tp);

		tp->t_iflag = ts->sc_termios.c_iflag;
		tp->t_oflag = ts->sc_termios.c_oflag;
		tp->t_cflag = ts->sc_termios.c_cflag;
		tp->t_lflag = ts->sc_termios.c_lflag;
		if (ts->sc_termios.c_ispeed <= B230400)
		{
			tp->t_ispeed = ts->sc_termios.c_ispeed;
			tp->t_ospeed = ts->sc_termios.c_ospeed;
		}
		else
		{
			tp->t_ispeed = B230400;
			tp->t_ospeed = B230400;
		}

		if (ISSET(sc->sc_swflags, COM_SW_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, COM_SW_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, COM_SW_MDMBUF))
			SET(tp->t_cflag, MDMBUF);

		serparam(tp, &tp->t_termios);
		ttsetwater(tp);

		serhw_activate(sc);
		if (!sertty_busy(sc))
			sertty_ins_actq(sc);
	}
	else if (ISSET(tp->t_state, TS_XCLUDE) && p->p_ucred->cr_uid != 0)
	{
		splx(s);
		return EBUSY;
	}

	if (ISSET(flag, O_NONBLOCK) == 0)
		error = serhw_wait_carrier(sc, ts);
	splx(s);

	if (error == 0)
	{
		error = (*linesw[tp->t_line].l_open)(dev, tp);
		if (error == 0)
		{
			if (ts->sc_wopen == 0)
				ts->sc_openf = 1;
			return error;
		}
	}

	if (!ttyline_active(ts))
		sertty_close(sc, ts, ISSET(tp->t_state, TS_ISOPEN));

	sertty_switch(sc, 0);
	return error;
}

/****************************************************
 * close
 ****************************************************/
#define	TIMEOUT_FACTOR	2
#define DRAIN_TIMEOUT	5

static void
serdrain(sc, tout)
	struct com_softc *sc;
	int tout;
{
	struct com_switch *cswp = sc->sc_cswp;
	int wait, error, s;
 
	if (cswp->cswt_hwdrain == NULL)
		return;

	s = spltty();
	for (wait = 0; wait < tout * TIMEOUT_FACTOR; wait ++)
	{
		error = (*cswp->cswt_hwdrain) (sc, 0);
		if (error <= 0)
			break;

		error = tsleep((caddr_t) sc, TTIPRI | PCATCH, "serdrain",
			       hz / TIMEOUT_FACTOR);
		if (error == EINTR)
			break;
	}

	(*cswp->cswt_hwdrain) (sc, 1);
	(void) splcom();
	sc->ob_cc = -1;
	sc->sc_cc = 0;
	splx(s);
}

static void
sertty_close(sc, ts, fclose)
	struct com_softc *sc;
	struct sertty_softc *ts;
	int fclose;
{
	struct tty *tp = ts->sc_tty;
	int s = spltty();

	ts->sc_openf = 0;
	serhw_deactivate(sc);
	if (sertty_busy(sc) == 0)
		sertty_rem_actq(sc);
	splx(s);
	if (fclose != 0)
		ttyclose(tp);
}

int
comclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct sertty_softc *ts = serttysoftc(dev);
	struct com_softc *sc = ts->sc_com;
	struct tty *tp = ts->sc_tty;
	int isopn;

	isopn = ISSET(tp->t_state, TS_ISOPEN);
	if (isopn != 0)
	{
		int drain_timeout;

		sc->sc_state = COM_STATE_CLOSING;
		(*linesw[tp->t_line].l_close)(tp, flag);

		if (sc->sc_state == COM_STATE_CLOSING_TIMEOUT)
			drain_timeout = 0;
		else
			drain_timeout = DRAIN_TIMEOUT;

		sc->sc_state = COM_STATE_DRAINING;
		serdrain(sc, drain_timeout);

		sc->sc_state = COM_STATE_CLOSED;
		sc->sc_counter = 0;
 	}

	sertty_close(sc, ts, isopn);
	sertty_switch(sc, hz / 2);
	return 0;
}

/****************************************************
 * read write
 ****************************************************/
int
comread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct sertty_softc *ts = serttysoftc(dev);
	register struct tty *tp = ts->sc_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
comwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct sertty_softc *ts = serttysoftc(dev);
	register struct tty *tp = ts->sc_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
comtty(dev)
	dev_t dev;
{
	register struct sertty_softc *ts = serttysoftc(dev);

	return ts->sc_tty;
}

/****************************************************
 * ioctl
 ****************************************************/
static struct speedtab serspeedtab[] = {
	{1200, 1200},
	{2400, 2400},
	{4800, 4800},
	{9600, 9600},
	{14400, 14400},
	{19200, 19200},
	{20800, 20800},
	{28800, 28800},
	{38400, 38400},
	{41600, 41600},
	{76800, 76800},
	{57600, 57600},
	{115200 * 1, 115200 * 1},
	{115200 * 2, 115200 * 2},
	{115200 * 4, 115200 * 4},
	{115200 * 8, 115200 * 8},
	{-1,	-1},
};

int
comioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct sertty_softc *ts = serttysoftc(dev);
	register struct tty *tp = ts->sc_tty;
	struct com_switch *cswp;
	struct com_softc *sc;
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	sc = ts->sc_com;
 	cswp = sc->sc_cswp;
	error = 0;
	switch(cmd)
	{
	case TIOCGFLAGS:
	{
		int userbits = 0;
		int driverbits = ts->sc_swflags;

		driverbits = sc->sc_swflags;
		if (ISSET(driverbits, COM_SW_SOFTCAR))
			SET(userbits, TIOCFLAG_SOFTCAR);
		if (ISSET(driverbits, COM_SW_CLOCAL))
			SET(userbits, TIOCFLAG_CLOCAL);
		if (ISSET(driverbits, COM_SW_CRTSCTS))
			SET(userbits, TIOCFLAG_CRTSCTS);
		if (ISSET(driverbits, COM_SW_MDMBUF))
			SET(userbits, TIOCFLAG_MDMBUF);

		*(int *)data = userbits;
		break;
	}

	case TIOCSFLAGS:
	{
		int userbits, driverbits = 0;

		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0)
			return(EPERM);

		userbits = *(int *)data;
		if (ISSET(userbits, TIOCFLAG_SOFTCAR) ||
		    ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			SET(driverbits, COM_SW_SOFTCAR);
		if (ISSET(userbits, TIOCFLAG_CLOCAL))
			SET(driverbits, COM_SW_CLOCAL);
		if (ISSET(userbits, TIOCFLAG_CRTSCTS))
			SET(driverbits, COM_SW_CRTSCTS);
		if (ISSET(userbits, TIOCFLAG_MDMBUF))
			SET(driverbits, COM_SW_MDMBUF);

		ts->sc_swflags = driverbits;
		if (ts == sc->sc_cts)
			sc->sc_swflags = driverbits;
		break;
	}

#ifdef	SERHWIOCSTERMIOS
	case SERHWIOCSTERMIOS:
	{
		struct serhw_ctrl *cp = (struct serhw_ctrl *) data;

		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;

		if (cp->ser_tios.c_ospeed != cp->ser_tios.c_ispeed ||
		    ttspeedtab(cp->ser_tios.c_ispeed, serspeedtab) == -1)
		{
			error = EINVAL;
			break;
		}

		ts->sc_termios.c_cflag = cp->ser_tios.c_cflag;
		ts->sc_termios.c_ispeed = cp->ser_tios.c_ispeed;
		ts->sc_termios.c_ospeed = cp->ser_tios.c_ospeed;
		if (ISSET(cp->ser_flags, SER_CTRL_FIXED))
			SET(sc->sc_cflags, COM_MODE_FIXED);
		else
			CLR(sc->sc_cflags, COM_MODE_FIXED);
		break;
	}

	case SERHWIOCGTERMIOS:
	{
		struct serhw_ctrl *cp = (struct serhw_ctrl *) data;

		cp->ser_tios = ts->sc_termios;

		cp->ser_flags = 0;
		if (ISSET(sc->sc_cflags, COM_MODE_FIXED))
			SET(cp->ser_flags, SER_CTRL_FIXED);

		break;
	}
#endif	/* SERHWIOCSTERMIOS */

	default:
		if (ts != sc->sc_cts)
			return EBUSY;
		error = ((*cswp->cswt_ioctl)(sc, cmd, data, flag, p));
	}

	return error;
}

static int
serparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	register struct sertty_softc *ts = serttysoftc(tp->t_dev);
	register struct com_softc *sc = ts->sc_com;

	if (tp != sc->sc_ctty)
		return EBUSY;

	return ((*sc->sc_cswp->cswt_comparam) (sc, tp, t));
}

void
comstop(tp, flag)
	struct tty *tp;
	int flag;
{
	register struct sertty_softc *ts = serttysoftc(tp->t_dev);
	register struct com_softc *sc = ts->sc_com;
	int s;

	s = splcom();
	if (sc->sc_ctty == tp && ISSET(tp->t_state, TS_BUSY) != 0)
	{
		if (sc->ob_cc > 0)
		{
			sc->sc_cc = sc->sc_cc - sc->ob_cc;
			sc->ob_cc = 0;
		}

		if (ISSET(tp->t_state, TS_TTSTOP) == 0)
			SET(tp->t_state, TS_FLUSH);
	}
	splx(s);
}

/****************************************************
 * inputs outputs
 ****************************************************/
static void
serpoll(arg)
	void *arg;
{
	struct com_softc *sc;
	struct tty *tp;
	int needs_soft = 0;

	for (sc = sertab.tqh_first; sc; sc = sc->com_chain.tqe_next)
	{
		if (sc->sc_vicr == 0)
			continue;

		needs_soft = 1;

		if (sc->sc_state > 0)
		{
			tp = sc->sc_ctty;		/* watch dog */
			switch(sc->sc_state)
			{
			case COM_STATE_CLOSING:
			case COM_STATE_CLOSING_TIMEOUT:
				if (sc->sc_counter ++ > 5 * hz)
				{
					int s = spltty();

					ttyflush(tp, FWRITE);
					CLR(tp->t_state, TS_BUSY | TS_FLUSH);
					splx(s);

					sc->sc_counter = 0;
					sc->sc_state = COM_STATE_CLOSING_TIMEOUT;
				}
				break;

			default:
				break;
			}
		}
			
		if (sc->sc_ser->sc_start == NULL)
		{
			tp = sc->sc_ctty;		/* watch dog */
			if (ISSET(tp->t_state, TS_ISOPEN))
			{
				sc->sc_check = 1;
				EMUL_INTR_REQUIRED(sc);
			}
		}
	}

	if (needs_soft != 0)
		setsofttty();

	timeout(serpoll, NULL, 1);
}

static void
serstart(tp)
	struct tty *tp;
{
	register struct sertty_softc *ts = serttysoftc(tp->t_dev);
	struct com_softc *sc = ts->sc_com;
	int s;

	s = spltty();
	if (ts != sc->sc_cts || sc->sc_enable == 0 ||
	    ISSET(tp->t_state, TS_BUSY | TS_TTSTOP | TS_TIMEOUT))
		goto out;

	if (tp->t_outq.c_cc > 0 && sc->ob_cc < 0)
	{
		SET(tp->t_state, TS_BUSY);
		(*sc->sc_cswp->cswt_start) (sc, tp);
	}

	if (tp->t_outq.c_cc > tp->t_lowat)
		goto out;

	if (ISSET(tp->t_state, TS_ASLEEP))
	{
		CLR(tp->t_state, TS_ASLEEP);
		wakeup((caddr_t)&tp->t_outq);
	}

	selwakeup(&tp->t_wsel);

out:
	splx(s);
	return;
}

void
comsoft(arg)
	void *arg;
{
	struct com_switch *cswp;
	struct com_softc *sc;
	struct tty *tp;
	int s, cc;

	for (sc = sertab.tqh_first; sc; sc = sc->com_chain.tqe_next)
	{
		if (sc->sc_vicr == 0)
			continue;

		cswp = sc->sc_cswp;
		tp = sc->sc_ctty;

		cc = sc->sc_invrb.vr_cc;
		if (cc == 0)
			goto skip;
			
		if (!ISSET(tp->t_state, TS_ISOPEN))
		{
			s = splcom();
			rb_flush(&sc->sc_invrb);
			splx(s);
		}
		else
		{
			register u_int idx;
			int tcc;

			idx = rb_rop(&sc->sc_invrb);
			tcc = cc;

			do
			{
				register u_int c;
				register u_int8_t stat;

				if (ISSET(tp->t_state, TS_TBLOCK))
					break;

				c = (u_int) sc->sc_rdata[idx];
				stat = sc->sc_rstat[idx];
				if (stat == 0)
				{
					(*linesw[tp->t_line].l_rint)(c, tp);
				}
				else if (stat != RBOSTAT)
				{
					c = (*cswp->cswt_errmap) (sc, c, stat);
					if (c == RBFLUSH)
					{
						s = splcom();
						rb_flush(&sc->sc_invrb);
						splx(s);
						goto skip;
					}
					(*linesw[tp->t_line].l_rint)(c, tp);
				}
				else
					comoerr(sc, COM_SOVF);

				idx ++;
				if (idx >= sc->sc_invrb.vr_size)
					idx = 0;
			}
			while (-- tcc > 0);

			s = splcom();
			rb_adj(&sc->sc_invrb, (cc - tcc));
			splx(s);
		}


skip:
		if (ISSET(tp->t_state, TS_ISOPEN) == 0 && tp->t_wopen == 0)
			continue;

		s = spltty();
		(*cswp->cswt_modem) (sc, tp);

		if (sc->sc_txintr != 0)
		{
			sc->sc_txintr = 0;
			CLR(tp->t_state, TS_BUSY);
			if (ISSET(tp->t_state, TS_FLUSH))
			{
				CLR(tp->t_state, TS_FLUSH);
				sc->sc_cc = 0;
			}
			else if (sc->sc_cc > 0)
			{
				ndflush(&tp->t_outq, sc->sc_cc);
				sc->sc_cc = 0;
			}
			(*linesw[tp->t_line].l_start)(tp);
		}
		splx(s);
	}
}

/**************************************************
 * misc control
 *************************************************/
static void
sertty_ins_actq(sc)
	struct com_softc *sc;
{
	struct ser_softc *ssc = sc->sc_ser;
	int s = splcom();

	if (sertab.tqh_first == NULL)
		timeout(serpoll, NULL, 1);

	if (ISSET(sc->sc_cflags, COM_POLL_QUEUED) == 0)
	{
		TAILQ_INSERT_TAIL(&sertab, sc, com_chain);
		SET(sc->sc_cflags, COM_POLL_QUEUED);
	}
	sc->sc_swtref ++;

	if (ssc->sc_start != NULL)
	{
		if (ssc->sc_hwtref == 0)
			(*ssc->sc_start) (ssc);
	 	ssc->sc_hwtref ++;
	}
	splx(s);
}

static void
sertty_rem_actq(sc)
	struct com_softc *sc;
{
	struct ser_softc *ssc = sc->sc_ser;
	int s = splcom();

	if (ssc->sc_stop != NULL && ssc->sc_hwtref > 0)
	{
		-- ssc->sc_hwtref;
		if (ssc->sc_hwtref == 0)
			(*ssc->sc_stop) (ssc);
	}

	-- sc->sc_swtref;
	if (sc->sc_swtref == 0)
	{
 		if (ISSET(sc->sc_cflags, COM_POLL_QUEUED))
		{
			TAILQ_REMOVE(&sertab, sc, com_chain);
			CLR(sc->sc_cflags, COM_POLL_QUEUED);
		}
	}

	if (sertab.tqh_first == NULL)
		untimeout(serpoll, NULL);
	splx(s);
}

/****************************************************
 * sertty <=> hardware interfaces
 ****************************************************/
static int
serhw_flow(tp, flags)
	struct tty *tp;
	int flags;
{
	struct sertty_softc *ts = serttysoftc(tp->t_dev);
	struct com_softc *sc = ts->sc_com;

	if (flags == 0 && sc->sc_invrb.vr_cc > 0)
		setsofttty();
		
	return 1;
}

static __inline void
serhw_buffer_init(sc, tp)
	struct com_softc *sc;
	struct tty *tp;
{

	sc->ob_cc = -1;
	sc->sc_cc = 0;
	rb_flush(&sc->sc_invrb);
	sc->sc_txintr = 0;
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);
}

static int
serhw_activate(sc)
	struct com_softc *sc;
{
	struct tty *tp = sc->sc_ctty;
	int carrier, s;

	/* rtscts */
	sc->sc_rtscts = ISSET(tp->t_cflag, CHWFLOW);
	CLR(sc->sc_cflags, COM_RTSOFF);

	s = splcom();
	/* clear pending data */
	serhw_buffer_init(sc, tp);

	/* call hardware */
	carrier = (*sc->sc_cswp->cswt_hwactivate) (sc);

	if (sc->sc_mdcd == 0 || carrier != 0)
		SET(tp->t_state, TS_CARR_ON);
	else
		CLR(tp->t_state, TS_CARR_ON);

	/* virtual interrupt open */
	sc->sc_vicr = 1;	
	splx(s);

	return 0;
}

static int
serhw_deactivate(sc)
	struct com_softc *sc;
{
	struct tty *tp = sc->sc_ctty;
	int s, hupctl, error;

	hupctl = ISSET(tp->t_cflag, HUPCL) &&
		 (ISSET(sc->sc_swflags, COM_SW_SOFTCAR) == 0);

	s = splcom();
	sc->sc_check = 0;
	sc->sc_vicr = 0;

	serhw_buffer_init(sc, tp);	/* clear all buffered data */

	error = (*sc->sc_cswp->cswt_hwdeactivate) (sc, hupctl);
	splx(s);

	sc->sc_ospeed = 0;
	return error;
}

static int
serhw_wait_carrier(sc, ts)
	struct com_softc *sc;
	struct sertty_softc *ts;
{
	struct tty *tp = ts->sc_tty;
	struct vnode *vp;					/* XXX */
	int error;

	error = vfinddev(tp->t_dev, VCHR, &vp);			/* XXX */
	if (error == 0)						/* XXX */
		return ENXIO;					/* XXX */

	while (sc->sc_cts != ts ||
	       (ISSET(tp->t_cflag, CLOCAL | MDMBUF) == 0 &&
		ISSET(tp->t_state, TS_CARR_ON) == 0))
	{
		ts->sc_wopen ++;
		tp->t_wopen ++;
		ts->sc_openf = 0;				/* XXX */

		error = ttysleep(tp, (caddr_t) &tp->t_rawq, TTIPRI | PCATCH,
				 ttopen, 0);

		ts->sc_openf = vcount(vp) - ts->sc_wopen;	/* XXX */
		tp->t_wopen --;
		ts->sc_wopen --;
		if (error != 0)
			return error;
	}

	return 0;
}
