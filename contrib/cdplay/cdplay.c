/*	$NecBSD: cdplay.c,v 1.16 1998/09/26 05:12:56 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <stdlib.h>
#include <unistd.h>
#include <curses.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/cdio.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>

/************************************************************
 * variables
 ************************************************************/
int ttyFd;
fd_set fd_rsel;

int cd_fd;
u_char dvname[FILENAME_MAX];
int ntocent, ctrack, index, paused, play, loopon;

#define	MAXTRK	256
struct ioc_toc_header toch;
struct ioc_vol vol;
struct cd_toc_entry tocent[MAXTRK];
u_int RelPos;
int RelPosExp;
struct toc_info {
	u_int abs;
	u_int sz;
	u_int pflag;
} toc_info[MAXTRK];

u_char *dvtab[] = {
 "cd0", "cd1", "cd2", "cd3", "cd4", "cd5", "cd6", "cd7", NULL,
};

/************************************************************
 * curses emulation staff
 ************************************************************/
#define	A_NORMAL	0x10
#define	CURSOR_OFF	0x11
#define	CURSOR_ON	0x12
#define COLOR_RUN	0x13
#define	NON_REVERSE	0x20
#define	RED	1
#define	GREEN	2
#define	YELLOW	3
#define	BLUE	4
#define CYAN	5
#define	SKY	6

static void add_string __P((int, int, u_char *));
static void add_nstring __P((int, int, u_char *, u_int));
static void Refresh __P((void));
static void attrset __P((int));
void write_bar __P((u_int, u_char));
void show_version __P((void));

static u_int cpos;
#define	TBUFSZ	8192
static u_char textbuf[TBUFSZ];
static u_char cbuf[TBUFSZ];

static void
add_string(x, y, str)
	int x, y;
	u_char *str;
{

	if (str == NULL)
		return;

	add_nstring(x, y, str, strlen(str));
}

static void
add_nstring(x, y, str, len)
	int x, y;
	u_char *str;
	u_int len;
{
	u_char gob[32];

	if (cpos + len + sizeof(gob) >= TBUFSZ)
		Refresh();
		
	if (x >= 0)
	{
		sprintf(gob, "\033[%d;%dH", x + 1, y + 1);
		strcpy(&cbuf[cpos], gob);
		cpos += strlen(gob);
	}

	strncpy(&cbuf[cpos], str, len);
	cpos += len;
}

static void
Refresh(void)
{

	write(ttyFd, cbuf, cpos);
	cpos = 0;
}

static void
attrset(attr)
	int attr;
{
	u_char attrbuf[0x20];

	switch (attr)
	{
	case A_NORMAL:
		sprintf(attrbuf, "\033[m");
		break;

	case CURSOR_OFF:
		sprintf(attrbuf, "\033[?25l");
		break;

	case CURSOR_ON:
		sprintf(attrbuf, "\033[?25h");
		break;

	case COLOR_RUN:
		sprintf(attrbuf, "\033[0;33;27m");
		break;

	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		sprintf(attrbuf, "\033[%d;7m", 30 + attr);
		break;

	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27:
		sprintf(attrbuf, "\033[0;%d;27m", attr - 2);
		break;
	default:
		return;
	}

	add_string(-1, -1, attrbuf);
}

void
close_show()
{

	refresh();
	endwin();

	attrset(CURSOR_ON);
	attrset(A_NORMAL);
	Refresh();

	close(ttyFd);
	resetty();
}

void
setup_show()
{

	initscr();
	savetty();

	raw();
	noecho();
	clear();

	FD_ZERO(&fd_rsel);
	ttyFd = open("/dev/tty", O_RDWR, 0);
	if (ttyFd < 0)
		exit(0);

	attrset(A_NORMAL);
	attrset(CURSOR_OFF);
	Refresh();
	refresh();
	show_version();
}

#define	clear_bar()	{ write_bar(0, ' '); }

