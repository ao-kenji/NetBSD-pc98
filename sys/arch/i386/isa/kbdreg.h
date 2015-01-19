/*	$NecBSD: kbdreg.h,v 1.9 1998/03/14 07:08:28 kmatsuda Exp $	*/
/*	$NetBSD: kbdreg.h,v 1.7 1995/06/28 04:30:59 cgd Exp $	*/

#ifndef ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */

/*
 * Keyboard definitions
 */

#ifdef	ORIGINAL_CODE
#include <dev/ic/i8042reg.h>
#endif	/* !PC-98 */

/* keyboard commands */
#ifdef	ORIGINAL_CODE
#define	KBC_RESET	0xFF	/* reset the keyboard */
#define	KBC_RESEND	0xFE	/* request the keyboard resend the last byte */
#define	KBC_SETDEFAULT	0xF6	/* resets keyboard to its power-on defaults */
#define	KBC_DISABLE	0xF5	/* as per KBC_SETDEFAULT, but also disable key scanning */
#define	KBC_ENABLE	0xF4	/* enable key scanning */
#define	KBC_TYPEMATIC	0xF3	/* set typematic rate and delay */
#define	KBC_SETTABLE	0xF0	/* set scancode translation table */
#define	KBC_MODEIND	0xED	/* set mode indicators (i.e. LEDs) */
#define	KBC_ECHO	0xEE	/* request an echo from the keyboard */
#else	/* PC-98 */
#define	KBC_RESET	0x00	/* reset the keyboard */
#define	KBC_TYPE	0x9F	/* get keyboard type */
#define	KBC_MODEIND	0x9D	/* set mode indicators (i.e. LEDs) */
#define	KBC_MODEIND2	0x70
#endif	/* PC-98 */

/* keyboard responses */
#ifdef	ORIGINAL_CODE
#define	KBR_EXTENDED	0xE0	/* extended key sequence */
#define	KBR_RESEND	0xFE	/* needs resend of command */
#define	KBR_ACK		0xFA	/* received a valid command */
#define	KBR_OVERRUN	0x00	/* flooded */
#define	KBR_FAILURE	0xFD	/* diagnosic failure */
#define	KBR_BREAK	0xF0	/* break code prefix - sent on key release */
#define	KBR_RSTDONE	0xAA	/* reset complete */
#define	KBR_ECHO	0xEE	/* echo response */
#else	/* PC-98 */
#define	KBR_RESEND	0xFC	/* needs resend of command */
#define	KBR_ACK		0xFA	/* received a valid command */
#endif	/* PC-98 */

#ifndef	ORIGINAL_CODE
#define	KBSTATP		0x43	/* kbd controller status port */
#define	KBS_DIB		0x02	/* kbd data in buffer */
#define	KBS_PERR	0x08	/* kbd parity error */
#define	KBS_OERR	0x10	/* kbd overrun error */
#define	KBS_FERR	0x20	/* kbd framing error */

#define	KBCMDP		0x43	/* kbd controller port */
#define	KBDATAP		0x41	/* kbd data port */
#define	KBOUTP		0x41	/* kbd data port */

#define	KC8_IR		0x40	/* i8251 internal reset */
#define	KC8_DIS		0x20	/* disable keyboard */
#define	KC8_ER		0x10	/* error reset */
#define	KC8_RST		0x08	/* keyboard reset */
#define	KC8_RxE		0x04	/* enable recieve */
#define	KC8_RTY		0x02	/* retry */
#define	KC8_TxE		0x01	/* enable transmitte */
#endif	/* PC-98 */
