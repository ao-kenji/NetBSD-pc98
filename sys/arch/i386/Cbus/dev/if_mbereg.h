/*	$NecBSD: if_mbereg.h,v 1.3 1999/04/06 07:21:25 kmatsuda Exp $	*/
/*	$NetBSD: if_fereg.h,v 1.2 1998/01/05 07:31:09 perry Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999
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
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold,
 * in both source and binary form provided that the above copyright,
 * these terms and the following disclaimer are retained.  The name of
 * the author and/or the contributor may not be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Hardware specification of various 86960/86965 based Ethernet cards.
 * Contributed by M.S. <seki@sysrap.cs.fujitsu.co.jp>
 */
/*
 * Register informations of
 *	Ratoc REX-9880/9881/9882/9883 Ethernet interface
 *	Ungermann-Bass Access/PC N98C+ (PC85152)
 *	Ungermann-Bass Access/NOET N98 (PC86132) Ethernet interface
 *	UB Networks Access/CARD (JC89532A) Ethernet interface
 *	CONTEC C-NET(98)P2 Ethernet interface
 *	TDK LAC-98012/13/25 Ethernet interface
 *	CONTEC C-NET(9N)E Ethernet interface
 *	CONTEC C-NET(9N)C Ethernet interface
 * for NetBSD/pc98 by Kouichi Matsuda.
 */

/*
 * EEPROM allocation of AT1700/RE2000.
 */
#define FE_ATI_EEP_ADDR		0x08	/* Station address.  (8-13)	*/
#define	FE_ATI_EEP_MEDIA	0x18	/* Media type.			*/
#define	FE_ATI_EEP_MAGIC	0x19	/* XXX Magic.			*/
#define FE_ATI_EEP_MODEL	0x1e	/* Hardware type.		*/
#define	FE_ATI_EEP_REVISION	0x1f	/* Hardware revision.		*/

#define FE_ATI_MODEL_AT1700T	0x00
#define FE_ATI_MODEL_AT1700BT	0x01
#define FE_ATI_MODEL_AT1700FT	0x02
#define FE_ATI_MODEL_AT1700AT	0x03

/*
 * Registers (ASIC) on RE1000.
 */
/* IRQ configuration. */
#define	FE_RE1000_ICR	16	/* IRQ Configuration Register */
#define	FE_RE1000_ICR_IRQ	0xf0
#define	FE_RE1000_ICR_IRQSHIFT	4

/* Station addrres. */
#define	FE_RE1000_SA0		17	/* Station address #1 */
#define	FE_RE1000_SA1		19	/* Station address #2 */
#define	FE_RE1000_SA2		21	/* Station address #3 */
#define	FE_RE1000_SA3		23	/* Station address #4 */
#define	FE_RE1000_SA4		25	/* Station address #5 */
#define	FE_RE1000_SA5		27	/* Station address #6 */

#define	FE_RE1000_CKSUM		29	/* Station address checksum */

/*
 * Registers on MBH10302.
 */

#define FE_MBH0		0x10	/* ???  Including interrupt.	*/
#define FE_MBH1		0x11	/* ???				*/
#define FE_MBH10	0x1A	/* Station address.  (10 - 15)	*/

/* Values to be set in MBH0 register.  */
#define FE_MBH0_MAGIC	0x0D	/* Just a magic constant?	*/
#define FE_MBH0_INTR	0x10	/* Master interrupt control.	*/

#define FE_MBH0_INTR_ENABLE	0x10	/* Enable interrupts.	*/
#define FE_MBH0_INTR_DISABLE	0x00	/* Disable interrupts.	*/

/*
 * Registers on Ratoc REX-9980/9981/9982/9883. 
 */
#define	FE_REX80_IECFR		16	/* IRQ conf and EPROM access */
#define	FE_REX80_IECFR_IRQ	0xF0	/* IRQ configuration mask */
#define	FE_REX80_IECFR_DATABIT	0x01	/* Data bit from EPROM */

/*
 * Registers on Ungermann-Bass Access/PC N98C+ (PC85152). 
 */
#define	FE_PC85152_IRQ		0x14	/* IRQ configuration */	
#define	FE_PC85152_ADDR		0x18	/* Station address. (offset 24-29) */	
#define	FE_PC85152_CKSUM	0x1e	/* Station address checksum. */	

/*
 * Registers on UB Networks Access/CARD (JC89532A). 
 */
