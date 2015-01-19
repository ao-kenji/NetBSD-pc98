/*	$NecBSD: ym2203reg.h,v 1.12 1998/03/14 07:12:52 kmatsuda Exp $	*/
/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
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
 * ym2203reg.h
 *
 * Register defs for YAMAHA FM sound chip OPN (YM2203).
 * Written by NAGAO Tadaaki, Feb 10, 1996.
 */

#ifndef	_YM2203REG_H_
#define	_YM2203REG_H_

/*
 * Offsets from I/O base address
 */
#define OPN_ADDR	0	/* writeonly */
#define OPN_DATA	2
#define OPN_STAT	0	/* readonly */


/*
 * Status bits
 */
#define OPN_FLAGA	0x01	/* Flag-A (Timer-A) */
#define OPN_FLAGB	0x02	/* Flag-B (Timer-B) */
#define OPN_BUSY	0x80	/* OPN Busy */


/*
 * Registers for chip control (SSG part)
 */
#define SSG_CHA_FT	0x00	/* Channel-A Tone Period (Fine Tune) */
#define SSG_CHA_CT	0x01	/* Channel-A Tone Period (Coarse Tune) */
#define SSG_CHB_FT	0x02	/* Channel-B Tone Period (Fine Tune) */
#define SSG_CHB_CT	0x03	/* Channel-B Tone Period (Coarse Tune) */
#define SSG_CHC_FT	0x04	/* Channel-C Tone Period (Fine Tune) */
#define SSG_CHC_CT	0x05	/* Channel-C Tone Period (Coarse Tune) */
#define SSG_NOISE	0x06	/* Noise Period (Period Control) */
#define SSG_ENABLE	0x07	/* /ENABLE (SSG Mixing & I/O control) */
#define SSG_CHA_AMP	0x08	/* Channel-A Amplitude */
#define SSG_CHB_AMP	0x09	/* Channel-B Amplitude */
#define SSG_CHC_AMP	0x0a	/* Channel-C Amplitude */
#define SSG_ENV_FT	0x0b	/* Envelope Period (Fine Tune) */
#define SSG_ENV_CT	0x0c	/* Envelope Period (Coarse Tune) */
#define SSG_ENV_SHAPE	0x0d	/* Envelope Shape, Cycle */
#define SSG_IOA		0x0e	/* I/O Port A */
#define SSG_IOB		0x0f	/* I/O Port B */

/* ch = 0, 1, 2 (corresponding to channel-A, B, C, respectively) */
#define SSG_FT(ch)	(SSG_CHA_FT + (ch) * 2)
#define SSG_CT(ch)	(SSG_CHA_CT + (ch) * 2)
#define SSG_AMP(ch)	(SSG_CHA_AMP + (ch))


/*
 * Control bits for SSG_ENABLE register
 */
#define SSG_EN_TONE_A	0x01
#define SSG_EN_TONE_B	0x02
#define SSG_EN_TONE_C	0x04
#define SSG_EN_NOISE_A	0x08
#define SSG_EN_NOISE_B	0x10
#define SSG_EN_NOISE_C	0x20
#define SSG_EN_IOA_OUT	0x40
#define SSG_EN_IOB_OUT	0x80

/* ch = 0, 1, 2 (corresponding to channel-A, B, C, respectively) */
#define SSG_EN_TONE(ch)		(SSG_EN_TONE_A << (ch))
#define SSG_EN_NOISE(ch)	(SSG_EN_NOISE_A << (ch))

#define SSG_MASK_EN_TONE	(SSG_EN_TONE_A | \
				 SSG_EN_TONE_B | \
				 SSG_EN_TONE_C)
#define SSG_MASK_EN_NOISE	(SSG_EN_NOISE_A | \
				 SSG_EN_NOISE_B | \
				 SSG_EN_NOISE_C)
#define SSG_MASK_EN_IO		(SSG_EN_IOA_OUT | SSG_EN_IOB_OUT)


