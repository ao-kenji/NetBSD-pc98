/*	$NecBSD: qws_pisa.c,v 1.9 1999/07/23 11:04:39 honda Exp $	*/
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

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <i386/Cbus/dev/nec86reg.h>
#include <i386/Cbus/dev/nec86hwvar.h>
#include <i386/Cbus/dev/nec86var.h>

#include <i386/Cbus/dev/ymsvar.h>
#include <i386/Cbus/dev/joyvar.h>
#include <i386/Cbus/dev/mpu401var.h>

#include <i386/Cbus/dev/qwsvar.h>

#include "joy_qws.h"
#include "pcm_qws.h"
#include "yms_qws.h"
#include "mpu_qws.h"

static int qws_pisa_match __P((struct device *, struct cfdata *, void *));
static void qws_pisa_attach __P((struct device *, struct device *, void *));
static int qws_pisa_deactivate __P((pisa_device_handle_t));
static int qws_pisa_activate __P((pisa_device_handle_t));

struct qws_pisa_softc {
	struct 	device sc_dev;

	int sc_enabled;

	void *sc_n86;
	void *sc_joy;
	void *sc_mpu;
	void *sc_yms;
};

extern struct cfdriver qws_cd;

struct cfattach qws_pisa_ca = {
	sizeof(struct qws_pisa_softc), qws_pisa_match, qws_pisa_attach
};

struct pisa_functions qws_pd = {
	qws_pisa_activate, qws_pisa_deactivate,
};

/*********************************************************
 *  qws probe attach
 *********************************************************/
static int
qws_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{

	/* always ok */
	return 1;
}

static void
qws_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct qws_pisa_softc *sc = (void *) self;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_tag_t iot = pa->pa_iot;
	slot_device_res_t dr = PISA_RES_DR(dh);
	struct qws_attach_args qa;
	
	printf("\n");

	pisa_register_functions(dh, self, &qws_pd);
	sc->sc_enabled = 1;

	qa.qa_dh = dh;
	qa.qa_iot = iot;
	qa.qa_cfgflags = PISA_DR_DVCFG(dr);

	/* 1) serach nec86 part */
	qa.qa_sc = NULL;
	qa.qa_name = "nec86";
	config_found(self, &qa, NULL);
	sc->sc_n86 = qa.qa_sc;

	/* 2) serach ym230x part */
	qa.qa_sc = NULL;
	qa.qa_name = "yms";
	config_found(self, &qa, NULL);
	sc->sc_yms = qa.qa_sc;

	/* 3) serach joy part */
	qa.qa_sc = NULL;
	qa.qa_name = "joy";
	config_found(self, &qa, NULL);
	sc->sc_joy = qa.qa_sc;

	/* 4) search midi part */
	qa.qa_sc = NULL;
	qa.qa_name = "mpu";
	config_found(self, &qa, NULL);
	sc->sc_mpu = qa.qa_sc;
}

static int
qws_pisa_activate(dh)
	pisa_device_handle_t dh;
{
	struct qws_pisa_softc *sc = PISA_DEV_SOFTC(dh);

#if	NPCM_QWS > 0
	if (sc->sc_n86 != NULL)
		nec86_attachsubr(sc->sc_n86);
#endif	/* NPCM_QWS > 0 */

#if	NYMS_QWS > 0
	if (sc->sc_yms != NULL)
		yms_activate(sc->sc_yms);
#endif	/* NPCM_QWS > 0 */

#if	NMPU_QWS > 0
	if (sc->sc_mpu != NULL)
		mpu401_activate(sc->sc_mpu);
#endif	/* NMPU_QWS > 0 */

	sc->sc_enabled = 1;
    	return 0;
}
	
static int
qws_pisa_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct qws_pisa_softc *sc = PISA_DEV_SOFTC(dh);

	sc->sc_enabled = 0;
	return 0;
}
