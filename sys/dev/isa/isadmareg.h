/*	$NecBSD: isadmareg.h,v 3.19 1999/08/05 08:54:05 kmatsuda Exp $	*/
/*	$NetBSD: isadmareg.h,v 1.6 1998/01/22 00:57:10 cgd Exp $	*/
#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */

#include <dev/ic/i8237reg.h>

/*
 * Register definitions for DMA controller 1 (channels 0..3):
 */
#define	DMA1_CHN(c)	(1*(2*(c)))		/* addr reg for channel c */
#define	DMA1_SR		(1*8)			/* status register */
#define	DMA1_SMSK	(1*10)			/* single mask register */
#define	DMA1_MODE	(1*11)			/* mode register */
#define	DMA1_FFC	(1*12)			/* clear first/last FF */
#define	DMA1_MASK	(1*15)			/* mask register */

#define	DMA1_IOSIZE	(1*16)

/*
 * Register definitions for DMA controller 2 (channels 4..7):
 */
#ifdef	ORIGINAL_CODE
#define	DMA2_CHN(c)	(2*(2*(c)))		/* addr reg for channel c */
#define	DMA2_SR		(2*8)			/* status register */
#define	DMA2_SMSK	(2*10)			/* single mask register */
#define	DMA2_MODE	(2*11)			/* mode register */
#define	DMA2_FFC	(2*12)			/* clear first/last FF */
#define	DMA2_MASK	(2*15)			/* mask register */

#define	DMA2_IOSIZE	(2*16)
#else	/* PC-98 */
#define	DMA2_CHN(c)	(1*(2*(c)))		/* addr reg for channel c */
#define	DMA2_SR		(1*8)			/* status register */
#define	DMA2_SMSK	(1*10)			/* single mask register */
#define	DMA2_MODE	(1*11)			/* mode register */
#define	DMA2_FFC	(1*12)			/* clear first/last FF */

#define	DMA2_IOSIZE	(1*12)
#endif	/* PC-98 */
