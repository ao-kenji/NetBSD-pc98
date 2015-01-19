/*	$NecBSD: ippinfo.c,v 1.13 1998/09/11 13:03:57 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * Copyright (c) 1996, Sujal M. Patel
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: pnpinfo.c,v 1.16 1996/05/05 23:56:38 smpatel Exp $
 */

#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <string.h>

#include <machine/cpufunc.h>

#include <i386/Cbus/pnp/pnpreg.h>
#include <i386/Cbus/pnp/ippiio.h>

int	DevFd;
int	DataPos;
struct	ippi_ipp_info Rdata;

static int power __P((int, int));
static int get_resource_info __P((char *, int));
static void report_dma_info __P((int));
static void report_memory_info __P((int));
static int handle_small_res __P((unsigned char *, int, int));
static void handle_large_res __P((unsigned char *, int, int));
static void dump_resdata __P((unsigned char *, int));
static int print_ippi_status __P((void));

int main __P((int, char **));

static int
power(base, exp)
	int base, exp;
{

	if (exp <= 1)
		return base;
	else
		return base * power(base, exp - 1);
}

/*
 * Fill's the buffer with resource info from the device.
 * Returns 0 if the device fails to report
 */
static int
get_resource_info(buffer, len)
	char *buffer;
	int len;
{
	int i;

	if (DataPos + len >= MAX_RDSZ)
	{
		printf("resource data too large!\n");
		exit(1);
	}

	for (i = 0; i < len; i++)
		buffer[i] = Rdata.ip_cis[DataPos ++];

	return 1;
}

static void
report_dma_info (x)
	int x;
{

	switch (x & 0x3) {
	case 0:
		printf ("DMA: 8-bit only\n");
		break;
	case 1:
		printf ("DMA: 8-bit and 16-bit\n");
		break;
	case 2:
		printf ("DMA: 16-bit only\n");
		break;
#ifdef DIAGNOSTIC
	case 3:
		printf ("DMA: Reserved\n");
		break;
#endif
	}

	if (x & 0x4)
		printf ("DMA: Device is a bus master\n");
	else
		printf ("DMA: Device is not a bus master\n");

	if (x & 0x8)
		printf ("DMA: May execute in count by byte mode\n");
	else
		printf ("DMA: May not execute in count by byte mode\n");

	if (x & 0x10)
		printf ("DMA: May execute in count by word mode\n");
	else
		printf ("DMA: May not execute in count by word mode\n");

	switch ((x & 0x60) >> 5) {
	case 0:
		printf ("DMA: Compatibility mode\n");
		break;
	case 1:
		printf ("DMA: Type A DMA channel\n");
		break;
	case 2:
		printf ("DMA: Type B DMA channel\n");
		break;
	case 3:
		printf ("DMA: Type F DMA channel\n");
		break;
	}
}

static void
report_memory_info (x)
	int x;
{

	if (x & 0x1)
		printf ("Memory Range: Writeable\n");
	else
		printf ("Memory Range: Not writeable (ROM)\n");

	if (x & 0x2)
		printf ("Memory Range: Read-cacheable, write-through\n");
	else
		printf ("Memory Range: Non-cacheable\n");

	if (x & 0x4)
		printf ("Memory Range: Decode supports high address\n");
	else
		printf ("Memory Range: Decode supports range length\n");

	switch ((x & 0x18) >> 3) {
	case 0:
		printf ("Memory Range: 8-bit memory only\n");
		break;
	case 1:
		printf ("Memory Range: 16-bit memory only\n");
		break;
	case 2:
		printf ("Memory Range: 8-bit and 16-bit memory supported\n");
		break;
#ifdef DIAGNOSTIC
	case 3:
		printf ("Memory Range: Reserved\n");
		break;
#endif
	}

	if (x & 0x20)
		printf ("Memory Range: Memory is shadowable\n");
	else
		printf ("Memory Range: Memory is not shadowable\n");

	if (x & 0x40)
		printf ("Memory Range: Memory is an expansion ROM\n");
	else
		printf ("Memory Range: Memory is not an expansion ROM\n");

#ifdef DIAGNOSTIC
	if (x & 0x80)
		printf ("Memory Range: Reserved (Device is brain-damaged)\n");
#endif
}

/*
 *  Small Resource Tag Handler
 *
 *  Returns 1 if checksum was valid (and an END_TAG was received).
 *  Returns -1 if checksum was invalid (and an END_TAG was received).
 *  Returns 0 for other tags.
 */
