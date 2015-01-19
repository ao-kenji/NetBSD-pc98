/*	$NecBSD: diskgen.c,v 1.8.4.5 1999/09/09 05:19:27 honda Exp $	*/
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

#define	FSTYPENAMES
#define	DKTYPENAMES
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <disktab.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/dkio.h>
#include <errno.h>
#include <curses.h>
#include <string.h>
#include <machine/diskutil.h>

#define	EDIT_DOS
/*******************************************************************
 * declare
 *******************************************************************/
void display __P((FILE *f, struct disklabel *));
void request __P((void));
int setup __P((struct disklabel *, u_int, u_int)); 
u_char *getresp __P((u_char *, int));
int writelabel __P((struct disklabel *));
int newfs __P((struct disklabel *));
void get_avail __P((struct disklabel *, u_int *, u_int *));
void clear_partition __P((struct disklabel *, int));
void help __P((void));

void WinInit __P((void));
void WinClose __P((void));
void WinDisplayWarn __P((char *));
void WinSwichW __P((struct disklabel *));
void WinDisplayLabelTop __P((struct disklabel *, int));
void WinDisplayPartition __P((struct disklabel *));
void Redraw __P((struct disklabel *));
#ifdef	EDIT_DOS
void WinDisplayDosPartition __P((struct disklabel *, struct dos_partition *, int));
u_int convert __P((u_char *));
void dos_help __P((void));
void dos_request __P((void));
void boot_request __P((void));
#endif	/* EDIT_DOS */
void WinSwichRaw __P((char *));

void SigExit __P((int));

void main __P((int, u_char **));

/*******************************************************************
 * var
 *******************************************************************/
#define	NBPG	4096
struct disklabel mydisklabel, reqdisklabel, tmplabel;
u_char buf[NBPG];
u_char specname[NBPG];
u_char diskname[FILENAME_MAX + 1], *unitchar;
int unitcyl = 0;
int docmd = 1;
int dosslot = -1;
u_char *version = "1.0";
u_int reserve_sec, start_cyl, last_cyl;
struct dos_partition *dosp;

/*******************************************************************
 * curses stuff
 *******************************************************************/
WINDOW *pwin, *topwin, *cmdwin, *ewin;

void
WinInit(void)
{

	initscr();
	refresh();
	topwin = subwin(stdscr, 7, COLS - 1, 0, 0);
	pwin = subwin(stdscr, 13, COLS - 1, 7 - 1, 0);
	cmdwin = subwin(stdscr, 3, COLS - 1, 6 + 13, 0);
	ewin = subwin(stdscr, 1, COLS - 1, LINES - 2, 0);
}

void
WinClose(void)
{

	endwin();
}

void
SigExit(sig)
	int sig;
{
	
	WinClose();
	exit(1);
}

void
WinDisplayLabelTop(lp, dos)
	struct disklabel *lp;
	int dos;
{
	int row, col;

	col = 3;
	row = 1;

	wclear(topwin);
	box(topwin, '*', '-');

	wmove(topwin, row ++, col);
	if (dos)
		wprintw(topwin, "Dos Partition table");
	else if (dosslot < 0)
		wprintw(topwin, "NO DOS partitions found. Use WHOLE DISK!");
	else
		wprintw(topwin, 
			"Target dos cyl region %u - %u \treserve %u sectors",
			 start_cyl, last_cyl, reserve_sec);

	row ++;
	wmove(topwin, row ++, col);
	wprintw(topwin, "device:%s   type:%s  disk: %.*s", 
		diskname, 
		dktypenames[lp->d_type],
		sizeof(lp->d_typename), lp->d_typename);

	wmove(topwin, row ++, col);
	wprintw(topwin, 
		"bytes/sector:%ld   sectors/track:%ld   tracks/cylinder:%ld", 
		(long) lp->d_secsize, 
		(long) lp->d_nsectors,
		(long) lp->d_ntracks);

	wmove(topwin, row ++, col);
	wprintw(topwin, 
		"sectors/cylinder:%ld   cylinders:%ld   total sectors:%ld", 
		(long) lp->d_secpercyl,
		(long) lp->d_ncylinders,
		(long) lp->d_secperunit);

	wrefresh(topwin);
}

void
WinDisplayPartition(lp)
	struct disklabel *lp;
{
	int i, row, col;
	double total, used;
	u_int totali, usedi;
 	struct partition *pp = lp->d_partitions;
	
	col = row = 1;
	wclear(pwin);
	box(pwin, '*', '-');

	wmove(pwin, row ++, col);
	wprintw(pwin, "         size   offset    fstype   [fsize bsize cpg]");

