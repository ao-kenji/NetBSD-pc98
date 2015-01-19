/*	$NecBSD: apm.c,v 1.34.2.7 1999/08/29 02:28:11 honda Exp $	*/
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
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include "opt_systm_daemon.h"
#include "opt_vm86biosd.h"
#include "opt_apm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/lock.h>
#include <sys/kthread.h>

#include <machine/systmbusvar.h>
#include <dev/cons.h>
#include <dev/isa/isareg.h>	

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/pio.h>
#include <machine/gdt.h>
#include <machine/psl.h>
#include <machine/apmvar.h>
#include <machine/signal.h>
#include <machine/vm86bios.h>
#include <machine/isa_machdep.h>

#include <i386/isa/icu.h>
#include <i386/isa/pc98spec.h>

/******************************************************
 * control macro
 ******************************************************/
/* #define	APM_GEN_POWEVENT */

#define	APM_BUGGYINFO

#define	APM_BTCHK_INTERVAL	32

#define	APM_WAIT_TIME	(5)		/* suspend wait time */

#define	APMUNIT(dev)	(minor(dev) & 0x0f)

#define	splapm spltty

/******************************************************
 * isa attach structures
 ******************************************************/
static int apmmatch __P((struct device *, struct cfdata *, void *));
static void apmattach __P((struct device *, struct device *, void *));

struct apm_event_tag {
	TAILQ_ENTRY(apm_event_tag) aet_chain;

	struct apm_event_info aet_event;
};

TAILQ_HEAD(apm_event_taghead, apm_event_tag);

struct apm_softc {
	struct device sc_dev;

	struct lock sc_lock;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	u_int sc_majver, sc_minver;

	struct proc *sc_proc;
	pid_t sc_pid;

#define	APM_USR_SUSPEND		0x01
#define	APM_USR_STANDBY		0x02
#define	APM_USR_ACK_SUSPEND	0x04
	u_int	sc_usrreq;

	u_int	sc_ctdef;
	u_int	sc_ct;

	struct	apm_power_info sc_pow;
	u_int	sc_powt;

#define	APM_HAS_BATTERY	0x01
#define	APM_CONNECTED	0x02
	u_int	sc_flags;

#define APM_MAX_EVENTS 4
	struct apm_event_taghead sc_evahead;
	struct apm_event_taghead sc_evfhead;

	systm_event_t sc_state;		/* event state */
	int sc_create_force;		/* defered */
};

struct cfattach apm_ca = {
	sizeof(struct apm_softc), apmmatch, apmattach
};

extern struct cfdriver apm_cd;

static struct apm_softc *apm_gsc;

struct farcall {
	u_int32_t offset;
	u_int16_t seg;
} apm_farcall;

u_int apm_idleon;

/******************************************************
 * extern
 ******************************************************/
static int apm_get_battery __P((struct apm_softc *, struct apm_power_info *));
static void apm_anal1_battery __P((u_int, struct apm_battery *));
static void apm_anal2_battery __P((u_int, struct apm_battery *));
static int apm_probe_battery __P((struct apm_power_info *));
static void apm_print_power __P((struct apm_power_info *));
static void apm_perror __P((const char *, struct apm_reg_args *));
static void apm_suspend __P((struct apm_softc *));
static int apm_pop_event __P((struct apm_softc *, struct apm_event_info *, struct proc *));
static int apm_push_event __P((struct apm_softc *, u_int));
static void apm_clear_event __P((struct apm_softc *));
static void apm_init_evtag __P((struct apm_softc *));
static struct apm_event_tag *apm_get_evtag __P((struct apm_softc *));
static void apm_free_evtag __P((struct apm_softc *, struct apm_event_tag *));
static int apm_ctrl_power __P((struct apm_softc *, u_int, u_int));
static int apm_ctrl_cpu __P((struct apm_softc *, int *, struct proc *));
static int apm_vm86_call __P((struct vm86_call *, int));
static void apm_kthread_create __P((void *));
int apm_connect_establish __P((struct apm_softc *));

/******************************************************
 * misc SMM control
 ******************************************************/
