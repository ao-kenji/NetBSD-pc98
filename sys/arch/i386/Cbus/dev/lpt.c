/*	$NecBSD: lpt.c,v 1.13 1999/07/23 05:38:49 kmatsuda Exp $	*/
/*	$NetBSD: lpt.c,v 1.43 1996/12/05 01:25:42 cgd Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1990 William F. Jolitz, TeleMuse
 * All rights reserved.
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Device Driver for AT parallel printer port
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/syslog.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>
#ifdef	ORIGINAL_CODE
#include <dev/isa/lptreg.h>
#else	/* PC-98 */
#include <i386/Cbus/dev/lptreg.h>
#include <machine/syspmgr.h>
#include <i386/Cbus/dev/lpthw.h>
#include <i386/Cbus/dev/lpthwtab.h>
#endif	/* PC-98 */

#define	TIMEOUT		hz*16	/* wait up to 16 seconds for a ready */
#define	STEP		hz/4

#define	LPTPRI		(PZERO+8)
#define	LPT_BSIZE	1024

#if !defined(DEBUG) || !defined(notdef)
#define LPRINTF(a)
#else
#define LPRINTF		if (lptdebug) printf a
int lptdebug = 1;
#endif

struct lpt_softc {
	struct device sc_dev;
	void *sc_ih;

	size_t sc_count;
	struct buf *sc_inbuf;
	u_char *sc_cp;
	int sc_spinmax;
	int sc_iobase;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	int sc_irq;
	u_char sc_state;
#define	LPT_OPEN	0x01	/* device is open */
#define	LPT_OBUSY	0x02	/* printer is busy doing output */
#define	LPT_INIT	0x04	/* waiting to initialize for open */
#ifndef	ORIGINAL_CODE
#define	LPT_MSGWAIT	0x10	/* wait status recovery */
#endif	/* PC-98 */
	u_char sc_flags;
#define	LPT_AUTOLF	0x20	/* automatic LF on CR */
#define	LPT_NOPRIME	0x40	/* don't prime on open */
#define	LPT_NOINTR	0x80	/* do not use interrupt */
	u_char sc_control;
	u_char sc_laststatus;
#ifndef	ORIGINAL_CODE
	int sc_ocount;
	struct lpt_hw *sc_hw;
#endif	/* PC-98 */
};

/* XXX does not belong here */
cdev_decl(lpt);

#ifdef __BROKEN_INDIRECT_CONFIG
int lptprobe __P((struct device *, void *, void *));
#else
int lptprobe __P((struct device *, struct cfdata *, void *));
#endif
void lptattach __P((struct device *, struct device *, void *));
int lptintr __P((void *));

struct cfattach lpt_ca = {
	sizeof(struct lpt_softc), lptprobe, lptattach
};

extern struct cfdriver lpt_cd;

#define	LPTUNIT(s)	(minor(s) & 0x1f)
#define	LPTFLAGS(s)	(minor(s) & 0xe0)

#define	LPS_INVERT	(LPS_SELECT|LPS_NERR|LPS_NBSY|LPS_NACK)
#define	LPS_MASK	(LPS_SELECT|LPS_NERR|LPS_NBSY|LPS_NACK|LPS_NOPAPER)
#ifdef	ORIGINAL_CODE
#define	NOT_READY()	((bus_space_read_1(iot, ioh, lpt_status) ^ LPS_INVERT) & LPS_MASK)
#define	NOT_READY_ERR()	not_ready(bus_space_read_1(iot, ioh, lpt_status), sc)
#else	/* PC-98 */
#define	NOT_READY()	((*sc->sc_hw->not_ready)(sc))
#define	NOT_READY_ERR()	((*sc->sc_hw->not_ready_error)(sc))
#endif	/* PC-98 */
#ifdef	ORIGINAL_CODE
static int not_ready __P((u_char, struct lpt_softc *));
#endif	/* PC-98 */

static void lptwakeup __P((void *arg));
static int pushbytes __P((struct lpt_softc *));

int	lpt_port_test __P((bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    bus_size_t, u_char, u_char));

#ifndef	ORIGINAL_CODE
#define	BUS_IOR(offs)	(bus_space_read_1(sc->sc_iot, sc->sc_ioh, (offs)))
#define	BUS_IOW(offs, val) \
			(bus_space_write_1(sc->sc_iot, sc->sc_ioh, (offs), (val)))

void lpt_message __P((void *));

