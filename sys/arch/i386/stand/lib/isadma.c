/*	$NecBSD: isadma.c,v 1.1 1999/08/02 05:42:39 kmatsuda Exp $	*/
/*	$NetBSD: isadma.c,v 1.1.1.1 1997/03/14 02:40:32 perry Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/* from: NetBSD:dev/isa/isadma.c */

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>

#include "isadmavar.h"

#ifdef	ORIGINAL_CODE
#define	IO_DMA1		0x000		/* 8237A DMA Controller #1 */
#define	IO_DMA2		0x0C0		/* 8237A DMA Controller #2 */
#else	/* PC-98 */
#define	IO_DMA1		0x001		/* 8237A DMA Controller #1 */
#endif	/* PC-98 */
#ifdef	ORIGINAL_CODE
#define	DMA37MD_CASCADE	0xc0	/* cascade mode */
#define	DMA1_SMSK	(IO_DMA1 + 1*10)	/* single mask register */
#define	DMA1_MODE	(IO_DMA1 + 1*11)	/* mode register */
#define	DMA2_SMSK	(IO_DMA2 + 2*10)	/* single mask register */
#define	DMA2_MODE	(IO_DMA2 + 2*11)	/* mode register */
#else	/* PC-98 */
#define	DMA1_SMSK	(0x15)			/* single mask register */
#define	DMA1_MODE	(0x17)			/* mode register */
#endif	/* PC-98 */

/*
 * isa_dmacascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 */
void
isa_dmacascade(chan)
	int chan;
{

#ifdef ISADMA_DEBUG
#ifdef	ORIGINAL_CODE
	if (chan < 0 || chan > 7)
#else	/* PC-98 */
	if (chan < 0 || chan > 3)
#endif	/* PC-98 */
		panic("isa_dmacascade: impossible request"); 
#endif

#ifdef	ORIGINAL_CODE
	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0) {
		outb(DMA1_MODE, chan | DMA37MD_CASCADE);
		outb(DMA1_SMSK, chan);
	} else {
		chan &= 3;

		outb(DMA2_MODE, chan | DMA37MD_CASCADE);
		outb(DMA2_SMSK, chan);
	}
#endif	/* !PC-98 */
}
