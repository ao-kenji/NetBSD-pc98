/*	$NecBSD: devices.c,v 1.6 1998/03/14 07:01:30 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * Device tables.  See the Configure file for a complete description.
 */

#include <stdio.h>
#include "msdos.h"

#ifdef DELL
struct device devices[] = {
	{'A', "/dev/rdsk/f0d9dt", 0L, 12, 0, (int (*) ()) 0, 40, 2, 9},
	{'A', "/dev/rdsk/f0q15dt", 0L, 12, 0, (int (*) ()) 0, 80, 2, 15},
	{'A', "/dev/rdsk/f0d8dt", 0L, 12, 0, (int (*) ()) 0, 40, 2, 8},
	{'B', "/dev/rdsk/f13ht", 0L, 12, 0, (int (*) ()) 0, 80, 2, 18},
	{'B', "/dev/rdsk/f13dt", 0L, 12, 0, (int (*) ()) 0, 80, 2, 9},
	{'C', "/dev/rdsk/dos", 0L, 16, 0, (int (*) ()) 0, 0, 0, 0},
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#endif /* DELL */
#if defined(__386BSD__) || defined(__NetBSD__)
# ifdef PC98
#  ifndef MULTISECTORSIZE
		error!!
#  endif
int	msector_size;

static set512sec()
{
	MSECTOR_SIZE = 512;
	return 0;
}

static set1024sec()
{
	MSECTOR_SIZE = 1024;
	return 0;
}

#if 1
/*  Modified by Honda
 *  A,B I  1.4   /dev/rfd?a   FOR MATE 
 *  C,D J  2HC   /dev/rfd?b
 *  E,F K  2HD   /dev/rfd?c
 *  G,H L  2DD   /dev/rfd?d
 */
struct device devices[] = {
        {'A', "/dev/rfd0a", 0L, 12, 0, set512sec,  80, 2, 18}, /* 1.44 */
        {'B', "/dev/rfd0a", 0L, 12, 0, set512sec,  80, 2, 15}, /* 2HC */
        {'C', "/dev/rfd0a", 0L, 12, 0, set1024sec, 77, 2, 8},  /* 2HD */
        {'D', "/dev/rfd0a", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
        {'E', "/dev/rfd0a", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
	/*E*/
        {'F', "/dev/rfd1a", 0L, 12, 0, set512sec,  80, 2, 18}, /* 1.44 */
        {'G', "/dev/rfd1a", 0L, 12, 0, set512sec,  80, 2, 15}, /* 2HC */
        {'H', "/dev/rfd1a", 0L, 12, 0, set1024sec, 77, 2, 8},  /* 2HD */
        {'I', "/dev/rfd1a", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
        {'J', "/dev/rfd1a", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
	/*J*/
        {'K', "/dev/rfd2a", 0L, 12, 0, set512sec,  80, 2, 18}, /* 1.44 */
        {'L', "/dev/rfd2a", 0L, 12, 0, set512sec,  80, 2, 15}, /* 2HC */
        {'M', "/dev/rfd2a", 0L, 12, 0, set1024sec, 77, 2, 8},  /* 2HD */
        {'N', "/dev/rfd2a", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
        {'O', "/dev/rfd2a", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
	/*O*/
        {'P', "/dev/rfd3a", 0L, 12, 0, set512sec,  80, 2, 18}, /* 1.44 */
        {'Q', "/dev/rfd3a", 0L, 12, 0, set512sec,  80, 2, 15}, /* 2HC */
        {'R', "/dev/rfd3a", 0L, 12, 0, set1024sec, 77, 2, 8},  /* 2HD */
        {'S', "/dev/rfd3a", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
        {'T', "/dev/rfd3a", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
        {'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#else /* above was added by honda (94/05/24) */
struct device devices[] = {
	{'A', "/dev/rfd0a", 0L, 12, 0, set512sec,  80, 2, 15}, /* 1.2m */
	{'C', "/dev/rfd0b", 0L, 12, 0, set1024sec, 77, 2, 8},  /* 2HD */
	{'E', "/dev/rfd0c", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
	{'B', "/dev/rfd1a", 0L, 12, 0, set512sec,  80, 2, 15}, /* 1.2m */
	{'D', "/dev/rfd1b", 0L, 12, 0, set1024sec, 77, 2, 8},  /* 2HD */
	{'F', "/dev/rfd1c", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
	{'G', "/dev/rfd2a", 0L, 12, 0, set512sec,  80, 2, 15}, /* 1.2m */
	{'I', "/dev/rfd2b", 0L, 12, 0, set1024sec, 77, 2, 8},  /* 2HD */
	{'K', "/dev/rfd2c", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
	{'H', "/dev/rfd3a", 0L, 12, 0, set512sec,  80, 2, 15}, /* 1.2m */
	{'J', "/dev/rfd3b", 0L, 12, 0, set1024sec, 77, 2, 8},  /* 2HD */
	{'L', "/dev/rfd3c", 0L, 12, 0, set512sec,  80, 2, 9},  /* 720k */
	{'M', "/dev/ras0d", 0L, 16, 0, set512sec, 9952, 1, 25},/* 3.5in MO */
	{'N', "/dev/ras1d", 0L, 16, 0, set512sec, 9952, 1, 25},/* 3.5in MO */
	{'O', "/dev/ras2d", 0L, 16, 0, set512sec, 9952, 1, 25},/* 3.5in MO */
	{'P', "/dev/ras3d", 0L, 16, 0, set512sec, 9952, 1, 25},/* 3.5in MO */
	{'Q', "/dev/ras4d", 0L, 16, 0, set512sec, 9952, 1, 25},/* 3.5in MO */
	{'R', "/dev/ras5d", 0L, 16, 0, set512sec, 9952, 1, 25},/* 3.5in MO */
	{'S', "/dev/ras6d", 0L, 16, 0, set512sec, 9952, 1, 25},/* 3.5in MO */
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#endif /* added by honda (94/05/24) */
#endif /* PC-98 */
#endif /* 386BSD */

#ifdef ISC
struct device devices[] = {
	{'A', "/dev/rdsk/f0d9dt", 0L, 12, 0, (int (*) ()) 0, 40, 2, 9},
	{'A', "/dev/rdsk/f0q15dt", 0L, 12, 0, (int (*) ()) 0, 80, 2, 15},
	{'A', "/dev/rdsk/f0d8dt", 0L, 12, 0, (int (*) ()) 0, 40, 2, 8},
	{'B', "/dev/rdsk/f13ht", 0L, 12, 0, (int (*) ()) 0, 80, 2, 18},
	{'B', "/dev/rdsk/f13dt", 0L, 12, 0, (int (*) ()) 0, 80, 2, 9},
	{'C', "/dev/rdsk/0p1", 0L, 16, 0, (int (*) ()) 0, 0, 0, 0},
	{'D', "/usr/vpix/defaults/C:", 8704L, 12, 0, (int (*) ()) 0, 0, 0, 0},
	{'E', "$HOME/vpix/C:", 8704L, 12, 0, (int (*) ()) 0, 0, 0, 0},
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#endif /* ISC */

#ifdef SPARC
struct device devices[] = {
	{'A', "/dev/rfd0c", 0L, 12, 0, (int (*) ()) 0, 80, 2, 18},
	{'A', "/dev/rfd0c", 0L, 12, 0, (int (*) ()) 0, 80, 2, 9},
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#endif /* SPARC */
#ifdef RT_ACIS
struct device devices[] = {
      {'A', "/dev/rfd0", 0L, 12, 0, (int (*) ()) 0, 80, 2, 15},
      {'A', "/dev/rfd0", 0L, 12, 0, (int (*) ()) 0, 40, 2, 9},
      {'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#endif /* RT_ACIS */


#ifdef UNIXPC
#include <sys/gdioctl.h>
#include <fcntl.h>

int init_unixpc();

struct device devices[] = {
	{'A', "/dev/rfp020", 0L, 12, O_NDELAY, init_unixpc, 40, 2, 9},
	{'C', "/usr/bin/DOS/dvd000", 0L, 12, 0, (int (*) ()) 0, 0, 0, 0},
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};

int
init_unixpc(fd, ntracks, nheads, nsect)
int fd, ntracks, nheads, nsect;
{
	struct gdctl gdbuf;

	if (ioctl(fd, GDGETA, &gdbuf) == -1) {
		ioctl(fd, GDDISMNT, &gdbuf);
		return(1);
	}

	gdbuf.params.cyls = ntracks;
	gdbuf.params.heads = nheads;
	gdbuf.params.psectrk = nsect;

	gdbuf.params.pseccyl = gdbuf.params.psectrk * gdbuf.params.heads;
	gdbuf.params.flags = 1;		/* disk type flag */
	gdbuf.params.step = 0;		/* step rate for controller */
	gdbuf.params.sectorsz = 512;	/* sector size */

	if (ioctl(fd, GDSETA, &gdbuf) < 0) {
		ioctl(fd, GDDISMNT, &gdbuf);
		return(1);
	}
	return(0);
}
#endif /* UNIXPC */

#ifdef RT_ACIS
struct device devices[] = {
	{'A', "/dev/rfd0", 0L, 12, 0, (int (*) ()) 0, 80, 2, 15},
	{'A', "/dev/rfd0", 0L, 12, 0, (int (*) ()) 0, 40, 2, 9},
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#endif /* RT_ACIS */

#ifdef SUN386
struct device devices[] = {
	{'A', "/dev/rfdl0c", 0L, 12, 0, (int (*) ()) 0, 80, 2, 9},
	{'A', "/dev/rfd0c", 0L, 12, 0, (int (*) ()) 0, 80, 2, 18},
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#endif /* SUN386 */

  
#ifdef SPARC_ODD
#include <sys/types.h>
#include <sun/dkio.h>
#include <fcntl.h>

int init_sparc();

struct device devices[] = {
	{'A', "/dev/rfd0c", 0L, 12, 0, init_sparc, 80, 2, 0},
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};

/*
 * Stuffing back the floppy parameters into the driver allows for gems
 * like 10 sector or single sided floppies from Atari ST systems.
 * 
 * Martin Schulz, Universite de Moncton, N.B., Canada, March 11, 1991.
 */

int
init_sparc(fd, ntracks, nheads, nsect)
int fd, ntracks, nheads, nsect;
{
	struct fdk_char dkbuf;
	struct dk_map   dkmap;

	if (ioctl(fd, FDKIOGCHAR, &dkbuf) != 0) {
		ioctl(fd, FDKEJECT, NULL);
		return(1);
	}

	if (ioctl(fd, DKIOCGPART, &dkmap) != 0) {
		ioctl(fd, FDKEJECT, NULL);
		return(1);
	}

	if (ntracks)
		dkbuf.ncyl = ntracks;
	if (nheads)
		dkbuf.nhead = nheads;
	if (nsect)
		dkbuf.secptrack = nsect;

	if (ntracks && nheads && nsect )
		dkmap.dkl_nblk = ntracks * nheads * nsect;

	if (ioctl(fd, FDKIOSCHAR, &dkbuf) != 0) {
		ioctl(fd, FDKEJECT, NULL);
		return(1);
	}

	if (ioctl(fd, DKIOCSPART, &dkmap) != 0) {
		ioctl(fd, FDKEJECT, NULL);
		return(1);
	}
	return(0);
}
#endif /* SPARC_ODD */
  
#ifdef XENIX
struct device devices[] = {
	{'A', "/dev/fd096ds15", 0L, 12, 0, (int (*) ()) 0, 80, 2, 15},
	{'A', "/dev/fd048ds9", 0L, 12, 0, (int (*) ()) 0, 40, 2, 9},
	{'B', "/dev/fd1135ds18", 0L, 12, 0, (int (*) ()) 0, 80, 2, 18},
	{'B', "/dev/fd1135ds9", 0L, 12, 0, (int (*) ()) 0, 80, 2, 9},
	{'C', "/dev/hd0d", 0L, 16, 0, (int (*) ()) 0, 0, 0, 0},
	{'\0', (char *) NULL, 0L, 0, 0, (int (*) ()) 0, 0, 0, 0}
};
#endif /* XENIX */
