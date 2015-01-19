/*	$NecBSD: spkrreg.h,v 3.13 1998/03/14 07:08:39 kmatsuda Exp $	*/
/*	$NetBSD: spkrreg.h,v 1.2 1994/10/27 04:18:16 cgd Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */

/*
 * PIT port addresses and speaker control values
 */

#ifdef	ORIGINAL_CODE
#define PITAUX_PORT	0x61	/* port of Programmable Peripheral Interface */
#define PIT_ENABLETMR2	0x01	/* Enable timer/counter 2 */
#define PIT_SPKRDATA	0x02	/* Direct to speaker */

#define PIT_SPKR	(PIT_ENABLETMR2|PIT_SPKRDATA)
#else	/* PC-98 */
#define PITAUX_PORT	0x35	/* pc98 port C. */
#define PIT_SPKR	0x08
#endif	/* PC-98 */
