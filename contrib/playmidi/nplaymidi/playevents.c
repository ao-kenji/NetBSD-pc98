/*	$NetBSD$	*/
/************************************************************************
   playevents.c  -- actually sends sorted list of events to device

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
#include <unistd.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <dev/packet_midi.h>

#define	MAX_MARK 10

double skew = 1.0;
struct playback_status playback_status, playback_status_mark[MAX_MARK];
struct channel_status channel_status[MAX_CHAN];

static int put_midi_packet __P((int, int, u_int, u_char, u_char *, u_int));
static u_long get_datalen __P((struct miditrack *));
static int finish __P((int));
static void midi_output __P((int, u_char));
static void midi_flush __P((int));
static int event_menu __P((void));
static int terminate __P((int fd));
static void dump_data __P((u_int, u_char, u_char *, int));
static void signalhand_kill __P(());
static void signalhand_int __P(());
static int process_key_event __P((int, struct key_event *));
static void mute_control __P((int, int, int));
static midi_note_off __P((int, int));

static u_char midimsg_GM_ON[] = {0x7e, 0x7f, 0x9, 0x01, 0xf7};
static int cmdlen[16] = {0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 1, 1, 2, 0};
static int sigposted;
#define	BUFSZ	128
static u_char midi_buf[BUFSZ];
static int midi_pos;
static int loop_mode;

static void
midi_output(fd, data)
	int fd;
	u_char data;
{

	if (midi_pos >= BUFSZ)
	{
		write(fd, midi_buf, midi_pos);
		midi_pos = 0;
	}

	midi_buf[midi_pos ++] = data;
}

static void
midi_flush(fd)
	int fd;
{

	if (midi_pos == 0)
		return;

	write(fd, midi_buf, midi_pos);
	midi_pos = 0;
}

static
midi_note_off(fd, ch)
	int fd;
	int ch;
{
	static u_char midimsg_NOTE_OFF[2] = {0x7b, 0x00};
	int i = 0;

	if (ch <= 0)
		for (i = 0; i < 16; i ++)
			put_midi_packet(fd, MIDI_PKT_DATA, 3, 0xb0 | i, 
					midimsg_NOTE_OFF, 0);
	else
		put_midi_packet(fd, MIDI_PKT_DATA, 3, 0xb0 | (ch - 1), 
				midimsg_NOTE_OFF, 0);
}

	
static int
put_midi_packet(fd, type, len, cmd, dp, ticks)
	int fd;
	int type;
	u_int len;
	u_char cmd;
	u_char *dp;
	u_int ticks;
{
	static u_char dummy_zero[2];
	struct pmidi_packet_header mph;
	u_char *p = (u_char *) &mph;
	int i;

	/*
	 * Put midi packets
	 */
	mph.mp_code = type;
	mph.mp_len = len;
	mph.mp_ticks = ticks;

	for (i = 0; i < sizeof(mph); i ++)
		midi_output(fd, p[i]);
	if (len == 0)
		return 0;

	midi_output(fd, cmd);

	if (channel_status[cmd & 0x0f].cs_mute != 0 &&
	    ((cmd & 0xf0) == MIDI_NOTEON))
		dp = dummy_zero;

	for (i = 0; i < len - 1; i ++)
		midi_output(fd, dp[i]);

	/*
	 * Synch graphics and sounds
	 */
	if (graphics || dumpon) 
	{
		midi_flush(fd);
		ioctl(fd, MIDI_DRAIN, 0);
		if (graphics)
			showevent(cmd, dp, len - 1);
		if (dumpon)
			dump_data((u_int) playback_status.pb_current,
				  cmd, dp, len - 1);
	}
	return 0;
}

static u_long
get_datalen(tp)
	struct miditrack *tp;
{
	u_long value;
    	u_char c;

