/*	$NetBSD$	*/
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
#include "gmvoices.h"

#define	LINE_BASE 3 
#define	TXT_BASE (LINE_BASE + 17)

/************************************************************
 * data
 ************************************************************/
int ttyFd;

char *metatype[7] =
{"Text", "Copyright Notice", "Sequence/Track name",
 "Instrument Name", "Lyric", "Marker", "Cue Point"};

char *sharps[12] =		/* for a sharp key */
{"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "B#", 
 "B "};
char *flats[12] =		/* for a flat key */
{"C ", "Db", "D ", "Eb", "E ", "F ", "Gb", "G ", "Ab", "A ", "Bb", 
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

static char **nn;
static int ytxt;
static u_char textbuf[2048];
static u_char cbuf[2048];
static u_int cpos;
static int perc = (1 << 9);
static int mask;

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

	add_nstring(x, y, str, 0);
}

static void
add_nstring(x, y, str, len)
	int x, y;
	u_char *str;
	int len;
{
	u_char gob[32];
	int xlen;

	if (x >= 0)
	{
		sprintf(gob, "\033[%d;%dH", x + 1, y + 1);
		strcpy(&cbuf[cpos], gob);
		cpos += strlen(gob);
	}

	xlen = strlen(str);
	if (len == 0)
		len = xlen;

	if (xlen < len)
	{
		strncpy(&cbuf[cpos], str, xlen);
		cpos += xlen;
		len -= xlen;
		while (len -- > 0)
			cbuf[cpos ++] = ' ';
	}
	else
	{
		strncpy(&cbuf[cpos], str, len);
		cpos += len;
	}
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
		printf("\033[%d;%dr", 1, LINES + 1);

		refresh();
		endwin();

		attrset(CURSOR_ON);
		attrset(A_NORMAL);
		Refresh();
	
		close(ttyFd);
        	resetty();
    	}

    	exit(error);
}

