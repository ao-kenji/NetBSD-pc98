/*	$NecBSD: ippctrl.c,v 1.19 1998/03/14 07:11:10 kmatsuda Exp $	*/
/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997 NetBSD/pc98 porting staff.
 *  All rights reserved.
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
 * Copyright (c) 1996, 1997 Naofumi HONDA.  All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/signal.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/proc.h>

#include <i386/Cbus/pnp/pnpreg.h>
#include <i386/Cbus/pnp/ippiio.h>

#include "keycap.h"

/*******************************************************
 * MACRO
 *******************************************************/
#define CAPBUFSZ 1024
#define	MAX_CONFFILE_SIZE 0x10000

/*******************************************************
 * STRUCTURE
 *******************************************************/
struct prog_args {
	u_char insert[FILENAME_MAX + 1];
	u_char remove[FILENAME_MAX + 1];
};

void usage __P((void));
int load_ipp_conf __P((void));
int do_prog __P((u_char *, u_char *));
int setup_ipp __P((struct ippidev_connect_args *, struct prog_args *));
void ippi_initres __P((ippi_res_t));
void ippi_print __P((u_int8_t *data, struct ippi_bid *));
void ippinfo_print __P((struct ippi_bid *, struct ippi_ipid *, ippres_res_t));
int card_attach __P((int, struct ippi_ipid *, struct ippi_bid *));
int card_remove __P((int, struct ippi_ipid *, struct ippi_bid *));
int scan_all_logical_devices __P((int));
void ippi_print_dr __P((ippi_res_t));
void show_status __P((int));
void generate_conf __P((int, struct ippi_ipp_info *));

u_char *Config_filename = "/etc/ipp.conf";
u_char *ConfBuf;
u_int	ConfBufSize;
int	Loadok;
#define	GENCONF		0x01
#define	ALLCONF		0x02
#define	SHOWSTAT	0x04
u_int	Gflags;

/*******************************************************
 * MISC
 *******************************************************/
void
usage(void)
{

	printf("usage: ippctrl csn? ldn? [up|down]\n");
}

#define	VALSET(NAME, REF) \
	{register int tmp = kgetnum(NAME); if (tmp != -1) (REF) = tmp;}

u_char *
getstring(name, ref)
	u_char *name, *ref;
{
	char *pos, **str;

	*ref = 0;
	pos = ref;
	str = &pos;

	return kgetstr(name, str);
}

/*******************************************************
 * setup ipp info
 *******************************************************/
void
ippi_initres(dr)
	ippi_res_t dr;
{
	int i;

	bzero(dr, sizeof(*dr));

	dr->dr_ndrq = 2;
	for (i = 0; i < SLOT_DEVICE_NDRQ; i ++)
		dr->dr_drq[i].dc_chan = SLOT_DEVICE_UNKVAL;

	dr->dr_npin = 2;
	for (i = 0; i < SLOT_DEVICE_NPIN; i++)
	{
		dr->dr_pin[i].dc_flags = IRTR_HIGH;
		dr->dr_pin[i].dc_chan = SLOT_DEVICE_UNKVAL;
	}

	dr->dr_nio = SLOT_DEVICE_NIO;
	dr->dr_nmem = SLOT_DEVICE_NMEM;
	dr->dr_nim = dr->dr_nio + dr->dr_nmem;
	for (i = 0; i < SLOT_DEVICE_NIM; i++)
	{
		dr->dr_im[i].im_hwbase = SLOT_DEVICE_UNKVAL;
		dr->dr_im[i].im_size = SLOT_DEVICE_UNKVAL;
		if (i >= SLOT_DEVICE_NIO)
		{
			dr->dr_im[i].im_flags = MCR_BUS16;
			dr->dr_im[i].im_type = SLOT_DEVICE_SPMEM;
		}
		else
		{
			dr->dr_im[i].im_flags = 0;
			dr->dr_im[i].im_type = SLOT_DEVICE_SPIO;
		}
	}
}

#define	IPP_RES_LOAD(items, val) 		\
{						\
	u_long tmp = (val);			\
						\
	if (tmp != SLOT_DEVICE_UNKVAL)		\
		(items) = tmp;			\
}

int
setup_ipp(ica, pap)
	struct ippidev_connect_args *ica;
	struct prog_args *pap;
{
	ippi_res_t dr = &ica->ica_dr;
	u_int i, val;

