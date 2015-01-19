/*	$NecBSD: sndtm.c,v 1.2 1999/04/15 01:36:48 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
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
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <i386/systm/sndtmvar.h>

void sndtmattach __P((int));

struct systm_timer_entry {
	struct systm_timer_entry *se_next;
	int (*se_func) __P((void *));
	void *se_arg;
};

struct sndtm_softc {
	struct device sc_dev;

	void *sc_arg;
	int (*sc_open) __P((void *, int, int (*) __P((void *)), void *));
	int (*sc_close) __P((void *));

	struct systm_timer_entry sc_tab;
} sndtm_gsc_data;

static struct sndtm_softc *sndtm_gsc = &sndtm_gsc_data;

void
sndtmattach(num)
	int num;
{

	return;
}

void *
systm_sound_timer_establish(sa)
	struct sndtm_attach_args *sa;
{
	struct sndtm_softc *sc = sndtm_gsc;

	if (sc == NULL || sc->sc_arg != NULL)
		return NULL;

	sc->sc_arg = sa->sa_arg;
	sc->sc_open = sa->sa_open;
	sc->sc_close = sa->sa_close;
	return sc;
}

int
systm_sound_timer_expire(arg)
	void *arg;
{
	struct sndtm_softc *sc = arg;
	struct systm_timer_entry *sep;

	for (sep = sc->sc_tab.se_next; sep != NULL; sep = sep->se_next)
		if (sep->se_func != NULL)
			sep->se_func (sep->se_arg);
	return 1;
}


int
systm_sound_timer_add(arg, func, timeo)
	void *arg;
	int (*func) __P((void *));
	int timeo;
{
	struct sndtm_softc *sc = sndtm_gsc;
	struct systm_timer_entry *sep;
	int error, s;

	if (sc == NULL || sc->sc_arg == NULL)
		return ENXIO;

	for (sep = sc->sc_tab.se_next; sep != NULL; sep = sep->se_next)
		if (sep->se_arg == arg)
			break;

	if (sep == NULL)
	{
		sep = malloc(sizeof(*sep), M_DEVBUF, M_NOWAIT);
		if (sep == NULL)
			return ENOMEM;

		sep->se_next = sc->sc_tab.se_next;
		sc->sc_tab.se_next = sep;
		sep->se_func = NULL;
		sep->se_arg = arg;
	}
	else if (sep->se_func != NULL)
		return EBUSY;

	s = splaudio();
	error = sc->sc_open(sc->sc_arg, timeo, systm_sound_timer_expire, sc);
	if (error == 0)
		sep->se_func = func;
	splx(s);

	return error;
}

int
systm_sound_timer_remove(arg)
	void *arg;
{
	struct sndtm_softc *sc = sndtm_gsc;
	struct systm_timer_entry *sep;
	int s;

	if (sc == NULL || sc->sc_arg == NULL)
		return ENXIO;

	for (sep = sc->sc_tab.se_next; sep != NULL; sep = sep->se_next)
		if (sep->se_arg == arg)
			break;
	if (sep == NULL || sep->se_func == NULL)
		return EINVAL;

	s = splaudio();
	sc->sc_close(sc->sc_arg);
	sep->se_func = NULL;
	splx(s);

	return 0;
}
