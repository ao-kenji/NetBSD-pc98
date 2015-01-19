/*	$NecBSD: io_ncurses.c,v 1.9 1998/02/08 08:01:24 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/* 
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */
/************************************************************************
   io_ncurses.c  -- shows midi events using ncurses or printf

   Copyright (C) 1994-1996 Nathan I. Laredo

   This program is modifiable/redistributable under the terms
   of the GNU General Public Licence.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   Send your comments and all your spare pocket change to
   laredo@gnu.ai.mit.edu (Nathan Laredo) or to PSC 1, BOX 709, 2401
   Kelly Drive, Lackland AFB, TX 78236-5128, USA.
 *************************************************************************/

#include "playmidi.h"
#include <stdlib.h>
#include <unistd.h>
#include <curses.h>
#include <sys/time.h>
#include <sys/fcntl.h>

/************************************************************
 * data
 ************************************************************/
char *metatype[7] =
{"Text", "Copyright Notice", "Sequence/Track name",
 "Instrument Name", "Lyric", "Marker", "Cue Point"};

char *sharps[12] =		/* for a sharp key */
{"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "B#", 
 "B "};
char *flats[12] =		/* for a flat key */
{"C ", "Db", "D ", "Eb", "E ", "F ", "Gb", "G ", "Gb", "A ", "Bb", 
 "B "};
char *majflat[15] =		/* name of major key with 'x' flats */
{"C ", "F ", "Bb", "Eb", "Ab", "Db", "Gb", "Cb", "Fb", "Bbb", "Ebb",
 "Abb", "Gbb", "Cbb", "Fbb"};	/* only first 8 defined by file format */
char *majsharp[15] =		/* name of major key with 'x' sharps */
{"C ", "G ", "D ", "A ", "E ", "B ", "F#", "C#", "G#", "D#", "A#",
 "E#", "B#", "F##", "C##"};	/* only first 8 defined by file format */
char *minflat[15] =		/* name of minor key with 'x' flats */
{"A ", "D ", "G ", "C ", "F ", "Bb", "Eb", "Ab", "Db", "Gb", "Cb",
 "Fb", "Bbb", "Ebb", "Abb"};	/* only first 8 defined by file format */
char *minsharp[15] =		/* name of minor key with 'x' sharps */
{"A ", "E ", "B ", "F#", "C#", "G#", "D#", "A#", "E#", "B#", "F##",
 "C##", "G##", "D##", "A##"};	/* only first 8 defined by file format */

#define SET(x) (x == 8 ? 1 : x >= 16 && x <= 23 ? 2 : x == 24 ? 3 : \
		x == 25  ? 4 : x == 32 ? 5 : x >= 40 && x <= 47 ? 6 : \
		x == 48 ? 7 : x == 56 ? 8 : x >= 96 && x <= 111 ? 9 : \
		x == 127 ? 10 : 0)

char *drumset[11] =
{
 "Standard Kit", "Room Kit", "Power Kit", "Electronic Kit", "TR-808 Kit",
    "Jazz Kit", "Brush Kit", "Orchestra Kit", "Sound FX Kit",
    "Downloaded Program", "MT-32 Kit"
};

char *drum3ch[11] =
{
    "STD", "RM.", "PWR", "ELE", "808",
    "JAZ", "BRU", "ORC", "SFX", "PRG", "M32"
};

extern int graphics, perc;
extern int format, ntrks, division;
extern char *filename;
extern float skew;

int ttyFd;
fd_set fd_rsel;
char **nn;

#define	SKEWLINE 10
#define	STATLINE 11
#define	TEXTLINE 12
int ytxt;

u_char textbuf[2048];
u_char cbuf[2048];
u_int cpos;

/************************************************************
 * curses emulation 
 ************************************************************/
