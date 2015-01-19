/*	$NecBSD: gatea20.c,v 1.2 1999/08/02 05:42:39 kmatsuda Exp $	*/
/*	$NetBSD: gatea20.c,v 1.2 1997/10/29 00:32:49 fvdl Exp $	*/

/* extracted from freebsd:sys/i386/boot/biosboot/io.c */

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>

#include "libi386.h"

#ifdef	ORIGINAL_CODE
#define K_RDWR 		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS 	0x64		/* keyboard status */
#define K_CMD	 	0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL 	0x01		/* output buffer full */
#define K_IBUF_FUL 	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0x9f		/* enable A20,
					   reset (!),
					   enable output buffer full interrupt
					   enable data line
					   disable clock line */
#else	/* PC-98 */
/* XXX: for ioports 0xf2, 0xf6 */
#endif	/* PC-98 */

/*
 * Gate A20 for high memory
 */
#ifdef	ORIGINAL_CODE
static unsigned char	x_20 = KB_A20;
#endif	/* !PC-98 */
void gateA20()
{
	__asm("pushfl ; cli");
#ifdef	ORIGINAL_CODE
#ifdef	IBM_L40
	outb(0x92, 0x2);
#else	IBM_L40
	while (inb(K_STATUS) & K_IBUF_FUL);
	while (inb(K_STATUS) & K_OBUF_FUL)
		(void)inb(K_RDWR);

	outb(K_CMD, KC_CMD_WOUT);
	delay(100);
	while (inb(K_STATUS) & K_IBUF_FUL);
	outb(K_RDWR, x_20);
	delay(100);
	while (inb(K_STATUS) & K_IBUF_FUL);
#endif	IBM_L40
#else	/* PC-98 */
	outb(0xf2, 0);
	outb(0xf6, 2);
#endif	/* PC-98 */
	__asm("popfl");
}
