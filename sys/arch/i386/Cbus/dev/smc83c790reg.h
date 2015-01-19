/*	$NecBSD: smc83c790reg.h,v 1.1 1999/01/18 09:58:46 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998, 1999
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * SMC 83C795 Lan Controller Registers (NIC)
 *
 *	Normal mode layout
 */
/*
 * Page 0 register offsets
 */
#define	WE790_P0_CMD		0	/* Command Register */
#define	WE790_P0_INCRL		1	/* (read) */
#define	WE790_P0_RSTART		1	/* Receive Start Page Reg (write) */
#define	WE790_P0_INCRH		2	/* (read) */
#define	WE790_P0_RSTOP		2	/* Receive Stop Page Reg (write) */
#define	WE790_P0_BOUND		3	/* Receive Boundary Page */
#define	WE790_P0_TSTAT		4	/* Transmit Status Reg (read) */
#define	WE790_P0_TSTARTH	4	/* Transmit Start Page, high (write) */
#define	WE790_P0_COLCNT		5	/* Collision Count (read) */
#define	WE790_P0_TCNTL		5	/* Transmit Frame Length, low (write) */
#define	WE790_P0_TCNTH		6	/* Transmit Frame Length, high (write) */
#define	WE790_P0_INTSTAT	7	/* Interrupt Status Reg */
#define	WE790_P0_ERWCNT		8	/* Early Receive Warning Threshold */
#define	WE790_P0_RENH		9	/* Receive Enhancement Reg */
#define	WE790_P0_RCNTL		10	/* Receive Byte Count, low (read) */
#define	WE790_P0_RCNTH		11	/* Receive Byte Count, high (read) */
#define	WE790_P0_RSTAT		12	/* Receive Packet Status Reg (read) */
#define	WE790_P0_RCON		12	/* Receive Configuration Reg (write) */
#define	WE790_P0_ALICNT		13	/* Alignment Error Counter (read) */
#define	WE790_P0_TCON		13	/* Transmit Configuration Reg (write) */
#define	WE790_P0_CRCCNT		14	/* CRC Error (read) */
#define	WE790_P0_DCON		14	/* Data Configuration (write) */
#define	WE790_P0_MPCNT		15	/* Missed Packet Error Counter Reg (read) */
#define	WE790_P0_INTMASK	15	/* Interrupt Mask Reg (write) */

/*
 * Page 1 register offsets
 */
#define	WE790_P1_CMD		0	/* Command Register */
#define	WE790_P1_STA0		1	/* Station Address Register 0 */
#define	WE790_P1_STA1		2	/* Station Address Register 1 */
#define	WE790_P1_STA2		3	/* Station Address Register 2 */
#define	WE790_P1_STA3		4	/* Station Address Register 3 */
#define	WE790_P1_STA4		5	/* Station Address Register 4 */
#define	WE790_P1_STA5		6	/* Station Address Register 5 */
#define	WE790_P1_CURR		7	/* Current Frame Buffer Pointer */
#define	WE790_P1_GROUP0		8	/* Multicast Filter Table 0 */
#define	WE790_P1_GROUP1		9	/* Multicast Filter Table 1 */
#define	WE790_P1_GROUP2		10	/* Multicast Filter Table 2 */
#define	WE790_P1_GROUP3		11	/* Multicast Filter Table 3 */
#define	WE790_P1_GROUP4		12	/* Multicast Filter Table 4 */
#define	WE790_P1_GROUP5		13	/* Multicast Filter Table 5 */
#define	WE790_P1_GROUP6		14	/* Multicast Filter Table 6 */
#define	WE790_P1_GROUP7		15	/* Multicast Filter Table 7 */

/*
 * Page 2 register offsets
 */
