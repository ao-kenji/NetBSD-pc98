/*	$NecBSD: mpu_scp.c,v 1.8 1999/07/31 15:18:34 honda Exp $	*/
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

#include <machine/cpu.h>
#include <machine/pio.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <i386/Cbus/dev/scpvar.h>
#include <i386/Cbus/dev/mpu401var.h>

static int	mpu_scp_match __P((struct device *, struct cfdata *, void *));
void	mpu_scp_attach __P((struct device *, struct device *, void *));

struct mpu_scp_softc {
	struct mpu401_softc sc_mpu401;
};

struct cfattach mpu_scp_ca = {
	sizeof(struct mpu_scp_softc), mpu_scp_match, mpu_scp_attach
};

static int
mpu_scp_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct scp_attach_args *sa = aux;

	if (strcmp("mpu", sa->sa_name) != 0)
		return 0;

	return 1;
}

void
mpu_scp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mpu_scp_softc *sc = (void *) self;
	struct mpu401_softc *msc = &sc->sc_mpu401;
	struct scp_attach_args *sa = aux;

	printf(": UART mode\n");

	msc->sc_iot = sa->sa_iot;
	msc->sc_ioh = sa->sa_ioh;
	mpu401_attachsubr(msc);

	sa->sa_arg = msc;
	sa->sa_activate = mpu401_activate;
	sa->sa_intr = mpu401_intr;
}