void
write_bar(pos, mark)
	u_int pos;
	u_char mark;
{
	int i, start;

	pos = pos % COLS;

	attrset(YELLOW);
	for (i = 0; i < pos; i ++)
		textbuf[i] = mark;
	textbuf[i] = 0;
	add_string(LINES - 1, 0, textbuf);

	attrset(BLUE);
	start = i;
	for (i = 0; i < COLS - start; i ++)
		textbuf[i] = ' ';
	textbuf[i] = 0;
	add_string(LINES - 1, start, textbuf);

	attrset(A_NORMAL);
}

/************************************************************
 * cdrom staff
 ************************************************************/
void print_toc __P((void));
int setvol __P((void));
int getvol __P((void));
void print_status __P((void));
int play_msf __P((void));
int read_toc __P((void));
void print_toc __P((void));
int cdplay_clean __P((void));
int cdplay_init __P((void));
int limit_check __P((void));
int check_track __P((int));
void play_stop __P((void));
void clgs __P((void));
u_int msf2abs __P((u_char *));
int play_msf_rel __P((u_int));
int play_ioctl __P((struct ioc_play_msf *));
void play_step __P((int step));

void
show_version(void)
{

	sprintf(textbuf, "- Cdplayer Version 1.0 -");
	add_string(0, 0, textbuf);
	Refresh();
}

u_int
msf2abs(s)
	u_char *s;
{
	u_int min, sec, frame;

	min = s[1];
	sec = s[2];
	frame = s[3];

	return min * 60 * 75 + sec * 75 + frame;
}

void
clgs(void)
{

	show_version();
	clear_bar();
	print_toc();
}

void
play_stop(void)
{

	if (play)
	{
		ioctl(cd_fd, CDIOCSTOP);
		play = 0;
	}

	paused = index = 0;

	clgs();
}

int
setvol(void)
{

	return ioctl(cd_fd, CDIOCSETVOL, &vol);
}

int
getvol(void)
{

	return ioctl(cd_fd, CDIOCGETVOL, &vol);
}

int
check_track(start)
	int start;
{
	int i;

	for (i = start; i < ntocent && toc_info[i].pflag == 0; i ++)
		;

	if (i == ntocent)
	{
		if (loopon)
		{
			for (i = 0; i <= start - 1 && toc_info[i].pflag == 0; i ++)
				;

			if (i <= start - 1)
				return i;
		}
		return -1;
	}

	return i;
}

void
print_status(void)
{
	struct ioc_read_subchannel s;
	struct cd_sub_channel_info data;
	int sel;
	u_int trk, min, sec, frame, status, pos, i;
	int totaln, total;
	static u_int fg;

	bzero(&s, sizeof(s));
	s.data = &data;
	s.data_len = sizeof (data);
	s.address_format = CD_MSF_FORMAT;
	s.data_format = CD_CURRENT_POSITION;

	if (ioctl(cd_fd, CDIOCREADSUBCHANNEL, (char *) &s) < 0)
	{
		cdplay_clean();
		return;
	}

	status = s.data->header.audio_status;
	if (play && status != CD_AS_PLAY_IN_PROGRESS)
	{
		if (paused)
			return;

		index = check_track(index + 1);
		if (index < 0)
		{
			play_stop();
			return;
		}

		play_msf();

		if (ioctl(cd_fd, CDIOCREADSUBCHANNEL, (char *) &s) < 0)
		{
			cdplay_clean();
			return;
		}
	}

	getvol();

	trk = s.data->what.position.track_number;
	min = s.data->what.position.reladdr.msf.minute;
	sec = s.data->what.position.reladdr.msf.second;
	frame = s.data->what.position.reladdr.msf.frame;
	status = s.data->header.audio_status;
	sel = trk - toch.starting_track;
	RelPos = msf2abs(s.data->what.position.reladdr.addr);

	if (ctrack != trk)
	{
		ctrack = trk;
		print_toc();
	}

	if (play && status == CD_AS_PLAY_IN_PROGRESS && sel >= 0 &&
	    toc_info[sel].sz > 0)
		pos = RelPos * (u_int) COLS / toc_info[sel].sz + 1;
	else
		pos = 0;

	sprintf(textbuf, "device(%s) status(%2d) track(%03d) lv(%03d) rv(%03d)",
		dvname, status, trk, (u_int) vol.vol[0], (u_int) vol.vol[1]);

	frame = frame / 10 * 10;
	add_string(1, 0, textbuf);
	attrset(COLOR_RUN);
	sprintf(textbuf," %02d.%02d.%02d", min, sec, 00);
	add_string(1, 60, textbuf);

	if (play)
	{
		attrset(A_NORMAL);
		add_string(1, 70 + (fg ++ % 10), " ");
		attrset((fg % 6) + 1);
		add_string(1, 70 + (fg % 10), " ");
		write_bar(pos, '*');
	}
	attrset(A_NORMAL);

	Refresh();
}

