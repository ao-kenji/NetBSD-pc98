/*	$NecBSD: scsi_low_pisa.c,v 1.13 1998/11/26 14:26:11 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
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

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isa/pisaif.h>

#include <machine/dvcfg.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_disk.h>

#include <i386/Cbus/dev/scsi_low.h>
#include <i386/Cbus/dev/scsi_low_pisa.h>

#define	SCSIBUS_RESCAN

int
scsi_low_deactivate(dh)
	pisa_device_handle_t dh;
{
	struct scsi_low_softc *sc = PISA_DEV_SOFTC(dh);

	sc->sl_flags |= HW_INACTIVE;

#ifdef	SCSI_LOW_POWFUNC
	untimeout(scsi_low_recover, sc);
#endif	/* SCSI_LOW_POWFUNC */
	untimeout(scsi_low_timeout, sc);

	return 0;
}

int
scsi_low_activate(dh)
	pisa_device_handle_t dh;
{
	struct scsi_low_softc *sc = PISA_DEV_SOFTC(dh);
	slot_device_res_t dr = PISA_RES_DR(dh);
	int error;

	sc->sl_flags &= ~HW_INACTIVE;
	sc->sl_cfgflags = DVCFG_MKCFG(DVCFG_MAJOR(sc->sl_cfgflags), \
				      DVCFG_MINOR(PISA_DR_DVCFG(dr)));
	sc->sl_irq = PISA_DR_IRQ(dr);

	if ((error = scsi_low_restart(sc, SCSI_LOW_RESTART_HARD, NULL)) != 0)
	{
		sc->sl_flags |= HW_INACTIVE;
		return error;
	}

	timeout(scsi_low_timeout, sc, SCSI_LOW_TIMEOUT_CHECK_INTERVAL * hz);

	/* rescan the scsi bus */
#ifdef	SCSIBUS_RESCAN
	if (PISA_RES_EVENT(dh) == PISA_EVENT_INSERT &&
	    sc->sl_start.tqh_first == NULL)
		scsi_probe_busses((int) sc->sl_link.scsipi_scsi.scsibus, -1, -1);
#endif
	return 0;
}

int
scsi_low_notify(dh, ev)
	pisa_device_handle_t dh;
	pisa_event_t ev;
{
	struct scsi_low_softc *sc = PISA_DEV_SOFTC(dh);

	switch(ev)
	{
	case PISA_EVENT_QUERY_SUSPEND:
		if (sc->sl_start.tqh_first != NULL)
			return SD_EVENT_STATUS_BUSY;
		break;

	default:
		break;
	}
	return 0;
}
