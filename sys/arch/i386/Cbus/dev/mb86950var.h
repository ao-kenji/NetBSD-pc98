/*	$NecBSD: mb86950var.h,v 1.5 1999/07/24 05:56:54 kmatsuda Exp $	*/
/*	$NetBSD: mb86960var.h,v 1.21 1998/03/22 04:25:36 enami Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 *  Copyright (c) 1998
 *	Kouichi Matsuda. All rights reserved.
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
 *      This product includes software developed by Kouichi Matsuda.
 * 4. The names of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Copyright (c) 1995 Mika Kortelainen
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
 *      This product includes software developed by  Mika Kortelainen
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Thanks for Aspecs Oy (Finland) for the data book for the NIC used
 * in this card and also many thanks for the Resource Management Force
 * (QuickNet card manufacturer) and especially Daniel Koch for providing
 * me with the necessary 'inside' information to write the driver.
 */

#include "rnd.h"

#if NRND > 0
#include <sys/rnd.h>
#endif

/*
 * Debug control.
 * 0: No debug at all.  All debug specific codes are stripped off.
 * 1: Silent.  No debug messages are logged except emergent ones.
 * 2: Brief.  Lair events and/or important information are logged.
 * 3: Detailed.  Logs all information which *may* be useful for debugging.
 * 4: Trace.  All actions in the driver is logged.  Super verbose.
 */
#ifndef FES_DEBUG
#define FES_DEBUG		1
#endif

/*
 * Supported hardware (Ethernet card) types
 * This information is currently used only for debugging
 */
enum fes_type {
	/* For cards which are successfully probed but not identified. */
	FES_TYPE_UNKNOWN = 0,

	/* Sun Electronics AngeLan AL-98 */
	FES_TYPE_AL98,

	/* Ungermann-Bass Access/PC N98C+ (PC85151) */
	FES_TYPE_PC85151,

	/* Ungermann-Bass Access/NOTE N98 (PC86131) */
	FES_TYPE_PC86131,
};

enum mb86950_type {
	MB86950_TYPE_86950
};

#ifndef	 ETHER_ADDR_LEN
#define ETHER_ADDR_LEN	6	/* number of bytes in an address. */
#endif

/*
 * fes_softc: per line info and status
 */
struct mb86950_softc {
	struct device sc_dev;
	struct ethercom sc_ec;		/* ethernet common */
	struct ifmedia sc_media;	/* supported media information */

	bus_space_tag_t sc_bst;		/* bus space */
	bus_space_handle_t sc_bsh;

	/* Set by probe() and not modified in later phases. */
	enum	mb86950_type type;	/* controller type */

	/* Vendor specific hooks. */
	void	(*init_card) __P((struct mb86950_softc *));
	void	(*stop_card) __P((struct mb86950_softc *));

	/* Transmission buffer management. */
	u_int8_t txb_pending;

	u_int8_t sc_enaddr[ETHER_ADDR_LEN];

#if NRND > 0
	rndsource_element_t rnd_source;
#endif

	int	sc_enabled;	/* boolean; power enabled on interface */

	int	(*sc_enable) __P((struct mb86950_softc *));
	void	(*sc_disable) __P((struct mb86950_softc *));

	int	(*sc_mediachange) __P((struct mb86950_softc *));
	void	(*sc_mediastatus) __P((struct mb86950_softc *,
		    struct ifmediareq *));

	/* NIC register access methods */
	u_char	(*sc_nic_get) __P((bus_space_tag_t, bus_space_handle_t,
		    u_int8_t));
	void	(*sc_nic_put) __P((bus_space_tag_t, bus_space_handle_t,
		    u_int8_t, u_int8_t));

	void	*sc_sh;		/* shutdownhook cookie */
};

/*
 * Fes driver specific constants which relate to 86950.
 */

/* Interrupt masks. */
#define FES_TMASK (FES_D1_COLL16 | FES_D1_TXDONE)
#define	FES_RMASK (FES_D3_PKTRDY | FES_D3_ALGERR |\
		  FES_D3_CRCERR | FES_D3_OVRFLO)

/* Maximum number of iterrations for a receive interrupt. */
#define	FES_MAX_RECV_COUNT 30

/*
 * NIC register access macros
 */
/*
 * XXX: NIC register access method depend on board implimentation.
 *	But it is used in MI code, so...
 */
#define NIC_GET(sc, reg)	\
	(*(sc)->sc_nic_get)((sc)->sc_bst, (sc)->sc_bsh, (reg))
#define NIC_PUT(sc, reg, val)	\
	(*(sc)->sc_nic_put)((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))

void	mb86950_attach	__P((struct mb86950_softc *, enum mb86950_type,
	    u_int8_t *));
void	mb86950_config	__P((struct mb86950_softc *, int *, int, int));
int	mb86950_intr	__P((void *));