static __inline void apm_enable_smm __P((struct apm_softc *sc));
static __inline void apm_disenable_smm __P((struct apm_softc *sc));

void apm_kthread __P((void *));
void apm_pow_off __P((void));

int apmopen __P((dev_t, int, int, struct proc *));
int apmclose __P((dev_t, int, int, struct proc *));
int apmioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int apm_call_direct __P((u_int, u_int, u_int, u_int));

int apm_NEC_SMMaddr;
u_int32_t apm_NEC_SMMmask;

static __inline void
apm_enable_smm(sc)
	struct apm_softc *sc;
{
	register bus_space_tag_t iot = sc->sc_iot;
	register bus_space_handle_t ioh = sc->sc_ioh;

	if (apm_NEC_SMMaddr != 0)
		bus_space_write_1(iot, ioh, 0, 
			       bus_space_read_1(iot, ioh, 0) | ~apm_NEC_SMMmask);
}

static __inline void
apm_disenable_smm(sc)
	struct apm_softc *sc;
{

	register bus_space_tag_t iot = sc->sc_iot;
	register bus_space_handle_t ioh = sc->sc_ioh;

	if (apm_NEC_SMMaddr != 0)
		bus_space_write_1(iot, ioh, 0, 
			       bus_space_read_1(iot, ioh, 0) & apm_NEC_SMMmask);
}

/******************************************************
 * EVENT CONTROL
 ******************************************************/
int apm_show_info;

static void
apm_init_evtag(sc)
	struct apm_softc *sc;
{
	int num;
	struct apm_event_tag *evp;

	TAILQ_INIT(&sc->sc_evahead);
	TAILQ_INIT(&sc->sc_evfhead);

	for (num = 0; num < APM_MAX_EVENTS; num ++)
	{
		evp = malloc(sizeof(*evp), M_DEVBUF, M_NOWAIT);
		if (evp == NULL)
			break;
		memset(evp, 0, sizeof(*evp));
		TAILQ_INSERT_TAIL(&sc->sc_evfhead, evp, aet_chain);
	}
}

static struct apm_event_tag *
apm_get_evtag(sc)
	struct apm_softc *sc;
{
	struct apm_event_tag *evp;
	struct apm_event_taghead *evhp;
	int s = splapm();

	evp = sc->sc_evfhead.tqh_first;
	if (evp)
	{
		evhp = &sc->sc_evfhead;
	}
	else
	{
		evhp = &sc->sc_evahead;
		if ((evp = sc->sc_evahead.tqh_first) == NULL)
			panic("%s lost event tag\n", sc->sc_dev.dv_xname);
	}

	TAILQ_REMOVE(evhp, evp, aet_chain);
	TAILQ_INSERT_TAIL(&sc->sc_evahead, evp, aet_chain);
	splx(s);

	return evp;
}

static void
apm_free_evtag(sc, evp)
	struct apm_softc *sc;
	struct apm_event_tag *evp;
{
	int s = splapm();

	TAILQ_REMOVE(&sc->sc_evahead, evp, aet_chain);
	TAILQ_INSERT_TAIL(&sc->sc_evfhead, evp, aet_chain);
	splx(s);
}

void
apm_clear_event(sc)
	struct apm_softc *sc;
{
	struct apm_event_tag *evp;
	int s = splapm();

	while ((evp = sc->sc_evahead.tqh_first) != NULL)
	{
		TAILQ_REMOVE(&sc->sc_evahead, evp, aet_chain);
		TAILQ_INSERT_TAIL(&sc->sc_evfhead, evp, aet_chain);
	}
	splx(s);
}

static int
apm_pop_event(sc, evp, p)
	struct apm_softc *sc;
	struct apm_event_info *evp;
	struct proc *p;
{
	struct apm_event_tag *evlp;
	int s;

	if (sc->sc_proc == NULL)
		return EPERM;

	if (sc->sc_proc != pfind(sc->sc_pid))
	{
		sc->sc_proc = NULL;
		return EPERM;
	}

	if (p != sc->sc_proc)
		return EPERM;

	s = splapm();
	if ((evlp = sc->sc_evahead.tqh_first) == NULL)
	{
		splx(s);
		return ENOENT;
	}

