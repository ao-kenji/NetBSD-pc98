/*	$NecBSD: pcsd.c,v 1.41.4.3 1999/08/24 20:32:18 honda Exp $	*/
/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996 NetBSD/pc98 porting staff.
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
 * Copyright (c) 1995, 1996 Naofumi HONDA.  All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <i386/pccs/tuple.h>
#include <i386/pccs/pccsio.h>
#include <i386/pccs/pccsvar.h>

#include "keycap.h"

#define	COMPAT_OLD	/* XXX */

/*******************************************************
 * MACRO
 *******************************************************/
#define	MAX_CONFFILE_SIZE 0x10000
#define CAPBUFSZ 1024
#define	MAX_SLOTS 4

#define	PCSD_MAX_NPRI	3
#define	PCSD_PRI_HIGH	0
#define	PCSD_PRI_MIDDLE	1
#define	PCSD_PRI_LOW	2

#define	_PATH_LOGPID	"/var/run/pcsd.pid"
char *PidFile = _PATH_LOGPID;

/*******************************************************
 * STRUCTURE
 *******************************************************/
struct callprog {
	struct callprog *next;
	u_char *prog;
	int mfcid;
};

struct pcsd_data {
	u_char device_name[FILENAME_MAX + 1];	/* pccs device name */
	int fd;					/* open descriptor */

	struct slot_info si;			/* current slot status */
	slot_status_t pstat;			/* target status */

	struct card_info cdata[PCCS_MAXMFC];	/* card informations */
	struct card_info bdata;			/* base card infomation */
	int fnio[PCCS_MAXMFC];			/* function io base */
	int fnmem[PCCS_MAXMFC];			/* function mem base */
	int ndevs;				/* num devices per a card */
	int common;				/* nmfc */
	u_int8_t index;				/* common index */

	struct callprog *remove;		/* progs for remove actions */
	struct callprog *insert;		/* progs for insert actions */
};

struct pcsd_data *pcsdp[MAX_SLOTS];		/* pcsd data */
u_char *debug_cons;				/* debug */
u_char *pccs_conf_path;
u_char *ConfBuf;
u_int ConfBufSize;
FILE *logfp;

struct entry_buffer;
void usage __P((void));
void pcsd_start __P((int));
int load_pccs_conf __P((void));
int start_daemon __P((int, u_char **));
int open_card __P((struct pcsd_data *));
int attach_devices __P((struct pcsd_data *, struct entry_buffer *, int));
int scan_card_section __P((struct pcsd_data *, struct entry_buffer *, int));
int scan_binding_section __P((struct pcsd_data *, struct entry_buffer *, int));
void pcsd_exec_callentry __P((struct callprog *));
void pcsd_free_callentry __P((struct callprog **));
void pcsd_add_callentry __P((struct callprog **, u_char *, struct pcsd_data *, struct pcdv_attach_args *));
u_char *getstring __P((u_char *, u_char *));
void debug_log __P((struct pcsd_data *, u_char *, int));
int pcsd_setup_info __P((int));
int check_slot __P((int, int));
void pcsd_exit __P(());
void pcsd_hungup __P(());
void pcsd_main __P(());
void pcsd_reprobe __P(());
void pcsd_signal_block __P((void));
void pcsd_signal_unblock __P((void));
int pcsd_parse_cis __P((struct pcsd_data *, struct card_info *, u_char *, int, u_int));
int pcsd_load_class __P((struct card_info *));
int pcsd_load_iomem __P((struct card_info *));

#define	ENTRY_NAMELEN	128

struct entry_buffer {
	struct entry_buffer *eb_next;
	u_char *eb_data;

	int eb_pri;
	u_long eb_id;
	u_long eb_idmask;
	u_char eb_name[ENTRY_NAMELEN];
	struct entry_buffer *eb_follow[PCCS_MAXMFC];
	u_char eb_product[PRODUCT_NAMELEN];
};

struct entry_buffer *card_section_p;
struct entry_buffer *bind_section_p;

/*******************************************************
 * DEBUG
 *******************************************************/
void
debug_log(pdp, str, flag)
	struct pcsd_data *pdp;
	u_char *str;
	int flag;
{
	time_t clock;
	int len;
	u_char *dvname, *tz;
	u_char dst[80];

	if (debug_cons == NULL)
		return;

	if (pdp)
		dvname = pdp->device_name;
	else
		dvname = "all";

	if (flag)
	{
		time(&clock);
		tz = ctime(&clock);
		len = strlen(tz);
		bcopy(tz, dst, len - 1);
		dst[len - 1] = 0;
		printf("\n%s: pcsd(%s) %s\n", dst, dvname, str);
	}
	else
		printf("%s\n", str);

	fflush(stdout);
}

