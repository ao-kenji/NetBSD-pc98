/*	$NecBSD: magic.c,v 1.14.2.9 1999/08/28 09:43:25 honda Exp $	*/
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
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 *	Naofumi HONDA.  All rights reserved.
 */

#include "opt_magic.h"
#include "opt_ddb.h"
#include "pciide.h"
#include "sd.h"

#ifdef	CONFIG_DEVICES
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <i386/Cbus/dev/magicvar.h>

#include <machine/systmbusvar.h>
#include <machine/bootinfo.h>
#include <machine/bootflags.h>

#include <dev/cons.h>

struct config_data {
	u_char *cd_name;
	u_int cd_pos;
#define	PNPAUTO		1
	u_int cd_fl;
	u_int cd_bdloc;
};

struct config_device_call {
	char *cc_name;
	int (*cc_func) __P((void *));
	void *cc_arg;
	char *cc_helpmsg;
};

static void config_device_main __P((void));

/****************************************************
 * String operations
 ****************************************************/
#define	NAMEBUF_LEN	128
#define	NUMBUF_LEN	32

static int gets __P((u_char *, u_int));
static int getnum __P((u_int *));

static int
gets(cp, size)
	u_char *cp;
	u_int size;
{
	register u_char *lp;
	register int c;

	lp = cp;
	for (;;)
	{
		if (lp >= cp + size)
		{
			*(lp - 1) = 0;
			return (u_int) *cp;
		}

		printf("%c", c = cngetc()&0177);
		switch (c)
		{
		case '\n':
		case '\r':
			*lp++ = '\0';
			return (u_int) *cp;
		case '\b':
		case '\177':
			if (lp > cp) {
				printf(" \b");
				lp--;
			}
			continue;
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;
		case '@':
		case 'u'&037:
			lp = cp;
			printf("%c", '\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}

static int
getnum(num)
	u_int *num;
{
	u_char numbuf[NUMBUF_LEN];
	u_char *buf;
	u_int c, card;

	*num = 0;
	if (gets(numbuf, NUMBUF_LEN) == 0)
		return ENOENT;

	if (strncmp(numbuf, "auto", 4) == 0 ||
	    strncmp(numbuf, "dis", 3) == 0)
	{
		*num = -1;
		return 0;
	}

	if (strlen(numbuf) >= 2 && numbuf[0] == '0' && numbuf[1] == 'x')
		card = 16;
	else
		card = 10;

	for (buf = numbuf; (c = *buf) != 0; buf++)
	{
		(*num) *= card;

		if (c >= '0' && c <= '9')
			(*num) += (c - '0');
		if (card == 16 && c >= 'a' && c <= 'f')
			(*num) += ((c - 'a') + 10);
	}
	return 0;
}

/****************************************************
 * cfdata operations
 ****************************************************/
static void modify_data __P((struct config_data *, u_int *));
static void show_data __P((struct config_data *, u_int *));
struct cfdata *select_devices __P((u_char *, struct cfdata *, struct config_data *, struct bootflags_device *));

static void
modify_data(info, data)
	struct config_data *info;
	u_int *data;
{
	u_int *pos, num;
	u_char *x;

	for ( ; info->cd_name != NULL; info ++)
	{
		pos = &data[info->cd_pos];
		if (*pos == (u_int) -1)
		{
			x = (info->cd_fl == PNPAUTO) ? "auto" : "dis/auto";
			printf("%s(%s):", info->cd_name, x);
		}
		else
			printf("%s(0x%x):", info->cd_name, *pos);

		if (getnum(&num) == 0)
			*pos = num;
	}
}

static void
show_data(info, data)
	struct config_data *info;
	u_int *data;
{
	u_int val;
	u_char *x;

	for ( ; info->cd_name != NULL; info ++)
	{
		val = data[info->cd_pos];
		if (val == (u_int) -1)
		{
			x = (info->cd_fl == PNPAUTO) ? "auto" : "dis/auto";
			printf("%s(%s) ", info->cd_name, x);
		}
		else
			printf("%s(0x%x) ", info->cd_name, val);
	}
}

struct config_data cds_flags[] = {
	{"flags", 0, 0, 0},
	{NULL, 0, 0, 0},
};

struct cfdata *
select_devices(resname, cf, cdp, bdp)
	u_char *resname;
	struct cfdata *cf;
	struct config_data *cdp;
	struct bootflags_device *bdp;
{
	struct cfdata *ncf;
	int no, num = 0;

	if (magic_is_sameG(resname, cf, magic_next_dev(cf)) != 0)
		return cf;

	for (ncf = cf, no = 0; magic_is_sameG(resname, cf, ncf) == 0; no++)
	{
		if (bdp == NULL)
		{
			printf("[%d] ", no);
			if (cdp != NULL)
				show_data(cdp, ncf->cf_loc);
			show_data(cds_flags, &ncf->cf_flags);
			printf("\n");
		}
		else
		{
			if (bdp->bd_loc[0] == no)
				return ncf;
		}
		ncf = magic_next_dev(ncf);
	}

	if (bdp == NULL)
	{
		printf("select device number([0]):");
		if (getnum(&num) == 0)
		{
			if (num >= no || num < 0)
			{
				printf("out of range!\n");
				return NULL;
			}
			return cf + num;
		}
	}
	return cf;
}

/************************************************
 * threads
 ************************************************/
static int show_threads __P((struct systm_kthread_dispatch_table *));
static int config_threads __P((void *));

static int
show_threads(kt)
	struct systm_kthread_dispatch_table *kt;
{
	u_char *s;
	int n;

	for (n = 0; kt->kt_ename != NULL; kt ++, n ++)
	{
		s = (kt->kt_flags & SYSTM_KTHREAD_DISPATCH_OK) ?
			"enable" : "disable";
		printf("[%d] %s(%s) %s\n", n, kt->kt_ename, kt->kt_xname,
			s);
	}
	return n;
}

static int 
config_threads(arg)
	void *arg;
{
	struct systm_kthread_dispatch_table *kt = arg;
	u_int num;
	int n;

 	num = 0;
	for (;;)
	{
		n = show_threads(kt);
		printf("select thread number(toggled) or CR(exit):");
		if (getnum(&num) != 0)
			break;

		if (num >= n)
		{
			printf("out of range!\n");
			continue;
		}
		(kt + num)->kt_flags ^= SYSTM_KTHREAD_DISPATCH_OK;
	}

	return 0;
}

/************************************************
 * isa device operations
 ************************************************/
static int config_isa_device __P((void *));

struct config_data cds_isa[] = {
	{"port", ISACF_PORT, 0, 2},
	{"maddr", ISACF_IOMEM, 0, 4},
	{"msize", ISACF_IOSIZ, 0, 5},
	{"irq", ISACF_IRQ, 0, 6},
	{"drq", ISACF_DRQ, 0, 7},
	{NULL, 0, 0},
};

static int
config_isa_device(arg)
	void *arg;
{
	struct cfdata *cf;
	struct cfdriver *dev;
	struct config_data *cdp;
	u_char name[NAMEBUF_LEN];

	for (;;)
	{
		printf("isa device name(ct, ne, ...):");
		if (gets(name, NAMEBUF_LEN) == 0)
			break;

		cf = magic_find_dev("isa", name);
		if (cf == NULL)
		{
			printf("device not found\n");
			continue;
		}

		if (magic_is_isadev(cf) == 0)
		{
			printf("device not isa device\n");
			continue;
		}

		cdp = cds_isa;
		cf = select_devices("isa", cf, cdp, NULL);
		if (cf == NULL)
			continue;

		dev = cf->cf_driver;
		printf("%s%d: ", dev->cd_name, cf->cf_unit);
		show_data(cds_isa, cf->cf_loc);
		show_data(cds_flags, &cf->cf_flags);
		printf("\n");

		modify_data(cds_isa, cf->cf_loc);
                modify_data(cds_flags, &cf->cf_flags);

		printf("%s%d: ", dev->cd_name, cf->cf_unit);
		show_data(cds_isa, cf->cf_loc);
		show_data(cds_flags, &cf->cf_flags);
		printf("\n");
	}
	return 0;
}

/************************************************
 * config geo
 ************************************************/
static int config_geometry __P((void *));

static int
config_geometry(arg)
	void *arg;
{
	struct config_device_geometry *cfd_tablep = arg;
	struct config_device_geometry *cfdp;
	int no, num;
	u_char name[NAMEBUF_LEN];

	for (no = 0, cfdp = cfd_tablep; no < CFD_NGEOMETRIES; cfdp ++, no ++)
	{
		char *s = "non";

		if (cfdp->cfd_name[0] != 0)
			s = cfdp->cfd_name;

		printf("[%d] target(%s) sectors(%d) tracks(%d)\n",
			no, s, cfdp->cfd_sectors, cfdp->cfd_tracks);
	}

	printf("select slot number:");
	if (getnum(&num) != 0)
		return 0;
	if (num < 0 || num >= CFD_NGEOMETRIES)
		return 0;

	cfdp = cfd_tablep + num;
	printf("target (ex: sd0, wd0, del):");
	if (gets(name, NAMEBUF_LEN))
	{
		if (strcmp(name, "del") == 0)
		{
			cfdp->cfd_name[0] = 0;
			return 0;
		}
		strncpy(cfdp->cfd_name, name, CFD_NAMELEN - 1);
	}

	printf("sectors per track:");
	if (getnum(&num) == 0)
		cfdp->cfd_sectors = num;

	printf("tracks per cyl:");
	if (getnum(&num) == 0)
		cfdp->cfd_tracks = num;

	return 0;
}

/************************************************
 * Interrupt registeration
 ************************************************/
static int config_interrupt __P((void *));

static int
config_interrupt(arg)
	void *arg;
{
	struct systm_intr_routing **sirp = arg;
	struct systm_intr_routing *sir = *sirp;
	u_int irq;

	irq = sir->sir_allow_global;
	printf("global intr bit map(0x%x): ", irq);
	if (getnum(&irq) == 0)
		sir->sir_allow_global = irq;

	irq = sir->sir_allow_pccs;
	printf("pccs intr bit map(0x%x): ", irq);
	if (getnum(&irq) == 0)
		sir->sir_allow_pccs = irq;

	irq = sir->sir_allow_ippi;
	printf("ippi intr bit map(0x%x): ", irq);
	if (getnum(&irq) == 0)
		sir->sir_allow_ippi = irq;

	printf("intr bit maps: global(0x%lx), pccs(0x%lx), ippi(0x%lx)\n",
		sir->sir_allow_global,
		sir->sir_allow_pccs,
		sir->sir_allow_ippi);
	return 0;
}

/************************************************
 * Misc
 ************************************************/
static int config_exit __P((void *));
static int config_help __P((void *));

#if	DDB > 0
static int config_ddb __P((void *));

static int
config_ddb(arg)
	void *arg;
{
	
	Debugger();
	return 0;
}
#endif	/* DDB */

#if	NPCIIDE > 0
static int config_pciide __P((void *));
extern int pciide_dma_block;

static int
config_pciide(arg)
	void *arg;
{
	char *s;
	int *vp = arg;

	*vp ^= 1;
	s = (*vp != 0) ? "disable" : "enable";
	printf("%s dma transfer\n", s);
	return 0;
}
#endif	/* NPCIIDE > 0 */

#if	NSD_SCSIBUS > 0
static int config_scsibios __P((void *));
extern int scsibios_scsibus_id;

static int
config_scsibios(arg)
	void *arg;
{
	int *vp = arg;

	printf("scsibus id(%d):", *vp);
	getnum(vp);

	return 0;
}
#endif	/* NSD_SCSIBUS > 0 */

static int
config_exit(arg)
	void *arg;
{

	return 1;
}

static int
config_help(arg)
	void *arg;
{
	struct config_device_call *ccp = arg;

	for ( ; ccp->cc_name != NULL; ccp ++)
	{
		printf("%s\t -- %s\n", ccp->cc_name, ccp->cc_helpmsg);
	}
	return 0;
}

/************************************************
 * general subdev operations
 ************************************************/
struct config_device_call config_device_call[] = {
	{"help",
	  config_help, &config_device_call[0],
	  "help"},

	{"interrupt",
	  config_interrupt, &systm_intr_routing,
	  "change interrupt routing"},

	{"disk",
	  config_geometry, &cfd_data[0],
	  "change disk geometries"},

	{"thread",
	  config_threads, &systm_kthread_dispatch_table[0],
	 "enable/disable threads dispatch"},

	{"isa",
	  config_isa_device, NULL,
	 "change isa device resources"},

#if	NSD_SCSIBUS > 0
	{"scsibios",
	  config_scsibios, &scsibios_scsibus_id,
	 "change a scsibus number connected with bios"},
#endif	/* NSD_SCSIBUS > 0 */

#if	NPCIIDE > 0
	{"pciide",
	  config_pciide, &pciide_dma_block,
	 "enable/disable dma transfers"},
#endif	/* NPCIIDE > 0 */

#if	DDB > 0
	{"debug",
	  config_ddb, NULL,
	 "enter into debugger mode"},
#endif	/* DDB */

	{"exit",
	  config_exit, NULL,
	 "exit"},

	{NULL}
};

static void
config_device_main(void)
{
	struct config_device_call *ccp;
	u_char name[NAMEBUF_LEN];

	printf("Devices reconfiguration mode:\n");

	while(1)
	{
		printf(">> cmd (or help):");
		if (gets(name, NAMEBUF_LEN) == 0)
			continue;

		for (ccp = &config_device_call[0]; ccp->cc_name != 0; ccp ++)
		{
			if (strcmp(name, ccp->cc_name) == 0)
				break;
		}

		if (ccp->cc_name != NULL)
		{
			if (((*ccp->cc_func) (ccp->cc_arg)) == 0)
				continue;
			break;
		}
	}
}

/************************************************
 * config bootflags
 ************************************************/
void config_bootflags __P((void));

void
config_bootflags()
{
	struct bootflags_btinfo_header *bbhp;
	struct bootflags_device *obdp, *bdp;
	struct cfdata *cf;
	struct cfdriver *dev;
	struct config_data *cdp;
	int slot, nent, target;

	bbhp = lookup_bootinfo(BTINFO_BOOTFLAGS);
	if (bbhp == NULL)
		return;
	if (bbhp->bb_bi.bi_signature != BOOTFLAGS_SIGNATURE)
		return;
#if	0
	if ((bbhp->bb_bootflags & BOOTFLAGS_FLAGS_DEVICES) == 0)
		return;
#endif

	nent = bbhp->bb_bi.bi_nentry;
	if (nent >= BOOTFLAGS_BTINFO_MAXENTRY)
		nent = BOOTFLAGS_BTINFO_MAXENTRY;
	if (nent == 0)
		return;

	bdp = malloc(nent * sizeof(*bdp), M_TEMP, M_NOWAIT);	
	if (bdp == NULL)
		return;

	obdp = bdp;
	bcopy((((u_int8_t *) bbhp) + sizeof(*bbhp)), bdp,
	      sizeof(*bdp) * nent);
	
	for (target = 0; target < nent; target ++, bdp ++)
	{
		cdp = cds_isa;

		bdp->bd_name[15] = 0;
		cf = magic_find_dev("isa", bdp->bd_name);
		if (cf == NULL)
			continue;

		if (magic_is_isadev(cf) == 0)
			continue;

		cf = select_devices("isa", cf, cdp, bdp);
		if (cf == NULL)
			continue;

		dev = cf->cf_driver;
		printf("config_devices: %s[%ld] configuration overwrite\n",
			dev->cd_name, bdp->bd_loc[0]);
		printf("%s[%ld]: ", dev->cd_name, bdp->bd_loc[0]);
		if (bdp->bd_flags != -1)
		{
			printf("flags 0x%x->0x%lx ",
				cf->cf_flags, bdp->bd_flags);
			cf->cf_flags = bdp->bd_flags;
		}

		for (; cdp->cd_name != NULL; cdp ++)
		{
			slot = cdp->cd_bdloc;
			if (bdp->bd_loc[slot] == -1)
				continue;
			printf("%s 0x%x->0x%lx ",
				cdp->cd_name,
				cf->cf_loc[cdp->cd_pos], 
				bdp->bd_loc[slot]);
			cf->cf_loc[cdp->cd_pos] = bdp->bd_loc[slot];
		}
		printf("\n");
	}

	free(obdp, M_TEMP);
}
/************************************************
 * NetBSD PROBE ATTACH
 ************************************************/
#define	DELAY_COUNT	200000

#if	CONFIG_DEVICES == 0
#undef	CONFIG_DEVICES
#define	CONFIG_DEVICES	2
#endif	/* CONFIG_DEVICES == 0 */

void
config_devices(void)
{
	u_int mode;
	int waits;
	u_char c;

	/* overwrite data */
	config_bootflags();

	/* setup wait count */
	waits = (CONFIG_DEVICES * 2);
	printf("config_devices: hit space key if you want, wait(%d sec).\n", waits);
	mode = lookup_sysinfo(SYSINFO_CONSMODE);

	waits *= 1000000;
	while (waits > 0)
	{
		delay(DELAY_COUNT);
		waits -= DELAY_COUNT;

		set_sysinfo(SYSINFO_CONSMODE, mode | CONSM_NDELAY);
		c = (cngetc() & 0x7f);
		set_sysinfo(SYSINFO_CONSMODE, mode);

		if (c == ' ')
		{
			config_device_main();
			break;
		}
	}

	set_sysinfo(SYSINFO_CONSMODE, mode | CONSM_NDELAY);
	(void) cngetc();
	set_sysinfo(SYSINFO_CONSMODE, mode);
}
#endif	/* CONFIG_DEVICES */