	*evp = evlp->aet_event;
	apm_free_evtag(sc, evlp);
	splx(s);

	return 0;
}

static int
apm_push_event(sc, type)
	struct apm_softc *sc;
	u_int type;
{
	static int aet_magic = 0;
	struct apm_event_tag *evlp;

	if (sc->sc_proc == NULL)
		return EPERM;

	if (sc->sc_proc != pfind(sc->sc_pid))
	{
		sc->sc_proc = NULL;
		return EPERM;
	}

	psignal(sc->sc_proc, SIGUSR1);

	evlp = apm_get_evtag(sc);
	if (evlp == NULL)
		return ENOENT;

	evlp->aet_event.av_type = type;
	evlp->aet_event.av_seqnum = ++ aet_magic;
	evlp->aet_event.av_time = time.tv_sec;

	return 0;
}

void
apm_kthread(arg)
	void *arg;
{
	struct apm_softc *sc = arg;
	struct apm_reg_args regs;
#ifdef	APM_GEN_POWEVENT
	struct apm_power_info pinfo;
#endif	/* APM_GEN_POWEVENT */
	u_int code;
	int s, bcold;
	extern int cold;
	static int apm_kthread_count;

   for (;;) 
   {
	while ((sc->sc_flags & APM_CONNECTED) == 0)
	{
		if (sc->sc_create_force == 0)
		{
			(void) tsleep((caddr_t) &sc->sc_create_force, 
				       PZERO, "apm_conn", 0);
		}
		(void) apm_connect_establish(sc);
		sc->sc_create_force = 0;
	}

	/*************************************************
	 * event status (bios)
	 *************************************************/
	memset(&regs, 0, sizeof(regs));
	if (apm_32b_call(APM_GET_PM_EVENT, &regs))
	{
		if (APM_ERR_CODE(regs.eax) != APM_ERR_NOEVENTS)
			apm_perror("get event", &regs);
		code = APM_EVENT_NULL;
	}
	else
		code = APM_EVENT_CODE(regs.ecx);

	/*************************************************
	 * suspend process
	 *************************************************/
	switch (sc->sc_state)
	{
	case	0:
		if (sc->sc_ct == 0)
		{
			u_int type;

			if (code & APM_EVENT_POWER)
				type = APM_SUSPEND_REQ;
			else if (sc->sc_usrreq & APM_USR_SUSPEND)
				type = APM_USER_SUSPEND_REQ;
			else
				break;

			if (sc->sc_ctdef > 0)
			{
				sc->sc_usrreq &= ~APM_USR_ACK_SUSPEND;
				sc->sc_ct = sc->sc_ctdef;
			}
			else
				sc->sc_usrreq |= APM_USR_ACK_SUSPEND;
			apm_push_event(sc, type);
		}
		else
		{
			if (-- sc->sc_ct == 0)
				sc->sc_usrreq |= APM_USR_ACK_SUSPEND;
		}

		if ((sc->sc_usrreq & APM_USR_ACK_SUSPEND) == 0)
			break;

		sc->sc_state = SYSTM_EVENT_NOTIFY_SUSPEND;
		s = splhigh();
		if (systmmsg_notify((struct device *) sc, sc->sc_state) != 0)
		{
			systmmsg_notify((struct device *) sc,
				        SYSTM_EVENT_ABORT_REQUEST);
			splx(s);
			printf("%s: suspend rejected\n", sc->sc_dev.dv_xname);
			sc->sc_state = 0;
			goto out;
		}
		splx(s);
		/* fall through */
	
	case	SYSTM_EVENT_NOTIFY_SUSPEND:
		sc->sc_state = SYSTM_EVENT_QUERY_SUSPEND;
		s = splhigh();
		if (systmmsg_notify((struct device *) sc, sc->sc_state) != 0)
		{
			splx(s);
			sc->sc_state = SYSTM_EVENT_NOTIFY_SUSPEND;
			goto out;
		}
		splx(s);
		/* fall through */

	case	SYSTM_EVENT_QUERY_SUSPEND:
		sc->sc_state = SYSTM_EVENT_SUSPEND;
		s = splhigh();
		bcold = cold;
		cold = 1;
		systmmsg_notify((struct device *) sc, sc->sc_state);

		/* sleep now */
		apm_suspend(sc);

		/* wake up */
		inittodr(time.tv_sec);

		sc->sc_state = SYSTM_EVENT_RESUME;
		systmmsg_notify((struct device *) sc, sc->sc_state);

		apm_push_event(sc, APM_NORMAL_RESUME);
		sc->sc_usrreq &= ~(APM_USR_SUSPEND | APM_USR_ACK_SUSPEND);
		sc->sc_powt = 0;
		sc->sc_ct = 0;
		sc->sc_state = SYSTM_EVENT_NOTIFY_RESUME;
		cold = bcold;
		inittodr(time.tv_sec);
		splx(s);

		systmmsg_notify((struct device *) sc, sc->sc_state);
		sc->sc_state = 0;

		/* unix system now ready */
		cnputc(0x07);
		break;

	default:
		break;
	}

	/*************************************************
	 * generate battery status event
	 *************************************************/
#ifdef	APM_GEN_POWEVENT
	if (((sc->sc_powt ++) & (APM_BTCHK_INTERVAL - 1)) == 0)
	{
		pinfo = sc->sc_pow;
		apm_get_battery(sc, &pinfo);
		if (bcmp(&pinfo, &sc->sc_pow, sizeof(pinfo)))
		{
			sc->sc_pow = pinfo;
			apm_push_event(sc, APM_POWER_CHANGE);
		}
	}
#endif	/* APM_GEN_POWEVENT */

	/*************************************************
	 * standby
	 *************************************************/
	if (sc->sc_usrreq & APM_USR_STANDBY)
	{
		sc->sc_usrreq &= ~APM_USR_STANDBY;
		apm_ctrl_power(sc, APM_DEV_ALLDEVS, APM_SYS_STANDBY);
	}

	/*************************************************
	 * OK all finished
	 *************************************************/
out:
	apm_disenable_smm(sc);

	apm_kthread_count ++;
	(void) tsleep((caddr_t) &apm_kthread_count, PZERO + 1, "apm_kthread", hz);
   }
}

