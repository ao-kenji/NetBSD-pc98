/*	$NecBSD: systmbusvar.h,v 3.17.4.2 1999/08/15 17:24:42 honda Exp $	*/
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
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#ifndef _I386_SYSTMBUSVAR_H_
#define	_I386_SYSTMBUSVAR_H_

#include <sys/device.h>
#include <machine/bus.h>

typedef void *systmbus_chipset_tag_t;

struct systmbus_attach_args {
	char *sba_busname;

	bus_space_tag_t sba_iot;
	bus_space_tag_t sba_memt;
	bus_dma_tag_t sba_dmat;

	systmbus_chipset_tag_t sba_ic;
};

struct systm_softc {
	struct device sc_dev;

	systmbus_chipset_tag_t sc_ic;

	bus_space_tag_t sc_iot;
	bus_space_tag_t sc_memt;
	bus_dma_tag_t sc_dmat;

	bus_space_handle_t sc_delaybah;
};

struct systm_attach_args {
	systmbus_chipset_tag_t sa_ic;

	bus_space_tag_t sa_iot;
	bus_space_tag_t sa_memt;
	bus_dma_tag_t sa_dmat;

	bus_space_handle_t sa_delaybah;
	u_int sa_cfgflags;
};

typedef int systm_event_t;
#define	SYSTM_EVENT_NOTIFY_SUSPEND	1
#define	SYSTM_EVENT_QUERY_SUSPEND	2
#define	SYSTM_EVENT_SUSPEND		3
#define	SYSTM_EVENT_RESUME		4	
#define	SYSTM_EVENT_NOTIFY_RESUME	5
#define	SYSTM_EVENT_QUERY_RESUME	6
#define	SYSTM_EVENT_ABORT_REQUEST	(-1)

#define	SYSTM_EVENT_STATUS_BUSY		0x0001
#define	SYSTM_EVENT_STATUS_REJECT	0x0002
#define	SYSTM_EVENT_STATUS_FERR		0x0004

typedef u_long systm_intr_line_t;
struct systm_intr_routing {
	systm_intr_line_t sir_global;
	systm_intr_line_t sir_allow_global;

	systm_intr_line_t sir_allow_isa;
	systm_intr_line_t sir_allow_pci;
	systm_intr_line_t sir_allow_pccs;
	systm_intr_line_t sir_allow_ippi;
};

struct systm_kthread_dispatch_table {
	u_char *kt_ename;
	u_char *kt_xname;
	u_char *kt_eargs;
	u_int	kt_flags;
#define	SYSTM_KTHREAD_DISPATCH_OK	0x0001
};

#ifdef	_KERNEL
extern struct systm_intr_routing *systm_intr_routing;
extern struct systm_kthread_dispatch_table systm_kthread_dispatch_table[];

void systmmsg_init __P((void));
void systmmsg_bind __P((struct device *, int (*) __P((struct device *, systm_event_t))));
int systmmsg_notify __P((struct device *, systm_event_t));
void systm_kthread_dispatch __P((void *));
#endif	/* _KERNEL */
#endif /* _I386_SYSTMBUSVAR_H_ */
