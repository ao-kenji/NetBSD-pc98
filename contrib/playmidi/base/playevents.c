/*	$NecBSD: playevents.c,v 1.7 1998/02/08 08:01:17 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/* 
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */
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
#include <signal.h>

struct timeval start_time;
int note_vel[16][128];
struct voicestate voice[2][36];
struct chanstate channel[16];

/***************************************************************
 * TIMER (METRONOME) 
 ***************************************************************/
/* The bsd interval timer's resolution is 10 ms (the ISA case).
 * However a MIDI needs at least 1 ms resolution.
 * Thus I propose two solutions.
 * 1) all required sleep ticks are round up to 10ms (P options).
 * 2) polling the timer if the required ticks below MIN_HOLE_PERIOD (default).
 * The second case, much time will be eaten in an idling loop.
 */
#define SLEEP_ALLOWABLE_PERIOD (49)
#define MIN_HOLD_PERIOD (12)

int timer_tick;
u_int64_t prev_time;

void update_msec_clock __P((void));
void timer_up __P((int));
void sig_alrm __P(());
void timer_init __P((void));

void
sig_alrm()
{
}

void
update_msec_clock(void)
{
	struct timeval ct;
	u_int64_t cur;

	gettimeofday(&ct, NULL);
	cur = (u_int64_t) ct.tv_sec * (u_int64_t) 1000000 + 
	      (u_int64_t) ct.tv_usec;

	timer_tick = (cur - prev_time) / (u_int64_t) 1000;
}
	
void
timer_up(timex)
	int timex;
{
	struct itimerval exp;

	exp.it_interval.tv_sec = 0;
	exp.it_interval.tv_usec = timex;
	exp.it_value.tv_sec = 0;
	exp.it_value.tv_usec = timex;
	setitimer(ITIMER_REAL, &exp, NULL);
}

void
timer_init(void)
{
	struct timeval ct;

	timer_tick = 0;
	gettimeofday(&ct, NULL);
	prev_time = (u_int64_t) ct.tv_sec * (u_int64_t)  1000000 + 
		    (u_int64_t) ct.tv_usec;
}

/***************************************************************
 * MAIN
 ***************************************************************/
#define	SEC2MSEC	(1000)
u_char midimsg_GM_ON[] = {0xf0, 0x7e, 0x7f, 0x9, 0x01, 0xf7};

void
MIDI_cmd_out(cmd, len)
	u_char *cmd;
	int len;
{
	int i;

	for (i = 0; i < len; i ++)
		MIDI_OUTPUT(cmd[i]);
	MIDI_FLUSH();
}

void
seq_stop_note(chan, note, vel)
	u_int chan, note, vel;
{

	MIDI_OUTPUT(chan + MIDI_NOTEOFF);
	MIDI_OUTPUT(note);
	MIDI_OUTPUT(vel);
	MIDI_FLUSH();
}

void
shutdown_MIDI(void)
{

    MIDI_cmd_out(midimsg_GM_ON, 6);
    MIDI_FLUSH();
}

	
unsigned long int 
rvl(s)
	struct miditrack *s;
{
	register unsigned long int value;
    	register u_char c;

    	if (s->index >= s->length)
		return 0;

	value = s->data[(s->index)++];
	if (value & 0x80)
	{
		value &= 0x7f;
		do 
		{
	    		if (s->index < s->length)
			{
				c = s->data[(s->index)++];
				value = (value << 7) + (c & 0x7f);
			}
	    		else
				c = 0;
		} 
		while (c & 0x80);
    	}

    	return value;
}

/* indexed by high nibble of command */
int cmdlen[16] =
{0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 1, 1, 2, 0};
static int rpn1[16] =
{127, 127, 127, 127, 127, 127, 127, 127,
 127, 127, 127, 127, 127, 127, 127, 127};
static int rpn2[16] =
{127, 127, 127, 127, 127, 127, 127, 127,
 127, 127, 127, 127, 127, 127, 127, 127};

#define CMD		seq[track].running_st
#define TIME		seq[track].ticks
#define CHN		(CMD & 0xf)
#define NOTE		data[0]
#define VEL		data[1]