/************************************************************
 * misc funcs
 ************************************************************/
static void
apm_perror(str, regs)
	const char *str;
	struct apm_reg_args *regs;
{

	printf("apm %s code %d\n", str, APM_ERR_CODE(regs->eax));
}

/*****************************************************
 * Battery Info
 *****************************************************/
static u_char *bs[BSTATE_MAX] =
{"valid", "none", "charge", "abnormal", "available"};

static void apm_pow_str __P((u_char *, struct apm_battery *));

static void
apm_pow_str(buf, binfo)
	u_char *buf;
	struct apm_battery *binfo;
{

	if (binfo->state)
		sprintf(buf, "%s", bs[binfo->state]);
	else if (binfo->life == 0)
		sprintf(buf, "%s", "empty");
	else
		sprintf(buf, "%d/100", binfo->life);
}

static void
apm_print_power(info)
	struct apm_power_info *info;
{
	u_char tbuf0[32];
	u_char tbuf1[32];
	u_char *ac_s = info->ac_state ? "on" : "off";

	apm_pow_str(tbuf0, &info->pack0);
	apm_pow_str(tbuf1, &info->pack1);
	printf("[A/C] %s [1st pack] %s [2nd pack] %s \n",
	       ac_s, tbuf0, tbuf1);
}

static void
apm_anal1_battery(stat, binfo)
	u_int stat;
	struct apm_battery *binfo;
{

	if (stat & APM_BATT_SPMASK)
	{
		switch (stat)
		{
		case APM_BATT_NONE:
			binfo->state = BSTATE_NONE;
			break;

		case APM_BATT_EMPTY:
			binfo->life = 0;
			break;

		case APM_BATT_FULL:
			binfo->life = 100;
			break;

		case APM_BATT_CHARGE:
			binfo->state = BSTATE_CHARGE;
			break;

		case APM_BATT_ABNORMAL:
			binfo->state = BSTATE_ABNORMAL;
			break;

		default:
			binfo->state = BSTATE_UNKNOWN;
			break;
		}
	}
	else if (stat & APM_BATT_INVALID)
		binfo->state = BSTATE_UNKNOWN;
	else
		binfo->life = (stat * 100) / 8;
}

