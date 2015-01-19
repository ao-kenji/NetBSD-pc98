/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998, 1999
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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/midivar.h>
#include <dev/packet_midi.h>

#define AUDIODEV_TYPE_PMIDI	127
#define	PMIDIUNIT(dev)	(minor(dev)&0x0f)

struct pmidi_rb {
	u_int8_t *mr_bp;

	u_int mr_size;
	u_int mr_mask;
	u_int mr_hiwater;
	u_int mr_lowater;
	u_int mr_wp;
	u_int mr_rp;
	u_int mr_used;
};

struct pmidi_softc {
	struct device sc_dev;

	struct midi_hw_if *sc_if;
	void *sc_hdl;

	struct pmidi_rb sc_outb;
	struct pmidi_rb sc_inb;

#define	MIDI_WOPEN	0x0001
#define	MIDI_ROPEN	0x0002
#define	MIDI_OPEN	(MIDI_WOPEN | MIDI_ROPEN)
#define	MIDI_WANTW	0x0020
#define	MIDI_WANTR	0x0040
#define	MIDI_PAUSED	0x0100
#define	MIDI_THRU	0x0200
	int sc_flags;

	u_int sc_ticks;

	/* poll */
	struct	selinfo sc_wsel;
	struct	selinfo sc_rsel;

	/* pmidi inputs stauff */
	u_int sc_pktlen;
	int sc_pkthdr;
	struct timeval sc_tv;
	int sc_explen;
	u_char sc_pstatus;

	int sc_hwtimer;			/* XXX */
};

int	pmidiprobe __P((struct device *, struct cfdata *, void *));
void	pmidiattach __P((struct device *, struct device *, void *));

extern struct cfdriver pmidi_cd;

struct cfattach pmidi_ca = {
	sizeof(struct pmidi_softc), pmidiprobe, pmidiattach
};

static int pmidi_init_rb __P((struct pmidi_rb *));
static void pmidi_timer __P((void *));
static void pmidi_timer_write __P((void *));
static void pmidi_output __P((struct pmidi_softc *));
static void pmidi_timer_read __P((void *, int));
static void pmidi_input __P((struct pmidi_softc *, int));
static int pmidi_drain __P((struct pmidi_softc *));
static int pmidiprint __P((void *, const char *));

int pmidiopen __P((dev_t, int, int, struct proc *));
int pmidiclose __P((dev_t, int, int, struct proc *));
int pmidiwrite __P((dev_t, struct uio *, int));
int pmidiread __P((dev_t, struct uio *, int));
int pmidiioctl __P((dev_t, int, caddr_t, int, struct proc *));
int pmidipoll __P((dev_t, int, struct proc *));

static int
pmidi_init_rb(rbp)
	struct pmidi_rb *rbp;
{
	if (rbp->mr_bp == NULL)
		rbp->mr_bp = malloc(MIDI_RING_SIZE, M_DEVBUF, M_NOWAIT);

	if (rbp->mr_bp == NULL)
		return ENOMEM;
	
	rbp->mr_size = MIDI_RING_SIZE - sizeof(struct pmidi_packet_header);
	rbp->mr_hiwater = rbp->mr_size;
	rbp->mr_lowater = rbp->mr_size / 3;
	rbp->mr_used = 0;
	rbp->mr_wp = rbp->mr_rp = 0;
	return 0;
}

int
pmidiprobe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct audio_attach_args *sa = aux;

	return (sa->type == AUDIODEV_TYPE_PMIDI) ? 1 : 0;
}

void
pmidiattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pmidi_softc *sc = (void *)self;
	struct audio_attach_args *sa = aux;

	printf("\n");
	sc->sc_if = sa->hwif;
	sc->sc_hdl = sa->hdl;

	pmidi_init_rb(&sc->sc_outb);
	pmidi_init_rb(&sc->sc_inb);
}