static void lpt_force_trad __P((struct lpt_softc *, u_char));
static void lpt_force_extmode __P((bus_space_tag_t, bus_space_handle_t, struct lpt_hw *, u_int));
void output_NECTRAD __P((void *));
void output_PS2 __P((void *));
int not_ready_NECTRAD __P((void *));
int not_ready_PS2 __P((void *));
int not_ready_error_PS2 __P((void *));
void lpt_autoinit __P((struct lpt_softc *));
static int lpt_find_hw __P((struct isa_attach_args *));

/* mode initialize */
static void
lpt_force_trad(sc, flags)
	struct lpt_softc *sc;
	u_char flags;
{

	syspmgr(SYSTM_SYSPMGR_TAG, SYSPMGR_PSTB_OFF);
	BUS_IOW(lpt_control, LPC_MODE8255);
	BUS_IOW(lpt_control, (flags & LPT_NOINTR) ? LPC_NIRQ8 : LPC_IRQ8);
	BUS_IOW(lpt_control, LPC_NPSTB);
	syspmgr(SYSTM_SYSPMGR_TAG, SYSPMGR_PSTB_ON);
}

static void
lpt_force_extmode(bst, bsh, hw, mode)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	struct lpt_hw *hw;
	u_int mode;
{
	u_int8_t regv;
	
	regv = bus_space_read_1(bst, bsh, lpt_extmode); 
	bus_space_write_1(bst, bsh, lpt_extmode, regv & (~LPTEM_EINTR));
	if (hw->subtype == LPT_HW_NEC)
	{
		regv = bus_space_read_1(bst, bsh, lpt_mode);
		bus_space_write_1(bst, bsh, lpt_mode, regv | LPTM_EXT);
	}
	delay(100);
	mode |= bus_space_read_1(bst, bsh, lpt_extmode) & (~LPTEM_MASK);
	bus_space_write_1(bst, bsh, lpt_extmode, mode);
}

/* output */
void
output_NECTRAD(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;

	BUS_IOW(lpt_data, *sc->sc_cp++);
	BUS_IOW(lpt_control, LPC_PSTB);
	sc->sc_count--;
	sc->sc_laststatus = 0;
	BUS_IOW(lpt_control, LPC_NPSTB);
}

void
output_PS2(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;

	BUS_IOW(lpt_data, *sc->sc_cp++);
	BUS_IOW(lpt_control, sc->sc_control | LPC_STROBE);
	sc->sc_count--;
	sc->sc_laststatus = 0;
	BUS_IOW(lpt_control, sc->sc_control);
}

/* status */
int
not_ready_NECTRAD(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;

	return ((BUS_IOR(lpt_status) & LPS_O_NBSY) != LPS_O_NBSY);
}

int
not_ready_PS2(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;

	return ((BUS_IOR(lpt_status) ^ LPS_INVERT) & LPS_MASK);
}

int
not_ready_error_PS2(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;
	u_char new, status;

	status = ((BUS_IOR(lpt_status) ^ LPS_INVERT) & LPS_MASK);
	new = (status && status != sc->sc_laststatus) ? status : 0;
	sc->sc_laststatus = status;

	if (new && (sc->sc_state & LPT_MSGWAIT) == 0)
	{
		sc->sc_state |= LPT_MSGWAIT;
		timeout(lpt_message, sc, 30 * hz);
	}
	return ((u_int)status);
}

/* auto init */
void
lpt_autoinit(sc)
	struct lpt_softc *sc;
{

	switch (sc->sc_hw->subtype)
	{
	case LPT_HW_TRADNEC:
		sc->sc_state = 0;
		lpt_force_trad(sc, LPT_NOINTR);
		break;

	default:
		BUS_IOW(lpt_extmode, BUS_IOR(lpt_extmode) & (~LPTEM_EINTR));
		BUS_IOW(lpt_control, LPC_NINIT);
		sc->sc_state = 0;
		BUS_IOW(lpt_control, LPC_NINIT);
		break;
	}
}

/* auto config */
static int
lpt_find_hw(ia)
	struct isa_attach_args *ia;
{
	static int lptlock = 0;
	struct lpt_hw *hw;
	u_long dvcfg;
	int i;