static void
apm_anal2_battery(stat, binfo)
	u_int stat;
	struct apm_battery *binfo;
{

	if (binfo->state != BSTATE_VALID || stat == BATT_UNKNOWN)
		return;

	binfo->life = stat;
}

static int
apm_probe_battery(info)
	struct apm_power_info *info;
{
	struct apm_battery *p0, *p1;

	p0 = &info->pack0;
	p1 = &info->pack1;

	if (p0->life + p1->life > 0)
		return 1;

	if (info->ac_state == APM_AC_OFF)
		return 1;

#ifdef	APM_BUGGYINFO
	if (p0->state == BSTATE_VALID && p1->state == BSTATE_NONE)
		p0->state = BSTATE_NONE;
#endif	/* APM_BUGGYINFO */

	if (p0->state == BSTATE_NONE && p1->state == BSTATE_NONE)
		return 0;

	return 1;
}

static int
apm_get_battery(sc, info)
	struct apm_softc *sc;
	struct apm_power_info *info;
{
	struct apm_reg_args regs;

	memset(info, 0, sizeof(*info));

	info->cpu_state = apm_idleon ? CPU_CTRL_ON : CPU_CTRL_OFF;

	memset(&regs, 0, sizeof(regs));
	if (apm_32b_call(APM_POWER_STATUS, &regs))
		return EIO;

	info->ac_state = regs.ebx & APM_AC_MASK;
	if ((sc->sc_flags & APM_HAS_BATTERY) == 0)
	{
		info->pack0.state = info->pack1.state = BSTATE_NONE;
		return 0;
	}

	apm_anal1_battery(BATT1_LIFE(regs.ecx), &info->pack0);
	apm_anal1_battery(BATT2_LIFE(regs.ecx), &info->pack1);

	if (sc->sc_minver == 0)
		return 0;

	memset(&regs, 0, sizeof(regs));
	regs.ebx = APM_DEV_ALLDEVS;
	if (apm_32b_call(APM_NEC_NEWPSTAT, &regs))
		return EIO;

	apm_anal2_battery(BATTN1_LIFE(regs.ecx), &info->pack0);
#ifdef	notyet
	apm_anal2_battery(BATTN2_LIFE(regs.ecx), &info->pack1);
#endif	/* notyet */
	return 0;
}

/*****************************************************
 * Power management & Resume
 *****************************************************/
static int
apm_ctrl_power(sc, dev, state)
	struct apm_softc *sc;
	u_int dev, state;
{
	struct apm_reg_args regs;

	memset(&regs, 0, sizeof(regs));
	regs.ebx = dev;
	regs.ecx = state;

	if (apm_32b_call(APM_SET_PWR_STATE, &regs) != 0)
	{
		apm_perror("ctrl power", &regs);
		return EIO;
	}
	return 0;
}

#define	APM_SUSPEND_TIMEOUT	(60 * 1000 * 1000)	/* 60 sec */
#define	APM_CHECK_INTERVAL	(500 * 1000)		/* 0.5 sec */

static void
apm_suspend(sc)
	struct apm_softc *sc;
{
	time_t base;
	int tc, i;

	if (sc->sc_usrreq & APM_USR_SUSPEND)
	{
		resettodr();
		inittodr(time.tv_sec);
		base = time.tv_sec;
		if (apm_ctrl_power(sc, APM_DEV_ALLDEVS, APM_SYS_SUSPEND) != 0)
			base = 0;
	}

	outb(IO_ICU1 + icu_imr, 0xff);
	outb(IO_ICU2 + icu_imr, 0xff);

	if ((sc->sc_usrreq & APM_USR_SUSPEND) == 0)
	{
		resettodr();
		inittodr(time.tv_sec);
		base = time.tv_sec;
	}