	/* check target */
	if (getstring("dv", ica->ica_name) == NULL)
		return EINVAL;

	if ((val = kgetnum("id")) == -1 || val != ica->ica_bid.bid_id)
		return EINVAL;

	val = 0; VALSET("ldn", val);
	if (val != ica->ica_ipd.ipd_ldn)
		return EINVAL;

	/* init resource data */
	ica->ica_name[IPPI_DVNAMELEN - 1] = 0;

	/* dvcfg */
	val = 0; VALSET("dvcfg", val);
	dr->dr_dvcfg = val;

	/* irq and drq */
	IPP_RES_LOAD(dr->dr_pin[0].dc_chan, kgetnum("irq"));
	IPP_RES_LOAD(dr->dr_drq[0].dc_chan, kgetnum("drq"));

	/* io windows */
	for (i = 0; i < dr->dr_nio; i ++)
	{
		u_char name[10];

		sprintf(name, "io%d", i);
		IPP_RES_LOAD(dr->dr_io[i].im_hwbase, kgetnum(name));
		sprintf(name, "ios%d", i);
		IPP_RES_LOAD(dr->dr_io[i].im_size, kgetnum(name));
	}

	/* mem windows */
	for (i = 0; i < dr->dr_nio; i ++)
	{
		u_char name[10];

		sprintf(name, "mw%d",i);
		IPP_RES_LOAD(dr->dr_mem[i].im_hwbase, kgetnum(name));
		sprintf(name, "mwfg%d",i);
		IPP_RES_LOAD(dr->dr_mem[i].im_flags, kgetnum(name));
		sprintf(name, "mws%d",i);
		IPP_RES_LOAD(dr->dr_mem[i].im_size, kgetnum(name));
	}

	getstring("remove", pap->remove);
	getstring("insert", pap->insert);
	return 0;
}

int
do_prog(name, dvname)
	u_char *name, *dvname;
{
	u_char progs[FILENAME_MAX + 1];

	sprintf(progs, "%s %s", name, dvname);
	return system(progs);
}

/************************************************************
 * activate
 ************************************************************/
int
card_attach(fd, ipd, bid)
	int fd;
	struct ippi_ipid *ipd;
	struct ippi_bid *bid;
{
	struct ippidev_connect_args ica;
	struct ippi_ipp_info ipdata;
	struct prog_args pap;
	int ndev = 0;
	u_char buf[CAPBUFSZ];

	kinit();

	ica.ica_ipd = *ipd;
	ica.ica_bid = *bid;
	do
	{
		if (kgetentnext(buf) <= 0)
			break;

		/* load preferable settings */
		ippi_initres(&ipdata.ip_dr);
		ipdata.ip_ipd = *ipd;
		ipdata.ip_bid = *bid;
		if (ioctl(fd, IPPI_IOC_PREFER, &ipdata))
			continue;
#ifdef	IPPI_DEBUG
		ippi_print_dr(&ipdata.ip_dr);
#endif	/* IPPI_DEBUG */

		/* overwirte */
		ica.ica_dr = ipdata.ip_dr;
		if (setup_ipp(&ica, &pap))
			continue;

		if (ioctl(fd, IPPI_IOC_MAP, &ica))
		{
			perror("mapping");
			exit(1);
		}

		if (strcmp(ica.ica_name, "NODV") == 0)
			continue;

		printf("ippctrl: ld(%d:%d) tries to connect with %s ...\n",
			ica.ica_ipd.ipd_csn, ica.ica_ipd.ipd_ldn, ica.ica_name);
		sleep(1);

		if (ioctl(fd, IPPI_IOC_CONNECT_PISA, &ica))
		{
			perror("device probe failed");
			exit(1);
		}

		if (pap.insert[0])
			do_prog(pap.insert, ica.ica_name);

		printf("ippctrl: ld(%d:%d) connected with %s at pisa.\n",
			ica.ica_ipd.ipd_csn, ica.ica_ipd.ipd_ldn, ica.ica_name);
		ndev ++;
	}
	while (1);

	return ndev;
}

/************************************************************
 * load configuration file
 ************************************************************/