int
pmidiopen(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int error, unit = PMIDIUNIT(dev);
	struct pmidi_softc *sc;
	struct midi_hw_if *hw;
	int s;

	if (unit >= pmidi_cd.cd_ndevs ||
	    (sc = pmidi_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	hw = sc->sc_if;
	if (!hw)
		return ENXIO;

#if	0
	if (((flags & FWRITE) && (sc->sc_flags & MIDI_WOPEN)) ||
	    ((flags & FREAD) && (sc->sc_flags & MIDI_ROPEN)))
		return EBUSY; 
#endif

	s = splaudio();
	if (flags & FREAD)
	{
		sc->sc_pktlen = 0;
		sc->sc_pkthdr = -1;
		if (pmidi_init_rb(&sc->sc_inb) != 0)
		{
			splx(s);
			return ENXIO;
		}
	}

	if (flags & FWRITE)
	{
		if (pmidi_init_rb(&sc->sc_outb) != 0)
		{
			splx(s);
			return ENXIO;
		}
	}
	splx(s);

	if ((sc->sc_flags & MIDI_OPEN) == 0)
	{
		error = hw->open(sc->sc_hdl, flags, pmidi_timer_read, 
				 pmidi_timer_write, sc);
		if (error != 0)
			return error;
	}

	if (flags & FWRITE)
	{
		if ((sc->sc_flags & MIDI_WOPEN) == 0)
		{
			sc->sc_ticks = 0;
			timeout(pmidi_timer, sc, 1);
		}
		sc->sc_flags |= MIDI_WOPEN;
	}
	if (flags & FREAD)
		sc->sc_flags |= MIDI_ROPEN;
	return 0;
}

int
pmidiclose(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = PMIDIUNIT(dev);
	struct pmidi_softc *sc = pmidi_cd.cd_devs[unit];
	struct midi_hw_if *hw = sc->sc_if;
	int s;

	pmidi_drain(sc);

	s = splaudio();
	untimeout(pmidi_timer, sc);
	sc->sc_hwtimer = 0;

	sc->sc_flags = 0;
	sc->sc_pktlen = 0;
	sc->sc_pkthdr = -1;
	pmidi_init_rb(&sc->sc_outb);
	pmidi_init_rb(&sc->sc_inb);

	hw->close (sc->sc_hdl);
	splx(s);

	return 0;
}

int
pmidiwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int unit = PMIDIUNIT(dev);
	struct pmidi_softc *sc = pmidi_cd.cd_devs[unit];
	struct pmidi_rb *rbp = &sc->sc_outb;
	u_int cc;
	int s, error = 0;

	while (uio->uio_resid > 0 && !error)
	{
		s = splaudio();
		while (rbp->mr_used >= rbp->mr_hiwater)
		{
			if (ioflag & IO_NDELAY)
			{
				splx(s);
				error = EWOULDBLOCK;
				goto out;
			}

			sc->sc_flags |= MIDI_WANTW;
			error = tsleep(rbp, PWAIT | PCATCH, "pmidirw", 0);
			if (error) 
			{
				sc->sc_flags &= ~MIDI_WANTW;
				splx(s);
				goto out;
			}
		}
		splx(s);

		cc = rbp->mr_size - rbp->mr_used;
		if (rbp->mr_wp + cc >= rbp->mr_size)
			cc = rbp->mr_size - rbp->mr_wp;

		if (cc > uio->uio_resid)
			cc = uio->uio_resid;

		error = uiomove(rbp->mr_bp + rbp->mr_wp, cc, uio);
		if (error != 0)
			break;

		s = splaudio();
		rbp->mr_used += cc;
		splx(s);
		rbp->mr_wp += cc;
		if (rbp->mr_wp >= rbp->mr_size)
			rbp->mr_wp = 0;
	}

out:
	s = splaudio();
	pmidi_output(sc);
	splx(s);
	return error;
}

int
pmidiread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int unit = PMIDIUNIT(dev);
	struct pmidi_softc *sc = pmidi_cd.cd_devs[unit];
	struct pmidi_rb *rbp = &sc->sc_inb;
	u_int cc;
	int s, error = 0;

	while (uio->uio_resid > 0 && !error)
	{
		s = splaudio();
		while (sc->sc_pktlen <= 0)
		{
			if (ioflag & IO_NDELAY)
			{
				splx(s);
				return EWOULDBLOCK;
			}

			sc->sc_flags |= MIDI_WANTR;
			error = tsleep(rbp, PWAIT | PCATCH, "pmidirw", 0);
			if (error) 
			{
				sc->sc_flags &= ~MIDI_WANTR;
				splx(s);
				return error;
			}
		}
		cc = sc->sc_pktlen;
		splx(s);

		if (rbp->mr_rp + cc >= rbp->mr_size)
			cc = rbp->mr_size - rbp->mr_rp;

		if (cc > uio->uio_resid)
			cc = uio->uio_resid;

		error = uiomove(rbp->mr_bp + rbp->mr_rp, cc, uio);
		if (error != 0)
			break;

		rbp->mr_rp += cc;
		if (rbp->mr_rp >= rbp->mr_size)
			rbp->mr_rp = 0;

		s = splaudio();
		rbp->mr_used -= cc;
		sc->sc_pktlen -= cc;
		cc = sc->sc_pktlen;
		splx(s);

		if (cc == 0)
			break;
	}
	return error;
}