void
play_step(step)
	int step;
{
	u_int pos;

	if (RelPosExp == 0)
		RelPosExp = RelPos;

	RelPosExp += step;

	if (RelPosExp <= 1)
		RelPosExp = 1;
	if (RelPosExp >= toc_info[index].sz - 10 * 75)
		RelPosExp = toc_info[index].sz - 10 * 75;

	pos = RelPosExp * (u_int) COLS / toc_info[index].sz + 1;
	write_bar(pos, '#');
	Refresh();
}

int
play_msf_rel(relpos)
	u_int relpos;
{
	struct ioc_play_msf a;
	u_int abspos;

	if (play == 0 || paused)
		return;

	if (relpos >= toc_info[index].sz)
		relpos >= toc_info[index].sz - 1;

	abspos = relpos + toc_info[index].abs;

	a.start_m = abspos / (75 * 60);
	a.start_s = (abspos / 75) % 60;
	a.start_f = abspos % 75;

	return play_ioctl(&a);
}

int
play_msf(void)
{
	struct ioc_play_msf a;

	clgs();
	if ((tocent[index].control & 0x04) || paused || play == 0)
		return EINVAL;

	a.start_m = tocent[index].addr.addr[1];
	a.start_s = tocent[index].addr.addr[2];
	a.start_f = tocent[index].addr.addr[3];

	return play_ioctl(&a);
}

int
play_ioctl(a)
	struct ioc_play_msf *a;
{

	a->end_m = tocent[index + 1].addr.addr[1];
	a->end_s = tocent[index + 1].addr.addr[2];
	a->end_f = tocent[index + 1].addr.addr[3] - 1;

	if (a->end_f >= 75)
	{
		a->end_f = 74;
		a->end_s --;
	}

	if (a->end_s >= 60)
	{
		a->end_s = 59;
		a->end_m --;
	}

	return ioctl(cd_fd, CDIOCPLAYMSF, a);
}

int
read_toc(void)
{
	struct ioc_read_toc_entry t;
	int i, n, error;

	error = ioctl(cd_fd, CDIOREADTOCHEADER, &toch);
	if (error < 0)
		return EIO;

	ntocent =  toch.ending_track - toch.starting_track + 1;

	t.address_format = CD_MSF_FORMAT;
	t.starting_track = 1;
	t.data_len = (sizeof(struct cd_toc_entry) * (ntocent + 2));
	t.data = &tocent[0];

	error = ioctl(cd_fd, CDIOREADTOCENTRYS, (char *) &t);
	if (error < 0)
		return EIO;

	tocent[ntocent].track = 255;

	return 0;
}