#define	WE790_P2_CMD		0	/* Command Register */
#define	WE790_P2_RSTART		1	/* Receive Start Page Reg (read) */
#define	WE790_P2_INCRL		1	/* (write) */
#define	WE790_P2_RSTOP		2	/* Receive Stop Page Reg (read) */
#define	WE790_P2_INCRH		2	/* (write) */
#define	WE790_P2_TCNTL		3	/* Transmit Frame Length, low (read) */
#define	WE790_P2_TSTARTH	4	/* Transmit Start Page, high (read) */
#define	WE790_P2_NEXT		5	/* DMA Controller Next Buffer Reg */
#define	WE790_P2_TCNTH		6	/* Transmit Frame Length, high (read) */
#define	WE790_P2_ENH		7	/* Enhancement Register */
#define	WE790_P2_RADDL		8	/* Receive Burst Starting Addr, low */
#define	WE790_P2_RADDH		9	/* Receive Burst Starting Addr, high */
#define	WE790_P2_TADDL		10	/* Transmit Burst Starting Addr, low */
#define	WE790_P2_TADDH		11	/* Transmit Burst Starting Addr, high */
#define	WE790_P2_RCON		12	/* Receive Configuration Reg (read) */
#define	WE790_P2_TCON		13	/* Transmit Configuration Reg (read) */
#define	WE790_P2_DCON		14	/* Data Configuration (read) */
#define	WE790_P2_INTMASK	15	/* Interrupt Mask Reg (read) */

/*
 * Page 3 register offsets
 */
#define	WE790_P3_CMD		0	/* Command Register */
#define	WE790_P3_TEST		1	/* */
#define	WE790_P3_RTEST		2	/* */
#define	WE790_P3_TTEST		3	/* */
#define	WE790_P3_TEST2		4	/* */
#define	WE790_P3_TSTARTL	5	/* Transmit Start Page, low */
#define	WE790_P3_TLEVEL		14	/* Transmit FIFO Track Reg */
#define	WE790_P3_MANCH		15	/* Manchester Management Reg */

/*
 *	Linked-List mode layout
 */
/*
 * Page 0 register offsets
 */
#define	WE790_L0_CMD		WE790_P0_CMD
#define	WE790_L0_RBEGIN		1	/* Receive Buffer Starting Addr (write) */
#define	WE790_L0_REND		2	/* Receive Buffer End Reg (write) */
#define	WE790_L0_CURRL		3	/* Current Frame Buffer Desc Ptr, high */
#define	WE790_L0_TSTAT		WE790_P0_TSTAT
#define	WE790_L0_TEND		4	/* Transfer Buffer End Reg (write) */
#define	WE790_L0_COLCNT		WE790_P0_COLCNT
#define	WE790_L0_TBEGIN		5	/* Transmit Buffer Starting Addr (write) */
#define	WE790_L0_ERWCNT		6	/* Early Receive Warning Threshold */
#define	WE790_L0_INTSTAT	WE790_P0_INTSTAT
#define	WE790_L0_RTABL		8	/* Receive Buffer Table Pointer, low */
#define	WE790_L0_RTABH		9	/* Receive Buffer Table Pointer, high */
#define	WE790_L0_TTABL		10	/* Transmit Buffer Pointer, low */
#define	WE790_L0_TTABH		11	/* Transmit Buffer Pointer, high */
#define	WE790_L0_RSTAT		WE790_P0_RSTAT
#define	WE790_L0_RCON		WE790_P0_RCON
#define	WE790_L0_ALICNT		WE790_P0_ALICNT
#define	WE790_L0_TCON		WE790_P0_TCON
#define	WE790_L0_CRCCNT		WE790_P0_CRCCNT
#define	WE790_L0_MPCNT		WE790_P0_MPCNT
#define	WE790_L0_INTMASK	WE790_P0_INTMASK

/*
 * Page 1 register offsets
 */
#define	WE790_L1_CMD		WE790_P1_CMD
#define	WE790_L1_STA0		WE790_P1_STA0
#define	WE790_L1_STA1		WE790_P1_STA1
#define	WE790_L1_STA2		WE790_P1_STA2
#define	WE790_L1_STA3		WE790_P1_STA3
#define	WE790_L1_STA4		WE790_P1_STA4
#define	WE790_L1_STA5		WE790_P1_STA5
#define	WE790_L1_CURRH		7	/* Current Frame Buffer Desc Ptr, low */
#define	WE790_L1_GROUP0		WE790_P1_GROUP0
#define	WE790_L1_GROUP1		WE790_P1_GROUP1
#define	WE790_L1_GROUP2		WE790_P1_GROUP2
#define	WE790_L1_GROUP3		WE790_P1_GROUP3
#define	WE790_L1_GROUP4		WE790_P1_GROUP4
#define	WE790_L1_GROUP5		WE790_P1_GROUP5
#define	WE790_L1_GROUP6		WE790_P1_GROUP6
#define	WE790_L1_GROUP7		WE790_P1_GROUP7