/*******************************************************
 * MISC
 *******************************************************/
#define	FLAGSET(name, ref, val)				\
	{						\
		if (kgetflag((name))) 			\
			(ref) |= (val);			\
	 }

#define	VALSET(name, ref) 				\
	{ 						\
		register int tmp = kgetnum((name)); 	\
							\
		if (tmp != -1) 				\
			(ref) = tmp;			\
	}

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

/************************************************************
 * programs call interface
 ************************************************************/
void
pcsd_add_callentry(cpp, progname, pdp, ca)
	struct callprog **cpp;
	u_char *progname;
	struct pcsd_data *pdp;
	struct pcdv_attach_args *ca;
{
	struct callprog *cp;
	u_char *bp;
	int len, mfcid;

	cp = (struct callprog *) malloc(sizeof(*cp));
	if (cp == NULL)
		return;

	mfcid = PCCS_GET_MFCID(&ca->ca_ci);
	len = strlen(progname) + 1;
	len += strlen(ca->ca_name) + 1;
	len += strlen(pdp->cdata[mfcid].ci_product) + 1;
	bp = (u_char *) malloc(len);
	if (bp == NULL)
		return;

	cp->next = NULL;
	cp->prog = bp;
	cp->mfcid = mfcid;
	sprintf(bp, "%s %s '%s'", progname, ca->ca_name,
		pdp->cdata[mfcid].ci_product);

	if (*cpp)
		(*cpp)->next = cp;
	else
		*cpp = cp;
}

void
pcsd_free_callentry(cpp)
	struct callprog **cpp;
{
	struct callprog *ncp, *cp;

	for (cp = *cpp; cp; cp = ncp)
	{
		ncp = cp->next;
		free(cp->prog);
		free(cp);
	}

	*cpp = NULL;
}

void
pcsd_exec_callentry(cp)
	struct callprog *cp;
{
	int error;

	for ( ; cp; cp = cp->next)
	{
		if (debug_cons)
			printf("call prog: %s\n", cp->prog);

		error = system(cp->prog);
		if (error)
			syslog(LOG_WARNING, "exec failed  prog:%s\n", cp->prog);
	}
}

/*******************************************************
 * setup card configurations
 *******************************************************/
#define IFSTR "cr: offset 0x%x lastc 0x%x mask 0x%x ccor 0x%x ccmor 0x%x ccsr 0x%x\n"

void
pcsd_show_ci(ci)
	struct card_info *ci;	
{
	struct card_registers *cr;

	cr = &ci->ci_cr;
	printf(IFSTR, cr->cr_offset, cr->cr_lastidx, cr->cr_mask, 
	       cr->cr_ccor, cr->cr_ccmor, cr->cr_ccsr);
	printf("io decode: lines %d address mask 0x%x\n", ci->ci_iobits,
		(1 << ci->ci_iobits) - 1);
	show_card_info(ci);
	fflush(stdout);
}

int
pcsd_load_class(ci)
	struct card_info *ci;
{
	u_char class_name[FILENAME_MAX + 1];
	int clsv, i;
	u_char *class[] = {"multi", "memory", "serial", "para", "fixed",
			   "video", "net", "aims", "scsi", NULL};

	clsv = kgetnum("class");
	if (clsv != -1)
	{
		ci->ci_function = clsv;
		return 0;
	}

	if (getstring("class", class_name) == NULL)
		return ENOENT;
	
	for (i = 0; class_name[i] != 0; i ++)
		class_name[i] = tolower(class_name[i]);

	for (clsv = 0; class[clsv] != 0; clsv ++ )
		if (strcmp(class[clsv], class_name) == 0)
			break;
	if (class[clsv] == NULL)
		return ENOENT;

	ci->ci_function = clsv;
	return 0;
}

int
pcsd_load_iomem(ci)
	struct card_info *ci;
{
	struct card_registers *cr = &ci->ci_cr;
	slot_device_res_t dr = &ci->ci_res;
	struct slot_device_iomem *wp;
	int index, i;

	if ((index = kgetnum("index")) != -1)
	{
		cr->cr_ccor = (u_int8_t) index;
		cr->cr_ccor &= INDEX_MASK;
	}