#define	LINEBASE 3
#define XGAP	25
void
print_toc(void)
{
	int i, y, x;

	for (i = 0; i <=ntocent; i ++)
	{
		u_char tpant[5];

		x = (i / ((LINES - LINEBASE - 1) - 1)) * XGAP + 1;
		y = i % ((LINES - LINEBASE - 1) - 1);

		if (toc_info[i].pflag)
			attrset(A_NORMAL);
		else
			attrset(CYAN | NON_REVERSE);

		if (tocent[i].track == 0xff)
			sprintf(tpant, loopon ? "Loop" : "End ");
		else
			sprintf(tpant, "%03d:", tocent[i].track);

		sprintf (textbuf, "%s%s %02d.%02d.%02d (%s)",
			tpant,
			toc_info[i].pflag ? "O" : "X",
			tocent[i].addr.addr[1],
			tocent[i].addr.addr[2],
			tocent[i].addr.addr[3],
			(tocent[i].control & 0x04) ? "data" :  "audi");

		if (tocent[i].track == ctrack)
		{
			if (paused)
				attrset(RED | NON_REVERSE);
			else
				attrset(SKY | NON_REVERSE);
		}

		if (i == index && tocent[index].track != ctrack)
			attrset(SKY);

		add_string(y + LINEBASE, x, textbuf);
	}
	attrset(A_NORMAL);

	Refresh();
}

int
cdplay_clean(void)
{
	int i;

	if (cd_fd < 0)
		return EINVAL;

	close(cd_fd);

	cd_fd = -1;
	paused = play = index = 0;

	for (i = 0; i < MAXTRK; i ++)
		toc_info[i].pflag = 0;

	clgs();
	return 0;
}

int
cdplay_init(void)
{
	int i, fd, error;
	u_int start;

	cd_fd = -1;

	fd = open(dvname, O_RDWR, 0);
	if (fd < 0)
		return ENODEV;

	cd_fd = fd;

	error = read_toc();
	if (error)
	{
		close(fd);
		cd_fd = -1;
		return error;
	}

	ctrack = -1;
	loopon = 1;
	index = play = paused = 0;
	RelPosExp = 0;

	for (i = 0, start = msf2abs(tocent[0].addr.addr); i < ntocent; i ++)
	{
		toc_info[i].abs = start;
		start = msf2abs(tocent[i + 1].addr.addr);
		toc_info[i].sz = start - toc_info[i].abs;

		if ((tocent[i].control & 0x04) == 0)
			toc_info[i].pflag = 1;
	}

	clear();
	refresh();
	clgs();

	return 0;
}

int
limit_check(void)
{

	if(index >= ntocent)
		index = 0;

	if(index < 0)
		index = ntocent - 1;

	return 0;
}

/************************************************************
 * signal (suspend, sigwinch)
 ************************************************************/
void
sig_stop()
{

	if (play && paused == 0)
	{
		ioctl(cd_fd, CDIOCPAUSE);
		paused = 1;
	}

	close_show();
	kill(getpid(), SIGTSTP);
}

void
sig_cont()
{

	if (play && paused == 1)
	{
		if (ioctl(cd_fd, CDIOCRESUME))
			cdplay_clean();
		paused = 0;
	}

	setup_show();
	clgs();
}

void
sig_winch()
{

	close_show();
	setup_show();
	clgs();
}

/************************************************************
 * main
 ************************************************************/
