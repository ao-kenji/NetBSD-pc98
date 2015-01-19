/*	$NecBSD: atapireg.h,v 1.4 1998/10/18 23:49:56 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
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

#ifndef	_ATAPIREG_H_
#define	_ATAPIREG_H_

#define	PHASE_CMDOUT		(ARS_DRQ | ARI_CMD)
#define	PHASE_DATAIN		(ARS_DRQ | ARI_IN)
#define	PHASE_DATAOUT		ARS_DRQ
#define	PHASE_COMPLETED		(ARI_IN | ARI_CMD)
#define	PHASE_COMPLETED_ALT	0

/*
 * ATA commands
 */
#define	ATAPIC_SRESET	0x08	/* software reset */
#define	ATAPIC_IDENTIFY	0xa1	/* get drive parameters */
#define	ATAPIC_PACKET	0xa0	/* execute packet command */

/*
 * Packet commands
 */
#define	ATAPI_TEST_UNIT_READY	0x00	/* check if the device is ready */
#define	ATAPI_REQUEST_SENSE	0x03	/* get sense data */
#define	ATAPI_START_STOP	0x1b	/* start/stop the media */
#define	ATAPI_PREVENT_ALLOW	0x1e	/* prevent/allow media removal */
#define	ATAPI_READ_CAPACITY	0x25	/* get volume capacity */
#define	ATAPI_READ_BIG		0x28	/* read data */
#define	ATAPI_WRITE_BIG		0x2a	/* write data */
#define	ATAPI_READ_TOC		0x43	/* get table of contents */
#define	ATAPI_READ_SUBCHANNEL	0x42	/* get subchannel info */
#define	ATAPI_PLAY_MSF		0x47	/* play by MSF address */
#define	ATAPI_PLAY_TRACK	0x48	/* play by track number */
#define	ATAPI_PAUSE		0x4b	/* stop/start audio operation */
#define	ATAPI_MODE_SELECT_BIG	0x55	/* set device parameters */
#define	ATAPI_MODE_SENSE	0x5a	/* get device parameters */
#define	ATAPI_PLAY_BIG		0xa5	/* play by logical block address */
#define	ATAPI_PLAY_CD		0xb4	/* universal play command */

#define	ATAPI_PKT_CMD_FTRE_DMA	0x01
#define	ATAPI_PKT_CMD_FTRE_OVL	0x02
#endif	/* !_ATAPIREG_H_ */
