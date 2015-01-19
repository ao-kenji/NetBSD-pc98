/*	$NecBSD: playmidi.h,v 1.6 1998/04/14 13:03:35 honda Exp $	*/
/*	$NetBSD$	*/

#define RELEASE "Playmidi 2.3"
/************************************************************************
   playmidi.h  --  defines and structures for use by playmidi package

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define MAXTRKS		128

#define	   CTL_BANK_SELECT		0x00
#define	   CTL_MODWHEEL			0x01
#define    CTL_BREATH			0x02
#define    CTL_FOOT			0x04
#define    CTL_PORTAMENTO_TIME		0x05
#define    CTL_DATA_ENTRY		0x06
#define    CTL_MAIN_VOLUME		0x07
#define    CTL_BALANCE			0x08
#define    CTL_PAN			0x0a
#define    CTL_EXPRESSION		0x0b
#define    CTL_GENERAL_PURPOSE1		0x10
#define    CTL_GENERAL_PURPOSE2		0x11
#define    CTL_GENERAL_PURPOSE3		0x12
#define    CTL_GENERAL_PURPOSE4		0x13
#define    CTL_DAMPER_PEDAL		0x40
#define    CTL_SUSTAIN			0x40	/* Alias */
#define    CTL_HOLD			0x40	/* Alias */
#define    CTL_PORTAMENTO		0x41
#define    CTL_SOSTENUTO		0x42
#define    CTL_SOFT_PEDAL		0x43
#define    CTL_HOLD2			0x45
#define    CTL_GENERAL_PURPOSE5		0x50
#define    CTL_GENERAL_PURPOSE6		0x51
#define    CTL_GENERAL_PURPOSE7		0x52
#define    CTL_GENERAL_PURPOSE8		0x53
#define    CTL_EXT_EFF_DEPTH		0x5b
#define    CTL_TREMOLO_DEPTH		0x5c
#define    CTL_CHORUS_DEPTH		0x5d
#define    CTL_DETUNE_DEPTH		0x5e
#define    CTL_CELESTE_DEPTH		0x5e	/* Alias for the above one */
#define    CTL_PHASER_DEPTH		0x5f
#define    CTL_DATA_INCREMENT		0x60
#define    CTL_DATA_DECREMENT		0x61
#define    CTL_NONREG_PARM_NUM_LSB	0x62
#define    CTL_NONREG_PARM_NUM_MSB	0x63
#define    CTL_REGIST_PARM_NUM_LSB	0x64
#define    CTL_REGIST_PARM_NUM_MSB	0x65
#define    CTRL_PITCH_BENDER		255
#define    CTRL_PITCH_BENDER_RANGE	254
#define    CTRL_EXPRESSION		253	/* Obsolete */
#define    CTRL_MAIN_VOLUME		252	/* Obsolete */

#define ISPERC(x)	(perc & (1 << x))
#define ISPLAYING(x)	(chanmask & (1 << x))

#define RIFF			0x52494646
#define CTMF			0x43544d46

#define MThd			0x4d546864
#define MTrk			0x4d54726b
#define	meta_event		0xff
#define	sequence_number 	0x00
#define	text_event		0x01
#define copyright_notice 	0x02
#define sequence_name    	0x03
#define instrument_name 	0x04
#define lyric	        	0x05
#define marker			0x06
#define	cue_point		0x07
#define channel_prefix		0x20
#define	end_of_track		0x2f
#define	set_tempo		0x51
#define	smpte_offset		0x54
#define	time_signature		0x58
#define	key_signature		0x59
#define	sequencer_specific	0x74

struct miditrack {
   u_char *data;		/* data of midi track */
   u_long length;	/* length of track data */
   u_long index;	/* current byte in track */
   u_long ticks;	/* current midi tick count */
   u_char running_st;		/* running status byte */
};

#define MIDI_NOTEOFF		0x80
#define MIDI_NOTEON		0x90
#define MIDI_KEY_PRESSURE	0xA0
#define MIDI_CTL_CHANGE		0xB0
#define MIDI_PGM_CHANGE		0xC0
#define MIDI_CHN_PRESSURE	0xD0
#define MIDI_PITCH_BEND		0xE0
#define MIDI_SYSTEM_PREFIX	0xF0

#define	MAX_CHAN	32
struct channel_status {
	int cs_mute;
	u_int cs_Mbank;
	u_int cs_Lbank;
	u_int cs_vcut;
};

#define	EVENT_NONE	0
#define	EVENT_EXIT	1
#define	EVENT_CHGSONG	2
#define EVENT_SKEW	3
#define	EVENT_MUTE	4
#define	EVENT_MARK	5
#define	EVENT_REPLAY	6
#define	EVENT_STOP	7
#define	EVENT_LOOP	8
#define	EVENT_VCUT	9
#define	EVENT_SCHAN	10

struct key_event {
	int ev_code;				/* key event code */
	int ev_arg;
};

struct playback_status {
	double pb_current;			/* current excution time */
	double pb_current_base;
	u_int pb_lasttime;			
	int pb_stop;				/* pause */

	u_int pb_tempo;				/* current tempo */

	u_int pb_ntrks;				/* ntrks */
	struct miditrack pb_trk[MAXTRKS];	/* trk data */
};

#define	FORCE_88PROMAP	0x0001			/* remap 88map to 88promap */
#define	FORCE_XGNATIVE	0x0002			/* select XG native tone */
#define	FORCE_GSRESET	0x0004			/* GS reset */
#define	FORCE_XGRESET	0x0008			/* XG reset */

extern int ntrks, find_header;
extern int division, format;
extern struct miditrack seq[MAXTRKS];
extern double skew;
extern char *filename;
extern int graphics;
extern int dumpon;
extern u_int cflags;
extern int ttyFd;
extern struct channel_status channel_status[];
extern struct playback_status playback_status;

int getkey __P((struct key_event *));
int playevents __P((int));
int readmidi __P((u_char *, off_t));
void setup_show __P((void));
void init_show __P((void));
void show_status __P((int));
void showevent __P((u_char, char *, int));
int updatestatus __P((void));
void close_show __P((int));
