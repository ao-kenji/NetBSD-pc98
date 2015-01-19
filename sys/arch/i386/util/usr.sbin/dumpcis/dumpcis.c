/*	$NecBSD: dumpcis.c,v 1.11 1998/11/19 16:49:04 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Code cleanup, bug-fix and extention
 * by Tatsumi Hosokawa <hosokawa@mt.cs.keio.ac.jp>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/proc.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>

#define	CIS_FEAT_POWER(x)	((x) & 0x3)
#define	CIS_FEAT_TIMING		0x4
#define	CIS_WAIT_SCALE(x)	((x) & 0x3)
#define	CIS_READY_SCALE(x)	(((x)>>2) & 0x7)
#define	CIS_RESERVED_SCALE(x)	(((x)>>5) & 0x7)
#define	CIS_FEAT_I_O		0x8
#define	CIS_IO_ADDR(x)		((x) & 0x1F)
#define	CIS_FEAT_IRQ		0x10
#define	CIS_FEAT_MEMORY(x)	(((x) >> 5) & 0x3)
#define	CIS_FEAT_MISC		0x80

struct tuple_info {
	char *name;
	unsigned char code;
	unsigned char length;	/* 255 means variable length */
};

static void showbuf __P((unsigned char *, int));
static void dumpcis __P((unsigned char *));
static struct tuple_info *get_tuple_info __P((unsigned char));
static char *tuple_name __P((unsigned char));
static void dump_cis_config __P((unsigned char *));
static void dump_config_map __P((unsigned char *));
static void dump_other_cond __P((unsigned char *));
static int dump_pwr_desc __P((unsigned char *));
static void dump_device_desc __P((unsigned char *, int, char *));
static void dump_info_v1 __P((unsigned char *, int));
static void dump_func_ext_serial __P((unsigned char *, int));
static void dump_func_ext_network __P((unsigned char *, int));
static void print_ext_speed __P((unsigned char, int));

int main __P((int, char **));

int main __P((int, char **));

u_int pccs_show_info;
int DevFd;

static void
showbuf(buf, len)
	unsigned char *buf;
	int len;
{
	int i;

	for (i = 0; i < len; i++)
	{
		if((i % 0x10) == 0)
			printf("\n    %03x: ", i);
		printf(" %02x", buf[i]);
	}
	printf("\n");
}

static void
dumpcis(buf)
	unsigned char *buf;
{
	unsigned char code, len, *tbuf;
	int done;
	int count = 0;
	int tplfe_type = 0;

	done = 0;
	while (!done) {
		code = *buf++;
		len = *buf++;
		tbuf = buf;
		buf += len;

		printf("Tuple #%d, code = 0x%x (%s), length = %d",
			++count, code, tuple_name(code),
			(code != CIS_END) ? len : 0);
		if (code == CIS_END) {
			done = 1;
			printf("\n");
			continue;
		} else {
			showbuf(tbuf, len);
		}

		switch (code) {
		default:
			break;
		case CIS_DEVICE:	/* 0x01 */
			dump_device_desc(tbuf, len, "Common");
			break;
		case CIS_CHECKSUM:	/* 0x10 */
			if (len == 5) {
				printf("\tChecksum from offset %d, length %d, value is 0x%x\n",
					(short) ((tbuf[1] << 8) | tbuf[0]),
					(tbuf[3] << 8) | tbuf[2], tbuf[4]);
			} else
				printf("\tIllegal length for checksum!\n");
			break;
		case CIS_LONGLINK_A:	/* 0x11 */
			printf("\tLong link to attribute memory, address 0x%x\n",
			       (tbuf[3] << 24) | (tbuf[2] << 16) |
			       (tbuf[1] << 8) | tbuf[0]);
			break;
		case CIS_LONGLINK_C:	/* 0x12 */
			printf("\tLong link to common memory, address 0x%x\n",
			       (tbuf[3] << 24) | (tbuf[2] << 16) |
			       (tbuf[1] << 8) | tbuf[0]);
			break;
		case CIS_VERS_1:	/* 0x15 */
			dump_info_v1(tbuf, len);
			break;
		case CIS_ALTSTR:	/* 0x16 */
			break;
		case CIS_DEVICE_A:	/* 0x17 */
			dump_device_desc(tbuf, len, "Attribute");
			break;
		case CIS_JEDEC_C:	/* 0x18 */
			break;
		case CIS_JEDEC_A:	/* 0x19 */
			break;
		case CIS_CONFIG:	/* 0x1A */
			dump_config_map(tbuf);
			break;
		case CIS_CFTABLE_ENTRY:	/* 0x1B */
			dump_cis_config(tbuf);
			break;
		case CIS_DEVICE_OC:	/* 0x1C */
			dump_other_cond(tbuf);
			break;
		case CIS_DEVICE_OA:	/* 0x1D */
			dump_other_cond(tbuf);
			break;
		case CIS_DEVICE_GEO:	/* 0x1E */
			break;
		case CIS_DEVICE_GEO_A:	/* 0x1F */
			break;
		case CIS_MANFID:	/* 0x20 */
			printf("\tPCMCIA ID = 0x%x, OEM ID = 0x%x\n",
			       (tbuf[1] << 8) | tbuf[0],
			       (tbuf[3] << 8) | tbuf[2]);
			break;
		case CIS_FUNCID:	/* 0x21 */
			tplfe_type = tbuf[0];
			switch (tbuf[0]) {
			default:
				printf("\tUnknown function");
				break;
			case 0:
				printf("\tMultifunction card");
				break;
			case 1:
				printf("\tMemory card");
				break;
			case 2:
				printf("\tSerial port/modem");
				break;
			case 3:
				printf("\tParallel port");
				break;
			case 4:
				printf("\tFixed disk card");
				break;
			case 5:
				printf("\tVideo adapter");
				break;
			case 6:
				printf("\tNetwork/LAN adapter");
				break;
			case 7:
				printf("\tAIMS");
				break;
			}
			printf("%s%s\n", (tbuf[1] & 1) ? " - POST initialize" : "",
			(tbuf[1] & 2) ? " - Card has ROM" : "");
			break;
		case CIS_FUNCE:	/* 0x22 */
			switch (tplfe_type) {
			case 2:	/* FUNCID_SERIAL */
				dump_func_ext_serial(tbuf, len);
				break;
			case 6:	/* FUNCID_NETWORK */
				dump_func_ext_network(tbuf, len);
				break;
			}
			break;
		case CIS_VERS_2:	/* 0x40 */
			break;
		}
	}
}