	for (i = 0; i < MAXPARTITIONS; i++, pp++)
	{
		wmove(pwin, row, col);
		if (pp->p_size) 
		{
			double size;

			wprintw(pwin, "  %c: %8d %8d  ", 'a' + i,
				pp->p_size, pp->p_offset);
			if ((unsigned) pp->p_fstype < FSMAXTYPES)
				wprintw(pwin, 
					"%8.8s", fstypenames[pp->p_fstype]);
			else
				wprintw(pwin, "%8d", pp->p_fstype);

			switch (pp->p_fstype)
			{
			case FS_UNUSED:				/* XXX */
				wprintw(pwin, "    %5d %5d %5.5s ",
					pp->p_fsize, 
					pp->p_fsize * pp->p_frag, "");
				break;

			case FS_BSDFFS:
				wprintw(pwin, "    %5d %5d %3d ",
				    pp->p_fsize, pp->p_fsize * pp->p_frag,
				    pp->p_cpg);
				break;

			default:
				wprintw(pwin, "%20.20s", "");
				break;
			}

			wmove(pwin, row, col + 51);
			size = pp->p_size;
			size = size / (1024 * 1024 / lp->d_secsize);
			wprintw(pwin, " %6.1fMB", (float)size);
			wprintw(pwin, " (%5d",
				pp->p_offset / lp->d_secpercyl);
			if (pp->p_offset % lp->d_secpercyl)
				waddch(pwin, '*');
			else
				waddch(pwin, ' ');
			wprintw(pwin, "- %5d", (pp->p_offset + 
				pp->p_size + lp->d_secpercyl - 1) /
				lp->d_secpercyl - 1);
			if (pp->p_size % lp->d_secpercyl)
				waddch(pwin, '*');
			wprintw(pwin, ")");
		}
		else
			wprintw(pwin, 
				"  %c:            --- not defined ---  ", 
				'a' + i);
		row ++;
	}

	row ++;
	get_avail(lp, &totali, &usedi);

	total = totali;
	used = usedi;
	if (unitcyl == 0)
	{
		total = total * lp->d_secpercyl / (1024 * 1024 / lp->d_secsize);
		used = used * lp->d_secpercyl / (1024 * 1024 / lp->d_secsize);
	}

	wmove(pwin, row, col + 2);
	wprintw(pwin, "total %.2f%s used %.2f%s avail %.2f%s",
		(float) total, unitchar, 
		(float) used, unitchar, 
		(float) (total - used), unitchar);

	wrefresh(pwin);
}

void
Redraw(lp)
	struct disklabel *lp;
{

	clear();
	refresh();
	WinDisplayLabelTop(lp, 0);
	WinDisplayPartition(lp);
}

void
WinDisplayWarn(msg)
	char *msg;
{
	
	wmove(ewin, 0, 0);
	wclrtoeol(ewin);
	wprintw(ewin, "Error: %s", msg);
	wrefresh(ewin);
}	

u_char *
getresp(msg, offset)
	u_char *msg;
	int offset;
{
#define	BUFSIZE	1024
	static u_char reqbuf[BUFSIZE];
	int col, c, i, len = 0;
	
	col = 0;
	wmove(cmdwin, offset, col);
	if (offset == 0)
		wclear(cmdwin);
	else
		wclrtoeol(cmdwin);

	if (msg != NULL)
	{
		wprintw(cmdwin, "%s ", msg);
		len = strlen(msg);
	}
	wmove(cmdwin, offset, col + len + 1);

	wrefresh(cmdwin);

	fpurge(stdin);
	cbreak();
	noecho();
	for (i = 0; i < BUFSIZE - 1; )
	{
		c = wgetch(cmdwin);
		if (c == EOF || c == '\n' || c == ' ')
			break;
		if (c == '\b' || c == 0177)
		{
			if (i > 0)
			{
				i --;
				waddch(cmdwin, '\b');
				wdelch(cmdwin);
			}
		}
		else if (c >= 0x20)
		{
			reqbuf[i ++] = c;
			waddch(cmdwin, c);
		}
		wrefresh(cmdwin);
	}
	echo();
	nocbreak();
	reqbuf[i] = 0;
	return reqbuf;
}

void
WinSwichRaw(msg)
	char *msg;
{
	
	clear();
	move(0,0);
	refresh();
	if (msg != NULL)
	{
		printf("%s\n", msg);
		fflush(stdout);
	}
}

void
WinSwichW(lp)
	struct disklabel *lp;
{

	printf("PUSH RETURN to come back screen:");
	fflush(stdout);
	getchar();

	Redraw(lp);
}

/*******************************************************************
 * main
 *******************************************************************/
