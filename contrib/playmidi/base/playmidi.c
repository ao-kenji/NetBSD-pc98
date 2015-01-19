/*	$NecBSD: playmidi.c,v 1.8 1998/02/08 08:01:18 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/* 
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */
/************************************************************************
   playmidi.c -- last change: 1 Jan 96

   Plays a MIDI file to any supported synth (including midi) device

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

/*
 * The followings are supported:
 * 1) serial interface  + an external midi sequencer     	    OK
 * 2) mpu98 midi interface(UART mode) + an external midi sequencer  OK
 * 3) PCMCIA SCP-55					 	    OK
 * 4) PCMCIA QVISION MIDI CARD				 	    OK
 */

#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "playmidi.h"
#include "gmvoices.h"

struct midi_hw *hw;
int iobase;
u_char dvname[FILENAME_MAX];

struct miditrack seq[MAXTRKS];

int chanmask = 0xffff, perc = PERCUSSION;
int useprog[16], usevol[16];
int graphics = 1;
int next_skip = 1;
int p_remap = 0;
int mfd, find_header = 0;
unsigned long int default_tempo;
char *filename;
float skew = 1.0;

static int hextoi __P((u_char *));
static void show_usage __P((u_char *));

int 
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
    	extern int optind;

    	int i, j, error = 0, newprog;
    	char *extra, *filebuf;
    	struct stat info;

    	printf("%s Copyright (C) 1994-1996 Nathan I. Laredo\n"
	       "This is free software with ABSOLUTELY NO WARRANTY.\n"
	       "For details please see the file COPYING.\n", RELEASE);

    	for (i = 0; i < 16; i++)
		useprog[i] = usevol[i] = 0;	/* reset options */

    while ((i = getopt(argc, argv, "4c:C:dD:eE:f:gh:G:i:IMm:p:P:rR:t:vV:x:")) != -1)
    {
	switch (i) 
	{
	case 'x':
	    	j = atoi(optarg);
	    	if (j < 1 || j > 16) 
		{
			fprintf(stderr, "option -x channel must be 1 - 16\n");
			exit(1);
	    	}
	    	j = 1 << (j - 1);
	    	chanmask &= ~j;
	    	break;

	case 'c':
	    	if (chanmask == 0xffff)
			chanmask = hextoi(optarg);
	    	else
			chanmask |= hextoi(optarg);
	    	break;

	case 'd':
	    	chanmask = ~perc;
	    	break;

	case 'f':
		strcpy(dvname, optarg);
		break;

	case 'h':
	    	find_header = atoi(optarg);
	    	if (find_header < 1) {
			fprintf(stderr, "option -h header must be > 0\n");
			exit(1);
	    	}
	    	break;

	case 'i':
	    	chanmask &= ~hextoi(optarg);
	    	break;

	case 'm':
	    	perc = hextoi(optarg);
	    	break;

	case 'p':
	    if (strchr(optarg, ',') == NULL) {	/* set all channels */
		newprog = atoi(optarg);
		if (newprog < 1 || newprog > 129) {
		    fprintf(stderr, "option -p prog must be 1 - 129\n");
		    exit(1);
		}
		for (j = 0; j < 16; j++)
		    useprog[j] = newprog;
	    } else {		/* set channels individually */
		extra = optarg;
		while (extra != NULL) {
		    j = atoi(extra);
		    if (j < 1 || j > 16) {
			fprintf(stderr, "opton -p chan must be 1 - 16\n");
			exit(1);
		    }
		    extra = strchr(extra, ',');
		    if (extra == NULL) {
			fprintf(stderr, "option -p prog needed for chan %d\n",
				j);
			exit(1);
		    } else
			extra++;
		    newprog = atoi(extra);
		    if (newprog < 1 || newprog > 129) {
			fprintf(stderr, "option -p prog must be 1 - 129\n");
			fprintf(stderr, "DANGER: 129 may screw everything!\n");
			exit(1);
		    }
		    useprog[j - 1] = newprog;
		    extra = strchr(extra, ',');
		    if (extra != NULL)
			extra++;
		}
	    }
	    break;

	case 'r':
	    	graphics = 0;
	    	break;

	case 't':
	    	if ((skew = atof(optarg)) < .25) {
			fprintf(stderr, "option -t skew under 0.25 unplayable\n");
			exit(1);
	    	}
	    	break;

	case 'P':
	    	p_remap = atoi(optarg);
	    	if (p_remap < 1 || p_remap > 16) {
			fprintf(stderr, "option -P channel must be 1 - 16\n");
			exit(1);
	    	}
	    	break;

	case 'V':
	    extra = optarg;
	    while (extra != NULL) {
		j = atoi(extra);
		if (j < 1 || j > 16) {
		    fprintf(stderr, "opton -V chan must be 1 - 16\n");
		    exit(1);
		}
		extra = strchr(extra, ',');
		if (extra == NULL) {
		    fprintf(stderr, "option -V volume needed for chan %d\n",
			    j);
		    exit(1);
		}
		extra++;
		newprog = atoi(extra);
		if (newprog < 1 || newprog > 127) {
		    fprintf(stderr, "option -V volume must be 1 - 127\n");
		    exit(1);
		}
		usevol[j - 1] = newprog;
		extra = strchr(extra, ',');
		if (extra != NULL)
		    extra++;
	    }
	    break;

	default:
	    	error++;
	    	break;
	}
    }

	if (error || optind >= argc) 
		show_usage(argv[0]);

    	i386_iopl(3);
	
	midi_hw_select();
	if (hw == NULL)
	{
		fprintf(stderr, "Midi hardware not found\n");
		exit(1);
	}
	else
		printf("midihw: %s\n", hw->hw_id);

	(*hw->hw_open)(iobase);

    	setup_show(argc, argv);

    	for (i = optind; i < argc; ) 
	{
		filename = argv[i];
		if (stat(filename, &info) < 0) 
		{
			u_char altfname[FILENAME_MAX];

	    		sprintf(altfname, "%s.mid", filename);
	    		if (stat(altfname, &info) < 0)
				close_show(-1);

	    		if ((mfd = open(altfname, O_RDONLY, 0)) < 0)
				close_show(-1);
		} 
		else if ((mfd = open(filename, O_RDONLY, 0)) < 0)
	    		close_show(-1);

		if ((filebuf = malloc(info.st_size)) == NULL)
	    		close_show(-1);

		if (read(mfd, filebuf, info.st_size) < info.st_size)
	    		close_show(-1);

		close(mfd);

		do 
		{

	    		default_tempo = 500000;
	    		error = readmidi(filebuf, info.st_size);
	    		newprog = 1;

	    		if (error > 0)
				while ((newprog = playevents()) == 0)
					;

	    		if (find_header)
				find_header += newprog;
		} 
		while (find_header);

		if ((i += newprog) < optind)
	    		i = optind;	

		free(filebuf);
    	}

	shutdown_MIDI();
    	close_show(0);
    	exit(0);
}

