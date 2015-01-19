/*	$NecBSD: ser.c,v 1.2 1999/07/23 05:39:10 kmatsuda Exp $	*/
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
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/servar.h>

extern struct cfdriver ser_cd;

int
serprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct commulti_attach_args *ca = aux;

	if (pnp)
		printf("ser at %s", pnp);
	printf(" slave %d", ca->ca_slave);
	return (UNCONF);
}

#ifndef	SERINTR_MAXLOOP
#define	SERINTR_MAXLOOP	256
#endif	/* !SERINTR_MAXLOOP */

int
serintr(arg)
	void *arg;
{
	struct ser_softc *sc = arg;
	int cnt = SERINTR_MAXLOOP, result;

	/*
	 * Hardware timer stop.
	 */
	if (sc->sc_stop != NULL)
		(*sc->sc_stop) (sc);

	do
	{
		result = 0;

		if (sc->sc_slaves[0] != NULL)
			result |= ((*sc->sc_intr)((void *)sc->sc_slaves[0]));

		if (sc->sc_slaves[1] != NULL)
			result |= ((*sc->sc_intr)((void *)sc->sc_slaves[1]));
#if	0
		if (sc->sc_slaves[2] != NULL)
			result |= ((*sc->sc_intr)((void *)sc->sc_slaves[2]));

		if (sc->sc_slaves[3] != NULL)
			result |= ((*sc->sc_intr)((void *)sc->sc_slaves[3]));
#endif
	}
	while (result != 0 && cnt -- > 0);

	/*
	 * Hardware timer restart.
	 */
	if (sc->sc_start != NULL && sc->sc_hwtref > 0)
		(*sc->sc_start) (sc);

	return 1;
}