void
main(argc, argv)
	int argc;
	u_char **argv;
{
	extern int opterr, optind;
	extern char *optarg;
	int ch, dosedit = 0, bootflags = 0;

	if (getenv("TERM") == NULL)
	{
		printf("The TERM variable not specified.\n");
		printf("Please specify the TERM enviorment variable!\n");
		exit(1);
	}

	if (isatty(0) == 0)
	{
		printf("This program require a console device\n");
		exit(1);
	}

	opterr = 0;
	while ((ch = getopt(argc, (char * const *) argv, "sb")) != EOF)
	{

		switch (ch)
		{
		case 's':
			dosedit = 1;
			break;
		case 'b':
			bootflags = 1;
			break;
		}
		opterr = 0;
	}
	
	argc -= optind;
	argv += optind;

	if (argc != 1)
	{
		printf("Specify diskname (wd?, sd?, fd?, vnd?, etc.)\n");
		exit(1);
	}

	strcpy(diskname, argv[0]);
	if (bootflags == 1)
	{
		boot_request();
		exit(0);
	}
	else if (dosedit == 1)
	{
		dos_request();
	}
	else
	{
		request();
	}
	WinClose();
}

/*******************************************************************
 * util
 *******************************************************************/
void
help(void)
{
	clear();
	box(stdscr, '|', '-');
	move(1,3);printw("<HELP>");
	move(3,3); printw("p or print -- show current settings.");
	move(4,3); printw("label      -- write current disklabel and newfs.");
	move(5,3); printw("q or quit  -- exit the program.");
	move(6,3); printw("u or unit  -- change units MB <=> cyls (toggle).");
	move(7,3); printw("partition name (a, b, e, f, g, h) -- set partition size.");
	move(8,3); printw("getdiskbyname -- get entry data in disktab.");
	move(9,3); printw("restore    -- restore old disklabel data.");
	move(10,3);printw("select     -- select target disk.");
	move(LINES - 2,1); printw("push return:");
	refresh();
	getchar();
}

