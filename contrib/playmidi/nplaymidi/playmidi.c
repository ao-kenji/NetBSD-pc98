/*	$NecBSD: playmidi.c,v 1.8 1999/08/02 17:15:03 honda Exp $	*/
/*	$NetBSD$	*/

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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "playmidi.h"

/*
 * Global variables
 */
struct miditrack seq[MAXTRKS];
char *filename;
int find_header = 0;
int graphics = 0;
int dumpon = 0;
u_int cflags;

/*
 * main staff
 */
int 
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
    	extern int optind;
	int fd, ch, mfd, find_header = 0;
    	int i, error = 0, newprog;
	u_char *filebuf, *dvname = "/dev/pmidi0";
    	struct stat info;

	while ((ch = getopt(argc, argv, "df:g8x")) != -1)
    	{
		switch (ch) 
		{
		case 'f':
			dvname = optarg;
			break;
		case 'g':
			graphics = 1;
			break;
		case 'd':
			dumpon = 1;
			break;
		case '8':
			cflags |= FORCE_88PROMAP;
			break;
		case 'x':
			cflags |= FORCE_XGNATIVE;
			break;
		}
	}

	fd = open(dvname, O_WRONLY, 0);
	if (fd < 0)
	{
		perror(dvname);
		return 1;
	}

	if (graphics)
		setup_show();

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

		printf("%s\n", filename);
		do 
		{
	    		error = readmidi(filebuf, info.st_size);
	    		newprog = 1;

	    		if (error > 0)
				while ((newprog = playevents(fd)) == 0)
					;

	    		if (find_header)
				find_header += newprog;
		} 
		while (find_header);

		if ((i += newprog) < optind)
	    		i = optind;	

		free(filebuf);
    	}

	close(fd);
	if (graphics)
		close_show(0);

	return 0;
}
