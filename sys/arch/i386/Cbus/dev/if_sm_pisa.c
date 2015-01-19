/*	$NecBSD: if_sm_pisa.c,v 1.11 1999/07/26 06:31:53 honda Exp $	*/
/*	$NetBSD: if_sm_pcmcia.c,v 1.2 1997/10/16 23:27:28 thorpej Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1997, 1998
 *	Kouichi Matsuda.  All rights reserved.
 */
/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pisaif.h>
#include <dev/ic/smc91cxxreg.h>
#include <dev/ic/smc91cxxvar.h>

#include <i386/pccs/tuple.h>

static int	sm_pisa_match __P((struct device *, struct cfdata *, void *));
void	sm_pisa_attach __P((struct device *, struct device *, void *));

struct sm_pisa_softc {
	struct	smc91cxx_softc sc_smc;		/* real "smc" softc */

	/* PISA-specific goo. */
	void	*sc_ih;				/* interrupt cookie */
	pisa_device_handle_t sc_dh;
};

struct cfattach sm_pisa_ca = {
	sizeof(struct sm_pisa_softc), sm_pisa_match, sm_pisa_attach
};

int	sm_pisa_nodeaddr __P((struct sm_pisa_softc *, u_int8_t *));

int	sm_deactivate __P((pisa_device_handle_t));
int	sm_activate __P((pisa_device_handle_t));
int	sm_pisa_enable __P((struct smc91cxx_softc *));
void	sm_pisa_disable __P((struct smc91cxx_softc *));

struct pisa_functions sm_pd = {
	sm_activate, sm_deactivate,
};

static int
sm_pisa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pisa_attach_args *pa = aux;
	pisa_device_handle_t dh = pa->pa_dh;
	slot_device_res_t dr = PISA_RES_DR(dh);
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;
	u_int16_t tmp;
	int rv = 0;
	extern const char *smc91cxx_idstrs[];

	/* XXX: check if ES_IO_PORTS * 2 (index 0x3) */
	if (PISA_DR_IMS(dr, PISA_IO0) != SMC_IOSIZE)
		return (0);

	/* Map i/o space. */
	if (pisa_space_map(dh, PISA_IO0, &ioh))
		return (0);

	/* Check that high byte of BANK_SELECT is what we expect. */
	tmp = bus_space_read_2(iot, ioh, BANK_SELECT_REG_W);
	if ((tmp & BSR_DETECT_MASK) != BSR_DETECT_VALUE)
		goto out;

	/*
	 * Switch to bank 0 and perform the test again.
	 * XXX INVASIVE!
	 */
	bus_space_write_2(iot, ioh, BANK_SELECT_REG_W, 0);
	tmp = bus_space_read_2(iot, ioh, BANK_SELECT_REG_W);
	if ((tmp & BSR_DETECT_MASK) != BSR_DETECT_VALUE)
		goto out;

	/*
	 * Check for a recognized chip id.
	 * XXX INVASIVE!
	 */
	bus_space_write_2(iot, ioh, BANK_SELECT_REG_W, 3);
	tmp = bus_space_read_2(iot, ioh, REVISION_REG_W);
	if (smc91cxx_idstrs[RR_ID(tmp)] == NULL)
		goto out;

	/*
	 * Assume we have an SMC91Cxx.
	 */
	rv = 1;

 out:
	pisa_space_unmap(dh, iot, ioh);
	return (rv);
}

void
sm_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sm_pisa_softc *psc = (struct sm_pisa_softc *)self;
	struct smc91cxx_softc *sc = &psc->sc_smc;
	struct pisa_attach_args *pa = aux;
	slot_device_handle_t dh = pa->pa_dh;
	bus_space_tag_t iot = pa->pa_iot;
	bus_space_handle_t ioh;
	u_int8_t *enaddr = NULL;

	printf("\n");

	if (pisa_space_map(dh, PISA_IO0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	psc->sc_dh = pa->pa_dh;
	pisa_register_functions(pa->pa_dh, self, &sm_pd);

	sc->sc_bst = iot;
	sc->sc_bsh = ioh;

	sc->sc_enabled = 1;
	sc->sc_enable = sm_pisa_enable;
	sc->sc_disable = sm_pisa_disable;

	/* XXX: mac address will out to a card twice. */
	sm_pisa_nodeaddr(psc, enaddr);
	/* Perform generic intialization. */
	smc91cxx_attach(sc, enaddr);

	psc->sc_ih = pisa_intr_establish(dh, PISA_PIN0, IPL_NET,
	    smc91cxx_intr, sc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}
}

int
sm_activate(dh)
	pisa_device_handle_t dh;
{
	struct sm_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct smc91cxx_softc *sc = &psc->sc_smc;
	u_int8_t *enaddr = NULL;	/* XXX */

	/* XXX: check return value */
	sm_pisa_nodeaddr(psc, enaddr);
	sc->sc_enabled = 1;
	return 0;
}

int
sm_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct sm_pisa_softc *psc = PISA_DEV_SOFTC(dh);
	struct smc91cxx_softc *sc = &psc->sc_smc;
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	int s;

	s = splimp();

	if_down(ifp);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_UP);
	sc->sc_enabled = 0;
	splx(s);

	return 0;
}

