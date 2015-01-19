/*	$NecBSD: servar.h,v 1.1 1998/12/31 02:33:14 honda Exp $	*/
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

/*
 * Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef	_SERVAR_H_
#define	_SERVAR_H_

#define	NSLAVES	4

struct ser_softc {
	struct device sc_dev;
	void *sc_ih;

	struct com_softc *sc_slaves[NSLAVES];	/* our children */

	/* shared resources */
	bus_space_tag_t sc_ibst;		/* shared int io tag */
	bus_space_handle_t sc_ibsh;		/* shared int io handle */
	int (*sc_intr) __P((void *));		/* shared interrupt handler */

	/* shared hardware timer */
	bus_space_tag_t sc_tbst;		/* shared timer io tag */
	bus_space_handle_t sc_tbsh;		/* shared timer io handle */
	void ((*sc_init) __P((struct ser_softc *)));
	void ((*sc_start) __P((struct ser_softc *)));
	void ((*sc_stop) __P((struct ser_softc *)));
	u_int sc_period;			/* timer cycle */
	int sc_hwtref;				/* timer ref count */
};

int serintr __P((void *));
int serprint __P((void *, const char *));
int ser_allocate_timer __P((struct ser_softc *, bus_space_tag_t, bus_space_handle_t));
#endif	/* !_SERVAR_H_ */
