/*	$NecBSD: cardinfo.c,v 1.22.10.1 1999/08/24 23:38:44 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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
 * Pcmcia card information service program.
 * Written by N. Honda.
 * Version 0.0 ALPHA(0)
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>

/* from libpcmcia */
void bits_expand __P((u_int, u_char *));
int PcmciaParseCis __P((int, struct card_info *, int, int));

static void show_class __P((struct card_info *));
static void show_creg __P((struct card_info *));
int main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	u_char pathname[FILENAME_MAX];
	int no, fd, ch;
	u_char *path;
	struct slot_info si;
	struct card_info oinfo;
	int i, ps = 0, cs = 0, nmfc, id, rmfcid = 0;

	opterr = 0;

	while ((ch = getopt(argc, argv, "spm:")) != EOF)
	{

		switch (ch) {
		case 's':
			cs = 1;
			break;	
		
		case 'p':
			ps = 1;
			break;

		case 'm':
			rmfcid = atoi(optarg);
			break;

		default:
			exit(1);
		}
		opterr = 0;
	}
	argc -= optind;
	argv += optind;

	path = (argc < 1) ? "slot0" : argv[0];
	if (strncmp(path, "slot", 4))
	{
		printf("cardinfo slot?\n");
		exit(1);
	}
	no = path[4] - '0';
	sprintf(pathname, "/dev/ata%d", no);
	fd = open(pathname, O_RDONLY, 777);

	if (fd < 0)
	{
		perror("cardinfo");
		exit(1);
	}

	si.si_mfcid = PUNKMFCID;
	if (ioctl(fd, PCCS_IOG_SSTAT, &si))
	{
		perror("cardinfo.stat");
		exit(1);
	}

	if (si.si_st == SLOT_NULL)
	{
		printf("no card in slot\n");
		exit(0);
	}

	if (si.si_st < SLOT_READY && ioctl(fd, PCCS_IOC_INIT, 0))
	{
		perror("cardinfo.init");
		exit(1);
	}

	if (cs != 0)
	{
		printf("PRODUCT: <%s> (0x%08x)\n",
			si.si_ci.ci_product, si.si_ci.ci_manfid);
		show_class(&si.si_ci);
		show_creg(&si.si_ci);
		show_card_info(&si.si_ci);
		for (i = 0; i < PCCS_MAXMFC; i ++)
		{
			si.si_mfcid = i;
			if (ioctl(fd, PCCS_IOG_SSTAT, &si) != 0)
				continue;
			if (si.si_fst == 0)
				continue;
			printf("\nFN(%d): device<%s>\n", i, si.si_xname);
			show_class(&si.si_fci);
			show_creg(&si.si_fci);
			show_card_info(&si.si_fci);
		}
	}
	else if (ps != 0)
	{
		/* show real slot settings */
		pccs_init_ci(&si.si_ci);		
		if (ioctl(fd, PCCS_IOC_PREFER, &si.si_ci))
		{
			perror("cardinfo.prefer");
			exit(1);
		}

		printf("PRODUCT: <%s> (0x%08x)\n",
			si.si_ci.ci_product, si.si_ci.ci_manfid);
		show_class(&si.si_ci);
		show_creg(&si.si_ci);
		show_card_info(&si.si_ci);
	}
	else
	{
		pccs_init_ci(&oinfo);
		PCCS_SET_MFCID(&oinfo, rmfcid);	 
		PcmciaParseCis(fd, &oinfo, PCCS_NOT_FOLLOW_LINK, 0);
		nmfc = oinfo.ci_nmfc;
		printf("PRODUCT: <%s> (0x%08x)\n",
			oinfo.ci_product, oinfo.ci_manfid);
		if (nmfc > 0)
			printf("\n<COMMON>\n");
		show_class(&oinfo);
		show_creg(&oinfo);
		pccs_init_ci(&oinfo);
		PcmciaParseCis(fd, &oinfo, PCCS_NOT_FOLLOW_LINK, 1);
		if (nmfc == 0)
			return 0;

		for (id = 0; id < nmfc; id ++) 
		{
			printf("\n<Function %d>\n", id);
			pccs_init_ci(&oinfo);
			PCCS_SET_MFCID(&oinfo, id);
			PcmciaParseCis(fd, &oinfo, PCCS_FOLLOW_LINK, 0);
			show_class(&oinfo);
			show_creg(&oinfo);

			PCCS_SET_MFCID(&oinfo, id);
			PCCS_SET_INDEX(&oinfo, PUNKINDEX);
			PcmciaParseCis(fd, &oinfo, PCCS_FOLLOW_LINK, 1);
		}
	}

	exit(0);
}

static u_char *class_name[] = {
	"MULTI", "MEMORY",
	"SERIAL", "PARA", "FIXED",
	"VIDEO", "NET", "AIMS", "SCSI",
};

static u_char *ifm_name[] = {
	"NO ENTRY", "MEMORY", "IO-CARD",
};

static void
show_class(ci)
	struct card_info *ci;
{

	printf("INTERFACE:");
	if (ci->ci_tpce.if_ifm >= 3)
		printf(" unknown(%d)", ci->ci_tpce.if_ifm);
	else
		printf(" %s", ifm_name[ci->ci_tpce.if_ifm]);
	printf(" flags ");
	bits_expand(ci->ci_tpce.if_flags, IFFLAGSBITS);

	printf("CLASS:");
	if (ci->ci_function > 8)
		printf(" %d (unknown)", ci->ci_function);
	else
		printf(" %d (%s)", ci->ci_function, class_name[ci->ci_function]);
	printf("\n");
}

static void
show_creg(ci)
	struct card_info *ci;
{
	struct card_registers *cr;
	int i = PCCS_GET_MFCID(ci);

	cr = &ci->ci_cr;
	printf("CR(%d): offset 0x%lx lastc 0x%x mask 0x%x ccor 0x%x ccsr 0x%x\n",
		i, cr->cr_offset, cr->cr_lastidx,
		cr->cr_mask, cr->cr_ccor, cr->cr_ccsr);
	printf("IO DECODE: lines %d address mask 0x%x\n",
		ci->ci_iobits, (1 << ci->ci_iobits) - 1);
}
