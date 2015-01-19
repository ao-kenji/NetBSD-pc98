/*	$NecBSD: ser_pisa.c,v 1.2 1999/07/26 06:32:07 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/dvcfg.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>

#include <i386/Cbus/dev/serial/comvar.h>
#include <i386/Cbus/dev/serial/servar.h>
#include <i386/Cbus/dev/serial/ser_console.h>
#include <i386/Cbus/dev/serial/ser_pc98_Cbus.h>

#define	DEFAULT_MAJOR	17	/* XXX */

static int ser_pisa_match __P((struct device *, struct cfdata *, void *));
static void ser_pisa_attach __P((struct device *, struct device *, void *));
static int ser_activate __P((pisa_device_handle_t));
static int ser_deactivate __P((pisa_device_handle_t));
static int ser_pisa_map __P((pisa_device_handle_t, struct commulti_attach_args *, struct com_hw *));
static int ser_pisa_unmap __P((pisa_device_handle_t, struct commulti_attach_args *));
static int ser_pisa_emulintr __P((void *));

struct cfattach ser_pisa_ca = {
	sizeof(struct ser_softc), ser_pisa_match, ser_pisa_attach
};

struct pisa_functions ser_pisa_pd = {
	ser_activate, ser_deactivate,
};

static int
ser_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	struct commulti_attach_args ca;
	struct com_hw *hw;
	u_long dvcfg;
	struct device dev;
	struct cfdata *cf;
	int ppri = SER_PRI_PROBEHW;

	if (is_ser_comconsole_machdep(pa->pa_iot, PISA_DR_IMB(dr, PISA_IO0)))
		return 0;

	dvcfg = PISA_DR_DVCFG(dr);
	if (DVCFG_MAJOR(dvcfg) == 0)
		dvcfg = DVCFG_MKCFG(DEFAULT_MAJOR, DVCFG_MINOR(dvcfg));
#ifdef	SER_PISA_TRUST_MANFID
	else
		ppri = SER_PRI_MANFID;
#endif	/* SER_PISA_TRUST_MANFID */
	hw = ser_find_hw(dvcfg);
	if (hw == NULL)
		return 0;
	if (hw->type == COM_HW_I8251 || hw->type == COM_HW_I8251_F ||
	    hw->type == COM_HW_I8251_C)
		return 0;

	memset(&ca, 0, sizeof(ca));
	ser_setup_ca(&ca, hw);
	ca.ca_h.cs_iot = pa->pa_iot;
	ca.ca_cfgflags = dvcfg;
	ca.ca_probepri = ppri;

	if (ser_pisa_map(dh, &ca, hw) != 0)
		return 0;

	memset(&dev, 0, sizeof(dev));			/* XXX */
	dev.dv_cfdata = match;				/* XXX */
	cf = config_search(NULL, &dev, &ca);

	ser_pisa_unmap(dh, &ca);
	return (cf != NULL) ? 1 : 0;
}

void
ser_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ser_softc *sc = (void *) self;
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	bus_space_tag_t sbst = NULL, iot = pa->pa_iot;
	bus_space_handle_t sbsh = NULL;
	slot_device_res_t dr = PISA_RES_DR(dh);
	struct commulti_attach_args ca;
	struct com_switch *cswp = NULL;
	struct com_softc *csc = NULL;
	struct com_hw *hw;
	int (*intrfunc) __P((void *)); 
	void *arg;
	u_int dvcfg;
	int sno, ppri = SER_PRI_PROBEHW;

	printf("\n");

	pisa_register_functions(dh, self, &ser_pisa_pd);

	dvcfg = PISA_DR_DVCFG(dr);
	if (DVCFG_MAJOR(dvcfg) == 0)
		dvcfg = DVCFG_MKCFG(DEFAULT_MAJOR, DVCFG_MINOR(dvcfg));
#ifdef	SER_PISA_TRUST_MANFID
	else
		ppri = SER_PRI_MANFID;
#endif	/* SER_PISA_TRUST_MANFID */
	hw = ser_find_hw(dvcfg);

	for (sno = 0; sno < 1 /* hw->maxunit */; sno ++)
	{
		memset(&ca, 0, sizeof(ca));
		ca.ca_slave = sno;
		ca.ca_h.cs_iot = iot;
		ca.ca_cfgflags = dvcfg;
		ca.ca_emulintr = ser_pisa_emulintr;
		ca.ca_arg = (void *) PISA_DR_IRQ(dr);
		ca.ca_probepri = ppri;
		ser_setup_ca(&ca, hw);

		if (ser_pisa_map(dh, &ca, hw) != 0)
		{
			if (sno != 0)
				continue;
			panic("%s: can not map load\n", sc->sc_dev.dv_xname);
		}

		if (sno == 0)
		{
			sbst = ca.ca_h.cs_iot;
			sbsh = ca.ca_h.cs_ioh;
		}
		sc->sc_tbst = sbst;
		sc->sc_tbsh = sbsh;
		sc->sc_ibst = sbst;
		sc->sc_ibsh = sbsh;

		(void) config_found(self, &ca, serprint);

		if (sno == 0)
		{
			csc = ca.ca_m;
			cswp = csc->sc_cswp;
		}
		sc->sc_slaves[sno] = ca.ca_m;
	}

	if (cswp == NULL)
		return;

	/* attach interrupt handler */
	sc->sc_intr = cswp->cswt_intr;
	if (sc->sc_start != NULL)
	{
		intrfunc = serintr;
		arg = sc;
	}
	else
	{
		intrfunc = sc->sc_intr;
		arg = csc;
	}

	sc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, cswp->cswt_ipl,
					    intrfunc, arg);
	return;
}

static int
ser_activate(dh)
	pisa_device_handle_t dh;
{
	struct ser_softc *sc = PISA_DEV_SOFTC(dh);
	slot_device_res_t dr = PISA_RES_DR(dh);
	struct com_softc *csc;
	int i;

	for (i = 0; i < NSLAVES; i++)
	{
		csc = sc->sc_slaves[i];
		if (csc == NULL || csc->sc_sertty_activate == NULL)
			continue;

		csc->sc_arg = (void *) PISA_DR_IRQ(dr);
		csc->sc_sertty_activate(csc);
	}

	return 0;
}

static int
ser_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct ser_softc *sc = PISA_DEV_SOFTC(dh);
	struct com_softc *csc;
	int i;

	for (i = 0; i < NSLAVES; i++)
	{
		csc = sc->sc_slaves[i];
		if (csc == NULL || csc->sc_sertty_deactivate == NULL)
			continue;

		csc->sc_sertty_deactivate(csc);
	}

	return 0;
}

static int
ser_pisa_emulintr(arg)
	void *arg;
{
	u_long irq = (u_long) arg;

	softintr(irq);
	return 1;
}

static int
ser_pisa_map(dh, ca, hw)
	pisa_device_handle_t dh;
	struct commulti_attach_args *ca;
	struct com_hw *hw;
{
	bus_space_handle_t ioh;

	if (ca->ca_slave != 0)
		return ENOTTY;		/* only 1 */

	if (pisa_space_map_load(dh, PISA_IO0, hw->iatsz, hw->iat, &ioh))
		return ENOSPC;

	ca->ca_h.cs_ioh = ioh;
	ca->ca_h.cs_spioh = NULL;
	return 0;
}

static int
ser_pisa_unmap(dh, ca)
	pisa_device_handle_t dh;
	struct commulti_attach_args *ca;
{

	pisa_space_unmap(dh, ca->ca_h.cs_iot, ca->ca_h.cs_ioh);
	return 0;
}