/*
 * Page 2 register offsets
 */
#define	WE790_L2_CMD		WE790_P2_CMD
#define	WE790_L2_RBEGIN		1	/* Receive Buffer Starting Addr Reg (read) */
#define	WE790_L2_REND		2	/* Receive Buffer End Reg (read) */
#define	WE790_L2_TBEGIN		3	/* Transmit Buffer Starting Addr Reg (read) */
#define	WE790_L2_TEND		4	/* Transfer Buffer End Reg (read) */
#define	WE790_L2_ENH		WE790_P2_ENH
#define	WE790_L2_RDOWNL		8	/* Buffer Room Remaining, low */
#define	WE790_L2_RDOWNH		9	/* Buffer Room Remaining, high */
#define	WE790_L2_TDOWNL		10	/* Transfer Count, low */
#define	WE790_L2_TDOWNH		11	/* Transfer Count, high */
#define	WE790_L2_RCON		WE790_P2_RCON
#define	WE790_L2_TCON		WE790_P2_TCON
#define	WE790_L2_DCON		WE790_P2_DCON
#define	WE790_L2_INTMASK	WE790_P2_INTMASK

/*
 * Page 3 register offsets
 */
#define	WE790_L3_CMD		WE790_P3_CMD
#define	WE790_L3_TSTARTL	WE790_P3_TSTARTL
#define	WE790_L3_TLEVEL		WE790_P3_TLEVEL
#define	WE790_L3_MANCH		WE790_P3_MANCH


/*
 *		Command Register (CR) definitions
 */
#define	WE790_CR_STP		0x01	/* Stop Bit */
#define	WE790_CR_STA		0x02	/* Start Bit */
#define	WE790_CR_TXP		0x04	/* Transmit packet */
#define	WE790_CR_DISETCH	0x08	/* Disable Early Transmit Checking */
#define	WE790_CR_ENETCH		0x10	/* Enable Early Transmit Checking */
#define	WE790_CR_RFU		0x20	/* Reserved for Future Use */
#define	WE790_CR_PS0		0x40	/* Page Select */
#define	WE790_CR_PS1		0x80	/* Page Select */

/* bit encoded aliases */
/* XXX: dubious, 83C795's datasheet says as follows... */
#define	WE790_CR_PAGE_0		0x00	/* (for consistency) */
#define	WE790_CR_PAGE_1		0x80
#define	WE790_CR_PAGE_2		0x40
#define	WE790_CR_PAGE_3		0xc0

/*
 *		Enhancement Register (EHN) definitions
 */
#define	WE790_EHN_SBACK		0x01	/* Enable Stop Backup Modifications */
#define	WE790_EHN_EOTINT	0x04	/* Interrupt on End-of-Transmit */
#define	WE790_EHN_SLOT0		0x08	/* Slot Time Selection */
#define	WE790_EHN_SLOT1		0x10	/* Slot Time Selection */
#define	WE790_EHN_ALTEGO	0x20	/* Buffering Format Selection */

/*
 *		Interrupt Mask Register (INTMASK) definitions
 */
#define	WE790_INTMASK_PRXE	0x01	/* Packet Received Enable */
#define	WE790_INTMASK_PTXE	0x02	/* Packet Transmitted Enable */
#define	WE790_INTMASK_RXEE	0x04	/* Receive Error Enable */
#define	WE790_INTMASK_TXEE	0x08	/* Transmit Error Enable */
#define	WE790_INTMASK_OVWE	0x10	/* Overwrite Warning Enable */
#define	WE790_INTMASK_CNTE	0x20	/* Counter Overflow Enable */
#define	WE790_INTMASK_ERWE	0x40	/* Early Receive Warning Enable */

/*
 *		Interrupt Status Register (INTSTAT) definitions
 */
