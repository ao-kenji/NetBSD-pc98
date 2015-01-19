/*	$NecBSD: atapivar.h,v 1.7 1999/07/26 21:57:49 honda Exp $	*/
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
/*
 * Copyright (C) 1995 Cronyx Ltd.
 * Author Serge Vakulenko, <vak@cronyx.ru>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 */

#ifndef	_ATAPIVER_H_
#define	_ATAPIVER_H_

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/atapiconf.h>
#include <dev/isa/ccbque.h>

#define	ATAPI_BUS_WAITS		100000
#define	ATAPI_DRQ_WAITS		200000

struct atapires {
	u_int8_t code;		/* result code */
#define	RES_OK		0	/* i/o done */
#define	RES_ERR		1	/* i/o finished with error */
#define	RES_NOTRDY	2	/* controller not ready */
#define	RES_NODRQ	3	/* no data request */
#define	RES_INVDIR	4	/* invalid bus phase direction */
#define	RES_OVERRUN	5	/* data overrun */
#define	RES_UNDERRUN	6	/* data underrun */
#define	RES_BUSY	7	/* busy status */
	u_int8_t status;	/* status register contents */
	u_int8_t error;		/* error register contents */
	u_int8_t pad;		/* dummy */
} __attribute__((packed));

struct ataccb {
	TAILQ_ENTRY(ataccb) ac_cmdchain; 	/* ccb chain */
	struct scsipi_xfer *ac_xs;		/* xfer struct */
	struct buf *ac_bp; 			/* strategy bufp */

	struct scsipi_generic *ac_pcd;		/* scsipi command */
	int ac_cmdlen;				/* cmdlen */

	u_int8_t *ac_data;			/* data pointer */
	int ac_datalen;

#define	ATAPIF_READ		0x0001
#define	ATAPIF_WRITE		0x0002
#define	ATAPIF_DMA		0x0004
#define	ATAPIF_POLL		0x0008
#define	ATAPIF_SENSE		0x0010
	int ac_flags;

	struct atapires ac_result; 		/* resulting error code */
	struct scsipi_sense ac_sense_cmd;
	struct scsipi_sense_data ac_sense;
	u_int8_t ac_shortsense;
};

#define	ATAPI_MAX_CCB	10
GENERIC_CCB_ASSERT(atapi, ataccb)

struct atapi_drive {
	struct ataccbtab av_cmdtab;		/* ccb queue head */

#define	ATAPI_FREE		0
#define	ATAPI_CMDOUT		1
#define	ATAPI_DATAIN		2
#define	ATAPI_DATAOUT		3
#define	ATAPI_DISCON		4
#define	ATAPI_WAIT_DRQOFF	5
#define	ATAPI_MAX_PHASE		6
	u_int av_nextph;			/* expected next phase */
	u_int av_maxio;				/* maxio */
	u_int av_maxblk;			/* maxblks */

	u_char *av_data;			/* current data pointer */
	int av_datalen;

#define	AVBF_32BIT	0x01
#define	AVBF_CMD16	0x02
#define	AVBF_INTRCMD	0x04
#define	AVBF_SLOW	0x08
#define	AVBF_EMULCOMP	0x10
#define	AVBF_WFIFO12	0x20
#define	AVBF_ACCEL	0x40
#define	AVBF_BITS "\020\007accel\006wfifo12\005cmplemul\004slowproc\003intrcmd\002cmd16\001dword"
	u_int av_bflags;
};

struct atapi_drive *wdc_link_atapi __P((struct channel_link *));
struct ata_atapi_attach *atabus_scsipi_establish __P((struct atabus_softc *, struct channel_softc *, struct ata_atapi_attach *));
int wdc_reset_atapi __P((struct channel_softc *, u_int));
#endif	/* !_ATAPIVER_H_ */