static int 
pmidi_drain(sc)
	struct pmidi_softc *sc;
{
	struct pmidi_rb *rbp = &sc->sc_outb;
	int s, error = 0;	
	u_int prevp;

	s = splaudio();
	while (rbp->mr_used >= sizeof(struct pmidi_packet_header))
	{
 		prevp = rbp->mr_rp;
		sc->sc_flags |= MIDI_WANTW;
		error = tsleep(rbp, PWAIT | PCATCH, "pmididr", 15 * hz);
		if (error) 
		{
			if (error == EWOULDBLOCK && rbp->mr_rp != prevp)
				continue;
			sc->sc_flags &= ~MIDI_WANTW;
			splx(s);
			return error;
		}
	}
	splx(s);

	return 0;
}

int
pmidiioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int unit = PMIDIUNIT(dev);
	struct pmidi_softc *sc = pmidi_cd.cd_devs[unit];
	int s, error = ENOTTY;

	if ((sc->sc_flags & MIDI_WOPEN) == 0)
		return error;

	switch (cmd) {
	case MIDI_DRAIN:
		error = pmidi_drain(sc);
		break;

	case MIDI_FLUSH:
		s = splaudio();
		pmidi_init_rb(&sc->sc_outb);
		splx(s);
		error = 0;
		break;
	}

	return (error);
}

#define	MIDI_INTRODUCER(ch)	(((ch) & 0x80) && (ch) != 0xf7)
#define	MIDI_REALTIME(ch)	((ch) >= 0xf8)

static void
pmidi_timer_read(arg, ch)
	void *arg;
	int ch;
{
	struct pmidi_softc *sc = arg;
	static int pmidicmdlen[8] = {3, 3, 3, 3, 2, 2, 3, 0};

	if ((sc->sc_flags & MIDI_ROPEN) == 0)
		return;

	if (MIDI_REALTIME(ch))
		goto out;

	if (MIDI_INTRODUCER(ch))
	{
		sc->sc_pstatus = ch;
		sc->sc_explen = pmidicmdlen[(ch >> 4) & 7];
	}
	else if (sc->sc_explen == 0)
	{
		sc->sc_explen = pmidicmdlen[(sc->sc_pstatus >> 4) & 7];
		pmidi_input(sc, sc->sc_pstatus);
	}

	pmidi_input(sc, ch);

out:
	/* pmidi thru */
	if (sc->sc_flags & MIDI_THRU)
		sc->sc_if->output(sc->sc_hdl, ch);
}

