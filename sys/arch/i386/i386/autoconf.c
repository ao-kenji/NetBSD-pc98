/*	$NecBSD: autoconf.c,v 3.7 1999/07/23 05:39:38 kmatsuda Exp $	*/
/*	$NetBSD: autoconf.c,v 1.37 1999/04/01 00:37:50 thorpej Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the vba 
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/dkio.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/bootinfo.h>

static int match_harddisk __P((struct device *, struct btinfo_bootdisk *));
static void matchbiosdisks __P((void));
void findroot __P((struct device **, int *));

extern struct disklist *i386_alldisks;
extern int i386_ndisks;

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
extern int	cold;		/* cold start flag initialized in locore.s */

struct devnametobdevmaj i386_nam2blk[] = {
	{ "wd",		0 },
	{ "sd",		4 },
	{ "cd",		6 },
	{ "mcd",	7 },
	{ "fd",		2 },
	{ "md",		17 },
	{ NULL,		0 },
};

/*
 * Determine i/o configuration for a machine.
 */
void
configure()
{

	startrtclock();

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("configure: mainbus not configured");

	printf("biomask %x netmask %x ttymask %x\n",
	    (u_short)imask[IPL_BIO], (u_short)imask[IPL_NET],
	    (u_short)imask[IPL_TTY]);

	spl0();
	cold = 0;

	/* Set up proc0's TSS and LDT (after the FPU is configured). */
	i386_proc0_tss_ldt_init();

	/* XXX Finish deferred buffer cache allocation. */
	i386_bufinit();
}

void
cpu_rootconf()
{
	struct device *booted_device;
	int booted_partition;

	findroot(&booted_device, &booted_partition);
	matchbiosdisks();

	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition, i386_nam2blk);
}

/*
 * XXX ugly bit of code. But, this is the only safe time that the
 * match between BIOS disks and native disks can be done.
 */