static int
handle_small_res(resinfo, item, len)
	int item, len;
	unsigned char *resinfo;
{
	int i;

	switch (item) {
	case PNP_VERSION:
		printf("PnP Version: %d.%d\n",
		    resinfo[0] >> 4,
		    resinfo[0] & (0xf));
		printf("Vendor Version: %d\n", resinfo[1]);
		break;
	case LOG_DEVICE_ID:
		printf("Logical Device ID: %c%c%c%02x%02x (%08lx)\n",
		    ((resinfo[0] & 0x7c) >> 2) + 64,
		    (((resinfo[0] & 0x03) << 3) |
			((resinfo[1] & 0xe0) >> 5)) + 64,
		    (resinfo[1] & 0x1f) + 64,
		    resinfo[2], resinfo[3], htonl(*(u_long *)resinfo));

		if (resinfo[4] & 0x1)
			printf ("Device powers up active\n");
		if (resinfo[4] & 0x2)
			printf ("Device supports I/O Range Check\n");
		if (resinfo[4] > 0x3)
			printf ("Reserved register funcs %02x\n",
				resinfo[4]);

		if (len == 6)
			printf("Vendor register funcs %02x\n", resinfo[5]);
		break;
	case COMP_DEVICE_ID:
		printf("Compatible Device ID: %c%c%c%02x%02x (%08x)\n",
		    ((resinfo[0] & 0x7c) >> 2) + 64,
		    (((resinfo[0] & 0x03) << 3) |
			((resinfo[1] & 0xe0) >> 5)) + 64,
		    (resinfo[1] & 0x1f) + 64,
		    resinfo[2], resinfo[3], *(int *)resinfo);
		break;
	case IRQ_FORMAT:
		printf("IRQ: ");

		for (i = 0; i < 8; i++)
			if (resinfo[0] & (char) (power(2, i)))
				printf("%d ", i);
		for (i = 0; i < 8; i++)
			if (resinfo[1] & (char) (power(2, i)))
				printf("%d ", i + 8);
		printf("\n");
		if (len == 3) {
			if (resinfo[2] & 0x1)
				printf("IRQ: High true edge sensitive\n");
			if (resinfo[2] & 0x2)
				printf("IRQ: Low true edge sensitive\n");
			if (resinfo[2] & 0x4)
				printf("IRQ: High true level sensitive\n");
			if (resinfo[2] & 0x8)
				printf("IRQ: Low true level sensitive\n");
		}
		break;
	case DMA_FORMAT:
		printf("DMA: ");
		for (i = 0; i < 8; i++)
			if (resinfo[0] & (char) (power(2, i)))
				printf("%d ", i);
		printf ("\n");
		report_dma_info (resinfo[1]);
		break;
	case START_DEPEND_FUNC:
		printf("Start Dependent Function\n");
		if (len == 1) {
			switch (resinfo[0]) {
			case 0:
				printf("Good Configuration\n");
				break;
			case 1:
				printf("Acceptable Configuration\n");
				break;
			case 2:
				printf("Sub-optimal Configuration\n");
				break;
			}
		}
		break;
	case END_DEPEND_FUNC:
		printf("End Dependent Function\n");
		break;
	case IO_PORT_DESC:
		if ((resinfo[0] & 0x01))
			printf("Device decodes the full 16-bit ISA address\n");
		else
			printf("Device does not decode the full 16-bit ISA address\n");
		if ((resinfo[0] & 0x80))
			printf("I/O address assign with skip 1 byte\n");
		else
			printf("I/O address assign continuously\n");

		printf("I/O Range minimum address: 0x%x\n",
		    resinfo[1] + (resinfo[2] << 8));
		printf("I/O Range maximum address: 0x%x\n",
		    resinfo[3] + (resinfo[4] << 8));
		printf("I/O alignment for minimum: %d\n",
		    resinfo[5]);
		printf("I/O length: %d\n", resinfo[6]);
		break;
	case FIXED_IO_PORT_DESC:
		printf ("I/O Range base address: 0x%x\n",
		    resinfo[1] + (resinfo[2] << 8));
		printf("I/O length: %d\n", resinfo[3]);
		break;
#ifdef DIAGNOSTIC
	case SM_RES_RESERVED:
		printf("Reserved Tag Detected\n");
		break;
#endif
	case SM_VENDOR_DEFINED:
		printf("*** Small Vendor Tag Detected\n");
		break;
	case END_TAG:
		printf("End Tag\n\n");
		/* XXX Record and Verify Checksum */
		return 1;
		break;
	}
	return 0;
}


