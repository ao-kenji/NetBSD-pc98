/*	$NecBSD: vmocs.c,v 1.10 1998/02/08 08:01:47 kmatsuda Exp $	*/
/*	$NetBSD$	*/

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/device.h>
#include <vm/vm.h>
#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <limits.h>

#define NEWVM			/* XXX till old has been updated or purged */
struct nlist namelist[] = {
	{ "_vm_oc_total" },
	{ "_pdm_statics" },
	{ "" },
};

/* Objects defined in dkstats.c */
kvm_t *kd;
char	*nlistf, *memf;

main(argc, argv)
	register int argc;
	register char **argv;
{
	extern int optind;
	extern char *optarg;
	register int c, todo;
	u_int interval;
	int reps;
        char errbuf[_POSIX2_LINE_MAX];

	memf = nlistf = NULL;
	interval = reps = todo = 0;

	if (nlistf != NULL || memf != NULL)
		setgid(getgid());

        kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf);
	if (kd == 0) {
		(void)fprintf(stderr,
		    "vmstat: kvm_openfiles: %s\n", errbuf);
		exit(1);
	}

	if ((c = kvm_nlist(kd, namelist)) != 0) {
		if (c > 0) {
			(void)fprintf(stderr,
			    "vmstat: undefined symbols:");
			for (c = 0;
			    c < sizeof(namelist)/sizeof(namelist[0]); c++)
				if (namelist[c].n_type == 0)
					fprintf(stderr, " %s",
					    namelist[c].n_name);
			(void)fputc('\n', stderr);
		} else
			(void)fprintf(stderr, "vmstat: kvm_nlist: %s\n",
			    kvm_geterr(kd));
		exit(1);
	}

	dovmocsstat();
}

void
kread(nlx, addr, size)
	int nlx;
	void *addr;
	size_t size;
{
	char *sym;

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		(void)fprintf(stderr,
		    "vmstat: symbol %s not defined\n", sym);
		exit(1);
	}
	if (kvm_read(kd, namelist[nlx].n_value, addr, size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		(void)fprintf(stderr, "vmstat: %s: %s\n", sym, kvm_geterr(kd));
		exit(1);
	}
}


u_long vm_oc_stat[5];
u_long ovm_oc_stat[5];
u_long pdm_stat[5];
u_long opdm_stat[5];
u_long npdm;

int
dovmocsstat(interval, reps)
	u_int interval;
	int reps;
{
	int line;

	kread(0, ovm_oc_stat, sizeof(vm_oc_stat));
	kread(1, opdm_stat, sizeof(pdm_stat));
	npdm = 0;
  do
 {
/*		12345671234567123456712345671234567 */
	printf("  mwait   miss   pgin   miss  short|");
	printf("   epac    pac    rel   erel   inuse\n");

	printf("%7d%7d%7d%7d%7d|",
		ovm_oc_stat[0], 
		ovm_oc_stat[1], 
		ovm_oc_stat[2], 
		ovm_oc_stat[3],
		ovm_oc_stat[4]);
	printf("%7d%7d%7d%7d%7d\n",
		opdm_stat[0], 
		opdm_stat[1], 
		opdm_stat[2],
		opdm_stat[3], 
		opdm_stat[4]);

	for (line = 0; line < 25; line ++)
	{
		kread(0, vm_oc_stat, sizeof(vm_oc_stat));
		kread(1, pdm_stat, sizeof(pdm_stat));

		printf("%7d%7d%7d%7d%7d|",
			vm_oc_stat[0] - ovm_oc_stat[0], 
			vm_oc_stat[1] - ovm_oc_stat[1], 
			vm_oc_stat[2] - ovm_oc_stat[2], 
			vm_oc_stat[3] - ovm_oc_stat[3],
			vm_oc_stat[4] - ovm_oc_stat[4]);
		printf("%7d%7d%7d%7d%7d\n",
			pdm_stat[0] - opdm_stat[0], 
			pdm_stat[1] - opdm_stat[1], 
			pdm_stat[2] - opdm_stat[2],
			pdm_stat[3] - opdm_stat[3], 
			pdm_stat[4]);
		bcopy(vm_oc_stat, ovm_oc_stat, sizeof(vm_oc_stat));
		bcopy(pdm_stat, opdm_stat, sizeof(pdm_stat));
		sleep(1);
	}
  }
  while(1);
}