static struct tuple_info tuple_info[] = {
	{"Null tuple", 0x00, 0},
	{"Common memory descriptor", 0x01, 255},
	{"Checksum", 0x10, 5},
	{"Long link to attribute memory", 0x11, 4},
	{"Long link to common memory", 0x12, 4},
	{"Link target", 0x13, 3},
	{"No link", 0x14, 0},
	{"Version 1 info", 0x15, 255},
	{"Alternate language string", 0x16, 255},
	{"Attribute memory descriptor", 0x17, 255},
	{"JEDEC descr for common memory", 0x18, 255},
	{"JEDEC descr for attribute memory", 0x19, 255},
	{"Configuration map", 0x1A, 255},
	{"Configuration entry", 0x1B, 255},
	{"Other conditions for common memory", 0x1C, 255},
	{"Other conditions for attribute memory", 0x1D, 255},
	{"Geometry info for common memory", 0x1E, 255},
	{"Geometry info for attribute memory", 0x1F, 255},
	{"Manufacturer ID", 0x20, 4},
	{"Functional ID", 0x21, 255},
	{"Functional EXT", 0x22, 255},
	{"Software interleave", 0x23, 2},
	{"Version 2 Info", 0x40, 255},
	{"Data format", 0x41, 255},
	{"Geometry", 0x42, 4},
	{"Byte order", 0x43, 2},
	{"Card init date", 0x44, 4},
	{"Battery replacement", 0x45, 4},
	{"Organisation", 0x46, 255},
	{"Terminator", 0xFF, 255},
	{0, 0, 0}
};

/*
 *	return table entry for code.
 */
static struct tuple_info *
get_tuple_info(code)
	unsigned char code;
{
	struct tuple_info *tp;

	for (tp = tuple_info; tp->name; tp++)
		if (tp->code == code)
			return (tp);
	return (0);
}

static char *
tuple_name(code)
	unsigned char code;
{
	struct tuple_info *tp;

	tp = get_tuple_info(code);
	if (tp)
		return (tp->name);
	return ("Unknown");
}

/*
 *	Dump a config entry.
 */