#define	WE790_INTSTAT_PRX	0x01	/* Packet Received */
#define	WE790_INTSTAT_PTX	0x02	/* Packet Transmitted */
#define	WE790_INTSTAT_RXE	0x04	/* Receive Error */
#define	WE790_INTSTAT_TXE	0x08	/* Transmit Error */
#define	WE790_INTSTAT_OVW	0x10	/* Overwrite Warning */
#define	WE790_INTSTAT_CNT	0x20	/* Counter Overflow */
#define	WE790_INTSTAT_ERW	0x40	/* Early Receive Warning */
#define	WE790_INTSTAT_RST	0x80	/* Reset Status */

/*
 *		Manchester Management Register (MANCH) definitions
 */
#define	WE790_MANCH_TLED	0x01	/* Transmit LED Readback */
#define	WE790_MANCH_RLED	0x02	/* Receive LED Readback */
#define	WE790_MANCH_LLED	0x04	/* Link Status LED Readback */
#define	WE790_MANCH_PLED	0x08	/* TPRX Polarity LED Readback */
#define	WE790_MANCH_ENAPOL	0x10	/* Automatic Polarity Correct */
#define	WE790_MANCH_SEL		0x40	/* Select AUI Mode For Idle State */
#define	WE790_MANCH_MANDIS	0x80	/* Manchester Disable */

/*
 *		Receiver Configuration Register (RCON) definitions
 */
#define	WE790_RCON_SEP		0x01	/* Save Errored Packets */
#define	WE790_RCON_RUNTS	0x02	/* Receive Runts Frames */
#define	WE790_RCON_BROAD	0x04	/* Receive Broadcast Frames */
#define	WE790_RCON_GROUP	0x08	/* Receive Multicast Frames */
#define	WE790_RCON_PROM		0x10	/* Promiscuous Reception */
#define	WE790_RCON_MON		0x20	/* Check Addresses/CRC without Buffering */
#define	WE790_RCON_RCA		0x40	/* Receive Abort Frame on Collision */

/*
 *		Receiver Status Register (RSTAT) definitions
 */
#define	WE790_RSTAT_PRX		0x01	/* Packet Received Intact */
#define	WE790_RSTAT_CRC		0x02	/* CRC Error */
#define	WE790_RSTAT_FAE		0x04	/* Frame Alignment Error */
#define	WE790_RSTAT_OVER	0x08	/* FIFO Overrun */
#define	WE790_RSTAT_MPA		0x10	/* Missed Packet */
#define	WE790_RSTAT_GROUP	0x20	/* Group Address Recognized */
#define	WE790_RSTAT_DIS		0x40	/* Receiver Disabled */
#define	WE790_RSTAT_DRF		0x80	/* Deferring IGSM */

/*
 *		Transmit Configuration Register (TCON) definitions
 */
#define	WE790_TCON_CRCN		0x01	/* CRC Generation Inhibition */
#define	WE790_TCON_LB0		0x02	/* Loopback Test Selection */
#define	WE790_TCON_LB1		0x04	/* Loopback Test Selection */

/*
 *		Transmit Status Register (TSTAT) definitions
 */
#define	WE790_TSTAT_PTX		0x01	/* Packet Transmitted */
#define	WE790_TSTAT_NDT		0x02	/* Non-deferred Transmission */
#define	WE790_TSTAT_TWC		0x04	/* Transmitted With Collisions */
#define	WE790_TSTAT_ABORT	0x08	/* Abort Transmission */
#define	WE790_TSTAT_CRL		0x10	/* Carrier Sense Lost */
#define	WE790_TSTAT_UNDER	0x20	/* FIFO or Buffer Underrun */
#define	WE790_TSTAT_CDH		0x40	/* Collision Detect Heartbeat */
#define	WE790_TSTAT_OWC		0x80	/* Out of Window Collision */

/*
 *		Receive Enhancement Register (RENH) definitions
 */
#define	WE790_RENH_WRAPEN	0x01	/* Automatic Ring-Wrap Enable */
#define	WE790_RENH_ERFBIT	0x02	/* Early Receive Fail Bit */
#define	WE790_RENH_REMPTY	0x04	/* Ring Bit Empty */


/*
 * SMC 83C795 Host Interface Internal Registers (ASIC)
 */

/*
 * Control Register (CR)
 */
#define	WE790_CR	0