	for (i = 0; i < lpt_hwsel.cfg_max && (hw = lpt_hwsel.cfg_sel[i]); i ++)
	{
		dvcfg = DVCFG_MKCFG(i, DVCFG_MINOR(ia->ia_cfgflags));
		if (hw->iobase && hw->iobase == ia->ia_iobase)
		{
			if (hw->irq != 0)
				ia->ia_irq = hw->irq;

			switch (hw->subtype)
			{
			case LPT_HW_TRADNEC:
				if (lptlock ++)
					return 0;
				ia->ia_iosize = hw->nports;
				ia->ia_msize = 0;
				ia->ia_cfgflags = dvcfg;
				return 1;

			case LPT_HW_NEC:
				if (lptlock ++)
					return 0;
			default:
				break;
			}

			ia->ia_cfgflags = dvcfg;
			return -1;
		}

		if (hw->subtype == LPT_HW_PS2)
		{
			ia->ia_cfgflags = dvcfg;
			return -1;
		}
	}
	return 0;
}

/* misc */
void
lpt_message(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;
	u_char status = ((BUS_IOR(lpt_status) ^ LPS_INVERT) & LPS_MASK);

	if ((status & (LPS_SELECT|LPS_NOPAPER)) == (LPS_SELECT|LPS_NOPAPER))
		printf("%s: out of paper\n", sc->sc_dev.dv_xname);
	else if (status & LPS_SELECT)
		printf("%s: offline\n", sc->sc_dev.dv_xname);
	else if (status & LPS_NERR)
		log(LOG_NOTICE, "%s: output error\n", sc->sc_dev.dv_xname);

	sc->sc_state &= ~LPT_MSGWAIT;
}
#endif	/* PC-98 */

/*
 * Internal routine to lptprobe to do port tests of one byte value.
 */
int
lpt_port_test(iot, ioh, base, off, data, mask)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t base;
	bus_size_t off;
	u_char data, mask;
{
	int timeout;
	u_char temp;

	data &= mask;
	bus_space_write_1(iot, ioh, off, data);
	timeout = 1000;
	do {
		delay(10);
		temp = bus_space_read_1(iot, ioh, off) & mask;
	} while (temp != data && --timeout);
	LPRINTF(("lpt: port=0x%x out=0x%x in=0x%x timeout=%d\n", base + off,
	    data, temp, timeout));
	return (temp == data);
}

/*
 * Logic:
 *	1) You should be able to write to and read back the same value
 *	   to the data port.  Do an alternating zeros, alternating ones,
 *	   walking zero, and walking one test to check for stuck bits.
 *
 *	2) You should be able to write to and read back the same value
 *	   to the control port lower 5 bits, the upper 3 bits are reserved
 *	   per the IBM PC technical reference manauls and different boards
 *	   do different things with them.  Do an alternating zeros, alternating
 *	   ones, walking zero, and walking one test to check for stuck bits.
 *
 *	   Some printers drag the strobe line down when the are powered off
 * 	   so this bit has been masked out of the control port test.
 *
 *	   XXX Some printers may not like a fast pulse on init or strobe, I
 *	   don't know at this point, if that becomes a problem these bits
 *	   should be turned off in the mask byte for the control port test.
 *
 *	3) Set the data and control ports to a value of 0
 */
int
lptprobe(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_long base;
	u_char mask, data;
	int i, rv;
#ifndef	ORIGINAL_CODE
	struct lpt_hw *hw;
	u_int extmode;
#endif	/* PC-98 */

#ifdef DEBUG
#define	ABORT	do {printf("lptprobe: mask %x data %x failed\n", mask, data); \
		    goto out;} while (0)
#else
#define	ABORT	goto out
#endif

	iot = ia->ia_iot;
	base = ia->ia_iobase;
#ifndef	ORIGINAL_CODE
	rv = lpt_find_hw(ia);
	if (rv >= 0)
		return rv;

	hw = DVCFG_HW(&lpt_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (hw == NULL)
		return 0;

	if (bus_space_map(iot, base, 0, 0, &ioh) != 0 ||
	    bus_space_map_load(iot, ioh, hw->iatsz, hw->iat,
			       BUS_SPACE_MAP_FAILFREE))
		return 0;

	extmode = LPT_ACCESS_MODE(DVCFG_MINOR(ia->ia_cfgflags));
	lpt_force_extmode(iot, ioh, hw, extmode);
#else	/* !PC-98 */
	if (bus_space_map(iot, base, LPT_NPORTS, 0, &ioh))
		return 0;
#endif	/* !PC-98 */

	rv = 0;
	mask = 0xff;

	data = 0x55;				/* Alternating zeros */
	if (!lpt_port_test(iot, ioh, base, lpt_data, data, mask))
		ABORT;

	data = 0xaa;				/* Alternating ones */
	if (!lpt_port_test(iot, ioh, base, lpt_data, data, mask))
		ABORT;

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking zero */
		data = ~(1 << i);
		if (!lpt_port_test(iot, ioh, base, lpt_data, data, mask))
			ABORT;
	}

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking one */
		data = (1 << i);
		if (!lpt_port_test(iot, ioh, base, lpt_data, data, mask))
			ABORT;
	}

	bus_space_write_1(iot, ioh, lpt_data, 0);
	bus_space_write_1(iot, ioh, lpt_control, 0);