	for (tc = APM_SUSPEND_TIMEOUT; time.tv_sec - base <= 3 && tc >= 0; )
	{
		outb(IO_ICU2 + icu_cmd, ICRS_OCW2 | OCW2_EOI);
		outb(IO_ICU1 + icu_cmd, ICRS_OCW2 | OCW2_EOI);

		for (i = ICU_LEN - 1; i >= 0; i --)
		{
			if (i < 8)
			{
				outb(IO_ICU1 + icu_cmd, 
				     ICRS_OCW2 | OCW2_EOI | OCW2_SL | i);
			}
			else
			{
				outb(IO_ICU2 + icu_cmd,
				     ICRS_OCW2 | OCW2_EOI | OCW2_SL | (i % 8));
				outb(IO_ICU1 + icu_cmd,
				     ICRS_OCW2 | OCW2_EOI | OCW2_SL | IRQ_SLAVE);
			}
		}
		
		apm_enable_smm(sc);

#define	NO_PORT_ACCESS_WAIT
#ifdef	NO_PORT_ACCESS_WAIT
		{
			extern int delaycount;
			int ctdown;

			ctdown = delaycount * (APM_CHECK_INTERVAL / 1000);
			for (; ctdown; ctdown --)
				;
		}
#else	/* !NO_PORT_ACCESS_WAIT */
		delay(APM_CHECK_INTERVAL);
#endif	/* !NO_PORT_ACCESS_WAIT */

		tc -= APM_CHECK_INTERVAL;
		base = time.tv_sec;
		apm_disenable_smm(sc);
		inittodr(base);
	}

	disable_intr();
	SET_ICUS();
	enable_intr();
}

/* exported funcs */
void
apm_pow_off(void)
{
	struct apm_softc *sc = apm_gsc;

	if (sc && (sc->sc_flags & APM_CONNECTED))
		apm_ctrl_power(sc, APM_DEV_ALLDEVS, APM_SYS_OFF);
}

/*****************************************************
 * CPU control
 *****************************************************/
static int
apm_ctrl_cpu(sc, flagp, p)
	struct apm_softc *sc;
	int *flagp;
	struct proc *p;
{
	int error;

	switch (*flagp)
	{
	case CPU_CTRL_ON:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return error;

		apm_idleon = 1;
		break;

	case CPU_CTRL_OFF:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return error;

		apm_idleon = 0;
		break;

	default:
		return EINVAL;
	}

	*flagp = apm_idleon;
	return 0;
}

/*****************************************************
 * NetBSD Interface
 *****************************************************/
static int
apmmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct systm_attach_args *sa = aux;
	bus_space_handle_t ioh;
	static int apm_pc;

	if (apm_pc != 0)
		return 0;

	if (bus_space_map(sa->sa_iot, APM_NECSMM_PORT, APM_NECSMM_PORTSZ,
			  0, &ioh))
		return 0;

	bus_space_unmap(sa->sa_iot, ioh, APM_NECSMM_PORTSZ);
	apm_pc = 1;
	return apm_pc;
}

static void
apm_kthread_create(arg)
	void *arg;
{
	struct apm_softc *sc = (void *) arg;

	if (kthread_create(apm_kthread, arg, NULL, "apm_kthread"))
		panic("%s: can not start apm threads\n", sc->sc_dev.dv_xname);
}

static void
apmattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct systm_attach_args *sa = aux;
	struct apm_softc *sc = (void *) self;
	bus_space_tag_t iot = sa->sa_iot;

	apm_gsc = sc;
	apm_NEC_SMMaddr = APM_NECSMM_PORT;
	apm_NEC_SMMmask = ~APM_NECSMM_EN;

	sc->sc_iot = iot;
	if (bus_space_map(iot, APM_NECSMM_PORT, APM_NECSMM_PORTSZ,
			  0, &sc->sc_ioh))
		panic("%s: couldn't map I/O ports", sc->sc_dev.dv_xname);

	printf(": waiting for the connection.\n");

	lockinit(&sc->sc_lock, PLOCK | PCATCH, "apmlk", 0, 0);

#ifdef	APM_AUTO_START
	sc->sc_create_force = 1;