static void
handle_large_res(resinfo, item, len)
	int item, len;
	unsigned char *resinfo;
{
	int i;

	switch (item) {
	case MEMORY_RANGE_DESC:
		report_memory_info(resinfo[0]);
		printf("Memory range minimum address: 0x%x\n",
		    (resinfo[1] << 8) + (resinfo[2] << 16));
		printf("Memory range maximum address: 0x%x\n",
		    (resinfo[3] << 8) + (resinfo[4] << 16));
		printf("Memory range base alignment: 0x%x\n",
		    (i = (resinfo[5] + (resinfo[6] << 8))) ? i : (1 << 16));
		printf("Memory range length: 0x%x\n",
		    (resinfo[7] + (resinfo[8] << 8)) * 256);
		break;
	case ID_STRING_ANSI:
		printf("Device Description: ");

		for (i = 0; i < len; i++) {
			printf("%c", resinfo[i]);
		}
		printf("\n");
		break;
	case ID_STRING_UNICODE:
		printf("ID String Unicode Detected (Undefined)\n");
		break;
	case LG_VENDOR_DEFINED:
		printf("Large Vendor Defined Detected\n");
		break;
	case _32BIT_MEM_RANGE_DESC:
		printf("32bit Memory Range Desc Unimplemented\n");
		break;
	case _32BIT_FIXED_LOC_DESC:
		printf("32bit Fixed Location Desc Unimplemented\n");
		break;
	case LG_RES_RESERVED:
		printf("Large Reserved Tag Detected\n");
		break;
	}
}

/*
 * Dump all the information about configurations.
 */
static void
dump_resdata(data, csn)
	unsigned char *data;
	int csn;
{
	int i, large_len;
	u_char tag, *resinfo;

	printf("Board Vendor ID: %c%c%c%02x%02x(0x%08lx)\n",
	    ((data[0] & 0x7c) >> 2) + 64,
	    (((data[0] & 0x03) << 3) | ((data[1] & 0xe0) >> 5)) + 64,
	    (data[1] & 0x1f) + 64, data[2], data[3],
	    htonl(*(u_long *)data));
	printf("Board Serial Number: %08lx\n", htonl(*(u_long *)&(data[4])));

	/* Allows up to 1kb of Resource Info,  Should be plenty */
	for (i = 0; i < 1024; i++) {
		if (!get_resource_info(&tag, 1))
			return;

#define TYPE	(tag >> 7)
#define	S_ITEM	(tag >> 3)
#define S_LEN	(tag & 0x7)
#define	L_ITEM	(tag & 0x7f)

		if (TYPE == 0) {
			/* Handle small resouce data types */

			resinfo = malloc(S_LEN);
			if (!get_resource_info(resinfo, S_LEN))
				return;

			if (handle_small_res(resinfo, S_ITEM, S_LEN) == 1)
				return;
			free(resinfo);
		} else {
			/* Handle large resouce data types */

			large_len = 0;	/* XXX */
			if (!get_resource_info((char *) &large_len, 2))
				return;

			resinfo = malloc(large_len);
			if (!get_resource_info(resinfo, large_len))
				return;

			handle_large_res(resinfo, L_ITEM, large_len);
			free(resinfo);
		}
	}
}

u_char *status_string[] =
	{ "unknown", "found", "sleep", "mapped", "device connected" };

static int
print_ippi_status(void)
{
	u_char *sstr;

	sstr = status_string[Rdata.ip_state];
	if (Rdata.ip_state < 4)
		strcpy(Rdata.ip_dvname, "none");

	printf("ld(%d:%d) status(%s) device(%s)\n",
		Rdata.ip_ipd.ipd_csn, Rdata.ip_ipd.ipd_ldn,
		sstr, Rdata.ip_dvname);

	return 0;
}

int
main(argc, argv)
	int argc;
	char **argv;
{

	u_char *dvname = "/dev/ippi0";
	u_char data[9];
	int csn;
	int num_pnp_devs = 0;

	DevFd = open(dvname, O_RDWR, 0);
	if (DevFd < 0)
	{
		perror(dvname);
		exit(1);
	}

	for (csn = 1; csn < 1 + MAX_CARDS; csn ++)
	{
		Rdata.ip_ipd.ipd_csn = csn;
		Rdata.ip_ipd.ipd_ldn = 0;
		if (ioctl(DevFd, IPPI_IOG_RD, &Rdata))
			continue;

		print_ippi_status();
		bcopy(Rdata.ip_cis, data, 9);
		DataPos = 9;
		dump_resdata(data, csn);
		num_pnp_devs ++;
	}

	if (!num_pnp_devs)
		printf("No Plug-n-Play devices were found\n");

	exit(0);
}