	VALSET("cr_ccmor", cr->cr_ccmor)
	VALSET("cr_offset", cr->cr_offset)
	VALSET("cr_ccsr", cr->cr_ccsr)
	VALSET("cr_scr", cr->cr_scr)
	VALSET("cr_mask", cr->cr_mask)

	/* io windows */
	wp = &dr->dr_io[0];
	for (i = 0; i < MAX_IOWF; i ++, wp ++)
	{
		u_char name[128];

		sprintf(name, "io%d_ds16",i);
		FLAGSET(name, wp->im_flags, SDIM_BUS_WIDTH16)
		sprintf(name, "io%d_cs16",i);
		FLAGSET(name, wp->im_flags, SDIM_BUS_AUTO)
		sprintf(name, "io%d_ws",i);
		FLAGSET(name, wp->im_flags, SDIM_BUS_WEIGHT)

		sprintf(name, "io%d",i);
		VALSET(name, wp->im_hwbase);
		wp->im_base = wp->im_hwbase;
		sprintf(name, "ios%d",i);
		VALSET(name, wp->im_size);
		wp->im_type = SLOT_DEVICE_SPIO;
		if (wp->im_hwbase != PUNKADDR || wp->im_size != PUNKSZ)
			wp->im_flags |= SDIM_BUS_ACTIVE;
	}

	/* (V) mem windows */
	wp = &dr->dr_mem[0];
	for (i = 0; i < MAX_MEMWF; i ++, wp ++)
	{
		u_char name[128];

		sprintf(name, "mw%d_ds16",i);
		FLAGSET(name, wp->im_flags, SDIM_BUS_WIDTH16)
		sprintf(name, "mw%d_ws0",i);
		FLAGSET(name, wp->im_flags, SDIM_BUS_WEIGHT)
		sprintf(name, "mw%d_ws1",i);
		FLAGSET(name, wp->im_flags, SDIM_BUS_WEIGHT)
		sprintf(name, "mw%d_attr",i);
		FLAGSET(name, wp->im_flags, WFMEM_ATTR)

		sprintf(name, "mw%d",i);
		VALSET(name, wp->im_hwbase);
		sprintf(name, "mwo%d",i);
		VALSET(name, wp->im_base);
		sprintf(name, "mws%d",i);
		VALSET(name, wp->im_size);
		wp->im_type = SLOT_DEVICE_SPMEM;
		if (wp->im_hwbase != PUNKADDR || wp->im_size != PUNKSZ ||
		    wp->im_base != PUNKADDR)
			wp->im_flags |= SDIM_BUS_ACTIVE;
	}

	return 0;
}

int
scan_card_section(pdp, p, pass)
	struct pcsd_data *pdp;
	struct entry_buffer *p;
	int pass;
{
	struct card_info *ci = &pdp->bdata;
	slot_device_res_t dr = &ci->ci_res;
	struct slot_device_channel *dc = &dr->dr_pin[0];
	struct slot_device_channel *dmadc = &dr->dr_drq[0];
	u_long mask;
	int i;

	pccs_init_ci(ci);
	strcpy(ci->ci_product, p->eb_product);
	mask = p->eb_idmask;
	ci->ci_manfid = p->eb_id;

	/* check check check */
	if (pdp->si.si_ci.ci_product[0] == 0)
		return EINVAL;	

	switch (pass)
	{
	case PCSD_PRI_HIGH:
		if (ci->ci_product[0] != 0 && strncmp(ci->ci_product, 
		    pdp->si.si_ci.ci_product, strlen(ci->ci_product)) != 0)
			return EINVAL;
		if (ci->ci_manfid != 0 &&
		    ci->ci_manfid != pdp->si.si_ci.ci_manfid)
			return EINVAL;
		strcpy(ci->ci_product, pdp->si.si_ci.ci_product);
		break;

	case PCSD_PRI_MIDDLE:
		if ((ci->ci_manfid & mask) != (pdp->si.si_ci.ci_manfid & mask))
			return EINVAL;
		strcpy(ci->ci_product, pdp->si.si_ci.ci_product);
		break;

	case PCSD_PRI_LOW:
		break;
	}

	/* Interrupt pin */
	FLAGSET("intrlevel", dc->dc_flags, INTR_EDGE)
	dc->dc_chan = kgetnum("irq");
	if (dc->dc_chan == PUNKIRQ)
		dc->dc_chan = PAUTOIRQ;

	/* DMA channel */
	dmadc->dc_chan = kgetnum("drq");

	if ((ci->ci_delay = kgetnum("wait")) == -1)
		ci->ci_delay = 0;

	/* load base data (global) */
	pcsd_load_iomem(ci);

	/* load class data (global) */
	ci->ci_function = -1;
	pcsd_load_class(ci);

	if (debug_cons)
	{
		printf(">>> Initial window settings as follows:\n");
		pcsd_show_ci(ci);
	}
	return 0;
}

