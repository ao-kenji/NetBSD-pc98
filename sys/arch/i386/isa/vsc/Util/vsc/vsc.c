/*	$NecBSD: vsc.c,v 1.23.16.1 1999/09/02 10:57:32 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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
 * NetBSD 1.0 pc98 	vsc.c by N. Honda.
 * Ver 0.0
 */
 /* Remarks.
  * -s savertime (second) 0 means no saver.
  * -X                    If X hangs up, you use this. Be sure to specify
  *                       tty name.
  * -k                    Remark that each vsc has its own mode.
  * -j                    force to interpret all jis78 chars with jis83
  * -J                    reload jis83 gaiji font. Must be in state
  *			  after -l or with -l flags.
  * -l                    all clear Gaiji fonts including jis83 fonts.
  */
/* K. Matsuda. May 09, 1995.
 * If define NEW_VSC_DEV, use /dev/ttyv0 instead of /dev/vga,
 * /dev/ttyv1 instead of /dev/vsc0, and so on.
 */

#define	SCREEN_SAVER

struct proc;

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <machine/vm86bios.h>

#include <machine/vscio.h>

#define	VSC_SCREENMODE_DATA
#include <i386/isa/vsc/config.h>
#include <i386/isa/vsc/vsc.h>
#include <i386/isa/vsc/vt100.h>
#include <i386/isa/vsc/fontload.h>
#include <i386/isa/vsc/kanji.h>
#include <i386/isa/vsc/vsc_sm.h>

/**************************************************
 * macro variables
 **************************************************/
#define	SAVER		0x0001
#define	CYRIX		0x0002
#define	KANJI		0x0004
#define	KEYRESET	0x0008
#define	SHOW		0x0010
#define	XWINKILL	0x0020	/* if x window hang up */
#define	FFJIS83		0x0040
#define	FLRESET		0x0080
#define	FLJIS83		0x0100
#define	GRAPH		0x0200
#define	DEBUGINFO	0x0400
#define	STATUS		0x0800
#define	SPLIT		0x1000
#define RCRT		0x2000
#define CRTLINES	0x4000
#define	KMESG		0x8000

#define	KANJITABSIZE	(sizeof(kanjiset) / sizeof(struct kanji))

char buff[FILENAME_MAX + 20];
char *fname;

/**************************************************
 * device file name
 **************************************************/
char *
get_dev_name(s)
	char *s;
{
	u_int c;
	int n;

	if (*s =='/' || *s == '.')
	{
		strncpy(buff, s, FILENAME_MAX);
		return buff;
	}
	else if (strncmp(s, "ttyv", 4) == 0)
		sprintf(buff, "/dev/%s", s);
	else if (strncmp(s, "console", 7) == 0)
		sprintf(buff, "/dev/%s", s);
	else if (strlen(s) == 1 && isdigit((u_int)s[0]))
	{
		sprintf(buff, "/dev/ttyv%s", s);
	}
	else
		sprintf(buff, "%s", s);
	return buff;
}

int
findkey(s)
	char *s;
{
	int i;

	for (i = 0; s[i] != 0; i++)
		s[i] = toupper(s[i]);

	for (i = 0; i < KANJITABSIZE; i++)
	{
		if (strstr(kanjiset[i].name, s))
			return i;
	}
	return -1;
}

/**************************************************
 * usage
 **************************************************/
void mode_usage __P((void));
int usage __P((int));

int
usage(fd)
	int fd;
{

	(void)fprintf(stderr,
"usage: vsc [DJXlrv][-cgjRS ON|OFF][-k kanjicode][-s saver][-m screenmode] tty\n");

	mode_usage();

	if (fd >= 0)
		show(fd);
}

void
mode_usage(void)
{
	int no;

	printf("Supported screenmode:\n");
	for (no = 0; no < VSC_MAX_SM; no ++)
		printf("%d = %2d khz %2d lines %3d %4s\n",
			no,
			screenmodes[no].sm_khz,
			screenmodes[no].sm_lines,
			screenmodes[no].sm_dots,
			(screenmodes[no].sm_flags & SM_EGC) ? "Egc" : "pEgc");
}

int
show(fd)
{
	int mode, i;

	ioctl(fd, VSC_GET_KANJIMODE, &mode);
	printf("Supported kanjicode:\n");
	for (i = 0; i < KANJITABSIZE; i++)
		printf("%s ", kanjiset[i].name);
	printf("\n");
	printf("Current code(%s): %s\n", fname, kanjiset[mode].name);
}