#define	FE_JC89532_ADDR		0x18	/* Station address. (offset 24-29) */	
#define	FE_JC89532_CKSUM	0x1e	/* Station address checksum. */	

/*
 * Registers on CONTEC C-NET(98)P2.
 */
/*
 * XXX: should be in sys/dev/ic/mb86960reg.h
 * Silicon Systems' 73Q8377 has EEPROM data and control features at BPMR12.
 */

/* BMPR12 -- EEPROM data and control */
#define	FE_B12_EEPROM	0x10	/* EEPROM feature bit			*/

#define	FE_B12_DATA	0x01	/* EEPROM data bit			*/
#define	FE_B12_SELECT	0x02	/* EEPROM chip select			*/
#define	FE_B12_CLOCK	0x04	/* EEPROM shift clock			*/

/* EEPROM (byte) allocation of CONTEC C-NET(98)P2. */
#define	FE_CNET98P2_EEP_PNPACT	0x01	/* PnP power up activation */
	/* PnP Configuration Registers */
#define	FE_CNET98P2_EEP_PNP41	0x02	/* PnP Reg 0x41: boot device address */
#define	FE_CNET98P2_EEP_PNP40	0x03	/* PnP Reg 0x40: boot device address */
#define	FE_CNET98P2_EEP_PNP44	0x05	/* PnP Reg 0x44: boot device enable */
#define	FE_CNET98P2_EEP_PNP61	0x06	/* PnP Reg 0x61: iobase LSB */
#define	FE_CNET98P2_EEP_PNP60	0x07	/* PnP Reg 0x60: iobase MSB */
#define	FE_CNET98P2_EEP_PNP71	0x08	/* PnP Reg 0x71: IRQ level/type */
#define	FE_CNET98P2_EEP_PNP70	0x09	/* PnP Reg 0x70: IRQ */
	/* 0x0a - 0x0f: RESERVED */
#define	FE_CNET98P2_EEP_ADDR	0x10	/* Station address. (16-21) */
#define	FE_CNET98P2_EEP_CKSUM	0x16	/* Station addres checksum key. */
#define	FE_CNET98P2_EEP_DUPLEX	0x19	/* duplex mode 1: full 0: half */

/*
 * Registers on TDK LAC-98012/98013/98025. 
 */
#define	FE_LAC2		18	/* Unknown; to be set to 0 or 1. */

#define	FE_LAC4		20	/* access to EEPROM and configure irq */
#define	FE_LAC4_EEP	0x0C	/* EEPROM access bit mask */
#define	FE_LAC4_IRQ	0xF0	/* IRQ configuration mask */
#define	FE_LAC4_EEP_DATA	0x08	/* X24C01 Serial Data (SDA) pin HIGH */
#define	FE_LAC4_EEP_CLOCK	0x04	/* X24C01 Serial Clock (SCL) pin HIGH */

#define	FE_LAC_EEP_DATABIT	0x01	/* Data bit from EEPROM */

/* EEPROM allocation of TDK LAC-98012/98013/98025. */
#define	FE_LAC_EEP_ADDR		0x00	/* Station address. (0-5) */

#define	FE_LAC_IRQMASK		0x1068	/* valid irq 3, 5, 6 and 12 */

/*
 * Registers on CONTEC C-NET(9N)E Ethernet.
 */
#define	FE_CNET9NE_IRR		16	/* interrupt related register */
/* all even ports have copy data of FE_CNET9NE_IRR */

#define	FE_CNET9NE_SA0		17	/* Station address #1 */
#define	FE_CNET9NE_SA1		19	/* Station address #2 */
#define	FE_CNET9NE_SA2		21	/* Station address #3 */
#define	FE_CNET9NE_SA3		23	/* Station address #4 */
#define	FE_CNET9NE_SA4		25	/* Station address #5 */
#define	FE_CNET9NE_SA5		27	/* Station address #6 */

#define	FE_CNET9NE_CKSUM	29	/* Station address checksum data */

/*
 * Registers on CONTEC C-NET(9N)C Ethernet.
 */
#define	FE_CNET9NC_SA0		17	/* Station address #1 */
#define	FE_CNET9NC_SA1		19	/* Station address #2 */
#define	FE_CNET9NC_SA2		21	/* Station address #3 */
#define	FE_CNET9NC_SA3		23	/* Station address #4 */
#define	FE_CNET9NC_SA4		25	/* Station address #5 */
#define	FE_CNET9NC_SA5		27	/* Station address #6 */

#define	FE_CNET9NC_CKSUM	29	/* Station address checksum data */