int 
playevents()
{
	unsigned long int tempo = default_tempo, lasttime = 0;
    	u_int lowtime, track, best, length;
    	u_char *data;
    	double current = 0.0, dtime = 0.0;
    	int i, j, play_status, playing = 1;

	for (i = 0; i < 16; i++) 
    	{
        	channel[i].program = 0;
		for (j = 0; j < 128; j++)
	    		note_vel[i][j] = 0;

		channel[i].bender = channel[i].oldbend = 8192;
		channel[i].bender_range = channel[i].oldrange = 2;
		channel[i].controller[CTL_PAN] = 64;
		channel[i].controller[CTL_SUSTAIN] = 0;
	}

	init_show();

	signal(SIGALRM, sig_alrm);
	timer_init();

    	MIDI_FLUSH();
    	MIDI_cmd_out(midimsg_GM_ON, 6);

	gettimeofday(&start_time, NULL);	/* for synchronization */
    	for (track = 0; track < ntrks; track++) 
	{
		seq[track].index = seq[track].running_st = 0;
		seq[track].ticks = rvl(&seq[track]);
    	}

	while (playing) 
	{
		lowtime = ~0;
		for (best = track = 0; track < ntrks; track++)
		{
	    		if (seq[track].ticks < lowtime) 
			{
				best = track;
				lowtime = TIME;
	    		}
		}

		if (lowtime == ~0)
	    		break;	
		track = best;

		if ((seq[track].data[seq[track].index] & 0x80) &&
	    	    (seq[track].index < seq[track].length))
	    		CMD = seq[track].data[seq[track].index++];

		if (CMD == 0xff && seq[track].index < seq[track].length)
	    		CMD = seq[track].data[seq[track].index++];

		if (CMD > 0xf7)
	    		length = 0;
		else if (!(length = cmdlen[(CMD & 0xf0) >> 4]))
	    		length = rvl(&seq[track]);

		if (seq[track].index + length < seq[track].length) 
		{

	    		data = &(seq[track].data[seq[track].index]);
	    		if (CMD == set_tempo)
				tempo = ((*(data) << 16) | (data[1] << 8) | data[2]);
	    		if (TIME > lasttime) 
	   		{
				MIDI_FLUSH();

				if (division > 0) 
				{
		    			dtime = ((double) ((TIME - lasttime) * (tempo / 10000)) / (double) (division)) * (double) skew * (double) 10.0;
		    			current += dtime;
		    			lasttime = TIME;
				}	 
				else if (division < 0)
					current = ((double) TIME / ((double) ((division & 0xff00 >> 8) * (double) (division & 0xff)) * 10000.0)) * (double) skew * (double) 10.0;

				if ((u_int) dtime > 40960)
				{
					playing = 0;
					continue;
				}

				update_msec_clock();
				while(timer_tick < (u_int) current)
				{
					if ((u_int) dtime >= SLEEP_ALLOWABLE_PERIOD || ((u_int) current - timer_tick) > MIN_HOLD_PERIOD)
					{
						timer_up(10000);
						pause();
						timer_up(0);
					}
					update_msec_clock();
				}
			}

		    	if (graphics)
		    	{
				if ((play_status = updatestatus()) != NO_EXIT)
			    		return play_status;
		    	}

			if (CMD > 0x7f && CMD < 0xf0 && ISPERC(CHN) && p_remap) 
			{
				CMD &= 0xf0;
				CMD |= (p_remap - 1);
			}

/* -------------------------------------------------------------------*/
	    	if (playing && (CMD & 0x80) && ISPLAYING(CHN))
	    	{
			switch (CMD & 0xf0) 
			{
			case MIDI_KEY_PRESSURE:
			    	MIDI_OUTPUT(CMD);
			    	MIDI_OUTPUT(NOTE);
			    	MIDI_OUTPUT(VEL);
		    		break;

			case MIDI_NOTEON:
		    		note_vel[CHN][NOTE] = VEL;
		    		MIDI_OUTPUT(CMD);
	            		MIDI_OUTPUT(NOTE);
	           		MIDI_OUTPUT(VEL);
		    		break;

			case MIDI_NOTEOFF:
		    		note_vel[CHN][NOTE] = 0;
		    		MIDI_OUTPUT(CMD);
	            		MIDI_OUTPUT(NOTE);
	            		MIDI_OUTPUT(VEL);
		    		break;

			case MIDI_CTL_CHANGE:
			{
		    		u_int p1, p2;

		    		p1 = NOTE;
		    		p2 = VEL;
		    		channel[CHN].controller[p1] = p2;
		    		MIDI_OUTPUT(CMD);
		    		MIDI_OUTPUT(p1);
		    		MIDI_OUTPUT(p2);
		    		switch (p1) 
				{
				case CTL_SUSTAIN:
					break;

			    	case CTL_REGIST_PARM_NUM_MSB:
					rpn1[CHN] = p2;
					break;

			    	case CTL_REGIST_PARM_NUM_LSB:
					rpn2[CHN] = p2;
					break;

			    	case CTL_DATA_ENTRY:
					if (rpn1[CHN] == 0 && rpn2[CHN] == 0) 
					{
				    		channel[CHN].oldrange = channel[CHN].bender_range;
				    		channel[CHN].bender_range = p2;
				    		rpn1[CHN] = rpn2[CHN] = 127;
					}
					break;

			    	default:
					break;
		    		}
		    		break;
			}

			case MIDI_CHN_PRESSURE:
		    		channel[CHN].pressure = VEL;
		    		MIDI_OUTPUT(CMD);
		    		MIDI_OUTPUT(VEL);
		    		break;

			case MIDI_PITCH_BEND:
			{
		    		u_int p1, p2, val;

		    		p1 = NOTE;
		    		p2 = VEL;
		    		MIDI_OUTPUT(CMD);
		    		MIDI_OUTPUT(p1);
		    		MIDI_OUTPUT(p2);
		    		val = (p2 << 7) + p1;
		    		channel[CHN].oldbend = channel[CHN].bender;
		    		channel[CHN].bender = val;
		    		break;
			}

			case MIDI_PGM_CHANGE:
    		    		channel[CHN].program = NOTE;
		    		MIDI_OUTPUT(CMD);
		    		MIDI_OUTPUT(NOTE);
		    		break;

			case MIDI_SYSTEM_PREFIX:
		    		MIDI_OUTPUT(CMD);
    		    		MIDI_cmd_out(data, length);
		    		break;

			default:
		    		break;
			}
		}
/* -------------------------------------------------------------------*/

			if (graphics) 
			{
				MIDI_FLUSH();
				showevent(CMD, data, length);
			}
		}

		seq[track].index += length;
		if (seq[track].index >= seq[track].length)
	    		seq[track].ticks = ~0;
		else
	    		seq[track].ticks += rvl(&seq[track]);
	}

    	MIDI_FLUSH();
    	return next_skip;
}