int
main(argc, argv)
	int argc;
	char **argv;
{
	struct timeval tout;
	int rup, update, error;
	int dvindex, i, tfd;
	u_char ch;

	if (argc > 1)
	{
		for (dvindex = 0; dvtab[dvindex]; dvindex ++)
			if (strcmp(dvtab[dvindex], argv[1]) == 0)
				break;

		if (dvtab[dvindex] == NULL)
		{
			printf("no device file\n");
			exit(1);
		}

		sprintf(dvname, "/dev/r%sa", dvtab[dvindex]);
	}
	else
		sprintf(dvname, "/dev/rcd0a");

	if ((tfd = open(dvname, O_RDWR, 0)) < 0)
	{
		perror(dvname);
		exit(1);
	}
	close(tfd);

	signal(SIGWINCH, sig_winch);
	signal(SIGCONT, sig_cont);

	setup_show();

	cdplay_init();

#define	UPDATEC	4
	for (rup = update = 0; update >= 0; )
	{
		if (play)
		{
			tout.tv_sec = 0;
			tout.tv_usec = 300 * 1000;
		}
		else
		{
			tout.tv_sec = 2;
			tout.tv_usec = 0;
		}

		FD_SET(ttyFd, &fd_rsel);
		select(ttyFd + 1, &fd_rsel, NULL, NULL, &tout);

		if (cd_fd < 0)
		{
			if (cdplay_init())
			{
				if (FD_ISSET(ttyFd, &fd_rsel) == 0)
					continue;
				if (read(ttyFd, &ch, 1) < 0)
					continue;

				if (ch == 'q')
					update = -1;
			}
			continue;
		}

		if (FD_ISSET(ttyFd, &fd_rsel) == 0)
		{
			if (rup > 0)
			{
				if (-- rup == 0 && play)
				{
					play_msf_rel(RelPosExp);
					RelPosExp = 0;
					print_status();
				}

				continue;
			}

			if (update > 0)
			{
				if (--update == 0 && play)
				{
					int exp;

					if ((exp = check_track(index)) >= 0)
					{
						index = exp;
						play_msf();
					}
					else
						play_stop();
				}

				continue;
			}

			print_status();
			continue;
		}

		if (read(ttyFd, &ch, 1) < 0)
			continue;

		switch (ch)
		{
		case '':
			sig_stop();
			break;

		case '':
			close_show();
			setup_show();
			print_toc();
			break;

		case '<': /* round 0 -> 255 */
#define	VOLSTEP	1
			vol.vol[0] -= VOLSTEP;
			vol.vol[1] -= VOLSTEP;
			vol.vol[2] = vol.vol[3] = 0;
			setvol();
			print_status();
			break;

		case '>': /* round 255 -> 0 */
			vol.vol[0] += VOLSTEP;
			vol.vol[1] += VOLSTEP;
			vol.vol[2] = vol.vol[3] = 0;
			setvol();
			print_status();
			break;

		case 'E':
			ioctl(cd_fd, CDIOCALLOW);
			ioctl(cd_fd, CDIOCEJECT);
			cdplay_clean();
			continue;

		case 'P':
			if (paused || play == 0)
				continue;
			ioctl(cd_fd, CDIOCPAUSE);
			paused = 1;
			print_toc();
			break;

		case 'r':
			if (paused == 0)
				continue;
			ioctl(cd_fd, CDIOCRESUME);
			paused = 0;
			if (tocent[index].track != ctrack)
				update = 1;
			print_toc();
			break;

		case 'k':
			index -= 1;
			if (play)
				update = UPDATEC;
			goto startplay;

		case 'j':
			index += 1;
			if (play)
				update = UPDATEC;
			goto startplay;

		case 'p':
			play = 1;
			update = 1;

startplay:
			limit_check();
			print_toc();
			break;

		case 's':
			play_stop();
			break;

		case 'L':
			loopon ^= 1;
			print_toc();
			break;

		case 'q':
		case 'Q':
		case 3:
			update = -1;
			break;

		case ' ':
			if ((tocent[index].control & 0x04) == 0)
				toc_info[index].pflag ^= 1;
			goto resume;

		case 'c':
			for (i = 0; i < ntocent; i ++)
				if ((tocent[i].control & 0x04) == 0)
					toc_info[i].pflag = 0;
			goto resume;

		case 'a':
			for (i = 0; i < ntocent; i ++)
				if ((tocent[i].control & 0x04) == 0)
					toc_info[i].pflag = 1;

resume:
			print_toc();
			if (play)
				update = UPDATEC;
			break;

#define	ABS_SKIP	(5 * 75)
		case 'l':
			if (play && paused == 0)
			{
				rup = 2;
				play_step(ABS_SKIP);
			}
			break;

		case 'h':
			if (play && paused == 0)
			{
				rup = 2;
				play_step(- ABS_SKIP);
			}
			break;

		default:
			break;
		}
	}

	close_show();
	exit(0);
}