int
sm_pisa_enable(sc)
	struct smc91cxx_softc *sc;
{
	struct sm_pisa_softc *psc = (struct sm_pisa_softc *) sc;

	return pisa_slot_device_enable(psc->sc_dh);
}

void
sm_pisa_disable(sc)
	struct smc91cxx_softc *sc;
{
	struct sm_pisa_softc *psc = (struct sm_pisa_softc *) sc;

	pisa_slot_device_disable(psc->sc_dh);
}

#define	PRODUCT_NAMELEN	256

#define	XJMANUF	"Megahertz"
#define	XJVERS	"CC10"
#define	MHVERS	"ETHERNET ADAPTOR"

int
sm_pisa_nodeaddr(psc, enaddr)
	struct sm_pisa_softc *psc;
	u_int8_t *enaddr;
{
	struct smc91cxx_softc *sc = &psc->sc_smc;
	pisa_device_handle_t dh = psc->sc_dh;
	slot_device_slot_tag_t st = dh->dh_stag;
	struct cis_vers_1 *vsp;
	u_int8_t *ids;
	u_short cur_bank;
	char manuf[PRODUCT_NAMELEN], vers[PRODUCT_NAMELEN];
	char add_info1[PRODUCT_NAMELEN], add_info2[PRODUCT_NAMELEN];
	char enaddr_str[12];
	u_int8_t myla[ETHER_ADDR_LEN];
	int i, j;

	if (st->st_type != PISA_SLOT_PCMCIA)
		return 0;

	/* XXX: check CIS_FUNCE:0x04 */

	/* get additional informations from CIS_VERS_1 tuple. */
	pisa_device_info(dh, CIS_REQINIT);
	ids = pisa_device_info(dh, CIS_REQCODE(CIS_VERS_1, CIS_REQUNK));
	if (ids == NULL)
		return ENOENT;

	vsp = (struct cis_vers_1 *) ids;
	ids = (u_int8_t *) vsp->v1_str;
	strncpy(manuf, ids, PRODUCT_NAMELEN - 1);
	while (*ids++);
	strncpy(vers, ids, PRODUCT_NAMELEN - 1);
	while (*ids++);
	strncpy(add_info1, ids, PRODUCT_NAMELEN - 1);
	while (*ids++);
	strncpy(add_info2, ids, PRODUCT_NAMELEN - 1);

	if (strncmp(manuf, XJMANUF, sizeof(XJMANUF) - 1) == 0) {
		/* we are Megahertz pcmcia card */

		if (strncmp(vers, XJVERS, sizeof(XJVERS) - 1) == 0) {
			strcpy(enaddr_str, add_info2);
		} else if (strncmp(vers, MHVERS, sizeof(MHVERS) - 1) == 0) {
			strcpy(enaddr_str, add_info1);
		} else {
			printf("%s: unable to read MAC address from CIS\n",
			    sc->sc_dev.dv_xname);
			return (1);
		}

		memset(myla, 0, sizeof(myla));
		for (i = 0; i < 6; i++) {
			for (j = 0; j < 2; j++) {
				/* Convert to upper case. */
				if (enaddr_str[(i * 2) + j] >= 'a' &&
				    enaddr_str[(i * 2) + j] <= 'z')
					enaddr_str[(i * 2) + j] -= 'a' - 'A';

				/* Parse the digit. */
				if (enaddr_str[(i * 2) + j] >= '0' &&
				    enaddr_str[(i * 2) + j] <= '9')
					myla[i] |= enaddr_str[(i * 2) + j]
					    - '0';
				else if (enaddr_str[(i * 2) + j] >= 'A' &&
					 enaddr_str[(i * 2) + j] <= 'F')
					myla[i] |= enaddr_str[(i * 2) + j]
					    - 'A' + 10;
				else {
					/* Bogus digit!! */
					goto out;
				}

				/* Compensate for ordering of digits. */
				if (j == 0)
					myla[i] <<= 4;
			}
		}
out:
		if (i >= 6) {
			/* Successfully parsed. */
			enaddr = myla;
		} else {
			printf("%s: unable to read MAC address from CIS\n",
			    sc->sc_dev.dv_xname);
			return (1);
		}
	}

	if (enaddr != NULL) {
		/* XXX */
		cur_bank = bus_space_read_2(sc->sc_bst, sc->sc_bsh, BANK_SELECT_REG_W);
		bus_space_write_2(sc->sc_bst, sc->sc_bsh, BANK_SELECT_REG_W, 1);
		for (i = 0; i < 3; i++) {
			bus_space_write_2(sc->sc_bst, sc->sc_bsh,
			    IAR_ADDR0_REG_W + i * 2,
			    enaddr[i * 2] |
			    (enaddr[i * 2 + 1] << 8));
		}
		bus_space_write_2(sc->sc_bst, sc->sc_bsh, BANK_SELECT_REG_W, cur_bank);
	}

	/* XXX: for more case? (Ositech, Motorola Mariner) */
	return 0;
}