/************************************************************
 * open MFC card and devices
 ************************************************************/
int
scan_binding_section(pdp, q, pass)
	struct pcsd_data *pdp;
	struct entry_buffer *q;
	int pass;
{
	struct entry_buffer *p;
	int len;

	pdp->ndevs = pdp->common = 0;
	for (len = 0; (p = q->eb_follow[len]) != NULL && len < PCCS_MAXMFC;
	     len ++)
	{
		if (debug_cons)
			printf("pass %d: %s\n", pass, p->eb_data);

		ksetup(p->eb_data);
		pdp->ndevs += attach_devices(pdp, p, pass);
	}

	debug_log(pdp, ">>> ALL DONE", 0);
	return pdp->ndevs;
}

int
attach_devices(pdp, p, pass)
	struct pcsd_data *pdp;
	struct entry_buffer *p;
	int pass;
{
	int fd = pdp->fd;
	struct pcdv_attach_args cdev;
	struct card_info *ci;
	struct card_registers *cr;
	struct slot_device_iomem *wp;
	slot_device_res_t dr;
	int dvcfg, mfcid, attach = 0;
	u_int disable = 0;

	u_char devname[FILENAME_MAX + 1];
	u_char modpath[FILENAME_MAX + 1];
	u_char insert[FILENAME_MAX + 1];
	u_char remove[FILENAME_MAX + 1];
	u_char name[256];

	/*******************************************
	 * GET BINDING SECTION'S INFO
	 *******************************************/
	bzero(&cdev, sizeof(cdev));
	mfcid = 0;
	VALSET("fn", mfcid);
	if (mfcid >= PCCS_MAXMFC || mfcid < 0)
		return attach;

	ci = &pdp->cdata[mfcid];
	cr = &ci->ci_cr;
	
	/* load base data */
	*ci = pdp->bdata;
	PCCS_SET_MFCID(ci, mfcid);

	pcsd_load_iomem(ci);
	pcsd_load_class(ci);

	/* get base window (io & mem) */
	pdp->fnio[mfcid] = -1;
	sprintf(name, "iobw");
	VALSET(name, pdp->fnio[mfcid]);	

	pdp->fnmem[mfcid] = -1;
	sprintf(name, "membw");
	VALSET(name, pdp->fnmem[mfcid]);	

	if (pdp->fnmem[mfcid] >= 0 || pdp->fnio[mfcid] >= 0)
	{
		disable = -1;
		pdp->common = 1;
		if (pdp->fnmem[mfcid] >= 0)
			disable &= ~(1 << (pdp->fnmem[mfcid] + SLOT_DEVICE_NIO));
		if (pdp->fnio[mfcid] >= 0)
			disable &= ~(1 << (pdp->fnio[mfcid]));
	}

	getstring("dv", devname);

	if (pcsd_parse_cis(pdp, ci, devname, pass, disable) != 0)
		return attach;

	pdp->index = PCCS_GET_INDEX(ci);

	dr = &ci->ci_res;
	if (pdp->common > 0)
	{
		int start, idx, alt;

		start = 0;
		alt = pdp->fnio[mfcid];
		if (alt >= 0)
		{
			start ++;
			dr->dr_io[0] = dr->dr_io[alt];
		}
		for (idx = start; idx < SLOT_DEVICE_NIO; idx ++)
			pccs_init_iomem(&dr->dr_io[idx]);

		start = 0;
		alt = pdp->fnmem[mfcid];
		if (alt >= 0)
		{
			start ++;
			dr->dr_mem[0] = dr->dr_mem[alt];
		}
		for (idx = start; idx < SLOT_DEVICE_NMEM; idx ++)
			pccs_init_iomem(&dr->dr_mem[idx]);
	}

	if (debug_cons)
	{
		printf(">>> window settings as follows:\n");
		pcsd_show_ci(ci);
	}

	getstring("mod", modpath);
	getstring("insert", insert);
	getstring("remove", remove);
	
	/*******************************************
	 * SETUP IOCTL ARGS 
	 *******************************************/
	strncpy(cdev.ca_name, devname, PCCS_NAMELEN);
	dvcfg = 0;
	VALSET("dvcfg", dvcfg);
	dr->dr_dvcfg = dvcfg;
	cdev.ca_ci = *ci;

	if (debug_cons)
	{
		debug_log(pdp, ">>> Device settings as follows:", 0);
		printf("target device %s: dvcfg 0x%x\n",
			cdev.ca_name, dr->dr_dvcfg);
	}

	/*******************************************
	 * DEVICE PROBE
	 *******************************************/
	cdev.ca_version = PCCSIO_VERSION;
	if (ioctl(fd, PCCS_IOC_CONNECT_BUS, &cdev) != 0)
	{
		syslog(LOG_WARNING, "(%s) %s attach failed\n",
		       pdp->device_name, devname);
		debug_log(pdp, "PROBE FAILED!", 0);
		printf("function id: %d", PCCS_GET_MFCID(ci));
	}
	else
	{
		attach = 1;
		if (cdev.ca_unit >= 0)
		{
			syslog(LOG_NOTICE, "(%s) %s connected with %s\n",
			       pdp->device_name, cdev.ca_name, ci->ci_product);
			debug_log(pdp, "PROBE SUCCESS!", 0);
		}
		else
		{
			syslog(LOG_NOTICE, "(%s) %s connected alreay with %s\n",
			       pdp->device_name, cdev.ca_name, ci->ci_product);
			debug_log(pdp, "DEVICE ALREADY ALLOCATED!", 0);
		}

		if (insert[0] != 0)
			pcsd_add_callentry(&pdp->insert, insert, pdp, &cdev);
		if (remove[0] != 0)
			pcsd_add_callentry(&pdp->remove, remove, pdp, &cdev);
	}

	return attach;
}