/**************************************************
 * kernel msg dump
 **************************************************/
int
kmesg(fd)
	int fd;
{
	int error;
	u_int pos, cpos, c;
	struct vsc_kmesg vsc_kmesg;
	u_char msgbuf[VSC_KMESG_SZ + 4];

	vsc_kmesg.data = msgbuf;
	error = ioctl(fd, VSC_GET_KMESG, &vsc_kmesg);
	if (error)
		return error;

	for (pos = 0; pos < VSC_KMESG_SZ; pos ++)
	{
		cpos = (vsc_kmesg.pos + pos) % VSC_KMESG_SZ;
		if (msgbuf[cpos])
		{
			switch (c = msgbuf[cpos])
			{
			case '\r':
				printf("\n");
				break;

			default:
				/* skip control characters */
				if (c < 0x20)
					break;
				printf("%c", c);
			}
		}
	}

	fflush(stdout);
	return 0;
}

/**************************************************
 * misc debug
 **************************************************/
void
print_wf(wf)
	wf_t *wf;
{
	int i;

	printf("--Win Frame--\n");
	printf("vsp %x, flags %x, Crtat %x, Atrat %x, row size %d\n",
	    wf->vsp, wf->flags, wf->Ctab, wf->Atab, wf->rsz);
	printf("ocol %d orow %d ncol %d nrow %d rcol %d rrow %d\n",
	    wf->o.col, wf->o.row, wf->n.col, wf->n.row, wf->r.col, wf->r.row);
	printf("sl %d sh %d ssz %d dmode %x tabs %x attrp %x attr %x\n",
	    wf->sc.lw, wf->sc.hg, wf->sc.sz, wf->decmode,
	    wf->tabs, wf->attrp, wf->attr);
	printf("GL=%x GR=%x SS=%x st=%d prm=%d",
	    wf->iso.GL, wf->iso.GR, wf->iso.SS, wf->state, wf->nparm);
	for (i = 0; i < 4; i++)
		printf("(%c,%x)[%d] ", wf->iso.g[i], wf->iso.charset[i], i);
	printf("\n");
}

int
debug(fd)
{
	int error, i;
	struct vc_debug xx;
	struct video_state *vsp;

	error= ioctl(fd, VSC_DEBUG, &xx);
	if (error < 0)
		return 0;
	vsp = &xx.vs;
	printf("--STATUS-- \n");
	printf("%s: flags %x tp %x cwf=%x backupwf %x\n",
	    fname,vsp->flags,vsp->tp, vsp->cwf, vsp->backupwf);
	printf("ring %x Ctab[0] %x Atab[0] %x vn.row=%d vr.row=%d\n",
	    vsp->ring, vsp->vscCtab[0], vsp->vscAtab[0], vsp->vn.row,
	    vsp->vr.row);
	for (i = 0; i < NSWIN; i++)
		print_wf(&xx.lwf[i]);
	return 0;
}

/**************************************************
 * Main
 **************************************************/
