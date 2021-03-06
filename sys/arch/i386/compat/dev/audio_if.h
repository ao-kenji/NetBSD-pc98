/*	$NecBSD: audio_if.h,v 1.7 1998/03/14 07:06:44 kmatsuda Exp $	*/
/*	$NetBSD: audio_if.h,v 1.13 1997/05/20 12:51:45 augustss Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting staff.  All rights reserved.
 */
/*
 * Copyright (c) 1994 Havard Eidnes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Generic interface to hardware driver.
 */

struct audio_softc;

struct audio_params {
	u_long	sample_rate;			/* sample rate */
	u_int	encoding;			/* e.g. ulaw, linear, etc */
	u_int	precision;			/* bits/sample */
	u_int	channels;			/* mono(1), stereo(2) */
	/* Software en/decode functions, set if SW coding required by HW */
	void	(*sw_code)__P((void *, u_char *, int));
};

/* The default audio mode: 8 kHz mono ulaw */
extern struct audio_params audio_default;

struct audio_hw_if {
	int	(*open)__P((dev_t, int));	/* open hardware */
	void	(*close)__P((void *));		/* close hardware */
	int	(*drain)__P((void *));		/* Optional: drain buffers */
	
	/* Encoding. */
	/* XXX should we have separate in/out? */
	int	(*query_encoding)__P((void *, struct audio_encoding *));

	/* Set the audio encoding parameters (record and play).
	 * Return 0 on success, or an error code if the 
	 * requested parameters are impossible.
	 * The values in the params struct may be changed (e.g. rounding
	 * to the nearest sample rate.)
	 */
        int	(*set_params)__P((void *, int, struct audio_params *, struct audio_params *));
  
	/* Hardware may have some say in the blocksize to choose */
	int	(*round_blocksize)__P((void *, int));

	/* Ports (in/out ports) */
	int	(*set_out_port)__P((void *, int));
	int	(*get_out_port)__P((void *));
	int	(*set_in_port)__P((void *, int));
	int	(*get_in_port)__P((void *));

	/*
	 * Changing settings may require taking device out of "data mode",
	 * which can be quite expensive.  Also, audiosetinfo() may
	 * change several settings in quick succession.  To avoid
	 * having to take the device in/out of "data mode", we provide
	 * this function which indicates completion of settings
	 * adjustment.
	 */
	int	(*commit_settings)__P((void *));

	/* Start input/output routines. These usually control DMA. */
	int	(*start_output)__P((void *, void *, int,
				    void (*)(void *), void *));
	int	(*start_input)__P((void *, void *, int,
				   void (*)(void *), void *));
	int	(*halt_output)__P((void *));
	int	(*halt_input)__P((void *));
	int	(*cont_output)__P((void *));
	int	(*cont_input)__P((void *));

	int	(*speaker_ctl)__P((void *, int));
#define SPKR_ON		1
#define SPKR_OFF	0

	int	(*getdev)__P((void *, struct audio_device *));
	int	(*setfd)__P((void *, int));
	
	/* Mixer (in/out ports) */
	int	(*set_port)__P((void *, mixer_ctrl_t *));
	int	(*get_port)__P((void *, mixer_ctrl_t *));

	int	(*query_devinfo)__P((void *, mixer_devinfo_t *));
	
	int full_duplex; /* non-null if HW is able to do full-duplex */
	int audio_unit;
};

/* Register / deregister hardware driver */
extern int	audio_hardware_attach __P((struct audio_hw_if *, void *));
extern int	audio_hardware_detach __P((struct audio_hw_if *));

/* Device identity flags */
#define SOUND_DEVICE		0
#define AUDIO_DEVICE		0x80
#define MIXER_DEVICE		0x10

#define ISDEVAUDIO(x)		((minor(x)&0xf0) == AUDIO_DEVICE)
#define ISDEVSOUND(x)		((minor(x)&0xf0) == SOUND_DEVICE)
#define ISDEVMIXER(x)		((minor(x)&0xf0) == MIXER_DEVICE)

#define AUDIOUNIT(x)		(minor(x)&0x0f)
#define AUDIODEV(x)		(minor(x)&0xf0)

#ifndef __i386__
#define splaudio splbio		/* XXX */
#define IPL_AUDIO IPL_BIO	/* XXX */
#endif