/************************************************************
 * open pccs card 
 ************************************************************/
int
pcsd_parse_cis(pdp, kci, dv, pass, disable)
	struct pcsd_data *pdp;
	struct card_info *kci;
	u_char *dv;
	int pass;
	u_int disable;
{
	int fd, id;
	struct card_info ddata;
	struct card_info *ci = &ddata;
	struct card_prefer_config cpc;

	fd = pdp->fd;
	id = PCCS_GET_MFCID(kci);

	strncpy(cpc.cp_name, dv, PRODUCT_NAMELEN);
	pccs_init_ci(&cpc.cp_ci);
	PCCS_SET_MFCID(&cpc.cp_ci, id);
	if (pdp->ndevs > 0 && pdp->common > 0)
	{
		/* CASE A:
		 * 1) classical multifunction cards before PC Card Standard
		 * 2) illegal multifunction cards whose multifuctions
		 *    share the same cf entry.
		 */
		PCCS_SET_INDEX(kci, pdp->index);
		PCCS_SET_INDEX(&cpc.cp_ci, pdp->index);
		cpc.cp_pri = 0;
	}
	else 
	{
		/* CASE B:
		 * 1) single function cards
		 * 2) true multifunction cards
		 */
		PCCS_SET_INDEX(&cpc.cp_ci, PCCS_GET_INDEX(kci));
	}

	cpc.cp_disable = disable;
	if (ioctl(fd, PCCS_IOC_PREFER, &cpc) != 0)
	{
		debug_log(pdp, "CIS PREFER FAIL!", 0);
		goto bad;
	}

	*ci = cpc.cp_ci;
	PCCS_SET_INDEX(kci, PCCS_GET_INDEX(ci));
	if (pccscis_match_config(kci, ci) != 0)
		goto bad;

	ci->ci_res.dr_pin[0].dc_chan = PAUTOIRQ;
	pccscis_merge_config(kci, ci, (pass != PCSD_PRI_LOW));

	if (debug_cons)
	{
		debug_log(pdp, ">>> Merged settings as follows:", 0);
		pcsd_show_ci(ci);
	}

	*kci = *ci;
	return 0;

bad:
	debug_log(pdp, "CIS NO MATCH!", 0);
	printf("function id: %d\n", PCCS_GET_MFCID(kci));
	return ENOENT;
}

int
open_card(pdp)
	struct pcsd_data *pdp;
{
	struct entry_buffer *p;
	int pass, error = 0;

	pdp->si.si_mfcid = PUNKMFCID;
	if (ioctl(pdp->fd, PCCS_IOG_SSTAT, &pdp->si))
		return EIO;
	if (pdp->si.si_st < SLOT_READY && ioctl(pdp->fd, PCCS_IOC_INIT, 0))
		return EIO;