static void
pmidi_input(sc, ch)
	struct pmidi_softc *sc;
	int ch;
{
	struct pmidi_rb *rbp = &sc->sc_inb;
	struct timeval tv, tsub;
	u_int len;

	if (MIDI_INTRODUCER(ch))
	{
		struct pmidi_packet_header *mp;

		microtime(&tv);
		if (sc->sc_pkthdr < 0)
		{
			sc->sc_tv = tv;
			sc->sc_pkthdr = 0;
			sc->sc_pktlen = 0;	
		}

		/*
		 * check packet header space.
		 */
		if (sc->sc_pkthdr != rbp->mr_wp ||
		    rbp->mr_used + sizeof(*mp) >= rbp->mr_size)
		{
overflow:
			pmidi_init_rb(rbp);
			sc->sc_pkthdr = -1;
			sc->sc_pktlen = 0;	
			return;
		} 

		/*
		 * make a new patcket frame
		 */
		mp = (struct pmidi_packet_header *) (rbp->mr_bp + sc->sc_pkthdr);

		rbp->mr_used += sizeof(*mp);
		rbp->mr_wp += sizeof(*mp);
		if (rbp->mr_wp >= rbp->mr_size)
			rbp->mr_wp -= rbp->mr_size;

		timersub(&tv, &sc->sc_tv, &tsub);
		mp->mp_code = MIDI_PKT_DATA;
		mp->mp_ticks = tsub.tv_sec * 1000;
		mp->mp_ticks += tsub.tv_usec / 1000;
	}
	else if (sc->sc_pkthdr < 0)
		return;

	/*
	 * write packet data.
	 */
	if (rbp->mr_used >= rbp->mr_size)
		goto overflow;

	*(rbp->mr_bp + rbp->mr_wp) = (u_int8_t) ch;
	rbp->mr_wp ++;
	if (rbp->mr_wp >= rbp->mr_size)
		rbp->mr_wp = 0;
	rbp->mr_used ++;

	/*
	 * get a complete packet.
	 */
	sc->sc_explen --;
	if (sc->sc_explen == 0 || ch == 0xf7)
	{
		struct pmidi_packet_header *mp;

		mp = (struct pmidi_packet_header *) (rbp->mr_bp + sc->sc_pkthdr);
		len = rbp->mr_used - sc->sc_pktlen;
		if (len >= rbp->mr_size || len < sizeof(*mp))
		{
			printf("%s: size mismatch(%d)\n",
				sc->sc_dev.dv_xname, len);
			goto overflow;
		}

		/* write the packet length */
		mp->mp_len = len - sizeof (*mp);

		/* round up if the header crosses the ring boundary */
		if (sc->sc_pkthdr + sizeof(*mp) > rbp->mr_size)
			bcopy(rbp->mr_bp + rbp->mr_size, rbp->mr_bp,
			      sc->sc_pkthdr + sizeof(*mp) -
			      rbp->mr_size);

		/* update the packet pointer */
		sc->sc_pktlen += len;
		sc->sc_pkthdr += len;
		if (sc->sc_pkthdr >= rbp->mr_size)
			sc->sc_pkthdr -= rbp->mr_size;

		selwakeup(&sc->sc_rsel);
		if (sc->sc_flags & MIDI_WANTR)
		{
			sc->sc_flags &= ~MIDI_WANTR;
			wakeup(rbp);
		}
	}
}
	
static void
pmidi_output(sc)
	struct pmidi_softc *sc;
{
	struct midi_hw_if *hw = sc->sc_if;
	struct pmidi_rb *rbp = &sc->sc_outb;
	struct pmidi_packet_header *mp;
	u_int pos, len, n;
	int error = 0;