int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	char *ss, *cs, *ks, *ds, *js, *gs, *Cs, *Ss, *Rs, *ms;
	int fd, wn, num;
	int flags = 0;
	extern int opterr, optind;
	extern char *optarg;

	/************************************************
	 * process args
	 ************************************************/
	opterr = 0;
	while ((ch = getopt(argc, argv, "DJ12S:dg:hlm:rR:vj:s:c:k:X")) != EOF)
	{

		switch (ch) {
		case 'D':
			flags |= KMESG;
			break;
		case 'd':
			flags |= DEBUGINFO;
			break;
		case 'h':
			usage(-1);
			break;
		case 's':
			ss = optarg;
			flags |=SAVER;
			break;
		case 'c':
			cs = optarg;
			flags |=CYRIX;
			break;
		case 'k':
			ks = optarg;
			flags |=KANJI;
			break;
		case 'r':
			flags |=KEYRESET;
			break;
		case 'R':
			Rs = optarg;
			flags |=RCRT;
			break;
		case 'v':
			flags |=SHOW;
			break;
		case 'j':
			js = optarg;
			flags |=FFJIS83;
			break;
		case 'l':
			flags |=FLRESET;
			break;
		case 'g':
			flags |=GRAPH;
			gs = optarg;
			break;
		case 'J':
			flags |=FLJIS83;
			break;
		case 'X':
			flags |=XWINKILL;
			break;
		case 'S':
			flags |= STATUS;
			Ss = optarg;
			break;
		case '1':
			flags |= SPLIT;
			wn = 1;
			break;
		case '2':
			flags |= SPLIT;
			wn = 2;
			break;

		case 'm':
			flags |= CRTLINES;
			ms = optarg;
			break;

		default:
			usage(-1);
			exit(1);
		}
		opterr = 0;
	}
	argc -= optind;
	argv += optind;

	if (flags == 0)
		flags |= SHOW;

	/************************************************
	 * open devices
	 ************************************************/
	switch (argc)
	{
	case 0:
		fname = "/dev/ttyv0";
		fd = open(fname, O_RDWR, 0777);
		if (fd < 0)
		{
			perror(fname);
			exit(1);
		}

		if (ioctl(fd, VT_GETACTIVE, &num))
		{
			perror("vsc");
			exit(1);
		}

		close(fd);

		if ((--num) > 0)
		{
			sprintf(buff, "/dev/ttyv%d", num);
			fname = buff;
		 }
		break;

	case 1:
	default:
		fname = get_dev_name(argv[0]);
		break;
	}

	fd = open(fname, O_RDWR, 0777);
	if (fd < 0)
	{
		perror(fname);
		exit(1);
	}

	/************************************************
	 * process the desired operations.
	 ************************************************/
	if (flags & KMESG)
		kmesg(fd);

	if (flags & CYRIX)
	{
		if (strstr("OFF", cs))
		{
			if (ioctl(fd, CYRIX_CACHE_OFF))
				perror("vsc");
			else
				printf("cpu: cache off\n");
		}
		else
		{
			if (ioctl(fd, CYRIX_CACHE_ON))
				perror("vsc");
			else
				printf("cpu: cache on\n");
		}
	}

	if (flags & KANJI)
	{
		int mode = findkey(ks);

		if (mode < 0)
			printf("vsc: no mode\n");
		else
			if (ioctl(fd, VSC_SET_KANJIMODE, &mode))
				perror("vsc");
	}

	if (flags & SAVER)
	{
		int tt = atoi(ss);

		if (tt < 0)
			printf("vsc: inappropriate time\n");
		if (ioctl(fd, VSC_SET_SAVERTIME, &tt))
				perror("vsc");
	}

	if (flags & KEYRESET)
		if (ioctl(fd, KBDRESET))
			perror("vsc");

	if (flags & FFJIS83)
	{
		int mode= (strstr("ON", js) ? 1 : 0);

		if (ioctl(fd, VSC_FORCE_JIS83, &mode))
			perror("vsc");
	}

	if (flags & GRAPH)
	{
		int mode= (strstr("ON", gs) ? 1 : 0);

		if (ioctl(fd, VSC_USE_VRAM, &mode))
			perror("vsc");
	}

	if (flags & FLRESET)
		if (ioctl(fd, VSC_RESET_FONT))
			perror("vsc");

	if (flags & FLJIS83)
		if (ioctl(fd, VSC_LOAD_JIS83))
			perror("vsc");

	if (flags & XWINKILL)
	{
		int mode = X_MODE_OFF;

		if (ioctl(fd, CONSOLE_X_MODE, &mode))
			perror("vsc");
	}

	if (flags & STATUS)
	{
		int mode = (strstr("ON",Ss) ? VSC_HAS_STATUS : 0);

		if (ioctl(fd, VSC_STATUS_LINE, &mode))
			perror("vsc");
	}

	if (flags & SPLIT)
	{
		int mode = ((wn == 1) ? 0 : VSC_SPLIT);

		if (ioctl(fd, VSC_SPLIT_SC, &mode))
			perror("vsc");
	}

	if (flags & RCRT)
	{
		int mode = 0;

		if (strstr("ON", Rs))
			mode = VSC_RCRT;
		if (ioctl(fd, VSC_SET_CRTMODE, &mode))
			perror("vsc");
	}

	if (flags & CRTLINES)
	{
		int mode = *ms - '0';

		if (mode < 0 || mode > VSC_MAX_SM)
			usage(0);
		else
			ioctl(fd, VSC_SET_CRTLINES, &mode);
	}

	if (flags & SHOW)
		usage(fd);

	if (flags & DEBUGINFO)
		debug(fd);

	/************************************************
	 * end
	 ************************************************/
	close(fd);
	exit(0);
}