	for (pass = 0; pass < PCSD_MAX_NPRI; pass ++)
	{
		for (p = card_section_p; p != NULL; p = p->eb_next)
		{
			ksetup(p->eb_data);

			/*
			 * Check the priority of the target entry.
			 */
			if (p->eb_pri != pass)
				continue;

			/*
			 * Check the slot status.
			 */
			pdp->si.si_mfcid = PUNKMFCID;
			error = ioctl(pdp->fd, PCCS_IOG_SSTAT, &pdp->si);
			if (error != 0)
				break;

			if (pdp->si.si_st < SLOT_READY)
			{
				error = ioctl(pdp->fd, PCCS_IOC_INIT, 0);
				if (error != 0)
					break;
			}

			/*
			 * Go! Scan card sections.
			 */
			debug_log(pdp, ">>> SCAN CARD SECTIONS", 1);
			if (debug_cons)
			{
				printf("pass %d: %s\n", pass, p->eb_data);
				printf("pri  %d: %s, id 0x%lx, mask 0x%lx\n",
					p->eb_pri, p->eb_name, p->eb_id, 
					p->eb_idmask);
			}

			if (scan_card_section(pdp, p, pass) != 0)
				continue;

			/*
			 * Scan binding sections.
			 */
			debug_log(pdp, ">>> SCAN BINDING SECTIONS", 0);
			if (scan_binding_section(pdp, p, pass) != 0)
				return 0;
		}
	}

	syslog(LOG_WARNING, "(%s) no match\n", pdp->device_name);
	debug_log(pdp, "NO MATCH!", 0);

	return ENOENT;
}

/************************************************************
 * main loops
 ************************************************************/
int
check_slot(slotno, flags)
	int slotno;
	int flags;
{
	struct pcsd_data *pdp = pcsdp[slotno];
	struct slot_event sev;
	int fd, ctrl, error, force, event;

	if (pdp == NULL)
		return EIO;

	fd = pdp->fd;
	pdp->si.si_mfcid = PUNKMFCID;
	error = ioctl(fd, PCCS_IOG_SSTAT, &pdp->si);
	if (error != 0)
		return error;

	if ((pdp->si.si_cst & PCCS_SS_POW_UP) == 0)
	{
		error = EIO;
		pdp->si.si_st = SLOT_NULL;
	}

	event = PCCS_SEV_NULL;
	force = flags;
	while (ioctl(fd, PCCS_IOG_EVENT, &sev) == 0)
	{
		if (debug_cons != NULL)
		{
			printf("%s: code %d\n", pdp->device_name, sev.sev_code);
			fflush(stdout);
		}

		if (sev.sev_code == PCCS_SEV_NULL)
			break;
		switch (sev.sev_code)
		{
		case PCCS_SEV_MRESUME:
			pcsd_free_callentry(&pdp->remove);
			pcsd_free_callentry(&pdp->insert);
			event = PCCS_SEV_HWREMOVE;
			pdp->pstat = 0;
			force = 1;
			continue;

		case PCCS_SEV_HWREMOVE:
			if (pdp->pstat == SLOT_HASDEV)
				pcsd_exec_callentry(pdp->remove);
			pcsd_free_callentry(&pdp->remove);
			event = PCCS_SEV_HWREMOVE;
			pdp->pstat = 0;
			continue;

		case PCCS_SEV_HWINSERT:
			if (pdp->pstat < SLOT_HASDEV)
				event = PCCS_SEV_HWINSERT;
			continue;

		default:
			continue;
		}
	}

	if (force == 0 && event != PCCS_SEV_HWINSERT)
		return 0;

	debug_log(pdp, "WAKE UP", 1);

	pdp->si.si_mfcid = PUNKMFCID;
	error = ioctl(fd, PCCS_IOG_SSTAT, &pdp->si);
	if (error != 0)
		return error;

	if (pdp->si.si_st > SLOT_NULL)
	{
		if (open_card(pdp) == 0)
			pcsd_exec_callentry(pdp->insert);
		pcsd_free_callentry(&pdp->insert);
		pdp->si.si_mfcid = PUNKMFCID;
		ioctl(fd, PCCS_IOG_SSTAT, &pdp->si);
	}
	pdp->pstat = pdp->si.si_st;
	return error;
}

void
pcsd_start(flags)
	int flags;
{
	int i;

	for (i = 0; i < MAX_SLOTS; i++)
	{
		if (pcsdp[i] != NULL)
			check_slot(i, flags);
	}
}

void
pcsd_main()
{

	pcsd_signal_block();
	pcsd_start(0);
	sleep(1);
	pcsd_signal_unblock();
}

