/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
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

#ifndef _I386_VSC_INTERFACE_H_
#define _I386_VSC_INTERFACE_H_
/*
 * VSC input methods interface.
 * All input methods are required to implement the following
 * interface functions.
 */

/*
 * vsc callback control functions
 */
struct kbd_service_functions;
struct kbd_connect_args {
	int (*ka_control) __P((void *, u_int, u_int *));
	void *ka_control_arg;

	struct kbd_service_functions *ka_ks;
};

/*
 * vsc kbd common informations
 */
struct vsc_kbd_info {
	u_int vi_flags;
	u_int vi_leds;
};

#define	VSC_CONTROL_CMDMASK	0x0ffff
#define	VSC_CONTROL_FORCE	0x10000
#define	VSC_CONTROL_WCHG	0x00001
#define	VSC_CONTROL_LCHG	0x00002
#define	VSC_CONTROL_VSCROLL	0x00003
#define	VSC_CONTROL_SETMODE	0x00004

#define	VSC_KBDMODE_SET_XMODE	0x00001
#define	VSC_KBDMODE_SET_RAWKEY	0x00002
#define	VSC_KBDMODE_SET_KEYLNM	0x00004

#define	VSC_KBDPOLL_DIRECT	0x00001

/* control: screen */
#define	VSC_SCREEN_CHG		0x0001
#define	VSC_SCREEN_SC_U		0x0003
#define	VSC_SCREEN_SC_D		0x0004
#define	VSC_SCREEN_SC_E		0x0005

/* control: character code sets */
#define	VSC_KBDMODE_CODESET0	0x0000
#define	VSC_KBDMODE_CODESET1	0x0001
#define	VSC_KBDMODE_CODESET2	0x0002

/*
 * kbd service functions
 */
struct tty;
struct saver_softc;
struct kbd_service_functions {
	/* console */
	int (*ks_console_establish) __P((void));
	u_char *((*ks_poll) __P((void *, int)));

	/* normal */
	void *((*ks_connect_establish) __P((struct kbd_connect_args *)));
	int (*ks_set_led) __P((void *, int));
	int (*ks_switch) __P((void *, int, struct vsc_kbd_info *, struct tty *));
	int (*ks_kbdioctl) __P((void *, int, int, caddr_t, int));
	int (*ks_setmode) __P((void *, int, int));
	int (*ks_connect_saver) __P((void *, struct saver_softc *));
};
#endif /* !_I386_VSC_INTERFACE_H_ */