void
request(void)
{
	struct dos_partition *dp;
	struct disklabel *lp;
	u_char *ans;
	int fd, part;
	u_char devname[FILENAME_MAX + 1];

	signal(SIGINT, SigExit);
	signal(SIGHUP, SigExit);
	WinInit();

Loop:
	lp = &mydisklabel;
	bzero(lp, sizeof(*lp));

	sprintf(devname, "/dev/r%sd", diskname);
	fd = open(devname, O_RDWR, 0);
	if (fd < 0)
	{
		wprintw(ewin, "Error: can not open %s", devname);
		wrefresh(ewin);
		return;
	}

	/*
	 * setup initial disklabel
	 */
	if (ioctl(fd, DIOCGDINFO, lp) < 0)
	{
		wprintw(ewin, "Error: DIOCGDINFO failed");
		wrefresh(ewin);
		return;
	}

	lseek(fd, (off_t) 0, 0);
	if (read(fd, buf, NBPG) != NBPG)
	{
		wprintw(ewin, "Error: can not read boot sector");
		wrefresh(ewin);
		return;
	}
	close(fd);

	dosp = dp = (struct dos_partition *) (buf + DOSPARTOFF);
	for (part = 0; part < MAXPARTITIONS; part ++, dp ++)
		if (is_dos_partition_bsd(dp) != 0)
			break;

	if (strncmp(diskname, "wd", 2) == 0)
		reserve_sec = lp->d_secpercyl * 2;
	else
		reserve_sec = 0;

	if (part == MAXPARTITIONS)
	{
		start_cyl = 0;
		last_cyl = lp->d_ncylinders - 1;
		dosslot = -1;
	}
	else
	{
		start_cyl = dp->dp_scyl;
		if (dp->dp_ecyl >= lp->d_ncylinders)
			last_cyl = lp->d_ncylinders - 1;
		else
			last_cyl = dp->dp_ecyl;
		dosslot = part + 1;
	}

	unitchar = "MB";

	/*
	 * setup c parition.
	 */
	reqdisklabel = mydisklabel;

 	lp = &reqdisklabel;
	lp->d_partitions[2].p_offset = start_cyl * lp->d_secpercyl;
	lp->d_partitions[2].p_size =  (last_cyl - start_cyl + 1) * lp->d_secpercyl;

	/*
	 * initialize all user's partitions.
	 */
	lp->d_npartitions = 4;
	for (part = 0; part < MAXPARTITIONS; part ++)
	{
		if (part == 2 || part == 3)
			continue;
		clear_partition(lp, part);
	}

	Redraw(lp);

	while(1)
	{
		WinDisplayPartition(lp);

		ans = getresp("what>", 0);
		if (strcmp(ans, "quit") == 0 || strcmp(ans, "exit") == 0 || 
		    *ans == 'q' || *ans == 'x')
			break;

		if (strcmp(ans, "print") == 0 || *ans == 'p')
		{
			WinSwichRaw(NULL);
			display(stdout, lp);
			WinSwichW(lp);
			continue;
		}

		if (strcmp(ans, "unit") == 0 || *ans == 'u')
		{
			unitcyl ^= 1;
			if (unitcyl)
				unitchar = "cyls";
			else
				unitchar = "MB";
			continue;
		}

		if (strcmp(ans, "restore") == 0)
		{
			*lp = mydisklabel;
			Redraw(lp);
			continue;
		}

		if (strcmp(ans, "select") == 0)
		{
			ans = getresp("target disk name:", 1);
			if (*ans == 0)
				continue;

			sprintf(devname, "/dev/r%sd", ans);
			fd = open(devname, O_RDWR, 0);
			if (fd < 0)
			{
				WinDisplayWarn( "can not open device");
				continue;
			}
			close(fd);
			strcpy(diskname, ans);
			goto Loop;
		}
			
		if (strcmp(ans, "getdiskbyname") == 0)
		{
			struct disklabel *tlp;

			ans = getresp("disktype name:", 1);
			if (*ans == 0)
				continue;

			tlp = getdiskbyname(ans);
			if (tlp == NULL)
			{
				WinDisplayWarn( "can not find entry in disktab");
				continue;
			
			}

			*lp = *tlp;
			Redraw(lp);
			continue;
		}

		if (strcmp(ans, "label") == 0)
		{
			ans = getresp("label name:", 1);
			if (*ans != 0)
				strncpy(lp->d_typename, ans, sizeof(lp->d_typename) - 1);	
			if (writelabel(lp) != 0)
				continue;
			newfs(lp);
			continue;
	 	}		

		if (ans[1] == 0 && *ans >= 'a' && *ans <= 'h')
		{
			u_char c = *ans;
			u_int part = c - 'a';
			double size;

			if (part == 2 || part == 3)
			{
				WinDisplayWarn( "can not change c or d partitions");
				continue;
			}

			ans = getresp("partition size (* means all avail):", 1);
			if (*ans == 0)
				continue;

			if (*ans == '*')
			{
				u_int total, used;

				clear_partition(lp, part);
				get_avail(lp, &total, &used);
				size = (total - used) * lp->d_secpercyl;
			}
			else
			{
				size = atof(ans);
				if (unitcyl)
					size *= lp->d_secpercyl;
				else
					size *= (1024 * 1024 / lp->d_secsize);
			}
			setup(lp, part, (u_int) size);
			continue;
		}
		
		if (*ans == 0)
		{
			Redraw(lp);
			continue;
		}

		if (strcmp(ans, "help") == 0 || *ans == '?')
		{
			help();
			Redraw(lp);
		}
	}
}
			
void
clear_partition(lp, part)
	struct disklabel *lp;
	int part;
{
	struct partition *pp;
	
	pp = &lp->d_partitions[part];
	pp->p_size = 0;
	pp->p_fstype = FS_UNUSED;
	pp->p_offset = 0;
	pp->p_fsize = 0;
	pp->p_frag = 0;
	pp->p_cpg = 0;
}

void
get_avail(lp, totalp, usedp)
	struct disklabel *lp;
	u_int *totalp, *usedp;
{
	u_int ssec, part;
	
	for (ssec = part = 0; part < MAXPARTITIONS; part ++)
	{
		if (part == 2 || part == 3)
			continue;
		
		if (lp->d_partitions[part].p_size == 0)
			continue;
		ssec += lp->d_partitions[part].p_size;
	}

	*totalp = lp->d_partitions[2].p_size / lp->d_secpercyl;
	*usedp = (ssec + reserve_sec) / lp->d_secpercyl;
}