static void
matchbiosdisks()
{
#ifndef	ORIGINAL_CODE
	/*
	 * not yet and never support the code in this file!
	 * what a foolish idea!
	 * disklabel_mbr.h? are you kidding? BAKA!
	 */
	return;
#else	/* !PC-98 */

	struct btinfo_biosgeom *big;
	struct bi_biosgeom_entry *be;
	struct device *dv;
	struct devnametobdevmaj *d;
	int i, ck, error, m, n;
	struct vnode *tv;
	char mbr[DEV_BSIZE];

	big = lookup_bootinfo(BTINFO_BIOSGEOM);

	if (big == NULL)
		return;

	/*
	 * First, count all native disks
	 */
	for (dv = alldevs.tqh_first; dv != NULL; dv = dv->dv_list.tqe_next)
		if (dv->dv_class == DV_DISK &&
		    (!strcmp(dv->dv_cfdata->cf_driver->cd_name, "sd") ||
		     !strcmp(dv->dv_cfdata->cf_driver->cd_name, "wd")))
			i386_ndisks++;

	if (i386_ndisks == 0)
		return;

	/* XXX M_TEMP is wrong */
	i386_alldisks = malloc(sizeof (struct disklist) + (i386_ndisks - 1) *
				sizeof (struct nativedisk_info),
				M_TEMP, M_NOWAIT);
	if (i386_alldisks == NULL)
		return;

	i386_alldisks->dl_nnativedisks = i386_ndisks;
	i386_alldisks->dl_nbiosdisks = big->num;
	for (i = 0; i < big->num; i++) {
		i386_alldisks->dl_biosdisks[i].bi_dev = big->disk[i].dev;
		i386_alldisks->dl_biosdisks[i].bi_sec = big->disk[i].sec;
		i386_alldisks->dl_biosdisks[i].bi_head = big->disk[i].head;
		i386_alldisks->dl_biosdisks[i].bi_cyl = big->disk[i].cyl;
		i386_alldisks->dl_biosdisks[i].bi_lbasecs = big->disk[i].totsec;
		i386_alldisks->dl_biosdisks[i].bi_flags = big->disk[i].flags;
	}

	/*
	 * XXX code duplication from findroot()
	 */
	n = -1;
	for (dv = alldevs.tqh_first; dv != NULL; dv = dv->dv_list.tqe_next) {
		if (dv->dv_class != DV_DISK)
			continue;
#ifdef GEOM_DEBUG
		printf("matchbiosdisks: trying to match (%s) %s\n",
		    dv->dv_xname, dv->dv_cfdata->cf_driver->cd_name);
#endif
		if (!strcmp(dv->dv_cfdata->cf_driver->cd_name, "sd") ||
		    !strcmp(dv->dv_cfdata->cf_driver->cd_name, "wd")) {
			n++;
			sprintf(i386_alldisks->dl_nativedisks[n].ni_devname,
			    "%s%d", dv->dv_cfdata->cf_driver->cd_name,
			    dv->dv_unit);

			for (d = i386_nam2blk; d->d_name &&
			   strcmp(d->d_name, dv->dv_cfdata->cf_driver->cd_name);
			   d++);
			if (d->d_name == NULL)
				return;

			if (bdevvp(MAKEDISKDEV(d->d_maj, dv->dv_unit, RAW_PART),
			    &tv))
				panic("matchbiosdisks: can't alloc vnode");

			error = VOP_OPEN(tv, FREAD, NOCRED, 0);
			if (error) {
				vrele(tv);
				continue;
			}
			error = vn_rdwr(UIO_READ, tv, mbr, DEV_BSIZE, 0,
			    UIO_SYSSPACE, 0, NOCRED, NULL, 0);
			VOP_CLOSE(tv, FREAD, NOCRED, 0);
			if (error) {
#ifdef GEOM_DEBUG
				printf("matchbiosdisks: %s: MBR read failure\n",
				    dv->dv_xname);
#endif
				continue;
			}

			for (ck = i = 0; i < DEV_BSIZE; i++)
				ck += mbr[i];
			for (m = i = 0; i < big->num; i++) {
				be = &big->disk[i];
#ifdef GEOM_DEBUG
				printf("match %s with %d\n", dv->dv_xname, i);
				printf("dev ck %x bios ck %x\n", ck, be->cksum);
#endif
				if (be->flags & BI_GEOM_INVALID)
					continue;
				if (be->cksum == ck &&
				    !memcmp(&mbr[MBR_PARTOFF], be->dosparts,
					NMBRPART *
					    sizeof (struct mbr_partition))) {
#ifdef GEOM_DEBUG
					printf("matched bios disk %x with %s\n",
					    be->dev, be->devname);
#endif
					i386_alldisks->dl_nativedisks[n].
					    ni_biosmatches[m++] = i;
				}
			}
			i386_alldisks->dl_nativedisks[n].ni_nmatches = m;
			vrele(tv);
		}
	}
#endif	/* !PC-98 */
}


u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */
struct device *booted_device;

/*
 * helper function for "findroot()":
 * return nonzero if disk device matches bootinfo
 */
static int
match_harddisk(dv, bid)
	struct device *dv;
	struct btinfo_bootdisk *bid;
{
	struct devnametobdevmaj *i;
	struct vnode *tmpvn;
	int error;
	struct disklabel label;
	int found = 0;

	/*
	 * A disklabel is required here.  The
	 * bootblocks don't refuse to boot from
	 * a disk without a label, but this is
	 * normally not wanted.
	 */
	if (bid->labelsector == -1)
		return(0);

	/*
	 * lookup major number for disk block device
	 */
	i = i386_nam2blk;
	while (i->d_name &&
	       strcmp(i->d_name, dv->dv_cfdata->cf_driver->cd_name))
		i++;
	if (i->d_name == NULL)
		return(0); /* XXX panic() ??? */