/*
 * Registers for chip control (FM sound part)
 */
#define FM_LSI_TEST	0x21	/* LSI Test */
#define FM_TIMER_A_H	0x24	/* Timer-A higher 8 bits */
#define FM_TIMER_A_L	0x25	/* Timer-A lower 2 bits */
#define FM_TIMER_B	0x26	/* Timer-B */
#define FM_MODE_TIMER	0x27	/* Mode of Channel-3 / Timer-A/B Control */
#define FM_KEY		0x28	/* Key-on/off */
#define FM_SCH_IRQEN	0x29	/* SCH / IRQ Enable */
#define FM_PRESCALER0	0x2d	/* Prescaler */
#define FM_PRESCALER1	0x2e	/* Prescaler */
#define FM_PRESCALER2	0x2f	/* Prescaler */
/* Detune / Multiple */
#define FM_CH1OP1_DT_ML	0x30	/* Ch-1 Op-1 */
#define FM_CH1OP2_DT_ML	0x38	/* Ch-1 Op-2 */
#define FM_CH1OP3_DT_ML	0x34	/* Ch-1 Op-3 */
#define FM_CH1OP4_DT_ML	0x3c	/* Ch-1 Op-4 */
#define FM_CH2OP1_DT_ML	0x31	/* Ch-2 Op-1 */
#define FM_CH2OP2_DT_ML	0x39	/* Ch-2 Op-2 */
#define FM_CH2OP3_DT_ML	0x35	/* Ch-2 Op-3 */
#define FM_CH2OP4_DT_ML	0x3d	/* Ch-2 Op-4 */
#define FM_CH3OP1_DT_ML	0x32	/* Ch-3 Op-1 */
#define FM_CH3OP2_DT_ML	0x3a	/* Ch-3 Op-2 */
#define FM_CH3OP3_DT_ML	0x36	/* Ch-3 Op-3 */
#define FM_CH3OP4_DT_ML	0x3e	/* Ch-3 Op-4 */
/* Total Level */
#define FM_CH1OP1_TL	0x40	/* Ch-1 Op-1 */
#define FM_CH1OP2_TL	0x48	/* Ch-1 Op-2 */
#define FM_CH1OP3_TL	0x44	/* Ch-1 Op-3 */
#define FM_CH1OP4_TL	0x4c	/* Ch-1 Op-4 */
#define FM_CH2OP1_TL	0x41	/* Ch-2 Op-1 */
#define FM_CH2OP2_TL	0x49	/* Ch-2 Op-2 */
#define FM_CH2OP3_TL	0x45	/* Ch-2 Op-3 */
#define FM_CH2OP4_TL	0x4d	/* Ch-2 Op-4 */
#define FM_CH3OP1_TL	0x42	/* Ch-3 Op-1 */
#define FM_CH3OP2_TL	0x4a	/* Ch-3 Op-2 */
#define FM_CH3OP3_TL	0x46	/* Ch-3 Op-3 */
#define FM_CH3OP4_TL	0x4e	/* Ch-3 Op-4 */
/* Key-Scaling / Attack Rate */
#define FM_CH1OP1_KS_AR	0x50	/* Ch-1 Op-1 */
#define FM_CH1OP2_KS_AR	0x58	/* Ch-1 Op-2 */
#define FM_CH1OP3_KS_AR	0x54	/* Ch-1 Op-3 */
#define FM_CH1OP4_KS_AR	0x5c	/* Ch-1 Op-4 */
#define FM_CH2OP1_KS_AR	0x51	/* Ch-2 Op-1 */
#define FM_CH2OP2_KS_AR	0x59	/* Ch-2 Op-2 */
#define FM_CH2OP3_KS_AR	0x55	/* Ch-2 Op-3 */
#define FM_CH2OP4_KS_AR	0x5d	/* Ch-2 Op-4 */
#define FM_CH3OP1_KS_AR	0x52	/* Ch-3 Op-1 */
#define FM_CH3OP2_KS_AR	0x5a	/* Ch-3 Op-2 */
#define FM_CH3OP3_KS_AR	0x56	/* Ch-3 Op-3 */
#define FM_CH3OP4_KS_AR	0x5e	/* Ch-3 Op-4 */
/* Decay Rate */
#define FM_CH1OP1_DR	0x60	/* Ch-1 Op-1 */
#define FM_CH1OP2_DR	0x68	/* Ch-1 Op-2 */
#define FM_CH1OP3_DR	0x64	/* Ch-1 Op-3 */
#define FM_CH1OP4_DR	0x6c	/* Ch-1 Op-4 */
#define FM_CH2OP1_DR	0x61	/* Ch-2 Op-1 */
#define FM_CH2OP2_DR	0x69	/* Ch-2 Op-2 */
#define FM_CH2OP3_DR	0x65	/* Ch-2 Op-3 */
#define FM_CH2OP4_DR	0x6d	/* Ch-2 Op-4 */
#define FM_CH3OP1_DR	0x62	/* Ch-3 Op-1 */
#define FM_CH3OP2_DR	0x6a	/* Ch-3 Op-2 */
#define FM_CH3OP3_DR	0x66	/* Ch-3 Op-3 */
#define FM_CH3OP4_DR	0x6e	/* Ch-3 Op-4 */
/* Sustain Rate */
#define FM_CH1OP1_SR	0x70	/* Ch-1 Op-1 */
#define FM_CH1OP2_SR	0x78	/* Ch-1 Op-2 */
#define FM_CH1OP3_SR	0x74	/* Ch-1 Op-3 */
#define FM_CH1OP4_SR	0x7c	/* Ch-1 Op-4 */
#define FM_CH2OP1_SR	0x71	/* Ch-2 Op-1 */
#define FM_CH2OP2_SR	0x79	/* Ch-2 Op-2 */
#define FM_CH2OP3_SR	0x75	/* Ch-2 Op-3 */
#define FM_CH2OP4_SR	0x7d	/* Ch-2 Op-4 */
#define FM_CH3OP1_SR	0x72	/* Ch-3 Op-1 */
#define FM_CH3OP2_SR	0x7a	/* Ch-3 Op-2 */
#define FM_CH3OP3_SR	0x76	/* Ch-3 Op-3 */
#define FM_CH3OP4_SR	0x7e	/* Ch-3 Op-4 */
/* Sustain Level / Release Rate */
#define FM_CH1OP1_SL_RR	0x80	/* Ch-1 Op-1 */
#define FM_CH1OP2_SL_RR	0x88	/* Ch-1 Op-2 */
#define FM_CH1OP3_SL_RR	0x84	/* Ch-1 Op-3 */
#define FM_CH1OP4_SL_RR	0x8c	/* Ch-1 Op-4 */
#define FM_CH2OP1_SL_RR	0x81	/* Ch-2 Op-1 */
#define FM_CH2OP2_SL_RR	0x89	/* Ch-2 Op-2 */
#define FM_CH2OP3_SL_RR	0x85	/* Ch-2 Op-3 */
#define FM_CH2OP4_SL_RR	0x8d	/* Ch-2 Op-4 */
#define FM_CH3OP1_SL_RR	0x82	/* Ch-3 Op-1 */
#define FM_CH3OP2_SL_RR	0x8a	/* Ch-3 Op-2 */
#define FM_CH3OP3_SL_RR	0x86	/* Ch-3 Op-3 */
#define FM_CH3OP4_SL_RR	0x8e	/* Ch-3 Op-4 */
/* SSG-type Envelope */
#define FM_CH1OP1_SSGEG	0x90	/* Ch-1 Op-1 */
#define FM_CH1OP2_SSGEG	0x98	/* Ch-1 Op-2 */
#define FM_CH1OP3_SSGEG	0x94	/* Ch-1 Op-3 */
#define FM_CH1OP4_SSGEG	0x9c	/* Ch-1 Op-4 */
#define FM_CH2OP1_SSGEG	0x91	/* Ch-2 Op-1 */
#define FM_CH2OP2_SSGEG	0x99	/* Ch-2 Op-2 */
#define FM_CH2OP3_SSGEG	0x95	/* Ch-2 Op-3 */
#define FM_CH2OP4_SSGEG	0x9d	/* Ch-2 Op-4 */
#define FM_CH3OP1_SSGEG	0x92	/* Ch-3 Op-1 */
#define FM_CH3OP2_SSGEG	0x9a	/* Ch-3 Op-2 */
#define FM_CH3OP3_SSGEG	0x96	/* Ch-3 Op-3 */
#define FM_CH3OP4_SSGEG	0x9e	/* Ch-3 Op-4 */
/* F-Number1 */
#define FM_CH1_FNUM1	0xa0	/* Ch-1 */
#define FM_CH2_FNUM1	0xa1	/* Ch-2 */
#define FM_CH3_FNUM1	0xa2	/* Ch-3 */
/* Block / F-Number2 */
#define FM_CH1_BLK_FNUM2	0xa4	/* Ch-1 */
#define FM_CH2_BLK_FNUM2	0xa5	/* Ch-2 */
#define FM_CH3_BLK_FNUM2	0xa6	/* Ch-3 */
/* F-Number1 (when Channel-3 is in Effect-mode or CSM-mode) */
#define FM_CH3OP1_FNUM1	0xa9	/* Ch-3 Op-1 */
#define FM_CH3OP2_FNUM1	0xaa	/* Ch-3 Op-2 */
#define FM_CH3OP3_FNUM1	0xa8	/* Ch-3 Op-3 */
#define FM_CH3OP4_FNUM1	0xa2	/* Ch-3 Op-4 */
/* Block / F-Number2 (when Channel-3 is in Effect-mode or CSM-mode) */
#define FM_CH3OP1_BLK_FNUM2	0xad	/* Ch-3 Op-1 */
#define FM_CH3OP2_BLK_FNUM2	0xae	/* Ch-3 Op-2 */
#define FM_CH3OP3_BLK_FNUM2	0xac	/* Ch-3 Op-3 */
#define FM_CH3OP4_BLK_FNUM2	0xa6	/* Ch-3 Op-4 */
/* FeedBack / Algorithm */
#define FM_CH1_FB_AL	0xb0	/* Ch-1 */
#define FM_CH2_FB_AL	0xb1	/* Ch-2 */
#define FM_CH3_FB_AL	0xb2	/* Ch-3 */

/*
 * ch = 0, 1, 2 (corresponding to Channel-1, 2, 3, respectively)
 * op = 0, 1, 2, 3 (corresponding to Operator-1, 2, 3, 4, respectively)
 */
#define FM_OP2OFF(op)		(((op) & 1) * 8 + ((op) & 2) * 2)

#define FM_DT_ML(ch, op)	(0x30 + (ch) + FM_OP2OFF(op))
#define FM_TL(ch, op)		(0x40 + (ch) + FM_OP2OFF(op))
#define FM_KS_AR(ch, op)	(0x50 + (ch) + FM_OP2OFF(op))
#define FM_DR(ch, op)		(0x60 + (ch) + FM_OP2OFF(op))
#define FM_SR(ch, op)		(0x70 + (ch) + FM_OP2OFF(op))
#define FM_SL_RR(ch, op)	(0x80 + (ch) + FM_OP2OFF(op))
#define FM_SSGEG(ch, op)	(0x90 + (ch) + FM_OP2OFF(op))
#define FM_FNUM1(ch)		(0xa0 + (ch))
#define FM_BLK_FNUM2(ch)	(0xa4 + (ch))
#define FM_FB_AL(ch)		(0xb0 + (ch))
#endif	/* !_YM2203REG_H_ */