#endif	/* APM_AUTO_START */
	kthread_create_deferred(apm_kthread_create, sc);
}

#define	MASK16(val)	((val) & 0xffff)

static int
apm_vm86_call(arg, fno)
	struct vm86_call *arg;
	int fno;
{

	arg->vc_intno = APM_SYSTEM_BIOS;
	arg->vc_flags = 0;
	arg->vc_reg.sc_eax = fno | (APM_BIOS_FNCODE << 8);

	vm86biosstart(arg, NULL, NULL, VM86BIOS_KIO_SWAITOK);
	return arg->vc_error;
}

int
apm_connect_establish(sc)
	struct apm_softc *sc;
{
	extern union descriptor *gdt;
	struct vm86_call vm86_arg;
	struct sigcontext *scp;
	struct apm_reg_args regs;
	vaddr_t vaddr;
	u_int16_t codesel, codeseg;
	u_int32_t vers;

	if (sc->sc_flags & APM_CONNECTED)
		return EBUSY;

	/* I: install check */
	scp = &vm86_arg.vc_reg;
	if (apm_vm86_call(&vm86_arg, APM_INSTALLATION_CHECK))
		return EINVAL;

	if ((scp->sc_eflags & PSL_C) || MASK16(scp->sc_ebx) != 0x504d)
		return ENODEV;

	vers = (u_int32_t) MASK16(scp->sc_eax);
	printf("%s: version 0x%x funcs 0x%x\n", sc->sc_dev.dv_xname,
		vers, MASK16(scp->sc_ecx));

	if (vers != 0x112 && vers != 0x111 && vers != 0x110)
		apm_NEC_SMMaddr = 0;

	if ((MASK16(scp->sc_ecx) & APM_32BIT_SUPPORTED) == 0)
		return EINVAL;

	/* II: try to connect with 32b i */
	scp->sc_ebx = APM_DEV_APM_BIOS;
	scp->sc_ecx = scp->sc_edx = 0;
	apm_vm86_call(&vm86_arg, APM_DISCONNECT);

	if (apm_vm86_call(&vm86_arg, APM_32BIT_CONNECT))
		return EINVAL;
	if (scp->sc_eflags & PSL_C)
		return EBUSY;

	codeseg = MASK16(scp->sc_eax);
	vaddr = ((vaddr_t) codeseg) << 4;
	if (vaddr < CBUSHOLE_BIOS_START || vaddr >= CBUSHOLE_BIOS_END)
		return EINVAL;

	printf("%s: code 0x%x:0x%x data 0x%x:0\n", sc->sc_dev.dv_xname,
		codeseg, MASK16(scp->sc_ebx), MASK16(scp->sc_edx));

	codesel= GSEL(GAPM32CODE_SEL,SEL_KPL);

	setsegment(&gdt[GAPM32CODE_SEL].sd,
		   (void *) ISA_HOLE_VADDR(codeseg << 4),
		   0xffff, SDT_MEMERA, SEL_KPL, 1, 0);

	setsegment(&gdt[GAPMDATA_SEL].sd,
		   (void *) ISA_HOLE_VADDR(MASK16(scp->sc_edx) << 4),
		   0xffff, SDT_MEMRWA, SEL_KPL, 1, 0);

	/* setup initial data */
	apm_farcall.offset = (u_int32_t) MASK16(scp->sc_ebx);
	apm_farcall.seg = (u_int16_t) codesel;
	sc->sc_majver = 1;
	sc->sc_ctdef = APM_WAIT_TIME;

	apm_init_evtag(sc);

	memset(&regs, 0, sizeof(regs));
	regs.ebx = APM_DEV_APM_BIOS;
	regs.ecx = APM_NEC_CONNECT_ID;
	if (apm_32b_call(APM_NEC_CONNECT, &regs) == 0)
	{
		sc->sc_majver = APM_MAJOR_VERS(regs.eax);
		sc->sc_minver = APM_MINOR_VERS(regs.eax);
	}
#if	0
	else
		apm_NEC_SMMaddr = 0;
#endif

