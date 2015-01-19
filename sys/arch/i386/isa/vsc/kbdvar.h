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

#ifndef _I386_VSC_KBDVAR_H_
#define _I386_VSC_KBDVAR_H_

struct kbd_ovlkey;
struct saver_softc;
struct scan_code;
struct kbd_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_ioh;
	void *sc_ih;

#define	KBD_CAP_CMDACCEPT	0x0001
	int sc_cap;
	int sc_cwin;

	int (*sc_vsc_control) __P((void *, u_int, u_int *));
	void *sc_vsc_control_arg;
	struct tty *sc_tp;
	struct saver_softc *sc_svp;

	struct vsc_kbd_info *sc_vi;
	u_int sc_shifts;
	u_int sc_hookkey;

	int sc_kmake_codeset;
	int sc_krel_codeset;

	u_char *sc_mc;
	u_char sc_capchar[4];
	u_char sc_Xchar[4];
	u_char sc_metachar[4];

	int sc_tpmrate;
	int sc_repflags;
	struct scan_code *sc_keytab;
	struct kbd_ovlkey *sc_ovltbl;
};

#define	USE_PC98_BSDEL	1
#define	USE_ALT_SWITCH	2
#define	NO_BSKEY	14
#define	NO_DELKEY	57

#define	CODE_SIZE	4	/* Use a max of 4 for now... */
#define	KBD_TABLE_SIZE	128
#define	OVLTBL_SIZE	64

typedef struct {
	u_char subtype;		/* subtype, string or function */
	union what {
		u_char *string;	/* ptr to string, null terminated */
		void (*func) __P((void));	/* ptr to function */
	} what;
} entry;

struct scan_code {
	u_short type;
	u_short ovlindex;
	entry unshift;
	entry shift;
	entry ctrl;
};
#endif /* !_I386_VSC_KBDVAR_H_ */