int
load_ipp_conf(void)
{
	int fd, error = EIO;
	struct stat file_stat;
	u_char *buf;
	u_int filesize, bufsize;

	fd = open(Config_filename, O_RDONLY, 0);
	if (fd < 0)
		return error;

	if (fstat(fd, &file_stat))
		goto out;

	filesize = (u_int) file_stat.st_size;
	if (filesize > MAX_CONFFILE_SIZE)
		goto out;

	bufsize = (filesize / 1024 + 2) * 1024;
	buf = malloc(bufsize);
	if (buf == NULL)
		goto out;

	bzero(buf, bufsize);
	lseek(fd, (off_t) 0, 0);
	if (read(fd, buf, filesize) != filesize)
	{
		free(buf);
		goto out;
	}

	if (ConfBuf)
		free(ConfBuf);

	ConfBuf = buf;
	ConfBufSize = filesize;
	error = 0;

out:
	close(fd);
	return error;
}

/************************************************************
 * deactivate
 ************************************************************/
int
card_detach(fd, ipd, bid)
	int fd;
	struct ippi_ipid *ipd;
	struct ippi_bid *bid;
{
	struct ippidev_connect_args ica;
	struct ippi_ctrl ctrl;
	struct prog_args pap;
	u_char buf[CAPBUFSZ];
	int error;

	ica.ica_bid = *bid;
	ica.ica_ipd = *ipd;
	while (Loadok == 0 && kgetentnext(buf) > 0)
	{
		if (setup_ipp(&ica, &pap) == 0 && pap.remove[0] != 0)
			do_prog(pap.remove, ica.ica_name);
	}

	ctrl.ic_ipd = *ipd;
	ctrl.ic_ctrl = IPPI_DEV_DEACTIVATE;
	error = ioctl(fd, IPPI_IOC_DEVICE, &ctrl);
	if (error)
	{
		perror("ippctrl");
		return 1;
	}
	return 0;
}

/************************************************************
 * status
 ************************************************************/
u_char *status_string[] = 
	{ "unknown", "found", "sleep", "mapped", "device connected" };
#define	STATUS_ARRAY_LEN (sizeof(status_string) / sizeof(u_char *))

void
sid2str(data, str)
	u_char *data, *str;
{

	sprintf(str, "%c%c%c%02x%02x", ((data[0] & 0x7c) >> 2) + 64,
		(((data[0] & 0x03) << 3) | ((data[1] & 0xe0) >> 5)) + 64,
		(data[1] & 0x1f) + 64, data[2], data[3]);
}	

void
ippi_print_dr(dr)
	ippi_res_t dr;
{
	int idx;

	for (idx = 0; idx < dr->dr_nio; idx ++)
		printf("io[%d]: 0x%lx 0x%lx\n", idx, 
			dr->dr_io[idx].im_hwbase,
			dr->dr_io[idx].im_size);

	for (idx = 0; idx < dr->dr_npin; idx ++)
		printf("irq[%d]: 0x%lx 0x%lx\n", idx, 
			dr->dr_pin[idx].dc_chan, 
			dr->dr_pin[idx].dc_flags);

	for (idx = 0; idx < dr->dr_ndrq; idx ++)
		printf("drq[%d]: 0x%lx 0x%lx\n", idx, 
			dr->dr_drq[idx].dc_chan, 
			dr->dr_drq[idx].dc_flags);

	for (idx = 0; idx < dr->dr_nmem; idx ++)
		printf("mem[%d]: 0x%lx 0x%lx 0x%lx\n", idx,
			dr->dr_mem[idx].im_hwbase,
			dr->dr_mem[idx].im_size,
			dr->dr_mem[idx].im_flags);
}

