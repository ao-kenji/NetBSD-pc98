/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_DEV_PMIDI_IF_H_
#define _SYS_DEV_PMIDI_IF_H_

/*
 * size of midi buffer
 */
#define	MIDI_RING_SIZE	(0x4000)

/*
 * midi packet structure
 */
#define	MIDI_PKT_DATA		0x0000		/* midi data packet */

#define	MIDI_PKT_SYSTM		0xff00		/* systm control packet */
#define	MIDI_TIMER_INIT		0x0001		/* initialize timer tick */
#define	MIDI_TIMER_PAUSE	0x0002		/* pause */
#define	MIDI_TIMER_UNPAUSE	0x0003		/* unpause */
#define	MIDI_PORT_THRU		0x0010		/* thru */

struct pmidi_packet_header {
	u_int16_t mp_code;		/* what kind of the packet */

	u_int16_t mp_len;		/* data len if exists */
	u_int mp_ticks;			/* excution ticks of the packet */
};

/*
 * primitive ioctl
 */
#define MIDI_DRAIN	_IO('M', 23)
#define MIDI_FLUSH	_IO('M', 24)

#ifdef	_KERNEL
void	pmidi_attach_mi __P((struct midi_hw_if *, void *, struct device *));
#endif	/* _KERNEL */

#endif /* _SYS_DEV_PMIDI_IF_H_ */
