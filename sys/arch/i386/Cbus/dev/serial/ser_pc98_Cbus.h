/*	$NecBSD: ser_pc98_Cbus.h,v 1.2 1999/04/15 01:36:18 kmatsuda Exp $	*/
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

#ifndef	_SER_PC98_CBUS_H_
#define	_SER_PC98_CBUS_H_
struct com_hw {
	int type;
	int subtype;
	u_int hwflags;

	u_long freq;
	struct comspeed *spt;
	void ((*speedfunc) __P((struct com_softc *, int)));
	u_int8_t ((*read_msr) __P((struct com_softc *)));
	void ((*control_intr) __P((struct com_softc *, u_int8_t)));

	int ((*init) __P((struct com_softc *, u_long, u_long)));

	u_int maxunit;
	bus_size_t skip;
	bus_size_t nports;

	bus_space_iat_t iat;	/* generic serial chip iat */
	bus_size_t iatsz;	/* generic serial chip iatsz */

	bus_space_iat_t spiat;	/* hardware specific iat */
	bus_size_t spiatsz;	/* hardware specific iatsz */
};

struct commulti_attach_args;
struct com_hw *ser_find_hw __P((u_int));
void ser_setup_ca __P((struct commulti_attach_args *, struct com_hw *));
#endif	/* !_SER_PC98_CBUS_H_ */