#ifdef	ORIGINAL_CODE
	ia->ia_iosize = LPT_NPORTS;
#else	/* PC-98 */
	ia->ia_iosize = hw->nports;
#endif	/* PC-98 */
	ia->ia_msize = 0;

	rv = 1;

out:
#ifdef	ORIGINAL_CODE
	bus_space_unmap(iot, ioh, LPT_NPORTS);
#else	/* PC-98 */
	bus_space_unmap(iot, ioh, hw->nports);
#endif	/* PC-98 */
	return rv;
}

void
lptattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lpt_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	if (ia->ia_irq != IRQUNK)
		printf("\n");
	else
		printf(": polled\n");

	sc->sc_iobase = ia->ia_iobase;
	sc->sc_irq = ia->ia_irq;
	sc->sc_state = 0;

	iot = sc->sc_iot = ia->ia_iot;
#ifndef	ORIGINAL_CODE
	sc->sc_hw = DVCFG_HW(&lpt_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (sc->sc_hw == NULL)
	{
		printf("%s: can not find hw\n", sc->sc_dev.dv_xname);
		return;
	}

	if (bus_space_map(iot, sc->sc_iobase, 0, 0, &ioh) ||
	    bus_space_map_load(iot, ioh, sc->sc_hw->iatsz, sc->sc_hw->iat,
			       BUS_SPACE_MAP_FAILFREE))
	{
		printf("%s: can not map\n", sc->sc_dev.dv_xname);
		return;
	}
#else	/* !PC-98 */
	if (bus_space_map(iot, sc->sc_iobase, LPT_NPORTS, 0, &ioh))
		panic("lptattach: couldn't map I/O ports");
#endif	/* !PC-98 */
	sc->sc_ioh = ioh;

#ifdef	ORIGINAL_CODE
	bus_space_write_1(iot, ioh, lpt_control, LPC_NINIT);
#else	/* PC-98 */
	{
		u_int extmode = LPT_ACCESS_MODE(DVCFG_MINOR(ia->ia_cfgflags));

		lpt_force_extmode(iot, ioh, sc->sc_hw, extmode);
		lpt_autoinit(sc);
	}
#endif	/* PC-98 */

	if (ia->ia_irq != IRQUNK)
		sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
		    IPL_TTY, lptintr, sc);
}

/*
 * Reset the printer, then wait until it's selected and not busy.
 */
int
lptopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = LPTUNIT(dev);
	u_char flags = LPTFLAGS(dev);
	struct lpt_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_char control;
	int error;
	int spin;

	if (unit >= lpt_cd.cd_ndevs)
		return ENXIO;
	sc = lpt_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (sc->sc_irq == IRQUNK && (flags & LPT_NOINTR) == 0)
		return ENXIO;

#ifdef DIAGNOSTIC
	if (sc->sc_state)
		printf("%s: stat=0x%x not zero\n", sc->sc_dev.dv_xname,
		    sc->sc_state);
#endif

	if (sc->sc_state)
		return EBUSY;

	sc->sc_state = LPT_INIT;
	sc->sc_flags = flags;
	LPRINTF(("%s: open: flags=0x%x\n", sc->sc_dev.dv_xname, flags));
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

#ifdef	ORIGINAL_CODE
	if ((flags & LPT_NOPRIME) == 0) {
		/* assert INIT for 100 usec to start up printer */
		bus_space_write_1(iot, ioh, lpt_control, LPC_SELECT);
		delay(100);
	}

	control = LPC_SELECT | LPC_NINIT;
	bus_space_write_1(iot, ioh, lpt_control, control);
