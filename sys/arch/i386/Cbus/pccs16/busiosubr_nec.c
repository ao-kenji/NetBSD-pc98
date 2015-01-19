/*	$NecBSD: busiosubr_nec.c,v 1.14.4.3 1999/08/19 12:06:08 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
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
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <sys/sysctl.h>

#define _I386_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <machine/isa_machdep.h>
#include <i386/isa/pc98spec.h>
#include <i386/Cbus/pccs16/nepcreg.h>
#include <i386/Cbus/pccs16/busiosubr_nec.h>

#define	NEPC_MAX_IOSIZE		128 	/* XXX */
#define	NEPC_MAX_MAPSIZE	32
#define	NEPC_FRAG_IOSIZE	16	/* XXX */
#define	NEPC_MAX_MEMSIZE	0x2000	/* XXX */

int nepc_bus_space_io_map __P((bus_space_tag_t, bus_addr_t, bus_size_t, int,
			    bus_space_handle_t *));
int nepc_bus_space_io_map_load __P((bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_iat_t, u_int));
int nepc_bus_space_io_map_prefer __P((bus_space_tag_t, bus_addr_t *,
				bus_addr_t, bus_size_t, bus_size_t));
int nepc_bus_space_mem_map __P((bus_space_tag_t, bus_addr_t, bus_size_t, int,
			    bus_space_handle_t *));
int nepc_bus_space_mem_map_load __P((bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_iat_t, u_int));
int nepc_bus_space_mem_map_prefer 
	__P((bus_space_tag_t, bus_addr_t *, bus_addr_t, bus_size_t, bus_size_t));


_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_io,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(NEPC_DA_io,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(NEPC_DA_io,u_int32_t,4)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int32_t,4)

_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_io,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(NEPC_RA_io,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(NEPC_RA_io,u_int32_t,4)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int32_t,4)

struct bus_space_tag NEPC_io_space_tag = {
	0,
	BUS_SPACE_IO,
	NULL,
	
	nepc_bus_space_io_map,
	i386_memio_unmap,
	i386_memio_free,
	i386_memio_subregion,
	nepc_bus_space_io_map_load,
	i386_bus_space_map_unload,
	i386_bus_space_map_activate,
	i386_bus_space_map_deactivate,
	nepc_bus_space_io_map_prefer,
	NULL,

	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_io,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(NEPC_DA_io,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(NEPC_DA_io,u_int32_t,4),
	},

	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_io,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(NEPC_RA_io,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(NEPC_RA_io,u_int32_t,4),
	}
};

struct bus_space_tag NEPC_mem_space_tag = {
	0,
	BUS_SPACE_MEM,
	NULL,
	
	nepc_bus_space_mem_map,
	i386_memio_unmap,
	i386_memio_free,
	i386_memio_subregion,
	nepc_bus_space_mem_map_load,
	i386_bus_space_map_unload,
	i386_bus_space_map_activate,
	i386_bus_space_map_deactivate,
	nepc_bus_space_mem_map_prefer,
	NULL,

	/* direct bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int32_t,4),
	},

	/* relocate bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int32_t,4),
	}
};

int
nepc_bus_space_io_map(t, bpa, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	bus_space_iat_t tiat;
	bus_addr_t base;
	int error, nc;

	if (size > NEPC_MAX_MAPSIZE)
		return EINVAL;

	error = i386_memio_map(t, bpa, 0, flags, bshp);
	if (error != 0 || size == 0)
		return error;

	tiat = (bus_space_iat_t) 
		malloc(size * sizeof(bus_addr_t), M_TEMP, M_NOWAIT);
	if (tiat == NULL)
	{
		i386_memio_unmap(t, *bshp, 0);
		return EINVAL;
	}

	base = _nepc_io_swapaddr(t, 0);
	for (nc = 0; nc < size; nc ++)
		tiat[nc] = _nepc_io_swapaddr(t, nc) - base;

	error = i386_bus_space_map_load(t, *bshp, size, tiat, flags); 
	if (error != 0)
		i386_memio_unmap(t, *bshp, 0);

	free(tiat, M_TEMP);
	return error;
}

int
nepc_bus_space_io_map_load(t, ioh, size, iat, flags) 
	bus_space_tag_t t;
	bus_space_handle_t ioh;
	bus_size_t size;
	bus_space_iat_t iat;
	u_int flags;
{
	bus_space_iat_t tiat;
	bus_addr_t base;
	int error, nc;

#ifdef	_NEPC_DEBUG
	printf("nepc_bus_space_io_map_load: IN size %lx, flags %x, iat",
	    size, flags);
	for (nc = 0; nc < size; nc++) {
		printf(" %lx", iat[nc]);
	}
	printf("\n");
#endif	/* _NEPC_DEBUG */