void
pcsd_reprobe()
{

	pcsd_signal_block();
	pcsd_start(1);
	sleep(1);
	pcsd_signal_unblock();
}

/************************************************************
 * load configuration file
 ************************************************************/
static int pcsd_alloc_config_entry __P((void));
static void pcsd_free_config_entry __P((struct entry_buffer **));

static void
pcsd_free_config_entry(op)
	struct entry_buffer **op;
{
	struct entry_buffer *p, *np;
	
	for (p = *op; p != NULL; p = np)
	{
		np = p->eb_next;
		if (p->eb_data != NULL)
			free(p->eb_data);
		free(p);
	}
	*op = NULL;
}

static int
pcsd_alloc_config_entry()
{
	u_char buf[CAPBUFSZ];
	u_char tbuf[CAPBUFSZ];
	u_char nbuf[CAPBUFSZ];
	struct entry_buffer *q, *p, **ocp, **obp;
	u_long id, idmask;
	int rv, len, type;

	ocp = &card_section_p;
	obp = &bind_section_p;

	kinit();
	while (1)
	{
		bzero(buf, sizeof(buf));
		rv = kgetentnext(buf);
		if (rv <= 0)
			break;

		ksetup(buf);
		buf[CAPBUFSZ - 1] = 0;

		id = 0;
		idmask = (u_long) -1;
		nbuf[0] = 0;
		if (getstring("cardname", tbuf) != NULL)
		{
			VALSET("id", id);
			VALSET("idmask", idmask);
			getstring("product", nbuf);
			type = 0;
		}
		else if (getstring("bind", tbuf) != NULL)
			type = 1;
		else
			continue;

		p = malloc(sizeof(*p));
		if (p == NULL)
			return ENOMEM;

		bzero(p, sizeof(*p));
		strncpy(p->eb_name, tbuf, ENTRY_NAMELEN);
		p->eb_name[ENTRY_NAMELEN - 1] = 0;
		strncpy(p->eb_product, nbuf, PRODUCT_NAMELEN);
		p->eb_product[PRODUCT_NAMELEN - 1] = 0;
		p->eb_id = id;
		p->eb_idmask = idmask;

		if (p->eb_product[0] != 0)
		{
			p->eb_pri = PCSD_PRI_HIGH;
		}
		else if (p->eb_id != 0)
		{
			if (p->eb_idmask == (u_long) -1)
				p->eb_pri = PCSD_PRI_HIGH;
			else
				p->eb_pri = PCSD_PRI_MIDDLE;
		}
		else
			p->eb_pri = PCSD_PRI_LOW;

		len = strlen(buf);
		p->eb_data = malloc(len + 1);
		if (p->eb_data == NULL)
		{
			free(p);
			return ENOMEM;
		}

		bcopy(buf, p->eb_data, len);
		p->eb_data[len] = 0;
		p->eb_next = NULL;

		if (type == 0)
		{
			*ocp = p;
			ocp = &p->eb_next;
		}
		else
		{
			*obp = p;
			obp = &p->eb_next;
		}
	}	

	if (rv < 0)
		return EINVAL;

	for (q = card_section_p; q != NULL; q = q->eb_next)
	{
		for (len = 0, p = bind_section_p;
		     p != NULL && len < PCCS_MAXMFC; p = p->eb_next)
		{
			if (strcmp(p->eb_name, q->eb_name) == 0)
			{
				q->eb_follow[len] = p;
				len ++;
			}
		}
	}
	return rv;
}

int
load_pccs_conf(void)
{
	int fd, error = EIO;
	struct stat file_stat;
	u_char *buf;
	u_int filesize, bufsize;

	fd = open(pccs_conf_path, O_RDONLY, 0);
	if (fd < 0)
		return error;

	if (fstat(fd, &file_stat) != 0)
		goto bad;

	filesize = (u_int) file_stat.st_size;
	if (filesize > MAX_CONFFILE_SIZE)
		goto bad;

	bufsize = (filesize / 1024 + 2) * 1024;
	buf = malloc(bufsize);
	if (buf == NULL)
		goto bad;

	bzero(buf, bufsize);
	lseek(fd, (off_t) 0, 0);
	if (read(fd, buf, filesize) != filesize)
	{
		free(buf);
		goto bad;
	}

	if (ConfBuf != NULL)
		free(ConfBuf);
	ConfBuf = buf;
	ConfBufSize = filesize;

	pcsd_free_config_entry(&card_section_p);
	pcsd_free_config_entry(&bind_section_p);

	error = pcsd_alloc_config_entry();
	if (error != 0)
		goto bad;

	close(fd);
	return error;

bad:
	close(fd);
	pcsd_free_config_entry(&card_section_p);
	pcsd_free_config_entry(&bind_section_p);
	if (ConfBuf != NULL)
		free(ConfBuf);
	ConfBuf = NULL;
	return error;
}