int
setup(tlp, tpart, secs)
	struct disklabel *tlp;
	u_int tpart, secs;
{
	u_int part, lastpart, ssec;
	struct disklabel *lp = &tmplabel;
	struct partition *pp, *ppc;

	if (tpart == 2 || tpart == 3)
		return 1;

	*lp = *tlp;
	secs -= secs % lp->d_secpercyl;
	lp->d_partitions[tpart].p_size = secs;

	ppc = &lp->d_partitions[2];
	for (ssec = part = lastpart = 0; part < MAXPARTITIONS; part ++)
	{
		if (part == 2 || part == 3)
			continue;
		
		pp = &lp->d_partitions[part];
		if (pp->p_size == 0)
		{
			clear_partition(lp, part);
			continue;
		}
		
		pp->p_offset = ssec + ppc->p_offset;
		if (ssec + reserve_sec + pp->p_size >= ppc->p_size)
			pp->p_size = ppc->p_size - ssec - reserve_sec;

		ssec += pp->p_size;

		if (part == 1)
			pp->p_fstype = FS_SWAP;
		else
		{
			pp->p_fstype = FS_BSDFFS;
			pp->p_fsize = 1024;
			pp->p_frag = 8;
			pp->p_cpg = 16;
		}
		lastpart = part;
	}

	for ( ; part < MAXPARTITIONS; part ++)
	{
		if (part == 2 || part == 3)
			continue;
		clear_partition(lp, part);
	}

	if (lastpart < 3)
		lp->d_npartitions = 4;
	else		
		lp->d_npartitions = lastpart + 1;

	*tlp = *lp;
	return 0;
}

void
display(f, lp)
	FILE *f;
	struct disklabel *lp;
{
	int i, j;
	struct partition *pp;

	if ((unsigned) lp->d_type < DKMAXTYPES)
		fprintf(f, "type: %s\n", dktypenames[lp->d_type]);
	else
		fprintf(f, "type: %d\n", lp->d_type);
	fprintf(f, "disk: %.*s\n", (int) sizeof(lp->d_typename), lp->d_typename);
	fprintf(f, "label: %.*s\n", (int) sizeof(lp->d_packname), lp->d_packname);
	fprintf(f, "flags:");
	if (lp->d_flags & D_REMOVABLE)
		fprintf(f, " removable");
	if (lp->d_flags & D_ECC)
		fprintf(f, " ecc");
	if (lp->d_flags & D_BADSECT)
		fprintf(f, " badsect");
	fprintf(f, "\n");
	fprintf(f, "bytes/sector: %ld\n", (long) lp->d_secsize);
	fprintf(f, "sectors/track: %ld\n", (long) lp->d_nsectors);
	fprintf(f, "tracks/cylinder: %ld\n", (long) lp->d_ntracks);
	fprintf(f, "sectors/cylinder: %ld\n", (long) lp->d_secpercyl);
	fprintf(f, "cylinders: %ld\n", (long) lp->d_ncylinders);
	fprintf(f, "total sectors: %ld\n", (long) lp->d_secperunit);
	fprintf(f, "rpm: %ld\n", (long) lp->d_rpm);
	fprintf(f, "interleave: %ld\n", (long) lp->d_interleave);
	fprintf(f, "trackskew: %ld\n", (long) lp->d_trackskew);
	fprintf(f, "cylinderskew: %ld\n", (long) lp->d_cylskew);
	fprintf(f, "headswitch: %ld\t\t# milliseconds\n",
		(long) lp->d_headswitch);
	fprintf(f, "track-to-track seek: %ld\t# milliseconds\n",
		(long) lp->d_trkseek);
	fprintf(f, "drivedata: ");
	for (i = NDDATA - 1; i >= 0; i--)
		if (lp->d_drivedata[i])
			break;
	if (i < 0)
		i = 0;
	for (j = 0; j <= i; j++)
		fprintf(f, "%d ", lp->d_drivedata[j]);
	fprintf(f, "\n\n%d partitions:\n", lp->d_npartitions);

	fprintf(f,
	    "#        size   offset    fstype   [fsize bsize   cpg]\n");
	pp = lp->d_partitions;
	for (i = 0; i < lp->d_npartitions; i++, pp++) {
		if (pp->p_size) {
			fprintf(f, "  %c: %8d %8d  ", 'a' + i,
			   pp->p_size, pp->p_offset);
			if ((unsigned) pp->p_fstype < FSMAXTYPES)
				fprintf(f, "%8.8s", fstypenames[pp->p_fstype]);
			else
				fprintf(f, "%8d", pp->p_fstype);
			switch (pp->p_fstype) {

			case FS_UNUSED:				/* XXX */
				fprintf(f, "    %5d %5d %5.5s ",
				    pp->p_fsize, pp->p_fsize * pp->p_frag, "");
				break;

			case FS_BSDFFS:
				fprintf(f, "    %5d %5d %5d ",
				    pp->p_fsize, pp->p_fsize * pp->p_frag,
				    pp->p_cpg);
				break;

			default:
				fprintf(f, "%20.20s", "");
				break;
			}
			fprintf(f, "\t# (Cyl. %4d",
			    pp->p_offset / lp->d_secpercyl);
			if (pp->p_offset % lp->d_secpercyl)
			    putc('*', f);
			else
			    putc(' ', f);
			fprintf(f, "- %d",
			    (pp->p_offset + 
			    pp->p_size + lp->d_secpercyl - 1) /
			    lp->d_secpercyl - 1);
			if (pp->p_size % lp->d_secpercyl)
			    putc('*', f);
			fprintf(f, ")\n");
		}
	}
	fflush(f);
}