static void
dump_cis_config(q)
	unsigned char *q;
{
	unsigned char *p, feat;
	int i, j;
	char c;

	p = q;
	printf("\tConfig index = 0x%x%s\n", *p & 0x3F,
	       *p & 0x40 ? "(default)" : "");
	if (*p & 0x80) {
		p++;
		printf("\tInterface byte = 0x%x ", *p);
		switch (*p & 0xF) {
		default:
			printf("(reserved)");
			break;
		case 0:
			printf("(memory)");
			break;
		case 1:
			printf("(I/O)");
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			printf("(custom)");
			break;
		}
		c = ' ';
		if (*p & 0x10) {
			printf(" BVD1/2 active");
			c = ',';
		}
		if (*p & 0x20) {
			printf("%c card WP active", c);	/* Write protect */
			c = ',';
		}
		if (*p & 0x40) {
			printf("%c +RDY/-BSY active", c);
			c = ',';
		}
		if (*p & 0x80)
			printf("%c wait signal supported", c);
		printf("\n");
	}
	p++;
	feat = *p++;
	switch (CIS_FEAT_POWER(feat)) {
	case 0:
		break;
	case 1:
		printf("\tVcc pwr:\n");
		p += dump_pwr_desc(p);
		break;
	case 2:
		printf("\tVcc pwr:\n");
		p += dump_pwr_desc(p);
		printf("\tVpp pwr:\n");
		p += dump_pwr_desc(p);
		break;
	case 3:
		printf("\tVcc pwr:\n");
		p += dump_pwr_desc(p);
		printf("\tVpp1 pwr:\n");
		p += dump_pwr_desc(p);
		printf("\tVpp2 pwr:\n");
		p += dump_pwr_desc(p);
		break;
	}
	if (feat & CIS_FEAT_TIMING) {
		i = *p++;
		j = CIS_WAIT_SCALE(i);
		if (j != 3) {
			printf("\tWait scale ");
			print_ext_speed(*p++, j);
			printf("\n");
		}
		j = CIS_READY_SCALE(i);
		if (j != 7) {
			printf("\tRDY/BSY scale ");
			print_ext_speed(*p++, j);
			printf("\n");
		}
		j = CIS_RESERVED_SCALE(i);
		if (j != 7) {
			printf("\tExternal scale ");
			print_ext_speed(*p++, j);
			printf("\n");
		}
	}
	if (feat & CIS_FEAT_I_O) {
		if (CIS_IO_ADDR(*p))
			printf("\tCard decodes %d address lines", CIS_IO_ADDR(*p));
		else
			printf("\tCard provides address decode");
		switch ((*p >> 5) & 3) {
		case 0:
			break;
		case 1:
			printf(", 8 Bit I/O only");
			break;
		case 2:
			printf(", limited 8/16 Bit I/O");
			break;
		case 3:
			printf(", full 8/16 Bit I/O");
			break;
		}
		printf("\n");
		if (*p++ & 0x80) {
			c = *p++;
			for (i = 0; i <= (c & 0xF); i++) {
				printf("\t\tI/O address # %d: ", i + 1);
				switch ((c >> 4) & 3) {
				case 0:
					break;
				case 1:
					printf("block start = 0x%x", *p++);
					break;
				case 2:
					printf("block start = 0x%x", (p[1] << 8) | *p);
					p += 2;
					break;
				case 3:
					printf("block start = 0x%x",
					       (p[3] << 24) | (p[2] << 16) |
					       (p[1] << 8) | *p);
					p += 4;
					break;
				}
				switch ((c >> 6) & 3) {
				case 0:
					break;
				case 1:
					printf(" block length = 0x%x", *p++ + 1);
					break;
				case 2:
					printf(" block length = 0x%x", ((p[1] << 8) | *p) + 1);
					p += 2;
					break;
				case 3:
					printf(" block length = 0x%x",
					       ((p[3] << 24) | (p[2] << 16) |
						(p[1] << 8) | *p) + 1);
					p += 4;
					break;
				}
				printf("\n");
			}
		}
	}

	/*
	 *	IRQ descriptor
	 */
	if (feat & CIS_FEAT_IRQ) {
		printf("\t\tIRQ modes:");
		c = ' ';
		if (*p & 0x20) {
			printf(" Level");
			c = ',';
		}
		if (*p & 0x40) {
			printf("%c Pulse", c);
			c = ',';
		}
		if (*p & 0x80)
			printf("%c Shared", c);
		printf("\n");
		if (*p & 0x10) {
			i = p[1] | (p[2] << 8);
			printf("\t\tIRQs: ");
			if (*p & 1)
				printf(" NMI");
			if (*p & 0x2)
				printf(" IOCK");
			if (*p & 0x4)
				printf(" BERR");
			if (*p & 0x8)
				printf(" VEND");
			for (j = 0; j < 16; j++)
				if (i & (1 << j))
					printf(" %d", j);
			printf("\n");
			p += 3;
		} else {
			printf("\t\tIRQ level = %d\n", *p & 0xF);
			p++;
		}
	}
	switch (CIS_FEAT_MEMORY(feat)) {
	case 0:
		break;
	case 1:
		printf("\tMemory space length = 0x%x\n", (p[1] << 8) | p[0]);
		p += 2;
		break;
	case 2:
		printf("\tMemory space address = 0x%x, length = 0x%x\n",
		       (p[3] << 8) | p[2],
		       (p[1] << 8) | p[0]);
		p += 4;
		break;
	/*
	 *	Memory descriptors.
	 */
	case 3:
		c = *p++;
		for (i = 0; i <= (c & 7); i++) {
			printf("\tMemory descriptor %d\n\t\t", i + 1);
			switch ((c >> 3) & 3) {
			case 0:
				break;
			case 1:
				printf(" blk length = 0x%x00", *p++);
				break;
			case 2:
				printf(" blk length = 0x%x00", (p[1] << 8) | *p);
				p += 2;
				break;
			case 3:
				printf(" blk length = 0x%x00",
				       (p[3] << 24) | (p[2] << 16) |
				       (p[1] << 8) | *p);
				p += 4;
				break;
			}
			switch ((c >> 5) & 3) {
			case 0:
				break;
			case 1:
				printf(" card addr = 0x%x00", *p++);
				break;
			case 2:
				printf(" card addr = 0x%x00", (p[1] << 8) | *p);
				p += 2;
				break;
			case 3:
				printf(" card addr = 0x%x00",
				       (p[3] << 24) | (p[2] << 16) |
				       (p[1] << 8) | *p);
				p += 4;
				break;
			}
			if (c & 0x80)
				switch ((c >> 5) & 3) {
				case 0:
					break;
				case 1:
					printf(" host addr = 0x%x00", *p++);
					break;
				case 2:
					printf(" host addr = 0x%x00", (p[1] << 8) | *p);
					p += 2;
					break;
				case 3:
					printf(" host addr = 0x%x00",
					       (p[3] << 24) | (p[2] << 16) |
					       (p[1] << 8) | *p);
					p += 4;
					break;
				}
			printf("\n");
		}
		break;
	}
	if (feat & CIS_FEAT_MISC) {
		printf("\tMax twin cards = %d\n", *p & 7);
		printf("\tMisc attr:");
		if (*p & 0x8)
			printf(" (Audio-BVD2)");
		if (*p & 0x10)
			printf(" (Read-only)");
		if (*p & 0x20)
			printf(" (Power down supported)");
		if (*p & 0x80) {
			printf(" (Ext byte = 0x%x)", p[1]);
			p++;
		}
		printf("\n");
		p++;
	}
}

