#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dev/packet_midi.h>
#include <sys/time.h>
#include "playmidi.h"

#define	BUFSIZE	4096

void dump_packet __P((int, u_char *, int));
int midi_pgchg __P((u_char *, int, int, int));
int do_gs_reset __P((int));

int timer_tick;
int graphics = 1;
int evlist, thru;

int curch, curbank, curpg;
u_char gs_reset[] = {0xf0, 0x41, 0x10, 0x42, 0x12, 0x40,
		     0x00, 0x7f, 0x00, 0x41, 0xf7};

int
do_gs_reset(fd)
	int fd;
{
	u_char buf[128];
	struct pmidi_packet_header *mp;

	mp = (struct pmidi_packet_header *) buf;
	mp->mp_code = MIDI_PKT_DATA;
	mp->mp_ticks = 0;
	mp->mp_len = sizeof(gs_reset);
	bcopy(gs_reset, buf + sizeof(*mp), mp->mp_len);
	write(fd, buf, mp->mp_len + sizeof(*mp));
}

int
midi_pgchg(buf, chan, bank, pg)
	u_char *buf;
	int chan;
	int bank;
	int pg;
{
	struct pmidi_packet_header *mp;
	int len = 0;

	mp = (struct pmidi_packet_header *) buf;
	mp->mp_code = MIDI_PKT_DATA;
	mp->mp_ticks = 0;
	mp->mp_len = 3;
	buf += sizeof(*mp);
	*buf ++ = 0xb0 | chan;
	*buf ++ = 0;
	*buf ++ = bank;
	len += mp->mp_len + sizeof(*mp);

	mp = (struct pmidi_packet_header *) buf;
	mp->mp_code = MIDI_PKT_DATA;
	mp->mp_ticks = 0;
	mp->mp_len = 3;
	buf += sizeof(*mp);
	*buf ++ = 0xb0 | chan;
	*buf ++ = 20;
	*buf ++ = 0;
	len += mp->mp_len + sizeof(*mp);

	mp = (struct pmidi_packet_header *) buf;
	mp->mp_code = MIDI_PKT_DATA;
	mp->mp_ticks = 0;
	mp->mp_len = 2;
	buf += sizeof(*mp);
	*buf ++ = 0xc0 | chan;
	*buf ++ = pg;
	len += mp->mp_len + sizeof(*mp);

	return len;
}

void
dump_packet(fd, bp, len)
	int fd;
	u_char *bp;
	int len;
{
	struct pmidi_packet_header *mp;
	u_int size;

	while (len >= sizeof(*mp))
	{
		mp = (struct pmidi_packet_header *) bp;
		timer_tick = mp->mp_ticks;
		mp->mp_ticks = 0;
		size = mp->mp_len;	
		if (thru == 0)
			write(fd, bp, size + sizeof(*mp));

		bp += sizeof(*mp);
		showevent(*bp, bp + 1, size - 1);

		bp += size;
		len -= sizeof(*mp) + size;
	}
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
    	extern int optind;
	int mode, fd, cnt, ch;
	u_char *dvname = "/dev/midi";
	u_char buf[BUFSIZE];
	fd_set rfds;
	int gs = 0;

	while ((ch = getopt(argc, argv, "f:ts")) != -1)
    	{
		switch (ch) 
		{
		case 'f':
			dvname = optarg;
			break;

		case 't':
			thru = 1;
			break;

		case 's':
			gs = 1;
			break;
		}
	}

	mode = O_RDWR;
	fd = open(dvname, mode, 0);
	if (fd < 0)
	{
		perror(dvname);
		exit(1);
	}

	if (thru != 0)
	{
		struct pmidi_packet_header mh;

		mh.mp_code = MIDI_PKT_SYSTM | MIDI_PORT_THRU;
		mh.mp_len = 0;
		mh.mp_ticks = 0;
		write(fd, &mh, sizeof(mh));	
	}

	setup_show();
	init_show();

	if (gs != 0)
		do_gs_reset(fd);

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	FD_SET(ttyFd, &rfds);

	while (select(32, &rfds, NULL, NULL, NULL) >= 0)
	{
		if (FD_ISSET(fd, &rfds))
		{
			cnt = read(fd, buf, BUFSIZE);
			if (cnt <= 0)
				continue;
			dump_packet(fd, buf, cnt);
			print_status();
		}

		if (FD_ISSET(ttyFd, &rfds))
			updatestatus();

		if (evlist != 0)
		{
			if (evlist & PGCHG_PLEASE)
			{
				print_status();
				evlist &= ~PGCHG_PLEASE;
				cnt = midi_pgchg(buf, curch, curbank, curpg);
				dump_packet(fd, buf, cnt);
			}	

			if (evlist & EXIT_PLEASE)
			{
				close_show(0);
				exit(1);
			}
		}

		FD_SET(ttyFd, &rfds);
		FD_SET(fd, &rfds);
	}

	close_show(0);
	return 0;
}