int
writelabel(lp)
	struct disklabel *lp;
{
	u_char *ans, *options;
	FILE *f;
	int error;
	u_char tempfile[FILENAME_MAX];
	u_char cmd[FILENAME_MAX];
	
	ans = getresp("write disklabel? (y/n)", 1);
	if (*ans != 'y')
		return 1;
#ifdef	USE_OLD_BOOT
	ans = getresp("install bootloader also? (y/n)", 2);
	if (*ans == 'y')
		options = "-r -R -B";
	else
#endif	/* USE_OLD_BOOT */
		options = "-r -R";

	strcpy(tempfile, "/tmp/disklabel.XXXX");
	mkstemp(tempfile);
	f = fopen(tempfile, "w+");
	if (f == NULL)
	{
		WinDisplayWarn("can not make temp file");
		return 1;
	}

	display(f, lp);
	fclose(f);

	sprintf(cmd, "/sbin/disklabel %s %s %s\n", options, diskname, tempfile);	
	WinSwichRaw("writing disklabel ....");
	if (docmd != 0)
		error = system(cmd);
	else
	{
		printf("%s", cmd);
		error = 0;
	}
	WinSwichW(lp);

	return error;
}	

int
newfs(lp)
	struct disklabel *lp;
{
	u_char *ans, cmd[FILENAME_MAX];
	struct partition *pp;
	int part, error = 0;
	
	ans = getresp("newfs now? (y/n)", 1);
	if (*ans != 'y')
		return 1;

	WinSwichRaw("making BSD file systems  ....");
	for (part = 0; part < lp->d_npartitions; part ++)
	{
		if (part == 2 || part == 3)
			continue;	
		
		pp = &lp->d_partitions[part];
		if (pp->p_fstype != FS_BSDFFS || pp->p_fsize == 0)
			continue;

		sprintf(cmd, "/sbin/newfs /dev/r%s%c\n", diskname, part + 'a');
		if (docmd != 0)
		{
			error = system(cmd);
			if (error != 0)
				break;
		}
		else
			printf("%s", cmd);
	}
	WinSwichW(lp);

#ifndef	USE_OLD_BOOT
	if (error != 0)
		return error;

	ans = getresp("install bootloader also? (y/n)", 2);
	if (*ans == 'y')
	{
		WinSwichRaw("install new boot loader  ....");
		sprintf(cmd, "/usr/mdec/installboot -v %s /dev/r%sa\n",
			"/usr/mdec/biosboot.sym", diskname);
		if (docmd != 0)
			error = system(cmd);
		else
			printf("%s", cmd);
		WinSwichW(lp);
	}
#endif	/* !USE_OLD_BOOT */

	return error;
}

#ifdef	EDIT_DOS
void
WinDisplayDosPartition(lp, dp, sel)
	struct disklabel *lp;
	struct dos_partition *dp;
	int sel;
{
	sysid_t id;
	int i, row, col, ocol;
	double size;
	u_char dp_name[18];
	
	col = row = 1;
	wclear(pwin);
	box(pwin, '*', '-');

	WinDisplayLabelTop(lp, 1);
	wmove(pwin, row ++, col);
	wprintw(pwin,
	"      System Name       sid  mid    From    To(cyl)      Size");

	ocol = col;
	for (i = 0; i < NDOSPART; i++, dp++)
	{
		id = dos_partition_read_sysid(dp);
		if (id == 0)
			break;

		col = ocol;
		row ++;

		wmove(pwin, row, col);
		if (sel == i)
			wprintw(pwin, ">>");
		else
			wprintw(pwin, "  ");

		strncpy(dp_name, dp->dp_name, 16);
		dp_name[16] = 0;
		wprintw(pwin, "%2d  %s", i, dp_name);

		col += 23;
		wmove(pwin, row, col);
		wprintw(pwin, "0x%2x ", (u_int) sysid_to_sid(id));
		wprintw(pwin, "0x%2x ", (u_int) sysid_to_mid(id));

		wprintw(pwin, "%7d   %7d  ", dp->dp_scyl, dp->dp_ecyl);

		size = disk_size_sector(lp, dp);
		size = size / (1024 * 1024 / lp->d_secsize);
		wprintw(pwin, "%7.2fMB  ", size);
	}
	wrefresh(pwin);
}