	while (rbp->mr_used >= sizeof(*mp))
	{
		if (rbp->mr_rp + sizeof(*mp) > rbp->mr_size)
		{
			len = rbp->mr_size - rbp->mr_rp;
			len = sizeof(*mp) - len;
			bcopy(rbp->mr_bp, rbp->mr_bp + rbp->mr_size, len);
		}

		mp = (struct pmidi_packet_header *) (rbp->mr_bp + rbp->mr_rp);

		len = mp->mp_len;
		if (len  + sizeof(*mp) > rbp->mr_used)
			break;

		if (mp->mp_ticks > sc->sc_ticks)
			break;

#ifdef MIDI_DEBUG
		printf("code 0x%ld, len 0x%ld, tick 0x%ld\n",
			(u_long) mp->mp_code, (u_long) mp->mp_len,
			(u_long) mp->mp_ticks);
#endif	/* MIDI_DEBUG */
		switch (mp->mp_code)
		{
		case MIDI_PKT_SYSTM | MIDI_TIMER_INIT:
			sc->sc_ticks = 0;
			break;

		case MIDI_PKT_SYSTM | MIDI_TIMER_PAUSE:
			sc->sc_flags |= MIDI_PAUSED;
			break;

		case MIDI_PKT_SYSTM | MIDI_TIMER_UNPAUSE:
			sc->sc_flags &= ~MIDI_PAUSED;
			break;

		case MIDI_PKT_SYSTM | MIDI_PORT_THRU:
			sc->sc_flags |= MIDI_THRU;
			break;

		case MIDI_PKT_DATA:
			pos = rbp->mr_rp + sizeof(*mp);
			if (pos >= rbp->mr_size)
				pos -= rbp->mr_size;

			for (n = 0; n < len && error == 0; n ++)
			{
				error = hw->output(sc->sc_hdl, rbp->mr_bp[pos]);
				if (++ pos >= rbp->mr_size)
					pos = 0;
			}
			break;
		}

		len += sizeof(*mp);
		rbp->mr_used -= len;
		rbp->mr_rp += len;
		if (rbp->mr_rp >= rbp->mr_size)
			rbp->mr_rp -= rbp->mr_size;
	}

	if (rbp->mr_used <= rbp->mr_lowater)
	{
		selwakeup(&sc->sc_wsel);
		if (sc->sc_flags & MIDI_WANTW)
		{
			sc->sc_flags &= ~MIDI_WANTW;
			wakeup(rbp);
		}
	}
}

static void
pmidi_timer_write(arg)
	void *arg;
{
	struct pmidi_softc *sc = arg;

	if ((sc->sc_flags & MIDI_PAUSED) == 0)
		sc->sc_ticks += 1;

	sc->sc_hwtimer ++;
	pmidi_output(sc);
}

static void
pmidi_timer(arg)
	void *arg;
{
	struct pmidi_softc *sc = arg;
	int s;

	if (sc->sc_hwtimer > 0)
		return;

	if ((sc->sc_flags & MIDI_PAUSED) == 0)
		sc->sc_ticks += 1000 / hz;

	s = splaudio();
	pmidi_output(sc);
	splx(s);

	timeout(pmidi_timer, sc, 1);
}

int
pmidipoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	int unit = PMIDIUNIT(dev);
	struct pmidi_softc *sc = pmidi_cd.cd_devs[unit];
	int revents = 0;
	int s = splaudio();

	if (events & (POLLIN | POLLRDNORM))
		if ((sc->sc_flags & MIDI_ROPEN) == 0 || sc->sc_pktlen > 0)
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if ((sc->sc_flags & MIDI_WOPEN) == 0 ||
		    sc->sc_outb.mr_used <= sc->sc_outb.mr_lowater)
			revents |= events & (POLLOUT | POLLWRNORM);

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &sc->sc_rsel);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(p, &sc->sc_wsel);
	}
	splx(s);
	return (revents);
}

void
pmidi_attach_mi(mhwp, hdlp, dev)
	struct midi_hw_if *mhwp;
	void *hdlp;
	struct device *dev;
{
	struct audio_attach_args arg;

#ifdef DIAGNOSTIC
	if (mhwp == NULL) {
		printf("pmidi_attach_mi: NULL\n");
		return;
	}
#endif
	arg.type = AUDIODEV_TYPE_PMIDI;
	arg.hwif = mhwp;
	arg.hdl = hdlp;
	(void) config_found(dev, &arg, pmidiprint);
}

static int
pmidiprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	const char *type;

	if (pnp != NULL)
	{
		type = "mpu";
		printf("%s at %s", type, pnp);
	}
	return (UNCONF);
}