/*
 *	Dump configuration map tuple.
 */
static void
dump_config_map(q)
	unsigned char *q;
{
	unsigned char *p, x;
	int rlen, mlen;
	int i;
	union {
		u_long l;
		unsigned char b[4];
	} u;

	rlen = (q[0] & 3) + 1;
	mlen = ((q[0] >> 2) & 3) + 1;
	u.l = 0;
	p = q + 2;
	for (i = 0; i < rlen; i++)
		u.b[i] = *p++;
	printf("\tReg len = %d, config register addr = 0x%lx, last config = 0x%x\n",
	       rlen, u.l, q[1]);
	if (mlen)
		printf("\tRegisters: ");
	for (i = 0; i < mlen; i++, p++) {
		for (x = 0x1; x; x <<= 1)
			printf("%c", x & *p ? 'X' : '-');
		printf(" ");
	}
	printf("\n");
}

/*
 *	dump_other_cond - Dump other conditions.
 */
static void
dump_other_cond(p)
	unsigned char *p;
{

	if (p[0]) {
		printf("\t");
		if (p[0] & 1)
			printf("(MWAIT)");
		if (p[0] & 2)
			printf(" (3V card)");
		if (p[0] & 0x80)
			printf(" (Extension bytes follow)");
		printf("\n");
	}
}

/*
 *	Dump power descriptor.
 */