u_int
convert(s)
	u_char *s;
{
	u_int num, val = 0, radix = 10;

	if (strncmp(s, "0x", 2) == 0)
		radix = 16;

	for (s; *s != 0; s++)
	{
		if (*s >= '0' && *s <= '9')
			num = *s - '0';
		else if (radix == 16 && *s <= 'f' && *s >= 'a')
			num = *s - 'a' + 10;
		else
			continue;

		val = val * radix;
		val += num;
	}

	return val;
}
	
void
dos_help(void)
{
	clear();
	box(stdscr, '|', '-');
	move(1,3);printw("<HELP>");
	move(2,3); printw("q or quit -- exit the program.");
	move(3,3); printw("x or exit -- exit the program.");
	move(4,3); printw("select    -- select target DOS partition.");
	move(5,3); printw("clear     -- clear current setting.");
	move(6,3); printw("NetBSD    -- change target DOS partition to NetBSD/pc98 partition.");
	move(7,3); printw("sid       -- change sid of target DOS partition.");
	move(8,3); printw("mid       -- change mid of target DOS partition.");
	move(9,3); printw("name      -- change system name of target DOS partition.");
	move(10,3);printw("from      -- change a start cylinder of target DOS partition.");
	move(11,3);printw("to        -- change a end cylinder of target DOS partition.");
	move(12,3);printw("delete    -- delete the selected partition.");
	move(13,3);printw("write     -- write current DOS partition table.");
	move(LINES - 2,1); printw("push return:");
	refresh();
	getchar();
}

void
dos_request(void)
{
	struct dos_partition *dp, *tdp;
	struct disklabel *lp;
	sysid_t id;
	u_char *ans;
	int flag, sel, fd, part;
	u_char devname[FILENAME_MAX + 1];

	signal(SIGINT, SigExit);
	signal(SIGHUP, SigExit);
	WinInit();

	sprintf(devname, "/dev/r%sd", diskname);
	fd = open(devname, O_RDWR, 0);
	if (fd < 0)
	{
		wprintw(ewin, "Error: can not open %s", devname);
		wrefresh(ewin);
		return;
	}

again:
	sel = -1;
	lp = &mydisklabel;
	bzero(lp, sizeof(*lp));

	/*
	 * setup initial disklabel
	 */
	if (ioctl(fd, DIOCGDINFO, lp) < 0)
	{
		wprintw(ewin, "Error: DIOCGDINFO failed");
		wrefresh(ewin);
		return;
	}

	lseek(fd, (off_t) 0, 0);
	if (read(fd, buf, NBPG) != NBPG)
	{
		wprintw(ewin, "Error: can not read boot sector");
		wrefresh(ewin);
		return;
	}

	dp = (struct dos_partition *) (buf + DOSPARTOFF);
	tdp = NULL;

	while (1)
	{
		WinDisplayDosPartition(lp, dp, sel);

		ans = getresp("what>", 0);
		if (strcmp(ans, "quit") == 0 || strcmp(ans, "exit") == 0 || 
		    *ans == 'q' || *ans == 'x')
			break;

		if (*ans == 0)
		{
			Redraw(lp);
			continue;
		}
		if (strcmp(ans, "help") == 0 || *ans == '?')
		{
			dos_help();
			Redraw(lp);
			continue;
		}

		if (strcmp(ans, "select") == 0)
		{
			ans = getresp("target partition:", 1);
			if (*ans == 0)
				continue;
			if (ans[0] < '0' || ans[0] > '7')
				WinDisplayWarn( "invalid dos partition");

			sel = ans[0] - '0';
			tdp = &dp[sel];
		}

		if (tdp == NULL)
		{
			WinDisplayWarn( "select partition please!");
			continue;
		}

		if (strcmp(ans, "clear") == 0)
			goto again;
		if (strcmp(ans, "delete") == 0)
		{
			int no, i;

			/* packing */
			for(no = sel, i = sel + 1; i < NDOSPART; i ++, no ++)
				bcopy(dp + i, dp + no, sizeof(*tdp));
			for ( ; no < NDOSPART; no ++)
				memset(dp + no, 0, sizeof(*tdp));
			sel = -1;
			tdp = NULL;
			continue;
		}

		if (strcmp(ans, "NetBSD") == 0)
		{
			strncpy(tdp->dp_name, "NetBSD/pc98", 16);
			id = sysid_make_id(DOSMID_NETBSD, DOSSID_NETBSD);
			dos_partition_write_sysid(tdp, id);
		}

		if (strcmp(ans, "sid") == 0)
		{
			ans = getresp("sid:", 1);
			if (*ans == 0)
				continue;
			
			id = dos_partition_read_sysid(tdp);
			id = sysid_make_id(sysid_to_mid(id), convert(ans));
			dos_partition_write_sysid(tdp, id);
		}

		if (strcmp(ans, "mid") == 0)
		{
			ans = getresp("mid:", 1);
			if (*ans == 0)
				continue;
			
			id = dos_partition_read_sysid(tdp);
			id = sysid_make_id(convert(ans), sysid_to_sid(id));
			dos_partition_write_sysid(tdp, id);
		}

		if (strcmp(ans, "name") == 0)
		{
			ans = getresp("system name:", 1);
			if (*ans == 0)
				continue;
			strncpy(tdp->dp_name, ans, 16);
		}

		if (strcmp(ans, "from") == 0)
		{
			ans = getresp("start cylinder:", 1);
			if (*ans == 0)
				continue;

			tdp->dp_scyl = convert(ans);
		}

		if (strcmp(ans, "to") == 0)
		{
			ans = getresp("end cylinder:", 1);
			if (*ans == 0)
				continue;

			tdp->dp_ecyl = convert(ans);
		}

		if (strcmp(ans, "write") == 0)
		{
			ans = getresp("really write(y/n):", 1);
			if (*ans == 0 || *ans != 'y')
				continue;

			flag = 1;
			if (ioctl(fd, DIOCWLABEL, &flag) < 0)
			{
				WinDisplayWarn( "DIOCWLABEL fail");
				continue;
			}

			if (lseek(fd, (off_t) 0, 0) != (off_t) 0)
			{
				WinDisplayWarn( "seek fail");
				continue;
			}
	
			if (write(fd, buf, NBPG) != NBPG)
				WinDisplayWarn( "write fail");
			flag = 0;
			ioctl(fd, DIOCWLABEL, &flag);
		}
	}
}
#endif	/* EDIT_DOS */