void 
setup_show()
{

	if (nn == NULL)
		nn = flats;

	if (graphics) 
	{
		initscr();
		savetty();

		raw();
		noecho();

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
#define YPOS		(CHN + LINE_BASE)
#define XPOS		(((NOTE * 2) % mask) + 20)
#define NNAME		nn[NOTE % 12]


void
show_status(all)
	int all;
{
	int i, d1, d2;
	u_char *s;

	if (all != 0)
	{
		attrset(A_NORMAL);
		sprintf(textbuf, "skew: %0.2f", (float) skew);
		add_string(2, 0, textbuf);

		for (i = 0; i < 16; i ++)
		{
			if (channel_status[i].cs_mute)
				s = "*";
			else
				s = " ";
			add_string(i + LINE_BASE, 13, s);
		}
	}

	d1 = ((u_int) playback_status.pb_current) / 1000;
	d2 = ((u_int) playback_status.pb_current) % 1000;

	attrset(COLOR_RUN);
#ifdef	notyet
	sprintf(textbuf, "%03d", playback_status.pb_tempo / 10000);
	add_string(0, 60, textbuf);
#endif
	sprintf(textbuf, "%02d:%02d.%03d", d1 / 60, d1 % 60, d2);
	add_string(0, 66, textbuf);
	Refresh();
}

int 
getkey(evp)
	struct key_event *evp;
{
	static int number;
	u_char ch;

	evp->ev_code = EVENT_NONE;
	if (read(ttyFd, &ch, 1) < 0)
		return 0;

	switch (ch) 
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		number = (ch - '0') + number * 10;
		break;

	case 's':
		evp->ev_code = EVENT_STOP;
		break;

	case 'u':
		evp->ev_code = EVENT_SKEW;
		evp->ev_arg = -1;
		break;

	case 'd':
		evp->ev_code = EVENT_SKEW;
		evp->ev_arg = 1;
		break;

	case 'q':
	case 'Q':
	case 3:
		evp->ev_code = EVENT_EXIT;
		break;

	case 'n':
		evp->ev_code = EVENT_CHGSONG;
		evp->ev_arg = 1;
		break;

	case 'p':
		evp->ev_code = EVENT_CHGSONG;
		evp->ev_arg = -1;
		break;

	case '':
		init_show();
		break;
		
	case 'm':
		evp->ev_code = EVENT_MUTE;
		evp->ev_arg = number;
		break;

	case ' ':
		evp->ev_code = EVENT_MARK;
		evp->ev_arg = number;
		break;

	case 'r':
		evp->ev_code = EVENT_REPLAY;
		evp->ev_arg = number;
		break;

	case 'l':
		evp->ev_code = EVENT_LOOP;
		break;

	case 'c':
		evp->ev_code = EVENT_SCHAN;
		evp->ev_arg = number;
		break;

	case 'v':
		evp->ev_code = EVENT_VCUT;
		evp->ev_arg = number;
		break;

	default:
		break;
	}

	if (evp->ev_code != EVENT_NONE)
		number = 0;
}

#define	CST	"  "

void 
showevent(cmd, data, length)
	u_char cmd;
	char *data;
	int length;
{
	u_int val;

	if (graphics == 0)
		return;

    	if (cmd < 8 && cmd > 0) 
	{
		attrset(NON_REVERSE | cmd);
	    	strncpy(textbuf, data, COLS);
	    	if (length < 1024)
			textbuf[length] = 0;

	    	add_nstring(ytxt, 0, textbuf, COLS);
	    	add_string(ytxt, 0, "\n");
	    	if ((++ytxt) > LINES)
			ytxt = LINES;
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
					add_nstring(YPOS, XPOS, NNAME, 2);
				}
				else if (gmvoice[NOTE + 128])
				{
					attrset(DRUM_COLOR | NON_REVERSE);
					add_nstring(YPOS, XPOS, gmvoice[NOTE + 128], 2);
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
			if (NOTE == 0)
			{
				u_char var[4];

				attrset(A_NORMAL);
				sprintf(var, "%03d", VEL);
				add_nstring(YPOS, 10, var, 3);
			}

		case MIDI_CHN_PRESSURE:
		case MIDI_KEY_PRESSURE:
	    		break;

		case MIDI_PGM_CHANGE:
			if (!ISPERC(CHN))
			{
				attrset((CHN % 6 + 1) | NON_REVERSE);
				add_nstring(YPOS, 0, gmvoice[NOTE], 9);
			}
			else
			{
				attrset(DRUM_COLOR);
				add_nstring(YPOS, 0, drumset[SET(NOTE)], 9);
			}
			break;

		case MIDI_PITCH_BEND:
			attrset((CHN % 6 + 1) | NON_REVERSE);

			val = (VEL << 7) | NOTE;
			if (val > 0x2000)
				add_string(YPOS, 14, "^^^");
			else if (val < 0x2000)
				add_string(YPOS, 14, "vvv");
			else
				add_string(YPOS, 14, "   ");
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
    	char *tmp, tbuf[128];
	int i;

	nn = flats;

    	if (graphics) 
	{
		ytxt = TXT_BASE;

		clear();
		refresh();
		Refresh();

		attrset(A_NORMAL);

		mask = 0;
		for (i = 0; i < COLS - 20; i += 24)
			mask = i;

		for (i = 0; i < 16; i ++)
		{
			sprintf(tbuf, "ch%d", i + 1);
			add_string(i + LINE_BASE, 0, tbuf);
			add_string(i + LINE_BASE, 10, "000");
		}
		printf("\033[%d;%dr", TXT_BASE + 1, LINES + 1);

		tmp = strrchr(filename, '/');
		strncpy(textbuf, (tmp == NULL ? filename : tmp + 1), 14);
		add_string(0, 0, textbuf);
		sprintf(textbuf, "%d track%c", ntrks, ntrks > 1 ? 's' : ' ');
		add_string(1, 0, textbuf);
		show_status(1);

		refresh();
		Refresh();
	}
}