void
ippinfo_print(bid, ipd, irr)
	struct ippi_bid *bid;
	struct ippi_ipid *ipd;
	ippres_res_t irr;
{
	int i;
	ippi_res_t dr = &irr->irr_dr;
	u_char vstr[16];
	u_char *s;

	printf("# -- allowable resource allocation ranges --\n");
	switch (irr->irr_pri)
	{
		case 0: s = "high"; break;
		case 1: s = "middle"; break;
		default:
		case 2: s = "low"; break;
	};
	printf("# priority(%s)\n", s);
	for (i = 0; i < dr->dr_nio; i++)
	{
		if (dr->dr_io[i].im_hwbase != SLOT_DEVICE_UNKVAL)
			printf("# io%d\t range 0x%x-0x%x(0x%x) size 0x%x\n", 
				i, dr->dr_io[i].im_hwbase,
				irr->irr_io[i].io_hi,
				irr->irr_io[i].io_al,
				dr->dr_io[i].im_size);
	}

	for (i = 0; i < dr->dr_nmem; i ++)
	{
		if (dr->dr_mem[i].im_hwbase != SLOT_DEVICE_UNKVAL)
			printf("# mem%d\t range 0x%x-0x%x(0x%x) size 0x%x\n",
				i, dr->dr_mem[i].im_hwbase,
				irr->irr_mem[i].mem_hi,
				irr->irr_mem[i].mem_al,
				dr->dr_mem[i].im_size);
	}
	printf("#\n");

	sid2str((u_char *) &bid->bid_id, vstr);
	printf("category-%s-sample:%s:\\\n", vstr, vstr);
	printf("\t:id#0x%x:ldn#0x%x:\\\n", bid->bid_id, ipd->ipd_ldn);
	if (dr->dr_drq[0].dc_chan != SLOT_DEVICE_UNKVAL)
		printf("\t:drq#%d:\\\n", dr->dr_drq[0].dc_chan);

	for (i = 0; i < dr->dr_nio; i++)
	{
		if (dr->dr_io[i].im_hwbase != SLOT_DEVICE_UNKVAL)
			printf("\t:io%d#0x%x:\\\n", i, dr->dr_io[i].im_hwbase);
	}

	for (i = 0; i < dr->dr_nmem; i ++)
	{
		if (dr->dr_mem[i].im_hwbase != SLOT_DEVICE_UNKVAL)
			printf("\t:mw%d#0x%x:mwfg%d#0x%x:mws%d#0x%x:\\\n",
				i, dr->dr_mem[i].im_hwbase,
				i, dr->dr_mem[i].im_flags,
				i, dr->dr_mem[i].im_size);
	}

	printf("\t:dv=???:dvcfg#???:\n\n");
}

void
ippi_print(data, bid)
	u_char *data;
	struct ippi_bid *bid;
{
	u_char vstr[16];

	sid2str(data, vstr);
	printf("%s(0x%08x) serial:0x%08x\n", vstr, bid->bid_id,
		bid->bid_serial);
}

void
show_status(fd)
	int fd;
{
	struct ippi_ipp_info rdata;
	struct ippi_ipid *ipd = &rdata.ip_ipd;
	u_char *sstr;

	ipd->ipd_csn = 1;
	for ( ; ipd->ipd_csn < 1 + MAX_CARDS; ipd->ipd_csn ++)
	{
		ipd->ipd_ldn = 0;
		for ( ; ipd->ipd_ldn < MAX_LDNS; ipd->ipd_ldn ++)
		{
			if (ioctl(fd, IPPI_IOG_RD, &rdata))
				continue;
			if (ipd->ipd_ldn > 0 && rdata.ip_state <= IPP_FOUND)
				continue;

			printf("ld(%d:%d):", ipd->ipd_csn, ipd->ipd_ldn);
			ippi_print(rdata.ip_cis, &rdata.ip_bid);
			sstr = status_string[rdata.ip_state];
			if (rdata.ip_state != IPP_DEVALLOC)
				strcpy(rdata.ip_dvname, "none");
			printf("\tstat(%s) dev(%s)\n", sstr, rdata.ip_dvname);
		}
	}
}

void
generate_conf(fd, ip)
	int fd;
	struct ippi_ipp_info *ip;
{
	struct ippres_resources irr, birr;
	struct ippres_request ir;
	ippi_res_t dr = &irr.irr_dr;
	ippi_res_t bdr = &birr.irr_dr;
	u_long msize[SLOT_DEVICE_NMEM];
	int i;

#ifdef	IPPCTRL_PREFER_DEBUG
	{
		struct ippi_ipp_info ipdata;
		ipdata = *ip;
		ippi_initres(&ipdata.ip_dr);
		if (ioctl(fd, IPPI_IOC_PREFER, &ipdata) == 0)
		{
			printf("#<PREFER SETTING>\n");
			ippi_print_dr(&ipdata.ip_dr);
		}
	}
#endif	/* IPPCTRL_PREFER_DEBUG */

	ir.ir_ldn = ip->ip_ipd.ipd_ldn;
	ir.ir_dfn = 0;
	ippi_initres(dr);
	ippi_initres(bdr);

	ippres_parse_items(ip->ip_cis, &ir, &irr);