static int
dump_pwr_desc(p)
	unsigned char *p;
{
	int len = 1, i;
	unsigned char mask;
	char **expp;
	static char *pname[] =
		{"Nominal operating supply voltage",
		"Minimum operating supply voltage",
		"Maximum operating supply voltage",
		"Continuous supply current",
		"Max current average over 1 second",
		"Max current average over 10 ms",
		"Power down supply current",
		"Reserved"
		};
	static char *vexp[] =
		{"10uV", "100uV", "1mV", "10mV", "100mV", "1V", "10V", "100V"};
	static char *cexp[] =
		{"10nA", "1uA", "10uA", "100uA", "1mA", "10mA", "100mA", "1A"};
	static char *mant[] =
		{"1", "1.2", "1.3", "1.5", "2", "2.5", "3", "3.5", "4", "4.5",
		"5", "5.5", "6", "7", "8", "9"};

	mask = *p++;
	expp = vexp;
	for (i = 0; i < 8; i++)
		if (mask & (1 << i)) {
			len++;
			if (i >= 3)
				expp = cexp;
			printf("\t\t%s: ", pname[i]);
			printf("%s x %s",
			       mant[(*p >> 3) & 0xF],
			       expp[*p & 7]);
			while (*p & 0x80) {
				len++;
				p++;
				printf(", ext = 0x%x", *p);
			}
			printf("\n");
			p++;
		}
	return (len);
}

static void
dump_device_desc(p, len, type)
	unsigned char *p;
	int len;
	char *type;
{
	static char *un_name[] =
		{"512b", "2Kb", "8Kb", "32Kb",
		"128Kb", "512Kb", "2Mb", "reserved"};
	static char *speed[] =
		{"No speed", "250nS", "200nS", "150nS",
		"100nS", "Reserved", "Reserved"};
	static char *dev[] =
		{"No device", "Mask ROM", "OTPROM", "UV EPROM",
		"EEPROM", "FLASH EEPROM", "SRAM", "DRAM",
		"Reserved", "Reserved", "Reserved", "Reserved",
		"Reserved", "Function specific", "Extended",
		"Reserved"};
	int count = 0;

	while (*p != 0xFF && len > 0) {
		unsigned char x;

		x = *p++;
		len -= 2;
		if (count++ == 0)
			printf("\t%s memory device information:\n", type);
		printf("\t\tDevice number %d, type %s, WPS = %s\n",
		       count, dev[x >> 4], (x & 0x8) ? "ON" : "OFF");
		if ((x & 7) == 7) {
			len--;
			if (*p) {
				printf("\t\t");
				print_ext_speed(*p++, 0);
				while (*p & 0x80) {
					p++;
					len--;
				}
			}
			p++;
		} else
			printf("\t\tSpeed = %s", speed[x & 7]);
		printf(", Memory block size = %s, %d units\n",
		       un_name[*p & 7], (*p >> 3) + 1);
		p++;
	}
}

/*
 *	Print version info
 */
static void
dump_info_v1(p, len)
	unsigned char *p;
	int len;
{

	printf("\tVersion = %d.%d", p[0], p[1]);
	p += 2;
	printf(", Manuf = [%s],", p);
	while (*p++);
	printf("card vers = [%s]\n", p);
	while (*p++);
	printf("\tAddit. info = [%s]", p);
	while (*p++);
	printf(",[%s]\n", p);
}

/*
 *	dump functional extension tuple.
 */
static void
dump_func_ext_serial(p, len)
	unsigned char *p;
	int len;
{

	if (len == 0)
		return;
	switch (p[0]) {
	case 0:
	case 8:
	case 10:
		if (len != 4) {
			printf("\tWrong length for serial extension\n");
			return;
		}
		printf("\tSerial interface extension:\n");
		switch (p[1] & 0x1F) {
		default:
			printf("\t\tUnkn device");
			break;
		case 0:
			printf("\t\t8250 UART");
			break;
		case 1:
			printf("\t\t16450 UART");
			break;
		case 2:
			printf("\t\t16550 UART");
			break;
		}
		printf(", Parity - %s%s%s%s",
		       (p[2] & 1) ? "Space," : "",
		       (p[2] & 2) ? "Mark," : "",
		       (p[2] & 4) ? "Odd," : "",
		       (p[2] & 8) ? "Even," : "");
		printf("\n");
		break;
	case 1:
	case 5:
	case 6:
	case 7:
		printf("\tModem interface capabilities:\n");
		break;
	case 2:
		printf("\tData modem services available:\n");
		break;
	case 9:
		printf("\tFax/modem services available:\n");
		break;
	case 4:
		printf("\tVoice services available:\n");
		break;
	}
}