	c = tp->data[tp->index ++];
	value = c & 0x7f;
	while(c & 0x80)
	{
		if (tp->index < tp->length)
		{
			c = tp->data[tp->index ++];
			value = (value << 7) + (c & 0x7f);
		}
		else
			c = 0;
    	}
    	return value;
}

static int
finish(fd)
	int fd;
{

	put_midi_packet(fd, MIDI_PKT_DATA, 6, 0xf0, midimsg_GM_ON, 0);
	midi_flush(fd);
	ioctl(fd, MIDI_DRAIN);
	return loop_mode > 0 ? 0 : 1;
}

static int
terminate(fd)
	int fd;
{
	int rv;

	midi_pos = 0;
	ioctl(fd, MIDI_FLUSH, 0);
	rv = finish(fd);
	if (graphics)
		close_show(0);
	return rv;
}

void
mute_control(fd, mute, ch)
	int fd;
	int mute;
	int ch;
{
	int i;

	if (ch > 0)
	{
		channel_status[ch - 1].cs_mute = mute;	
		if (mute > 0)
			midi_note_off(fd, ch);
	}
	else
	{
		for (i = 0; i < MAX_CHAN; i ++)
			channel_status[i].cs_mute = mute;	
		if (mute > 0)
			midi_note_off(fd, -1);
	}			
}

#define	GET_TEMPO(p) \
	((((u_int)p[0]) << 16) | (((u_int)p[1]) << 8) | (u_int) p[2])
#define STATUS(tp) ((tp)->running_st)

