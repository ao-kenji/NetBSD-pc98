/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
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

#ifndef _I386_VSC_VT100_H_
#define _I386_VSC_VT100_H_

#define	NUL	0x00
#define	SOH	0x01
#define	STX	0x02
#define	ETX	0x03
#define	EOT	0x04
#define	ENQ	0x05
#define	ACK	0x06
#define	BEL	0x07
#define	BS	0x08
#define	HT	0x09
#define	LF	0x0a
#define	VT	0x0b
#define	FF	0x0c
#define	CR	0x0d
#define	SO	0x0e
#define	SI	0x0f
#define	DLE	0x10
#define	DC1	0x11
#define	DC2	0x12
#define	DC3	0x13
#define	DC4	0x14
#define	NAK	0x15
#define	SYN	0x16
#define	ETB	0x17
#define	CAN	0x18
#define	EM	0x19
#define	SUB	0x1a
#define	ESC	0x1b
#define	FS	0x1c
#define	GS	0x1d
#define	RS	0x1e
#define	US	0x1f
#define	SS2	0x8e
#define	SS3	0x8f

/* iso2022 */
#define	CHARSET0	0	/* multi byte 21-7f */
#define	CHARSET1	1	/* multi byte 20-80 */
#define	CHARSET2	2	/* single byte 21-7f */
#define	CHARSET3	3	/* single byte 20-80 */

/* CODE SETS */
#define	XASCII		0x42
#define	JIS7		0x40
#define	JIS8		0x42
#define	ROMAJI		0x4a
#define	XKANA		0x49
#define	LATIN1		'A'
#define	SUPGRAPH	'<'
#define	TECH		'>'
#define	SPEGRAPH	'0'
#define	ROMSTAND	'1'
#define	ROMSPECIAL	'2'
#define	EUCKANJI	JIS8
#define	SJIS		0

#define	NOCTRL		0
#define	STDCTRL		1

/* state */
#define	SINGLEBYTE		0
#define	MULTIBYTEST		1
#define	MULTIBYTEEND		2
#define	ESCSEQ			3
#define	CONTROLCHAR		4
#define	EXT8SINGLEBYTE		5
#define	EXT8CONTROLCHAR		6
#define	EXT8MULTIBYTE		7
#define	EXT8MULTIBYTEEND	8
#define	ERROR			9
#define	ESCEND			10
#define	CHGWINID		11
#endif /* !_I386_VSC_VT100_H_ */