	/*
	 * Fake a temporary vnode for the disk, open
	 * it, and read the disklabel for comparison.
	 */
	if (bdevvp(MAKEDISKDEV(i->d_maj, dv->dv_unit, bid->partition), &tmpvn))
		panic("findroot can't alloc vnode");
	error = VOP_OPEN(tmpvn, FREAD, NOCRED, 0);
	if (error) {
#ifndef DEBUG
		/*
		 * Ignore errors caused by missing
		 * device, partition or medium.
		 */
		if (error != ENXIO && error != ENODEV)
#endif
			printf("findroot: can't open dev %s%c (%d)\n",
			       dv->dv_xname, 'a' + bid->partition, error);
		vrele(tmpvn);
		return(0);
	}
	error = VOP_IOCTL(tmpvn, DIOCGDINFO, (caddr_t)&label, FREAD, NOCRED, 0);
	if (error) {
		/*
		 * XXX can't happen - open() would
		 * have errored out (or faked up one)
		 */
		printf("can't get label for dev %s%c (%d)\n",
		       dv->dv_xname, 'a' + bid->partition, error);
		goto closeout;
	}

	/* compare with our data */
	if (label.d_type == bid->label.type &&
	    label.d_checksum == bid->label.checksum &&
	    !strncmp(label.d_packname, bid->label.packname, 16))
		found = 1;

closeout:
	VOP_CLOSE(tmpvn, FREAD, NOCRED, 0);
	vrele(tmpvn);