int 
playevents(fd)
	int fd;
{
	register struct miditrack *tp, *btp;
	struct timeval tout;
	struct key_event ev;
    	u_int ticks, lowtime, track, length;
    	u_char *data;
	fd_set rsel;
    	int rv, i;

	FD_ZERO(&rsel);
	bzero(&tout, sizeof(tout));
	bzero(&playback_status, sizeof(playback_status));
	bzero(&channel_status, sizeof(channel_status));

	/*
	 * initialize trk data
	 */
	playback_status.pb_ntrks = ntrks;		/* XXX */
	for (i = 0; i < MAXTRKS; i ++)			/* XXX */
		playback_status.pb_trk[i] = seq[i];
    	for (i = 0; i < playback_status.pb_ntrks; i ++) 
	{
		tp = &playback_status.pb_trk[i];
		tp->index = tp->running_st = 0;
		tp->ticks = get_datalen(tp);
    	}
	playback_status.pb_tempo = 500 * 1000;		/* tempo 120 */
	for (i = 0; i < MAX_MARK; i ++)
		playback_status_mark[i] = playback_status;
	mute_control(fd, 0, -1);

	if (graphics == 0)
	{
		sigposted = 0;
		signal(SIGINT, signalhand_int);
		signal(SIGKILL, signalhand_kill);
	}
	else
		init_show();

	/*
	 * setup midi interface
	 */
	put_midi_packet(fd, MIDI_PKT_SYSTM | MIDI_TIMER_PAUSE, 0, 0, NULL, 0);
	put_midi_packet(fd, MIDI_PKT_SYSTM | MIDI_TIMER_INIT, 0, 0, NULL, 0);
	put_midi_packet(fd, MIDI_PKT_DATA, 6, 0xf0, midimsg_GM_ON, 0);
	midi_flush(fd);
	sleep(1);

	put_midi_packet(fd, MIDI_PKT_SYSTM | MIDI_TIMER_INIT, 0, 0, NULL, 0);
	put_midi_packet(fd, MIDI_PKT_SYSTM | MIDI_TIMER_UNPAUSE,0, 0, NULL, 0);

	/*
	 * playback loop start
	 */

loop:
	/*
	 * check user key or signals.
	 */
	if (graphics)
	{
		FD_SET(ttyFd, &rsel);
		if (select(ttyFd + 1, &rsel, NULL, NULL, &tout) < 0)
			close_show(0);

		if (FD_ISSET(ttyFd, &rsel) != 0)
		{
			getkey(&ev);
			if (ev.ev_code != EVENT_NONE)
			{
				rv = process_key_event(fd, &ev);
				if (rv != 0)
					return rv;
			}
			show_status(1);
			goto loop;
		}

		if (playback_status.pb_stop != 0)
		{
			sleep(1);
			goto loop;
		}
	}

	if (sigposted)
	{
		rv = terminate(fd);
		if (sigposted == 2)
			rv = event_menu();
		return rv;
	}

	/*
	 * find a bast match track 
	 */
	lowtime = ~0;
	for (track = 0; track < playback_status.pb_ntrks; track++)
	{
		tp = &playback_status.pb_trk[track];
		if (tp->ticks < lowtime)
		{
			btp = tp;
			lowtime = tp->ticks;
		}
	}

	tp = btp;
	if (lowtime == ~0)
		return finish(fd);

	/*
	 * get a midi (running) status
	 */
	if ((tp->data[tp->index] & 0x80) && (tp->index < tp->length))
		STATUS(tp) = tp->data[tp->index ++];

	if (STATUS(tp) == 0xff && tp->index < tp->length)
		STATUS(tp) = tp->data[tp->index ++];

	/*
	 * get data length
	 */
	if (STATUS(tp) < 0xf8)
	{
		length = cmdlen[(STATUS(tp) & 0xf0) >> 4];
		if (length == 0)
			length = get_datalen(tp);
	}
	else
		length = 0;	/* real time message */

	if (tp->index + length >= tp->length) 
		goto next;

	/*
	 * calculate ticks
	 */
	data = &tp->data[tp->index];
	if (STATUS(tp) == set_tempo)
		playback_status.pb_tempo = GET_TEMPO(data);

	if (tp->ticks > playback_status.pb_lasttime) 
	{
		double dtime;

		if (division > 0) 
		{
			dtime =
			 ((double) (tp->ticks - playback_status.pb_lasttime) * 
		          (playback_status.pb_tempo / 10000)) / 
			  ((double) (division)) * skew * (double) 10.0;
			playback_status.pb_current += dtime;
			playback_status.pb_lasttime = tp->ticks;
		}	 
		else if (division < 0)
		{
			dtime = 0.0;
			playback_status.pb_current = ((double) tp->ticks / 
				   ((double) ((division & 0xff00 >> 8) *
				   (double) (division & 0xff)) * 
				   10000.0)) * skew * (double) 10.0;
		}

		if ((u_int) dtime > 40960)
			return finish(fd);
		if (graphics)
			show_status(0);
	}

	/*
	 * send midi packets
	 */
	if (playback_status.pb_current >= playback_status.pb_current_base)
	{
		u_char st = STATUS(tp);
		struct channel_status *csp = &channel_status[st & 0x0f];

		if (st & 0x80)
		{
			switch (st & 0xf0)
			{
			case MIDI_CTL_CHANGE:
				if (data[0] == 0x20)	/* MSB */
				{
					if ((cflags & FORCE_88PROMAP) && 
					    data[1] == 0x02)
						data[1] = 0x0;	
					csp->cs_Mbank = data[1];
				}
				if (data[0] == 0x00)	/* LSB */
				{
					if ((cflags & FORCE_XGNATIVE) &&
					    csp->cs_Mbank == 0)
						data[1] = 126;	
					csp->cs_Lbank = data[1];
				}
				break;

			case MIDI_NOTEON:
#ifdef	notyet
				if (csp->cs_vcut != 0)
					data[1] = (((u_int) data[1]) *
						   (100 - csp->cs_vcut)) / 100;				
#endif	/* notyet */
				break;
			}

			ticks = (u_int) (playback_status.pb_current -
					 playback_status.pb_current_base);
			put_midi_packet(fd, MIDI_PKT_DATA, length + 1,
					STATUS(tp), data, ticks);
		}
		else	/* meta events */
		{
			if (graphics)
				showevent(STATUS(tp), data, length);
		}
	}	

next:
	tp->index += length;
	if (tp->index >= tp->length)
		tp->ticks = ~0;
	else
		tp->ticks += get_datalen(tp);
	goto loop;
}

