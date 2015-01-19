/*	$NecBSD: playmidi.h,v 1.8 1998/02/08 08:01:19 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/* 
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */
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
#include <machine/pio.h>
#include <machine/sysarch.h>

#define PERCUSSION	0x0200
#define DEFAULT_PLAYBACK_MODE
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
#define NO_EXIT		100

struct chanstate {
    int program;
    int bender;
    int oldbend;	/* used for graphics */
    int bender_range;
    int oldrange;	/* used for graphics */
    int controller[255];
    int pressure;
};

struct voicestate {
    int note;
    int channel;
    int timestamp;
    int dead;
};

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
   unsigned long int length;	/* length of track data */
   unsigned long int index;	/* current byte in track */
   unsigned long int ticks;	/* current midi tick count */
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

#define	mpu401_data	0
#define	mpu98_data	0
#define	mpu401_stat	1
#define	mpu98_stat	2
#define	mpu401_cmd	1
#define	mpu98_cmd	2
#define SR_RRDY		0x80
#define	SR_WRDY		0x40

#define	qv_cmd	4
#define	qv_stat	6
#define SR_BUSY 2

struct midi_hw {
	u_char *hw_id;

	u_char *hw_dvname;

	int (*hw_probe) __P((int));

	int (*hw_open) __P((int));

	int (*hw_output) __P((int, u_char));

	u_char (*hw_input) __P((int));

	int (*hw_flush) __P((int));

	int hw_dfiobase;
};

#define	MIDI_OUTPUT(data)	((*hw->hw_output)(iobase, data))
#define	MIDI_INPUT()		((*hw->hw_input)(iobase))
#define	MIDI_FLUSH()		{if (hw->hw_flush) (*hw->hw_flush)(iobase);}

extern struct midi_hw *hw;
extern int iobase;
extern int timer_tick;
extern u_char dvname[];

extern int ntrks;
extern int graphics, division, ntrks, format;
extern int p_remap;
extern int chanmask;
extern struct miditrack seq[MAXTRKS];
extern float skew;
extern perc, dochan;
extern unsigned long int default_tempo;
extern int next_skip;
extern char *gmvoice[];

void init_show __P(());
int updatestatus __P(());
void showevent __P((int, u_char *, int));

void shutdown_MIDI __P((void));
void seq_stop_note __P((u_int, u_int, u_int));
int midi_hw_select __P((void));

int playevents __P((void));
int readmidi __P((u_char *, off_t));
void setup_show __P((int, char **));
void close_show __P((int));