static void
dump_func_ext_network(p, len)
	unsigned char *p;
	int len;
{
	int i;
	unsigned long speed;

	if (len == 0)
		return;
	switch (p[0]) {
	case 1:	/* LAN_TECH */
		printf("\tLAN technology: ");
		switch (p[1]) {
		case 1: printf("Arcnet"); break;
		case 2: printf("Ethernet"); break;
		case 3: printf("TokenRing"); break;
		case 4: printf("LocalTalk"); break;
		case 5: printf("FDDI/CDDI"); break;
		case 6: printf("ATM"); break;
		case 7: printf("Wireless"); break;
		}
		printf("\n");
		break;
	case 2: /* LAN_SPEED */
		speed = (p[4] << 24) | (p[3] << 16) | (p[2] << 8) | p[1];
		printf("\tLAN speed: ");
		if (speed < 1024)
			printf("%ld bps\n", speed);
		else if (speed < 1024 * 1024)
			printf("%ld Kbps (%ld bps)\n", (speed / 1024), speed);
		else
			printf("%ld Mbps (%ld bps)\n", speed / (1024 * 1024), speed);
		break;
	case 3:	/* LAN_MEDIA */
		printf("\tLAN media: ");
		switch (p[1]) {
		case 1: printf("Unshielded Twisted Pair"); break;
		case 2: printf("Shielded Twisted Pair"); break;
		case 3: printf("Thin Coax"); break;
		case 4: printf("Thick Coax"); break;
		case 5: printf("Fiber"); break;
		case 6: printf("900 MHz"); break;
		case 7: printf("2.4 GHz"); break;
		case 8: printf("5.4 GHz"); break;
		case 9: printf("Diffuse Infrared"); break;
		case 10: printf("Point-to-Point Infrared"); break;
		}
		printf("\n");
		break;
	case 4:	/* LAN_NID */
		printf("\tLAN node ID:");
		for (i = 0; i < p[1]; i++)
			printf(" %02x", p[2 + i]);
		printf(" (%d bytes)\n", p[1]);
		break;
	case 5:	/* LAN_CONN */
		printf("\tLAN connector: ");
		if (p[1] == 0)
			printf("Open connector standard\n");
		else
			printf("Closed connector standard\n");
		break;
	}
}

/*
 *	print_ext_speed - Print extended speed.
 */
static void
print_ext_speed(x, scale)
	unsigned char x;
	int scale;
{
	static char *mant[] =
		{"Reserved", "1.0", "1.2", "1.3", "1.5", "2.0", "2.5", "3.0",
		"3.5", "4.0", "4.5", "5.0", "5.5", "6.0", "7.0", "8.0"};
	static char *exp[] =
		{"1 ns", "10 ns", "100 ns", "1 us", "10 us", "100 us",
		"1 ms", "10 ms"};
	static char *scale_name[] =
		{"None", "10", "100", "1,000", "10,000", "100,000",
		"1,000,000", "10,000,000"};

	printf("Speed = %s x %s", mant[(x >> 3) & 0xF], exp[x & 7]);
	if (scale)
		printf(", scaled by %s", scale_name[scale & 7]);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	unsigned char buf[CARD_ATTR_SZ];
	unsigned char pathname[FILENAME_MAX];
	unsigned char *path;
	int i;
	struct slot_info si;

	path = (argc <= 1) ? "slot0" : argv[1];
	if (strncmp(path, "slot", 4))
	{
		printf("dumpcis slot?\n");
		exit(1);
	}

	i = path[4] - '0';
	sprintf(pathname, "/dev/ata%d", i);
	DevFd = open(pathname, O_RDONLY, 777);
	if (DevFd < 0)
	{
		perror("dumpcis");
		exit(1);
	}

	si.si_mfcid = PUNKMFCID;
	if (ioctl(DevFd, PCCS_IOG_SSTAT, &si))
	{
		perror("dumpcis.stat");
		exit(1);
	}

	if (si.si_st == SLOT_NULL)
	{
		printf("no card in slot\n");
		exit(0);
	}

	if (si.si_st < SLOT_READY && ioctl(DevFd, PCCS_IOC_INIT, 0))
	{
		perror("dumpcis.init");
		exit(1);
	}

	if (ioctl(DevFd, PCCS_IOG_ATTR, buf))
	{
		perror("dumpcis");
		exit(1);
	}

	printf("Configuration data for card in slot %d\n", i);
	dumpcis(buf);

	exit(0);
}
