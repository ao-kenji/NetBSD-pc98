/*	$NecBSD: rdstart.c,v 1.12 1998/03/14 07:01:50 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * RAMDISK with UFS format start program.
 * N. Honda.
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/signal.h>

#include <dev/ramdisk.h>

#define	BUFSZ	256

void
rdstart_hungup()
{
}

void
start_daemon(fd, nblks)
	int fd, nblks;
{
	struct rd_conf rd;
	int error;

	rd.rd_size = nblks << DEV_BSHIFT;

	rd.rd_addr = mmap(NULL, rd.rd_size,
				PROT_READ | PROT_WRITE,
				MAP_ANON | MAP_PRIVATE,
				-1, 0);

	if (rd.rd_addr == (caddr_t)-1) {
		perror("mmap");
		exit(1);
	}

	/* Become server! */
	rd.rd_type = RD_UMEM_SERVER;
	if (ioctl(fd, RD_SETCONF, &rd)) {
		perror("ioctl");
		exit(1);
	}
	exit(0);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct rd_conf rd;
	int nblks, fd, error;
	u_char cmdline[BUFSZ];
	u_char pathname[BUFSZ];

	if (argc <= 2)
	{
		fprintf(stderr, "usage: rdstart <unit-no> <%d-byte-blocks>\n",
				DEV_BSIZE);
		exit(1);
	}

	nblks = atoi(argv[2]);
	if (nblks <= 0)
	{
		fprintf(stderr, "invalid number of blocks\n");
		exit(1);
	}

	sprintf(pathname, "/dev/rrd%s", argv[1]);
	sprintf(cmdline, "/sbin/newfs -T ramdisk -s %s /dev/rrd%s > /dev/null",
		argv[2], argv[1]);
#if	0
	sprintf(cmdline, "/sbin/newfs /dev/rrd%s > /dev/null", argv[1]);
#endif

	fd = open(pathname, O_RDWR, 0);
	if (fd < 0)
	{
		perror(pathname);
		exit(1);
	}
	if (ioctl(fd, RD_GETCONF, &rd))
	{
		perror("ioctl");
		exit(1);
	}
	if (rd.rd_type != RD_UNCONFIGURED)
	{
		fprintf(stderr, "already configured\n");
		exit(1);
	}
	close(fd);

	if (fork() == 0)
	{
		signal(SIGHUP, rdstart_hungup);

		setsid();

		fd = open(pathname, O_RDWR, 0);
		if (fd >= 0)
			start_daemon(fd, nblks);
		exit(1);
	}

	/* wait 1 sec to be ready */
	sleep(1);
	system(cmdline);
}