#else	/* PC-98 */
	switch (sc->sc_hw->subtype)
	{
	default:
		if ((flags & LPT_NOPRIME) == 0) {
			/* assert INIT for 100 usec to start up printer */
			BUS_IOW(lpt_control, LPC_SELECT);
			delay(100);
		}

		control = LPC_SELECT | LPC_NINIT;
		BUS_IOW(lpt_control, control);
		break;

	case LPT_HW_TRADNEC:
		control = 0;
	}
	sc->sc_laststatus = 0;
#endif	/* PC-98 */

	/* wait till ready (printer running diagnostics) */
	for (spin = 0; NOT_READY_ERR(); spin += STEP) {
		if (spin >= TIMEOUT) {
			sc->sc_state = 0;
			return EBUSY;
		}

		/* wait 1/4 second, give up if we get a signal */
		error = tsleep((caddr_t)sc, LPTPRI | PCATCH, "lptopen", STEP);
		if (error != EWOULDBLOCK) {
			sc->sc_state = 0;
			return error;
		}
	}

#ifdef	ORIGINAL_CODE
	if ((flags & LPT_NOINTR) == 0)
		control |= LPC_IENABLE;
	if (flags & LPT_AUTOLF)
		control |= LPC_AUTOLF;
	sc->sc_control = control;
	bus_space_write_1(iot, ioh, lpt_control, control);
#else	/* PC-98 */
	switch (sc->sc_hw->subtype)
	{
	case LPT_HW_TRADNEC:
		lpt_force_trad(sc, flags);
		break;

	default:
		if ((flags & LPT_NOINTR) == 0)
		{
			control |= LPC_IENABLE;
			BUS_IOW(lpt_extmode, BUS_IOR(lpt_extmode) | LPTEM_EINTR);
		}
		if (flags & LPT_AUTOLF)
			control |= LPC_AUTOLF;
		BUS_IOW(lpt_control, control);
		break;
	}

	sc->sc_control = control;
	sc->sc_laststatus = 0;
#endif	/* PC-98 */

	sc->sc_inbuf = geteblk(LPT_BSIZE);
	sc->sc_count = 0;
	sc->sc_state = LPT_OPEN;

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		lptwakeup(sc);

	LPRINTF(("%s: opened\n", sc->sc_dev.dv_xname));
	return 0;
}

#ifdef	ORIGINAL_CODE
int
not_ready(status, sc)
	u_char status;
	struct lpt_softc *sc;
{
	u_char new;

	status = (status ^ LPS_INVERT) & LPS_MASK;
	new = status & ~sc->sc_laststatus;
	sc->sc_laststatus = status;

	if (new & LPS_SELECT)
		log(LOG_NOTICE, "%s: offline\n", sc->sc_dev.dv_xname);
	else if (new & LPS_NOPAPER)
		log(LOG_NOTICE, "%s: out of paper\n", sc->sc_dev.dv_xname);
	else if (new & LPS_NERR)
		log(LOG_NOTICE, "%s: output error\n", sc->sc_dev.dv_xname);

	return status;
}
#endif	/* !PC-98 */

void
lptwakeup(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;
	int s;

#ifdef	ORIGINAL_CODE
	s = spltty();
	lptintr(sc);
	splx(s);
#else	/* PC-98 */
	/* XXX:
	 * This trick is needed to prohibit the stray interrupt
	 * in the fast transfer!
	 */
	if (sc->sc_ocount == sc->sc_count)
	{
		s = spltty();
		lptintr(sc);
		splx(s);
	}
	sc->sc_ocount = sc->sc_count;
#endif	/* PC-98 */

	timeout(lptwakeup, sc, STEP);
}

/*
 * Close the device, and free the local line buffer.
 */
int
lptclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = LPTUNIT(dev);
	struct lpt_softc *sc = lpt_cd.cd_devs[unit];
#ifdef	ORIGINAL_CODE
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
#endif	/* PC-98 */

	if (sc->sc_count)
		(void) pushbytes(sc);

#ifdef	ORIGINAL_CODE
	if ((sc->sc_flags & LPT_NOINTR) == 0)
		untimeout(lptwakeup, sc);

	bus_space_write_1(iot, ioh, lpt_control, LPC_NINIT);
	sc->sc_state = 0;
	bus_space_write_1(iot, ioh, lpt_control, LPC_NINIT);
#else	/* PC-98 */
	sc->sc_count = 0;	/* keep this */
	if ((sc->sc_flags & LPT_NOINTR) == 0)
		untimeout(lptwakeup, sc);
	lpt_autoinit(sc);
	untimeout(lpt_message, sc);