static int 
hextoi(s)
	u_char *s;
{
	int i, j, k, l;

    	k = strlen(s);

    	for (j = i = 0; i < k; i++) 
	{
		l = toupper(s[i]);
		if (l > 64 && l < 71)
			j += (l - 55) << ((k - i - 1) * 4);
		else if (l > 47 && l < 58)
			j += (l - 48) << ((k - i - 1) * 4);
		else if (l != 88) 
		{
			fprintf(stderr, "invalid character in hexidecimal mask\n");
	    		exit(1);
		}
    	}

	return j & 0xffff;
}

void
show_usage(prog)
	u_char *prog;
{

	fprintf(stderr, "usage: %s [-options] file1 [file2 ...]\n", prog);
	fprintf(stderr, "  -v       verbosity (additive)\n"
		"  -i x     ignore channels set in bitmask x (hex)\n"
		"  -c x     play only channels set in bitmask x (hex)\n"
		"  -x x     exclude channel x from playable bitmask\n"
		"  -p [c,]x play program x on channel c (all if no c)\n"
		"  -V c,x   play channel c with volume x (1-127)\n"
		"  -t x     skew tempo by x (float)\n"
		"  -m x     override percussion mask (%04x) with x\n"
		"  -d       don't play any percussion (see -m)\n"
		"  -P x     play percussion on channel x (see -m)\n"
		"  -f x     mpu device name: cua?, mpu98, qvision, roland\n"
		"  -h x     skip to header x in large archive\n"
		"  -I       show list of all GM programs (see -p)\n"
		"  -r       disenable real-time playback graphics\n",
		PERCUSSION);
	exit(1);
}