	return(found);
}

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
void
findroot(devpp, partp)
	struct device **devpp;
	int *partp;
{
	struct btinfo_bootdisk *bid;
	struct device *dv;
	int i, majdev, unit, part;
	char buf[32];

	/*
	 * Default to "not found."
	 */
	*devpp = NULL;
	*partp = 0;

	if (booted_device) {
		*devpp = booted_device;
		return;
	}
	if (lookup_bootinfo(BTINFO_NETIF)) {
		/*
		 * We got netboot interface information, but
		 * "device_register()" couldn't match it to a configured
		 * device. Bootdisk information cannot be present at the
		 * same time, so give up.
		 */
		printf("findroot: netboot interface not found\n");
		return;
	}

	bid = lookup_bootinfo(BTINFO_BOOTDISK);
	if (bid) {
#ifndef	ORIGINAL_CODE
#ifdef	_BOOTINFO_DEBUG
		printf("findroot: btinfo_bootdisk -> common.len %x "
		    "common.type %x labelsector %x label.type %x "
		    "label.checksum %x biosdev %x partition %x\n",
		    bid->common.len,
		    bid->common.type, bid->labelsector, bid->label.type,
		    bid->label.checksum, bid->biosdev, bid->partition);
#endif	/* _BOOTINFO_DEBUG */
#endif	/* PC-98 */
		/*
		 * Scan all disk devices for ones that match the passed data.
		 * Don't break if one is found, to get possible multiple
		 * matches - for problem tracking. Use the first match anyway
		 * because lower device numbers are more likely to be the
		 * boot device.
		 */
		for (dv = alldevs.tqh_first; dv != NULL;
		dv = dv->dv_list.tqe_next) {
			if (dv->dv_class != DV_DISK)
				continue;

			if (!strcmp(dv->dv_cfdata->cf_driver->cd_name, "fd")) {
				/*
				 * Assume the configured unit number matches
				 * the BIOS device number.  (This is the old
				 * behaviour.)  Needs some ideas how to handle
				 * BIOS's "swap floppy drive" options.
				 */
#ifdef	ORIGINAL_CODE
				if ((bid->biosdev & 0x80) ||
				    dv->dv_unit != bid->biosdev)
					continue;
#else	/* PC-98 */
				if ((((bid->biosdev & 0xf0) != 0x30) &&
				    ((bid->biosdev & 0xf0) != 0xb0) &&
				    ((bid->biosdev & 0xf0) != 0x90)) ||
				    dv->dv_unit != bid->biosdev)
					continue;
#endif	/* PC-98 */

				goto found;
			}

#ifdef	ORIGINAL_CODE
			if (!strcmp(dv->dv_cfdata->cf_driver->cd_name, "sd") ||
			    !strcmp(dv->dv_cfdata->cf_driver->cd_name, "wd")) {
				/*
				 * Don't trust BIOS device numbers, try
				 * to match the information passed by the
				 * bootloader instead.
				 */
				if ((bid->biosdev & 0x80) == 0 ||
				    !match_harddisk(dv, bid))
					continue;

				goto found;
			}
#else	/* PC-98 */
			if (!strcmp(dv->dv_cfdata->cf_driver->cd_name, "sd")) {
				/*
				 * Don't trust BIOS device numbers, try
				 * to match the information passed by the
				 * bootloader instead.
				 */
				if ((bid->biosdev & 0xf0) != 0xa0 ||
				    !match_harddisk(dv, bid))
					continue;

				goto found;
			}
			if (!strcmp(dv->dv_cfdata->cf_driver->cd_name, "wd")) {
				/*
				 * Don't trust BIOS device numbers, try
				 * to match the information passed by the
				 * bootloader instead.
				 */
				if ((bid->biosdev & 0xf0) != 0x80 ||
				    !match_harddisk(dv, bid))
					continue;

				goto found;
			}
#endif	/* PC-98 */

			/* no "fd", "wd" or "sd" */
			continue;

found:
			if (*devpp) {
				printf("warning: double match for boot "
				    "device (%s, %s)\n", (*devpp)->dv_xname,
				    dv->dv_xname);
				continue;
			}
			*devpp = dv;
			*partp = bid->partition;
		}

		if (*devpp)
			return;
	}

#if 0
	printf("howto %x bootdev %x ", boothowto, bootdev);
#endif

	if ((bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;

	majdev = (bootdev >> B_TYPESHIFT) & B_TYPEMASK;
	for (i = 0; i386_nam2blk[i].d_name != NULL; i++)
		if (majdev == i386_nam2blk[i].d_maj)
			break;
	if (i386_nam2blk[i].d_name == NULL)
		return;

	part = (bootdev >> B_PARTITIONSHIFT) & B_PARTITIONMASK;
	unit = (bootdev >> B_UNITSHIFT) & B_UNITMASK;

	sprintf(buf, "%s%d", i386_nam2blk[i].d_name, unit);
	for (dv = alldevs.tqh_first; dv != NULL;
	    dv = dv->dv_list.tqe_next) {
		if (strcmp(buf, dv->dv_xname) == 0) {
			*devpp = dv;
			*partp = part;
			return;
		}
	}
}

#include "pci.h"

#include <dev/isa/isavar.h>
#if NPCI > 0
#include <dev/pci/pcivar.h>
#endif

void
device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	/*
	 * Handle network interfaces here, the attachment information is
	 * not available driver independantly later.
	 * For disks, there is nothing useful available at attach time.
	 */
	if (dev->dv_class == DV_IFNET) {
		struct btinfo_netif *bin = lookup_bootinfo(BTINFO_NETIF);
		if (bin == NULL)
			return;

		/* check driver name */
		if (strcmp(bin->ifname, dev->dv_cfdata->cf_driver->cd_name))
			return;

		if (bin->bus == BI_BUS_ISA &&
		    !strcmp(dev->dv_parent->dv_cfdata->cf_driver->cd_name,
		    "isa")) {
			struct isa_attach_args *iaa = aux;

			/* compare IO base address */
			if (bin->addr.iobase == iaa->ia_iobase)
				goto found;
		}
#if NPCI > 0
		if (bin->bus == BI_BUS_PCI &&
		    !strcmp(dev->dv_parent->dv_cfdata->cf_driver->cd_name,
		    "pci")) {
			struct pci_attach_args *paa = aux;
			int b, d, f;

			/*
			 * Calculate BIOS representation of:
			 *
			 *	<bus,device,function>
			 *
			 * and compare.
			 */
			pci_decompose_tag(paa->pa_pc, paa->pa_tag, &b, &d, &f);
			if (bin->addr.tag == ((b << 8) | (d << 3) | f))
				goto found;
		}
#endif
	}
	return;

found:
	if (booted_device) {
		/* XXX should be a "panic()" */
		printf("warning: double match for boot device (%s, %s)\n",
		    booted_device->dv_xname, dev->dv_xname);
		return;
	}
	booted_device = dev;
}