#define	WE790_CR_RP		0x0f	/* bit 15-13 of RAM Offset */
#define	WE790_CR_CR3		0x08	/* RESERVED */
#define	WE790_CR_CR4		0x10	/* RESERVED */
#define	WE790_CR_MENB		0x40	/* Memory Enable */
#define	WE790_CR_RNIC		0x80	/* Reset Network Interface Controller */

/*
 * EEROM Register (EER)
 */
#define	WE790_EER	1

#define	WE790_EER_JMP0		0x01	/* (R) Initialization Jumpers */
#define	WE790_EER_JMP1		0x02	/* (R) Initialization Jumpers */
#define	WE790_EER_EA1		0x02	/* (W) EEPROM Address Field */
#define	WE790_EER_JMP2		0x04	/* (R) Initialization Jumpers */
#define	WE790_EER_EA2		0x04	/* (W) EEPROM Address Field */
#define	WE790_EER_JMP3		0x08	/* (R) Initialization Jumpers */
#define	WE790_EER_EA3		0x08	/* (W) EEPROM Address Field */
#define	WE790_EER_UNLOCK	0x10	/* (RW) Unlock Store */
#define	WE790_EER_EA4		0x20	/* (RW) EEPROM Address Field */
#define	WE790_EER_RC		0x40	/* (RW) Recall EEPROM */
#define	WE790_EER_STO		0x80	/* (RW) Store Into Non-Volatile EEPROM */

/*
 * I/O Pipe Data Location Low (IOPL)
 *	bit 7-0 of I/O Pipe Data Location
 */
#define	WE790_IOPL	2

/*
 * I/O Pipe Data Location High (IOPH)
 *	bit 15-8 of I/O Pipe Data Location
 */
#define	WE790_IOPH	3

/*
 * Hardware Support Register (HWR)
 */
#if 0	/* conflict with if_wereg.h */
#define	WE790_HWR	4
#endif

#define	WE790_HWR_GPOE		0x01	/* (RW) GPX Pin Output Enable */
#define	WE790_HWR_PNPJMP	0x02	/* (R) Plug and Play Jumper Installed */
#define	WE790_HWR_ISTAT		0x04	/* (R) Interrupt Status */
#define	WE790_HWR_NUKE		0x08	/* (W) Restart */
#define	WE790_HWR_HOST16	0x10	/* (R) 0: 8-bit host, 1: 16-bit host */
#define	WE790_HWR_ETHR		0x20	/* (R) MAC Protocol Type */
#if 0	/* conflict with if_wereg.h */
#define	WE790_HWR_SWH		0x80	/* (RW) Switch Register Set */
#endif

/*
 * BIOS Page Register (BPR)
 */
#define	WE790_BPR	5

#define	WE790_BPR_SOFT0		0x01
#define	WE790_BPR_SOFT1		0x02
#define	WE790_BPR_BP		0x70	/* bit 15-13 of ROM Offset */
#define	WE790_BPR_M16EN		0x80	/* Memory 16-bit Enable */

/*
 * Interrupt Control Register (ICR)
 */
#if 0	/* conflict with if_wereg.h */
#define	WE790_ICR	6

#define	WE790_ICR_EIL		0x01	/* Enable Interrupts */
#endif
#define	WE790_ICR_MASK1		0x02	/* NOT USED */
#define	WE790_ICR_MASK2		0x04	/* Mask Interrupt Sources */
#define	WE790_ICR_SINT		0x08	/* Software Interrupt */
#define	WE790_ICR_IOPEN		0x10	/* I/O Pipe Enable */
#define	WE790_ICR_IOPAV		0x20	/* I/O Pipe Address Visible */
#define	WE790_ICR_STAG		0x40	/* Staggerd Address Enable */
#define	WE790_ICR_MCTEST	0x80	/* Memory Cache Test Bit */

/*
 * Revision Register (REV)	(Read)
 */
#if 0	/* conflict with if_wereg.h */
#define	WE790_REV	7
#endif

#define	WE790_REV_REV		0x0f	/* (R) Revision Number */
#define	WE790_REV_CHIP		0xf0	/* (R) Chip Type */

/*
 * I/O Pipe Address Register (IOPA)	(Write)
 *	bit 7-0 of I/O Pipe Address
 */
#define	WE790_IOPA	7