/* 
 * non curses interface staff
 */

static void
signalhand_kill()
{

	sigposted = 1;
}

static void
signalhand_int()
{

	sigposted = 2;
}

static int
event_menu()
{
	int ch;

	system("stty raw");
	printf("\r");
	while (1)
	{
		printf("cmd(q,n,p,c): ");
		ch = getchar();
		printf("%c\r", ch);
		system("stty sane");
		switch (ch)
		{
		case 'q':
		case '3':
			printf("\n\r");
			exit(0);

		case 'c':
		case '\n':
		case '\r':
			return;

		case 'n':
			return 1;

		case 'p':
			return -1;
		}
	}
}

void
dump_data(ticks, cmd, data, len)
	u_int ticks;
	u_char cmd;
	u_char *data;
	int len;
{
	int i;

	printf("ticks %d cmd 0x%x len %d:", ticks, cmd, len);
	for (i = 0; i < len; i ++)
		printf(" %d", data[i]);
	printf("\n");
}

int
process_key_event(fd, evp)
	int fd;
	struct key_event *evp;
{
	static int target_chan;
	int val;

	switch (evp->ev_code)
	{
	case EVENT_EXIT:
		terminate(fd);
		return 0;

	case EVENT_CHGSONG:
		finish(fd);
		return evp->ev_arg;

	case EVENT_SKEW:
		if (evp->ev_arg < 0)
			skew -= 0.01;
		else
			skew += 0.01;
		return 0;

	case EVENT_MUTE:
		if (evp->ev_arg == 255)
			mute_control(fd, 1, -1);
		if (evp->ev_arg >= 1 || evp->ev_arg <= 16)	/* XXX */
		{
			val = channel_status[evp->ev_arg - 1].cs_mute ^ 1;
			mute_control(fd, val, evp->ev_arg);
		}
		return 0;

	case EVENT_LOOP:
		loop_mode ^= 1;
		break;

	case EVENT_STOP:
		if (playback_status.pb_stop == 0)
		{
			playback_status.pb_stop = 1;
			put_midi_packet(fd, MIDI_PKT_SYSTM | 
				MIDI_TIMER_PAUSE, 0, 0, NULL, 0);
			midi_note_off(fd, -1);
		}
		else
		{
			playback_status.pb_stop = 0;
			put_midi_packet(fd, MIDI_PKT_SYSTM | 
				MIDI_TIMER_UNPAUSE, 0, 0, NULL, 0);
		}
		return 0;

	case EVENT_MARK:
		if (evp->ev_arg < 0 || evp->ev_arg >= MAX_MARK)
			return 0;
		val = evp->ev_arg;
		playback_status_mark[val] = playback_status;
		playback_status_mark[val].pb_current_base =
			playback_status.pb_current;
		break;

	case EVENT_REPLAY:
		if (evp->ev_arg < 0 || evp->ev_arg >= MAX_MARK)
			return 0;
		val = evp->ev_arg;
		playback_status = playback_status_mark[val];
		midi_flush(fd);
		midi_note_off(fd, -1);
		put_midi_packet(fd, MIDI_PKT_SYSTM | MIDI_TIMER_INIT,
				0, 0, NULL, 0);
		return 0;

	case EVENT_SCHAN:
		if (evp->ev_arg <= 0 || evp->ev_arg > 16)
			return 0;
		target_chan = evp->ev_arg - 1;
		return 0;

	case EVENT_VCUT:
		if (evp->ev_arg < 0 || evp->ev_arg >= 100)
			return 0;
		channel_status[target_chan].cs_vcut = evp->ev_arg;
		break;
	}
	return 0;
}
