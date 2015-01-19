/*	$NecBSD: ct_isa.c,v 1.6 1999/07/26 06:32:01 honda Exp $	*/
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

#define	SCSIBUS_RESCAN

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_disk.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>
#include <machine/syspmgr.h>

#include <i386/Cbus/dev/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <i386/Cbus/dev/ct/ctvar.h>
#include <i386/Cbus/dev/ct/bshwvar.h>

#define	BSHW_IOSZ	0x08
#define	BSHW_IOBASE 	0xcc0
#define	BSHW_MEMSZ	(NBPG * 2)

static int ct_isa_match __P((struct device *, struct cfdata *, void *));
static void ct_isa_attach __P((struct device *, struct device *, void *));
static int ct_space_map __P((struct isa_attach_args *, struct bshw *, bus_space_handle_t *, bus_space_handle_t *));
static struct bshw *ct_find_hw __P((struct isa_attach_args *));

struct ct_isa_softc {
	struct ct_softc sc_ct;
	struct bshw_softc sc_bshw;
};

struct cfattach ct_isa_ca = {
	sizeof(struct ct_isa_softc), ct_isa_match, ct_isa_attach
};

static int
ct_isa_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh, memh;
	struct bshw *hw;
	int rv = 0;

	if (ia->ia_iobase == IOBASEUNK)
		ia->ia_iobase = BSHW_IOBASE;

	if ((hw = ct_find_hw(ia)) == NULL)
		return rv;
	if (ct_space_map(ia, hw, &ioh, &memh) != 0)
		return rv;

	rv = ctprobesubr(ia->ia_iot, ioh, 0,
			 BSHW_DEFAULT_HOSTID, BSHW_DEFAULT_CHIPCLK);
	if (rv != 0)
	{
		struct bshw_softc bshw_tab;
		struct bshw_softc *bs = &bshw_tab;

		memset(bs, 0, sizeof(*bs));
		bshw_read_settings(ia->ia_iot, ioh, bs);
		ia->ia_irq = bs->sc_irq;
		ia->ia_drq = bs->sc_drq;
		ia->ia_iosize = BSHW_IOSZ;
		if (memh != NULL)
			ia->ia_msize = BSHW_MEMSZ;
	}

	bus_space_unmap(ia->ia_iot, ioh, BSHW_IOSZ);
	if (memh != NULL)
		bus_space_unmap(ia->ia_memt, memh, BSHW_MEMSZ);
	return rv;
}

static void
ct_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ct_isa_softc *pct = (void *) self;
	struct ct_softc *ct = &pct->sc_ct;
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct bshw_softc *bs = &pct->sc_bshw;
	struct isa_attach_args *ia = aux;
	bus_dma_segment_t seg;
	struct bshw *hw;
	int rseg, error;
	caddr_t addr;

	printf("\n");
	hw = ct_find_hw(ia);
	if (ct_space_map(ia, hw, &ct->sc_ioh, &ct->sc_memh) != 0)
		panic("%s: bus io mem map failed\n", slp->sl_dev.dv_xname);
	
	ct->sc_iot = ia->ia_iot;
	ct->sc_memt = ia->ia_memt;
	ct->sc_dmat = ia->ia_dmat;
	syspmgr_alloc_delayh(&ct->sc_delayt, &ct->sc_delaybah);

	/* setup DMA map */
	if (isa_dmamap_create(ia->ia_ic, ia->ia_drq, MAXBSIZE, BUS_DMA_NOWAIT))
	{
		printf("%s: can't set up ISA DMA map\n", slp->sl_dev.dv_xname);
		return;
	}

	error = bus_dmamem_alloc(ct->sc_dmat, MAXBSIZE, NBPG, 0, &seg, 1,
				 &rseg, BUS_DMA_NOWAIT);
	if (error != 0 || rseg != 1)
	{
		printf("%s: can't set up ISA DMA map\n", slp->sl_dev.dv_xname);
		return;
	}

	error = bus_dmamem_map(ct->sc_dmat, &seg, rseg, MAXBSIZE, &addr,
			       BUS_DMA_NOWAIT);
	if (error != 0)
	{
		printf("%s: can't allocate DMA mem\n", slp->sl_dev.dv_xname);
		return;
	}

	/* setup machdep softc */
	bs->sc_hw = hw;
	bs->sc_bounce_phys = (caddr_t) vtophys(addr);
	bs->sc_bounce_addr = addr;
	bs->sc_bounce_size = MAXBSIZE;
	bs->sc_minphys = (1 << 24);
	bshw_read_settings(ct->sc_iot, ct->sc_ioh, bs);

	/* setup ct driver softc */
	ct->ct_hw = bs;
	ct->ct_dma_xfer_start = bshw_dma_xfer_start;
	ct->ct_pio_xfer_start = bshw_smit_xfer_start;
	ct->ct_dma_xfer_stop = bshw_dma_xfer_stop;
	ct->ct_pio_xfer_stop = bshw_smit_xfer_stop;
	ct->ct_bus_reset = bshw_bus_reset;
	ct->ct_synch_setup = bshw_synch_setup;

	ct->sc_xmode = CT_XMODE_DMA;
	if (ct->sc_memh != NULL)
		ct->sc_xmode |= CT_XMODE_PIO;
	ct->sc_chiprev = CT_WD33C93_B;
	ct->sc_chipclk = BSHW_DEFAULT_CHIPCLK;

	slp->sl_hostid = bs->sc_hostid;
	slp->sl_irq = ia->ia_irq;
	slp->sl_cfgflags = ia->ia_cfgflags;

	ctattachsubr(ct);

	ct->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE, 
					IPL_BIO, ctintr, ct);

	config_found(self, &slp->sl_link, ctprint);
}

static struct bshw *
ct_find_hw(ia)
	struct isa_attach_args *ia;
{

	return DVCFG_HW(&bshw_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
}

static int
ct_space_map(ia, hw, iohp, memhp)
	struct isa_attach_args *ia;
	struct bshw *hw;
	bus_space_handle_t *iohp;
	bus_space_handle_t *memhp;
{
	int error;

	*memhp = NULL;
	error = bus_space_map(ia->ia_iot, ia->ia_iobase, BSHW_IOSZ, 0, iohp);
	if (error != 0)
		return error;

	if ((hw->hw_flags & BSHW_SMFIFO) == 0 || ia->ia_maddr == MADDRUNK)
		return error;

	error = bus_space_map(ia->ia_memt, ia->ia_maddr, BSHW_MEMSZ, 0, memhp);
	if (error != 0)
		bus_space_unmap(ia->ia_iot, *iohp, BSHW_IOSZ);
	return error;
}