/*
 * LAN Address Registers (LAN5-LAN0)
 *	Enabled with SWH bit=0 in HWR register
 */
#define	WE790_LAN0	8
#define	WE790_LAN1	9
#define	WE790_LAN2	0x0a
#define	WE790_LAN3	0x0b
#define	WE790_LAN4	0x0c
#define	WE790_LAN5	0x0d

/*
 * Board ID Register (BDID)	(Read)
 *	Enabled with SWH bit=0 in HWR register
 */
#define	WE790_BDID	0x0e

/*
 * Checksum Register (CKSUM)
 *	Enabled with SWH bit=0 in HWR register
 */
#define	WE790_CKSM	0x0f

/*
 * General Control Register 2 (GCR2)
 *	Enabled with SWH bit=1 in HWR register
 */
#define	WE790_GCR2	8

#define	WE790_GCR2_PNPIOP	0x01	/* PNP and I/O Mapped Pipe */

/*
 * I/O Address Register (IAR)
 *	Enabled with SWH bit=1 in HWR register
 */
#define	WE790_IAR	0x0a

#define	WE790_IAR_PNPBOOT	0x01	/* Plug and Play Boot Bit */
#define	WE790_IAR_IAL		0xfe	/* bit 15-13, 8-5 of I/O Address Lines */

/*
 * RAM Address Register (RAR)
 *	Enabled with SWH bit=1 in HWR register
 */
#if 0	/* conflict with if_wereg.h */
#define	WE790_RAR	0x0b
#endif

#define WE790_RAR_RAF		0x4f	/* bit 17-13 of RAM Base Address Field */
#if 0	/* conflict with if_wereg.h */
#define WE790_RAR_SZ8		0x00	/* 8k memory buffer */
#define WE790_RAR_SZ16		0x10	/* 16k memory buffer */
#define WE790_RAR_SZ32		0x20	/* 32k memory buffer */
#define WE790_RAR_SZ64		0x30	/* 64k memory buffer */
#endif
#define	WE790_RAR_HRAM		0x80	/* High RAM Address */

/*
 * ROM Control Register (BIO)
 *	Enabled with SWH bit=1 in HWR register
 */
#define	WE790_BIO	0x0c

#define	WE790_BIO_BAF		0x4f	/* bit 17-13 of Base Address Field */
#define WE790_BIO_SZ8		0x00	/* 8k ROM Window */
#define WE790_BIO_SZ16		0x10	/* 16k ROM Window */
#define WE790_BIO_SZ32		0x20	/* 32k ROM Window */
#define WE790_BIO_DIS		0x30	/* ROM Window Disabled */
#define	WE790_BIO_FINE		0x80	/* Fine Decode */

/*
 * General Control Register (GCR)
 *	Enabled with SWH bit=1 in HWR register
 */
#if 0	/* conflict with if_wereg.h */
#define	WE790_GCR	0x0d

#define	WE790_GCR_LIT		0x01	/* Link Integrity Test */
#define	WE790_GCR_GPOUT		0x02	/* General Purpose Output */
#define	WE790_GCR_IR0		0x04	/* Interrupt Request Field */
#define	WE790_GCR_IR1		0x08	/* Interrupt Request Field */
#endif
#define	WE790_GCR_RIPL		0x10	/* Software Flag */
#if 0	/* conflict with if_wereg.h */
#define	WE790_GCR_ZWSEN		0x20	/* Zero Wait State Enable */
#define	WE790_GCR_IR2		0x40	/* Interrupt Request Field */
#endif
#define	WE790_GCR_XLENGTH	0x80	/* Extended Length Bit Enable */

/*
 * Early Receive Fail Address Low Register (ERFAL)
 *	Enabled with SWH bit=1 in HWR register
 */
#define	WE790_ERFAL	0x0e

#define	WE790_ERFAL_PNPEN	0x01	/* Plug and Play Enable */
#define	WE790_ERFAL_ERFAL	0xfc	/* bit 7-2 of Early Receive Failure Address */

/*
 * Early Receive Fail Address High Register (ERFAH)
 *	Enabled with SWH bit=1 in HWR register
 */
#define	WE790_ERFAH	0x0f
#define	WE790_ERFAH_ERFAH	0xff	/* bit 15-8 of Early Receive Failure Address */