#define	_KERNEL
#include <machine/bootinfo.h>
#include <machine/bootflags.h>

void
boot_request(void)
{
	struct dos_partition *dp;
	struct disklabel *lp;
	char *ans;
	int fd, c, part;
	u_char devname[FILENAME_MAX + 1];
	struct biosboot_header *bh;
	off_t offset;

	lp = &mydisklabel;
	bzero(lp, sizeof(*lp));

	sprintf(devname, "/dev/r%sd", diskname);
	fd = open(devname, O_RDWR, 0);
	if (fd < 0)
	{
		printf("Error: can not open %s\n", devname);
		return;
	}

	/*
	 * setup initial disklabel
	 */
	if (ioctl(fd, DIOCGDINFO, lp) < 0)
	{
		printf("Error: DIOCGDINFO failed\n");
		return;
	}

	lseek(fd, (off_t) 0, 0);
	if (read(fd, buf, NBPG) != NBPG)
	{
		printf("Error: can not read boot sector\n");
		return;
	}

	dosp = dp = (struct dos_partition *) (buf + DOSPARTOFF);
	for (part = 0; part < MAXPARTITIONS; part ++, dp ++)
		if (is_dos_partition_bsd(dp) != 0)
			break;

	if (part == MAXPARTITIONS)
	{
		printf("Error: NO boot section\n");
		return;
	}

	offset = dp->dp_scyl;
	offset = offset * lp->d_secpercyl;
	offset = offset * lp->d_secsize;

	if (lseek(fd, offset , 0) != offset)
	{
		printf("Error: lseek failed\n");
		return;
	}
	if (read(fd, buf, NBPG) != NBPG)
	{
		printf("Error: can not read boot sector\n");
		return;
	}

	bh = (void *) buf;
	if (bh->bh_signature != BOOTFLAGS_SIGNATURE)
	{
		printf("Error: signature mismatch\n");
		return;
	}

	if (bh->bh_bootflags & BOOTFLAGS_FLAGS_BIOSGEOM)
		ans = "yes";
	else
		ans = "no";
	printf("lookup bios geometry = %s\n", ans);

	printf("change the flags? y/n: ");
	fflush(NULL);
	fpurge(stdin);
	c = getchar();
	if (c == 'y')
		bh->bh_bootflags ^= BOOTFLAGS_FLAGS_BIOSGEOM;
	
	printf("write back the flags? y/n: ");
	fflush(NULL);
	fpurge(stdin);
	c = getchar();
	if (c == 'y')
	{
		int flag;

		if (lseek(fd, offset , 0) != offset)
		{
			printf("Error: lseek failed\n");
			return;
		}

		flag = 1;
		if (ioctl(fd, DIOCWLABEL, &flag) < 0)
		{
			printf("Error: write failed\n");
			return;
		}

		if (write(fd, buf, NBPG) != NBPG)
		{
			printf("Error: can not write boot sector\n");
			return;
		}

		flag = 0;
		ioctl(fd, DIOCWLABEL, &flag);
	}
}