	tiat = (bus_space_iat_t) 
		malloc(size * sizeof(bus_addr_t), M_TEMP, M_NOWAIT);
	if (tiat == NULL)
		return EINVAL;

	base = _nepc_io_swapaddr(t, 0);
	for (nc = 0; nc < size; nc ++)
		tiat[nc] = _nepc_io_swapaddr(t, iat[nc]) - base;
#ifdef	_NEPC_DEBUG
	printf("nepc_bus_space_io_map_load: reallocated, tiat ");
	for (nc = 0; nc < size; nc++) {
		printf(" %lx", tiat[nc]);
	}
	printf("\n");
#endif	/* _NEPC_DEBUG */

	error = i386_bus_space_map_load(t, ioh, size, tiat, flags); 

	free(tiat, M_TEMP);
	return error;
}

int
nepc_bus_space_io_map_prefer(t, start, end, align, size)
	bus_space_tag_t t;
	bus_addr_t *start;
	bus_addr_t end;
	bus_size_t align, size;
{
	bus_addr_t base, ofs;

#ifdef	_NEPC_DEBUG
	printf("nepc_bus_space_io_map_prefer: IN "
	    "start %lx end %lx align %lx size %lx\n", *start, end, align,
	    size);
#endif	/* _NEPC_DEBUG */
	for (ofs = 0; ofs < NEPC_MAX_IOSIZE; ofs += NEPC_FRAG_IOSIZE)
	{
		base = _nepc_io_swapaddr(t, ofs);
		if (i386_bus_space_map_prefer(t, &base, base, 0, NEPC_FRAG_IOSIZE))
			return ENOSPC;
	}

	*start = _nepc_io_swapaddr(t, *start);
#ifdef	_NEPC_DEBUG
	printf("nepc_bus_space_io_map_prefer: OUT start %lx -> %lx "
	    "end %lx align %lx size %lx\n",
	    *start, base, end, align, size);
#endif	/* _NEPC_DEBUG */
	return 0;
}

int
nepc_bus_space_mem_map_prefer(t, start, end, align, size)
	bus_space_tag_t t;
	bus_addr_t *start;
	bus_addr_t end;
	bus_size_t align, size;
{
	bus_addr_t wbase = 0;

	if (_nepc_mem_swapaddr(t, &wbase, NULL) != 0)
		return EINVAL;
	if (*start >= wbase && *start + size <= wbase + NEPC_MAX_MEMSIZE)
		return 0;
	return ENOSPC;
}

int
nepc_bus_space_mem_map(t, bpa, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	bus_space_handle_t pbsh;

	/* XXX:
	 * check check
	 */

	if (_nepc_mem_swapaddr(t, NULL, &pbsh) != 0)
		return EINVAL;
	if (bus_space_subregion(t, pbsh, 0, size, bshp) != 0)
		return EINVAL;

	return 0;
}

int
nepc_bus_space_mem_map_load(t, ioh, size, iat, flags) 
	bus_space_tag_t t;
	bus_space_handle_t ioh;
	bus_size_t size;
	bus_space_iat_t iat;
	u_int flags;
{

	/* not yet */
	return EINVAL;
}
