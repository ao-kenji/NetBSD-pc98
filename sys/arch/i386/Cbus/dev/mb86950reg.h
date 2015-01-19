/*	$NecBSD: mb86950reg.h,v 1.1 1998/06/08 02:09:33 kmatsuda Exp $	*/
/*	$NetBSD: if_qnreg.h,v 1.2 1995/11/30 00:57:04 jtc Exp $	*/

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
 * Copyright (c) 1995 Mika Kortelainen
 * All rights reserved.
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
 *      This product includes software developed by  Mika Kortelainen
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Thanks for Aspecs Oy (Finland) for the data book for the NIC used
 * in this card and also many thanks for the Resource Management Force
 * (QuickNet card manufacturer) and especially Daniel Koch for providing
 * me with the necessary 'inside' information to write the driver.
 *
 */

/*
 * The QuickNet card uses the Fujitsu's MB86950B NIC (Network Interface
 * Controller) chip, located at card base address + 0xff00. NIC registers
 * are accessible only at even byte addresses, so the register offsets must
 * be multiplied by two. Actually, these registers are read/written as words.
 *
 * As the card doesn't use DMA, data is input/output at FIFO register
 * (base address + 0xff20).  The card has 64K buffer memory and is pretty
 * fast despite the lack of DMA.
 *
 * The FIFO register MUST be accessed as a word (16 bits).
 */

/* Data Link Control Registrs. */
#define FES_DLCR0	0	/* Transmit status	*/
#define FES_DLCR1	1	/* Transmit masks	*/
#define FES_DLCR2	2	/* Receive status	*/
#define FES_DLCR3	3	/* Receive masks	*/
#define FES_DLCR4	4	/* Transmit mode	*/
#define FES_DLCR5	5	/* Receive mode		*/
#define FES_DLCR6	6	/* Software reset	*/
#define FES_DLCR7	7	/* TDR (LSB)		*/
#define FES_DLCR8	8	/* Node ID0		*/
#define FES_DLCR9	9	/* Node ID1		*/
#define FES_DLCR10	10	/* Node ID2		*/
#define FES_DLCR11	11	/* Node ID3		*/
#define FES_DLCR12	12	/* Node ID4		*/
#define FES_DLCR13	13	/* Node ID5		*/
#define FES_DLCR14	14	/* Reserved		*/
#define FES_DLCR15	15	/* TDR (MSB)		*/

/* Buffer Memory Port Registers. (word access required) */
#define FES_BMPR0	16	/* Buffer memory port (FIFO, LSB)	*/
#define FES_BMPR1	17	/* Buffer memory port (FIFO, MSB)	*/
#define FES_BMPR2	18	/* Packet length (LSB)			*/
#define FES_BMPR3	19	/* Packet length (MSB)			*/
#define FES_BMPR4	20	/* DMA enable				*/

/*
 * Definitions of registers.
 */

/* DLCR0 -- transmitter status */
#define FES_D0_BUSERR	0x01	/* Bus write error			*/
#define FES_D0_COLL16	0x02	/* Collision limit (16) encountered	*/
#define FES_D0_COLLID	0x04	/* Collision on last transmission	*/
#define FES_D0_UDRFLO	0x08	/* Transmitter buffer underflow		*/
#define FES_D0_CRLOST	0x10	/* Carrier lost on last transmission	*/
#define FES_D0_PKTRCD	0x20	/* No corrision on last transmission	*/
#define FES_D0_NETBSY	0x40	/* Network Busy (Carrier Detected)	*/
#define FES_D0_TXDONE	0x80	/* Transmission complete		*/

/* DLCR1 -- transmitter interrupt control; same layout as DLCR0 */
#define FES_D1_BUSERR	FES_D0_BUSERR
#define FES_D1_COLL16	FES_D0_COLL16
#define FES_D1_COLLID	FES_D0_COLLID
#define FES_D1_UDRFLO	FES_D0_UDRFLO
#define FES_D1_TXDONE	FES_D0_TXDONE

/* DLCR2 -- receiver status */
#define FES_D2_OVRFLO	0x01	/* Receiver buffer overflow		*/
#define FES_D2_CRCERR	0x02	/* CRC error on last packet		*/
#define FES_D2_ALGERR	0x04	/* Alignment error on last packet	*/
#define FES_D2_SRTPKT	0x08	/* Short (RUNT) packet is received	*/
#define FES_D2_RMTRST	0x10	/* Remote reset packet (type = 0x0900)	*/
#define FES_D2_BUSERR	0x40	/* Bus read error			*/
#define FES_D2_PKTRDY	0x80	/* Packet(s) ready on receive buffer	*/

#define	FES_D2_ERRBITS	"\20\4SRTPKT\3ALGERR\2CRCERR\1OVRFLO"

/* DLCR3 -- receiver interrupt control; same layout as DLCR2 */
#define FES_D3_OVRFLO	FES_D2_OVRFLO
#define FES_D3_CRCERR	FES_D2_CRCERR
#define FES_D3_ALGERR	FES_D2_ALGERR
#define FES_D3_SRTPKT	FES_D2_SRTPKT
#define FES_D3_RMTRST	FES_D2_RMTRST
#define FES_D3_BUSERR	FES_D2_BUSERR
#define FES_D3_PKTRDY	FES_D2_PKTRDY

/* DLCR4 -- transmitter operation mode */
#define FES_D4_DSC	0x01	/* Disable carrier sense on trans.	*/
#define FES_D4_LBC	0x02	/* Loop back test control		*/

#define FES_D4_LBC_ENABLE	0x00	/* Perform loop back test	*/
#define FES_D4_LBC_DISABLE	0x02	/* Normal operation		*/

/* DLCR5 -- receiver operation mode */
/*
 * Normal mode: accept physical address, multicast group addresses
 * which match the 1st three bytes and broadcast address.
 */
#define FES_D5_AFM0	0x01	/* Receive packets for other stations	*/
#define FES_D5_AFM1	0x02	/* Receive packets for this station	*/
#define FES_D5_RMTRST	0x04	/* Enable remote reset operation	*/
#define FES_D5_SRTPKT	0x08	/* Accept short (RUNT) packets		*/
#define FES_D5_SRTADR	0x10	/* Short (16 bits?) MAC address		*/
#define FES_D5_BUFEMP	0x40	/* Receive buffer is empty		*/
#define FES_D5_TEST	0x80	/* Test output				*/

/* DLCR6 -- Enable Data Link Controller */
#define FES_D6_DLC	0x80	/* Disable DLC (recever/transmitter)	*/

#define FES_D6_DLC_ENABLE	0x00	/* Normal operation		*/
#define FES_D6_DLC_DISABLE	0x80	/* Stop sending/receiving	*/

/* DLCR7 and DLCR15 are for TDR (Time Domain Reflectometry). */

/* DLCR8 thru DLCR13 are for Ethernet station address.  */

/* DLCR14 is reserved. */

/* BMPR0 and BMPR1 are for packet data.  */

/* BMPR3:BMPR2 -- Packet Length Registers (Write-only) */
#define FES_B2_START	0x80	/* Start transmitter			*/
#define FES_B2_COUNT	0x7F	/* Packet count				*/

/* BMPR4 -- DMA enable */
#define FES_B4_TXDMA	0x01	/* Enable transmitter DMA		*/
#define FES_B4_RXDMA	0x02	/* Enable receiver DMA			*/
