/*	$NecBSD: mainbus.c,v 3.16 1999/08/01 10:53:04 honda Exp $	*/
/*	$NetBSD: mainbus.c,v 1.28 1999/03/19 04:58:46 cgd Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/eisa/eisavar.h>
#include <dev/pci/pcivar.h>

#include <dev/isa/isareg.h>		/* for ISA_HOLE_VADDR */

#ifndef	ORIGINAL_CODE
#include "opt_magic.h"
#include <machine/systmbusvar.h>
#include <i386/Cbus/dev/magicvar.h>
#endif	/* PC-98 */

#include "pci.h"
#include "eisa.h"
#include "isa.h"
#ifdef	ORIGINAL_CODE
#include "apm.h"

#if NAPM > 0
#include <machine/bioscall.h>
#include <machine/apmvar.h>
#endif
#endif /* PC-98 */

int	mainbus_match __P((struct device *, struct cfdata *, void *));
void	mainbus_attach __P((struct device *, struct device *, void *));

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

int	mainbus_print __P((void *, const char *));

union mainbus_attach_args {
	const char *mba_busname;		/* first elem of all */
	struct pcibus_attach_args mba_pba;
	struct eisabus_attach_args mba_eba;
	struct isabus_attach_args mba_iba;
#ifdef	ORIGINAL_CODE
#if NAPM > 0
	struct apm_attach_args mba_aaa;
#endif
#else	/* PC-98 */
	struct systmbus_attach_args mba_sba;
#endif	/* PC-98 */
};

/*
 * This is set when the ISA bus is attached.  If it's not set by the
 * time it's checked below, then mainbus attempts to attach an ISA.
 */
int	isa_has_been_seen;

/*
 * Same as above, but for EISA.
 */
int	eisa_has_been_seen;

#ifndef	ORIGINAL_CODE
/*
 * Same as above, but for systmbus.
 */
int	systm_has_been_seen;
#endif	/* PC-98 */

/*
 * Probe for the mainbus; always succeeds.
 */
int
mainbus_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{

	return 1;
}

/*
 * Attach the mainbus.
 */
void
mainbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	union mainbus_attach_args mba;
#ifndef	ORIGINAL_CODE
	void *ic = NULL, *dmat = NULL;
#endif	/* PC-98 */

	printf("\n");

#ifdef	CONFIG_DEVICES
	config_devices();
#endif	/* CONFIG_DEVICES */

	/*
	 * XXX Note also that the presence of a PCI bus should
	 * XXX _always_ be checked, and if present the bus should be
	 * XXX 'found'.  However, because of the structure of the code,
	 * XXX that's not currently possible.
	 */
#if NPCI > 0
	if (pci_mode_detect() != 0) {
		mba.mba_pba.pba_busname = "pci";
		mba.mba_pba.pba_iot = I386_BUS_SPACE_IO;
		mba.mba_pba.pba_memt = I386_BUS_SPACE_MEM;
		mba.mba_pba.pba_dmat = &pci_bus_dma_tag;
		mba.mba_pba.pba_flags = pci_bus_flags();
		mba.mba_pba.pba_bus = 0;
		config_found(self, &mba.mba_pba, mainbus_print);
	}
#endif

#ifdef	ORIGINAL_CODE
	if (memcmp(ISA_HOLE_VADDR(EISA_ID_PADDR), EISA_ID, EISA_ID_LEN) == 0 &&
	    eisa_has_been_seen == 0) {
		mba.mba_eba.eba_busname = "eisa";
		mba.mba_eba.eba_iot = I386_BUS_SPACE_IO;
		mba.mba_eba.eba_memt = I386_BUS_SPACE_MEM;
#if NEISA > 0
		mba.mba_eba.eba_dmat = &eisa_bus_dma_tag;
#endif
		config_found(self, &mba.mba_eba, mainbus_print);
	}
#endif	/* !PC-98 */

	if (isa_has_been_seen == 0) {
		mba.mba_iba.iba_busname = "isa";
		mba.mba_iba.iba_iot = I386_BUS_SPACE_IO;
		mba.mba_iba.iba_memt = I386_BUS_SPACE_MEM;
#ifdef	ORIGINAL_CODE
#if NISA > 0
		mba.mba_iba.iba_dmat = &isa_bus_dma_tag;
#endif
		config_found(self, &mba.mba_iba, mainbus_print);
#else	/* PC-98 */
		mba.mba_iba.iba_ic = NULL;
		mba.mba_iba.iba_dmat = NULL;
#if NISA > 0
		mba.mba_iba.iba_dmat = &isa_bus_dma_tag;
#endif
		config_found(self, &mba.mba_iba, mainbus_print);
		ic = mba.mba_iba.iba_ic;
		dmat = mba.mba_iba.iba_dmat;
#endif	/* PC-98 */
	}

#ifdef	ORIGINAL_CODE
#if NAPM > 0
	if (apm_busprobe()) {
		mba.mba_aaa.aaa_busname = "apm";
		config_found(self, &mba.mba_aaa, mainbus_print);
	}
#endif
#else	/* PC-98 */
	if (systm_has_been_seen == 0) {
		systm_has_been_seen = 1;
		mba.mba_sba.sba_busname = "systm";
		mba.mba_sba.sba_iot = I386_BUS_SPACE_IO;
		mba.mba_sba.sba_memt = I386_BUS_SPACE_MEM;
		mba.mba_sba.sba_dmat = dmat;
		mba.mba_sba.sba_ic = ic;
		config_found(self, &mba.mba_sba, mainbus_print);
	}
#endif	/* PC-98 */
}

int
mainbus_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	union mainbus_attach_args *mba = aux;

	if (pnp)
		printf("%s at %s", mba->mba_busname, pnp);
	if (strcmp(mba->mba_busname, "pci") == 0)
		printf(" bus %d", mba->mba_pba.pba_bus);
	return (UNCONF);
}