static void add_string __P((int, int, u_char *));
static void add_nstring __P((int, int, u_char *, int));
static void Refresh __P((void));
static void attrset __P((int));

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
	int len;
{
	u_char gob[32];

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

/************************************************************
 * attribute emulate
 ************************************************************/
#define	DRUM_COLOR	2

#define	A_NORMAL	0x10
#define	CURSOR_OFF	0x11
#define	CURSOR_ON	0x12
#define COLOR_RUN	0x13
#define	NON_REVERSE	0x20
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
		
/************************************************************
 * open close
 ************************************************************/
void 
close_show(error)
	int error;
{

	if (graphics) 
	{
		refresh();
		endwin();

		attrset(CURSOR_ON);
		attrset(A_NORMAL);
		Refresh();
	
		close(ttyFd);
        	resetty();
    	}

	shutdown_MIDI();
    	exit(error);
}

void 
setup_show(argc, argv)
	int argc;
	char **argv;
{

	if (nn == NULL)
		nn = flats;

	if (graphics) 
	{
		initscr();
		savetty();

		raw();
		noecho();

		FD_ZERO(&fd_rsel);
		ttyFd = open("/dev/tty", O_RDWR, 0);

		attrset(A_NORMAL);
		attrset(CURSOR_OFF);
		Refresh();
    	}
}

/************************************************************
 * main
 ************************************************************/
#define CHN		(cmd & 0xf)
#define NOTE		((u_int)data[0])
#define VEL		((u_int)data[1])
#define OCTAVE		(NOTE / 12)
#define OCHAR		(0x30 + OCTAVE)
#define XPOS		(4 * CHN)
#define YPOS		(LINES - NOTE % (LINES - 2))
#define NNAME		nn[NOTE % 12]

static void
print_status(void)
{
	u_char *s;

	attrset(A_NORMAL);
	sprintf(textbuf, "skew: %0.2f", skew);
	add_string(SKEWLINE, 64, textbuf);

	s = (next_skip == 0) ? "cycle play" : "          ";
	add_string(STATLINE, 64, s);
}

static struct timeval tout;

int 
updatestatus()
{
	int d1, d2;
	u_char ch;

	while(1)
	{
		FD_SET(ttyFd, &fd_rsel);
		if (select(ttyFd + 1, &fd_rsel, NULL, NULL, &tout) < 0)
			close_show(0);

		if (FD_ISSET(ttyFd, &fd_rsel) == 0)
			break;

		if (read(ttyFd, &ch, 1) < 0)
			break;

		switch (ch) 
		{
		case 'u':
		case 'd':
			if (ch == 'u')
			{
				if ((skew -= 0.01) < 0.25)
					skew = 0.25;
			}
			else
			{
				if ((skew += 0.01) > 4)
					skew = 4.0;
			}

			print_status();
			break;


		case 'q':
		case 'Q':
		case 3:
			close_show(0);
			break;

		case 'l':
			if (next_skip)
				next_skip = 0;
			else
				next_skip = 1;

			print_status();
			break;

		case 'r':
			return 0;

		case 'n':
			return 1;

		case 'p':
			return -1;

		case '':
			init_show();
			break;
			
		default:
			break;
		}
	}

	d1 = timer_tick / 1000;
	d2 = timer_tick % 1000;

	attrset(COLOR_RUN);
	sprintf(textbuf, "%02d:%02d.%03d", d1 / 60, d1 % 60, d2);
	add_string(0, 66, textbuf);
	Refresh();

	return NO_EXIT;
}

#define	CST	"   "

void 
showevent(cmd, data, length)
	int cmd;
	u_char *data;
	int length;
{
	u_int val;

	if (graphics == 0)
		return;

    	if (cmd < 8 && cmd > 0) 
	{
		attrset(NON_REVERSE | cmd);
	    	strncpy(textbuf, data, length < COLS - 66 ? length : COLS - 66);

	    	if (length < 1024)
			textbuf[length] = 0;

	    	add_string(ytxt, 64, textbuf);
	    	if ((++ytxt) > LINES - 2)
			ytxt = TEXTLINE;
	}
	if (cmd == key_signature) 
	{
		nn = ((NOTE & 0x80) ? flats : sharps);
	}
	else
	{
		switch (cmd & 0xf0) 
		{
		case MIDI_NOTEON:
			if (VEL) 
			{
			        if (!ISPERC(CHN))
				{
					attrset(CHN % 6 + 1);
					sprintf(textbuf, "%s%c", NNAME, OCHAR);
					add_string(YPOS, XPOS, textbuf);
				}
				else if (gmvoice[NOTE + 128])
				{
					attrset(DRUM_COLOR | NON_REVERSE);
					add_nstring(YPOS, XPOS, gmvoice[NOTE + 128], 3);
				}
			} 
			else
			{
				attrset(A_NORMAL);
				add_string(YPOS, XPOS, CST);
			}
	    		break;

		case MIDI_NOTEOFF:
			attrset(A_NORMAL);
			add_string(YPOS, XPOS, CST);
	    		break;

		case MIDI_CTL_CHANGE:
		case MIDI_CHN_PRESSURE:
		case MIDI_KEY_PRESSURE:
	    		break;

		case MIDI_PGM_CHANGE:
			if (!ISPERC(CHN))
			{
				attrset((CHN % 6 + 1) | NON_REVERSE);
				add_nstring(0, 4 * CHN, gmvoice[NOTE], 3);
			}
			else
			{
				attrset(DRUM_COLOR);
				add_string(0, 4 * CHN, drum3ch[SET(NOTE)]);
			}
			break;

		case MIDI_PITCH_BEND:
			attrset(COLOR_RUN);

			val = (VEL << 7) | NOTE;
			if (val > 0x2000)
				add_string(1, 4 * CHN, "^^^");
			else if (val < 0x2000)
				add_string(1, 4 * CHN, "vvv");
			else
				add_string(1, 4 * CHN, "   ");
			break;

		default:
	   		break;
		}
	}

	Refresh();
}

void 
init_show()
{
    	char *tmp;

	nn = flats;

    	if (graphics) 
	{
		clear();
		attrset(A_NORMAL);

		ytxt = TEXTLINE;

		add_string(0, 0, "ch1 ch2 ch3 ch4 ch5 ch6 ch7 ch8 ch9 c10 c11 c12 c13 c14 c15 c16");
		add_string(3, 64, RELEASE);
		add_string(4, 70, "by");
		add_string(5, 64, "Nathan Laredo");

		tmp = strrchr(filename, '/');
		strncpy(textbuf, (tmp == NULL ? filename : tmp + 1), 14);
		add_string(7, 64, textbuf);
		sprintf(textbuf, "%d track%c", ntrks, ntrks > 1 ? 's' : ' ');
		add_string(8, 64, textbuf);

		print_status();

		refresh();
		Refresh();
	}
}