	for (i = 0; i < SLOT_DEVICE_NMEM; i ++)		/* XXX */
		msize[i] = dr->dr_mem[i].im_size;
	birr = irr;
	*bdr = ip->ip_dr;
	for (i = 0; i < SLOT_DEVICE_NMEM; i ++)		/* XXX */
		bdr->dr_mem[i].im_size = msize[i];

	printf("#<CURRENT KERNEL SETTING>\n");
	ippinfo_print(&ip->ip_bid, &ip->ip_ipd, &birr);

	if (ir.ir_ndfn == 0)
	{
		/* ippi_print_dr(dr); */		
		ippinfo_print(&ip->ip_bid, &ip->ip_ipd, &irr);
	}
	else for (ir.ir_dfn = 0; ir.ir_dfn < ir.ir_ndfn; ir.ir_dfn ++)
	{
		printf("#<SELECTION NO %d>\n", ir.ir_dfn);
		ippi_initres(dr);
		ippres_parse_items(ip->ip_cis, &ir, &irr);
		/* ippi_print_dr(&tdr);	*/	 
		ippinfo_print(&ip->ip_bid, &ip->ip_ipd, &irr);
	}
}

/************************************************************
 * main
 ************************************************************/
int
scan_all_logical_devices(fd)
	int fd;
{
	struct ippi_ipp_info rdata;
	struct ippi_ipid *ipd = &rdata.ip_ipd;

	ipd->ipd_csn = 1;
	for ( ; ipd->ipd_csn < 1 + MAX_CARDS; ipd->ipd_csn ++)
	{
		ipd->ipd_ldn = 0;
		for ( ; ipd->ipd_ldn < MAX_LDNS; ipd->ipd_ldn ++)
		{

			if (ioctl(fd, IPPI_IOG_RD, &rdata))
				continue;
			if (rdata.ip_state != IPP_FOUND)
				continue;
			card_attach(fd, ipd, &rdata.ip_bid);
		}
	}
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int num, ch, fd, error;
	u_int bid, csn, ldn;
	u_char *dvname = "/dev/ippi0";
	struct ippi_ipp_info rdata;
	u_char argb[1024];
	extern char *optarg;
	extern int optind;
	extern int optopt;
	extern int opterr;

	while ((ch = getopt(argc, argv, "ascdf:")) != -1)
	{
		switch (ch)
		{
		case 'f':
			Config_filename = optarg;
			break;

		case 'c':
			Gflags |= GENCONF;
			break;

		case 'a':
			Gflags |= ALLCONF;
			break;

		case 's':
			Gflags |= SHOWSTAT;
			break;

		case 'h':
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	fd = open(dvname, O_RDWR, 0);
	if (fd < 0)
	{
		perror(dvname);
		exit(0);
	}

	if (Gflags & ALLCONF)
	{
		Loadok = load_ipp_conf();
		scan_all_logical_devices(fd);
		exit(0);
	}

	if (argc == 0)
	{
		show_status(fd);
		exit(0);
	}
	else if (argc != 2 && argc != 3)
	{
		usage();
		exit(0);
	}

	if (strncmp(argv[0], "csn", 3) != 0)
	{
		usage();
		exit(1);
	}
	csn = atoi(&(argv[0][3]));

	if (strncmp(argv[1], "ldn", 3) != 0)
	{
		usage();
		exit(1);
	}
	ldn = atoi(&(argv[1][3]));

	rdata.ip_ipd.ipd_csn = csn;
	rdata.ip_ipd.ipd_ldn = ldn;
	if (ioctl(fd, IPPI_IOG_RD, &rdata))
	{
		perror("ippctrl");
		exit(0);
	}

	if (Gflags & GENCONF)
	{
		generate_conf(fd, &rdata);
		return 0;
	}
		
	if (argc == 2)
	{
		if (Gflags & SHOWSTAT)
			ippi_print_dr(&rdata.ip_dr);		
		else
			usage();
		exit(0);
	}

	Loadok = load_ipp_conf();
	if (strcmp(argv[2], "down") == 0)
	{
		return card_detach(fd, &rdata.ip_ipd, &rdata.ip_bid);

	}
	else if (strcmp(argv[2], "up") == 0)
	{
		if (Loadok != 0)
		{
			printf("initialization failed\n");
			exit(1);
		}
		card_attach(fd, &rdata.ip_ipd, &rdata.ip_bid);
		return 0;
	}

	usage();
	exit(1);
}