void
usage(void)
{

	printf("usage: pcsd [-f conf] -d\n");
	exit(1);
}

/************************************************************
 * main
 ************************************************************/
void
pcsd_hungup()
{

	pcsd_signal_block();
	if (load_pccs_conf())
		syslog(LOG_WARNING, "RELOAD PCCS CONFIGURATIONS FAILED");
	else
		syslog(LOG_WARNING, "RELOAD PCCS CONFIGURATION");
	pcsd_signal_unblock();
}

void
pcsd_exit()
{
	struct pcsd_data *pdp;
	int slotno;

	for (slotno = 0; slotno < MAX_SLOTS; slotno ++)
	{
		if ((pdp = pcsdp[slotno]) == NULL)
			continue;

		if (pdp->pstat == SLOT_HASDEV)
			pcsd_exec_callentry(pdp->remove);
	}

	debug_log(NULL, "SIGNAL TERMINATION", 1);
	exit(0);
}

int
pcsd_setup_info(slotno)
	int slotno;
{
	struct pcsd_data *pdp;

	pcsdp[slotno] = NULL;

	pdp = malloc(sizeof(*pdp));
	if (pdp == NULL)
		return ENOMEM;

	bzero(pdp, sizeof(*pdp));
	sprintf(pdp->device_name, "/dev/ata%d", slotno);

	pdp->fd = open(pdp->device_name, O_RDWR, 0);
	if (pdp->fd < 0)
	{
		free(pdp);
		return ENOTTY;
	}

	if (ioctl(pdp->fd, PCCS_IOC_CONNECT, 0))
	{
		close(pdp->fd);
		free(pdp);
		return EIO;
	}

	pcsdp[slotno] = pdp;
	return 0;
}

void
pcsd_signal_block()
{
	sigset_t sigs;

	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	sigaddset(&sigs, SIGUSR2);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGHUP);
	sigprocmask(SIG_BLOCK, &sigs, NULL);
}

void
pcsd_signal_unblock()
{
	sigset_t sigs;

	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	sigaddset(&sigs, SIGUSR2);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGHUP);
	sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int i, num, ch, fd, reprobe;
	FILE *fp;
	extern char *optarg;
	extern int optind;
	extern int optopt;
	extern int opterr;

	reprobe = 0;
	pccs_conf_path = PCCSCAP_PATH;

	while ((ch = getopt(argc, argv, "Fdf:")) != -1)
	{
		switch (ch)
		{
		case 'f':
			pccs_conf_path = optarg;
			break;

		case 'd':
			debug_cons = "/tmp/pcsd.log";
			break;

		case 'F':
			reprobe = 1;
			break;
			
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	fd = open(pccs_conf_path, O_RDONLY, 0);
	if (fd < 0)
	{
		perror(pccs_conf_path);
		exit(1);
	}
	close(fd);

	if (fork() == 0)
	{

		chdir("/");

		for (i = 0; i < FOPEN_MAX; i++)
			close(i);

		setsid();

		if (debug_cons)
		{
			logfp = fopen(debug_cons, "a+");
			if (logfp)
			{
				dup(0);
				dup(1);
			}
			else
				debug_cons = NULL;

		}

		pcsd_signal_block();
		if (load_pccs_conf())
		{
			syslog(LOG_WARNING, "initialization failed\n");
			exit(1);
		}

		for (num = i = 0; i < MAX_SLOTS; i ++)
			if (pcsd_setup_info(i) == 0)
				num ++;

		if (num == 0)
			exit(1);

		fp = fopen(PidFile, "w");
		if (fp != NULL)
		{
			fprintf(fp, "%d\n", getpid());
			(void) fclose(fp);
		}

		if (reprobe != 0)
		{
			pcsd_start(reprobe);
			sleep(1);
		}

		signal(SIGINT, pcsd_exit);
		signal(SIGTERM, pcsd_exit);
		signal(SIGHUP, pcsd_hungup);
		signal(SIGUSR1, pcsd_main);
		signal(SIGUSR2, pcsd_reprobe);
		pcsd_signal_unblock();

		while (1)
			pause();
	}

	exit(0);
}