	/* check battery */
	sc->sc_flags |= APM_HAS_BATTERY;
	if (apm_get_battery(sc, &sc->sc_pow) == 0)
	{
		if (apm_probe_battery(&sc->sc_pow) == 0)
			sc->sc_flags &= ~APM_HAS_BATTERY;

		printf("%s: ", sc->sc_dev.dv_xname);
		apm_print_power(&sc->sc_pow);
	}
	else
		sc->sc_flags &= ~APM_HAS_BATTERY;

	systmmsg_bind((struct device *) sc, NULL);

	sc->sc_flags |= APM_CONNECTED;

	return 0;
}

int
apmopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = APMUNIT(dev);
	struct apm_softc *sc;

	if (unit >= apm_cd.cd_ndevs)
		return ENXIO;

	if ((sc = apm_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	return 0;
}

int
apmclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return 0;
}

int
apmioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct apm_softc *sc = apm_cd.cd_devs[APMUNIT(dev)];
	int s, error = 0;
	u_int cdata;

	error = lockmgr(&sc->sc_lock, LK_EXCLUSIVE, NULL);
	if (error != 0)
		return error;

	if ((sc->sc_flags & APM_CONNECTED) == 0)
	{
		error = suser(p->p_ucred, &p->p_acflag);
		if (error == 0)
		{
			if (cmd == APM_IOC_CONNECT_BIOS)
				wakeup(&sc->sc_create_force);
			else
				error = ENODEV;
		}
		goto done;
	}

	switch (cmd)
	{
	case APM_IOC_APM:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;

		cdata = *(u_int *) data;
		s = splapm();
		switch (cdata & (~APM_CTRL_ARG_MASK))
		{
		case APM_CTRL_REQ_SUSPEND:
			sc->sc_usrreq &= ~APM_USR_ACK_SUSPEND;
			sc->sc_usrreq |= APM_USR_SUSPEND;
			break;

		case APM_CTRL_REQ_STANDBY:
			sc->sc_usrreq |= APM_USR_STANDBY;
			break;

		case APM_CTRL_ACK_SUSPEND:
			if (sc->sc_ct > 0 || (sc->sc_usrreq & APM_USR_SUSPEND))
				sc->sc_usrreq |= APM_USR_ACK_SUSPEND;
			break;

		case APM_CTRL_SET_TOUTCNT:
			cdata &= APM_CTRL_ARG_MASK;
			sc->sc_ctdef = cdata;
			if (sc->sc_ct > sc->sc_ctdef)
				sc->sc_ct = sc->sc_ctdef + 1;
			break;

		default:
			error = EINVAL;
			break;
		}

		splx(s);
		break;

	case APM_IOC_CONNECT_BIOS:
		error = EBUSY;
		break;

	case APM_IOC_CONNECT:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;

		if (sc->sc_proc && pfind(sc->sc_pid) != sc->sc_proc)
			sc->sc_proc = NULL;

		if (sc->sc_proc && sc->sc_proc != p)
		{
			error = EBUSY;
			break;
		}

		sc->sc_proc = p;
		sc->sc_pid = p->p_pid;
		apm_clear_event(sc);
		break;

	case APM_IOG_EVENT:
		error = apm_pop_event(sc, (struct apm_event_info *) data, p);
		break;

	case APM_IOG_POWER:
		error = apm_get_battery(sc, (struct apm_power_info *) data);
		break;

	case APM_IOC_CPU:
		error = apm_ctrl_cpu(sc, (int *) data, p);
		break;

	default:
		error = ENOTTY;
		break;
	}

done:
	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
	return error;
}

/*************************************************************
 * DEBUG: you may call the following funcs from DDB
 *************************************************************/
int apm_oebx, apm_oecx;

int
apm_call_direct(code, ebx, ecx, edx)
	u_int code;
	u_int ebx, ecx, edx;
{
	struct apm_reg_args regs;

	regs.ebx = ebx;
	regs.ecx = ecx;
	regs.edx = edx;

	if (apm_32b_call(code, &regs) != 0)
	{
		apm_perror("call direct", &regs);
		return EIO;
	}

	apm_oebx = regs.ebx;
	apm_oecx = regs.ecx;

	return 0;
}
