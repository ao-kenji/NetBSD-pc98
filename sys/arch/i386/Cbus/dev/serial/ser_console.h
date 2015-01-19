/*	$NecBSD: ser_console.h,v 1.2 1999/04/15 01:36:17 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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

#ifndef	_SER_CONSOLE_H_
#define	_SER_CONSOLE_H_
#include <dev/cons.h>

struct comcons_switch {
	struct com_softc *(*comcnmalloc) __P((void));
	int (*comcnprobe) __P((struct commulti_attach_args *));
	void (*comcnhwinit) __P((struct com_softc *, int));
	int (*comcngetc) __P((struct com_softc *, dev_t));
	void (*comcnputc) __P((struct com_softc *, dev_t, int));
	void (*comcnpollc) __P((struct com_softc *, dev_t, int));
};

int ser_connect_comconsole __P((struct com_softc *));

/* These two functions are machdep, you should define */
struct com_softc *ser_console_probe_machdep __P((struct consdev *, struct commulti_attach_args *));
int is_ser_comconsole_machdep __P((bus_space_tag_t, bus_addr_t));

extern struct com_softc *sercnsgsc;
extern struct com_space_handle comconsole_cs;

#ifdef	KGDB
void comkgdb_attach __P((struct com_softc *));
#endif	/* KGDB */

#if defined(i386)
#include <i386/isa/pc98spec.h>	/* comconsole mode definition only */
#define	CONSMODE_NOWAIT_INPUT (lookup_sysinfo(SYSINFO_CONSMODE) & CONSM_NDELAY)
#else	/* !i386 */
#define	CONSMODE_NOWAIT_INPUT (1)
#endif	/* !i386 */
#endif	/* !_SER_CONSOLE_H_ */