#endif	/* PC-98 */

	brelse(sc->sc_inbuf);

	LPRINTF(("%s: closed\n", sc->sc_dev.dv_xname));
	return 0;
}

int
pushbytes(sc)
	struct lpt_softc *sc;
{
#ifdef	ORIGINAL_CODE
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
#endif	/* PC-98 */
	int error;

	if (sc->sc_flags & LPT_NOINTR) {
		int spin, tic;
#ifdef	ORIGINAL_CODE
		u_char control = sc->sc_control;
#endif	/* !PC-98 */

		while (sc->sc_count > 0) {
			spin = 0;
			while (NOT_READY()) {
				if (++spin < sc->sc_spinmax)
					continue;
				tic = 0;
				/* adapt busy-wait algorithm */
				sc->sc_spinmax++;
				while (NOT_READY_ERR()) {
					/* exponential backoff */
					tic = tic + tic + 1;
					if (tic > TIMEOUT)
						tic = TIMEOUT;
					error = tsleep((caddr_t)sc,
					    LPTPRI | PCATCH, "lptpsh", tic);
					if (error != EWOULDBLOCK)
						return error;
				}
				break;
			}

#ifdef	ORIGINAL_CODE
			bus_space_write_1(iot, ioh, lpt_data, *sc->sc_cp++);
			bus_space_write_1(iot, ioh, lpt_control, control | LPC_STROBE);
			sc->sc_count--;
			bus_space_write_1(iot, ioh, lpt_control, control);
#else	/* PC-98 */
			(*sc->sc_hw->output)(sc);
#endif	/* PC-98 */

			/* adapt busy-wait algorithm */
			if (spin*2 + 16 < sc->sc_spinmax)
				sc->sc_spinmax--;
		}
	} else {
		int s;

		while (sc->sc_count > 0) {
			/* if the printer is ready for a char, give it one */
			if ((sc->sc_state & LPT_OBUSY) == 0) {
				LPRINTF(("%s: write %d\n", sc->sc_dev.dv_xname,
				    sc->sc_count));
				s = spltty();
				(void) lptintr(sc);
				splx(s);
			}
			error = tsleep((caddr_t)sc, LPTPRI | PCATCH,
			    "lptwrite2", 0);
			if (error)
				return error;
		}
	}
	return 0;
}

/* 
 * Copy a line from user space to a local buffer, then call putc to get the
 * chars moved to the output queue.
 */
int
lptwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct lpt_softc *sc = lpt_cd.cd_devs[LPTUNIT(dev)];
	size_t n;
	int error = 0;

	while ((n = min(LPT_BSIZE, uio->uio_resid)) != 0) {
		uiomove(sc->sc_cp = sc->sc_inbuf->b_data, n, uio);
		sc->sc_count = n;
		error = pushbytes(sc);
		if (error) {
			/*
			 * Return accurate residual if interrupted or timed
			 * out.
			 */
			uio->uio_resid += sc->sc_count;
			sc->sc_count = 0;
			return error;
		}
	}
	return 0;
}

/*
 * Handle printer interrupts which occur when the printer is ready to accept
 * another char.
 */
int
lptintr(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;
#ifdef	ORIGINAL_CODE
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
#endif	/* PC-98 */

#if 0
	if ((sc->sc_state & LPT_OPEN) == 0)
		return 0;
#endif

	/* is printer online and ready for output */
	if (NOT_READY() && NOT_READY_ERR())
		return 0;

	if (sc->sc_count) {
#ifdef	ORIGINAL_CODE
		u_char control = sc->sc_control;
		/* send char */
		bus_space_write_1(iot, ioh, lpt_data, *sc->sc_cp++);
		bus_space_write_1(iot, ioh, lpt_control, control | LPC_STROBE);
		sc->sc_count--;
		bus_space_write_1(iot, ioh, lpt_control, control);
#else	/* PC-98 */
		do
			(*sc->sc_hw->output)(sc);
		while (sc->sc_count && NOT_READY() == 0);
#endif	/* PC-98 */
		sc->sc_state |= LPT_OBUSY;
	} else
		sc->sc_state &= ~LPT_OBUSY;

	if (sc->sc_count == 0) {
		/* none, wake up the top half to get more */
		wakeup((caddr_t)sc);
	}

	return 1;
}

int
lptioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error = 0;

	switch (cmd) {
	default:
		error = ENODEV;
	}

	return error;
}
