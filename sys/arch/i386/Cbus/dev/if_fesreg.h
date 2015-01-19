/*	$NecBSD: if_fesreg.h,v 1.1 1998/06/08 02:09:32 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 *  Copyright (c) 1998
 *	Kouichi Matsuda. All rights reserved.
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
 *      This product includes software developed by Kouichi Matsuda.
 * 4. The names of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Register informations of
 *	Sun Electronics AngeLan AL-98 Ethernet interface
 *	Ungermann-Bass Access/PC N98C+ (PC85151) Ethernet interface
 * for NetBSD/pc98 by Kouichi Matsuda.
 */

/*
 * EEPROM allocation of AL-98.
 */
#define FES_AL98_EEP_ADDR	0x00	/* Station address.  (0-5)	*/

/*
 * Registers (ASIC) on AL-98.
 */
/* AngeLan Asic register 0 -- EEPROM control */
#define	FES_AL98_EEP	0	/* EEPROM control register */
#define FES_AL98_EEP_SELECT	0x01	/* EEPROM chip select		*/
#define FES_AL98_EEP_CLOCK	0x04	/* EEPROM shift clock		*/
#define FES_AL98_EEP_DATA	0x08	/* EEPROM data bit		*/
#define FES_AL98_EEP_EEPROM	0x80	/* EEPROM feature bit		*/

#define FES_AL98_EEP_DATABIT	0x10	/* data bit (EEPROM to CPU)	*/

/* AngeLan Asic register 1 -- EEPROM configuration */
#define	FES_AL98_ECFG	1	/* EEPROM configuration register */
#define FES_AL98_ECFG_EEPEN	0x00	/* EEPROM access enable		*/

#define FES_AL98_EEPROM_SIZE	32	/* valid EEPROM data (max 128 bytes) */

/*
 * Registers on Ungermann-Bass Access/PC N98C+ (PC85151). 
 */
/* Ungerman-Bass Access/PC N98C+ (PC85151) ASIC register -- Station address */
#define	FES_PC85151_SA0		0	/* Station address #1 */
#define	FES_PC85151_SA1		1	/* Station address #2 */
#define	FES_PC85151_SA2		2	/* Station address #3 */
#define	FES_PC85151_SA3		3	/* Station address #4 */
#define	FES_PC85151_SA4		4	/* Station address #5 */
#define	FES_PC85151_SA5		5	/* Station address #6 */
#define	FES_PC85151_CKSUM	6	/* Station address checksum. */	
#define	FES_PC85151_IDENT	7	/* Identification, always 0xFF */
#define	FES_IDENT_PC85151	0xFF
