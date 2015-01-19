/*	$NecBSD: busiosubr.c,v 1.30.4.4 1999/08/28 02:25:35 honda Exp $	*/
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

#include "opt_cputype.h"
#include "opt_ddb.h"
#include "opt_pmap_new.h"

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

#include <machine/vm_interface.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>

_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_io,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_io,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_io,u_int32_t,4)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int32_t,4)

_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_io,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_io,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_io,u_int32_t,4)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int32_t,4)

struct bus_space_tag SBUS_io_space_tag = {
	0,
	BUS_SPACE_IO,
	NULL,

	i386_memio_map,
	i386_memio_unmap,
	i386_memio_free,
	i386_memio_subregion,
	i386_bus_space_map_load,
	i386_bus_space_map_unload,
	i386_bus_space_map_activate,
	i386_bus_space_map_deactivate,
	i386_bus_space_map_prefer,
	NULL,

	/* direct bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_io,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_io,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_io,u_int32_t,4),
	},

	/* relocate bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_io,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_io,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_io,u_int32_t,4),
	}
};

struct bus_space_tag SBUS_mem_space_tag = {
	0,
	BUS_SPACE_MEM,
	NULL,
	
	i386_memio_map,
	i386_memio_unmap,
	i386_memio_free,
	i386_memio_subregion,
	i386_bus_space_map_load,
	i386_bus_space_map_unload,
	i386_bus_space_map_activate,
	i386_bus_space_map_deactivate,
	i386_bus_space_map_prefer,
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

#define	STATIC_IOH_NUM	32

extern int ioport_malloc_safe;
extern struct extent *ioport_ex;
extern struct extent *iomem_ex;

int i386_bus_space_handle_alloc __P((bus_space_tag_t, bus_addr_t, bus_size_t, bus_space_handle_t *, bus_space_handle_t));
void i386_bus_space_handle_free __P((bus_space_tag_t, bus_space_handle_t, size_t));
int i386_mem_add_mapping __P((bus_space_tag_t, bus_addr_t, bus_size_t, int, bus_addr_t *));
void i386_mem_remove_mapping __P((bus_space_tag_t, bus_addr_t, bus_size_t));

#define	BMAP_LOAD	1
#define	BMAP_UNLOAD	0
void i386_iomap_subregion __P((bus_space_tag_t, bus_space_handle_t, int));
int i386_iomap_scan __P((bus_space_tag_t, bus_space_handle_t, int));
int i386_iomap_alloc __P((bus_space_tag_t, bus_space_handle_t));
void i386_iomap_free __P((bus_space_tag_t, bus_space_handle_t));
static int i386_iomap_check __P((bus_addr_t));
static void i386_iomap_set __P((bus_addr_t));
static void i386_iomap_clr __P((bus_addr_t));
static int i386_iomap_check_range __P((bus_addr_t, bus_size_t));

int i386_mem_activate __P((bus_space_tag_t, bus_space_handle_t));
int i386_mem_deactivate __P((bus_space_tag_t, bus_space_handle_t));

static struct bus_space_handle i386_bss_handle[STATIC_IOH_NUM];
static void bus_space_iat_init __P((bus_space_handle_t));
bus_addr_t i386_bus_space_iat_1[BUS_SPACE_IAT_MAXSIZE];
bus_addr_t i386_bus_space_iat_2[BUS_SPACE_IAT_MAXSIZE];

#define	IO_SPACE_MAXADDR 0x10000
#define IO_SPACE_MAXIDX	(0x10000 / (sizeof(u_int32_t) * NBBY))

bus_space_handle_t i386_bus_space_systm_handle;
struct bus_space_handle i386_bus_space_systm_handle_store;

u_int32_t i386_iomap_array[IO_SPACE_MAXIDX];
static bus_addr_t pc98_systm_port[] =
	{/* ICU */ 	0x00, 0x02, 0x08, 0x0a,
			0xf0, 0xf8
	};

/*************************************************************************
 * map init
 *************************************************************************/
static void
bus_space_iat_init(bsh)
	bus_space_handle_t bsh;
{
	register int i;

	for (i = 0; i < bsh->bsh_maxiatsz; i ++)
		bsh->bsh_iat[i] = bsh->bsh_base + i;
}
	
void
i386_bus_space_init()
{
	bus_space_tag_t t = I386_BUS_SPACE_IO;
	bus_space_handle_t ioh;
	register int i;

	ioh = &i386_bus_space_systm_handle_store;

	i386_bus_space_systm_handle = ioh;
	ioh->bsh_sz = IO_SPACE_MAXADDR;
	ioh->bsh_flags = BUSIOH_SYSTM | BUSIOH_INUSE | BUSIOH_STATIC;
	ioh->bsh_bam = t->bs_da;
	bus_space_iat_init(ioh);
	
	for (i = 0; i < BUS_SPACE_IAT_MAXSIZE; i ++)
	{
		i386_bus_space_iat_1[i] = i;
		i386_bus_space_iat_2[i] = 2 * i;
	}

	/* XXX:
	 * register io ports which are allocated out of bus space map.
	 */
	for (i = 0; i < sizeof(pc98_systm_port) /sizeof(pc98_systm_port[0]);
	     i ++)
		i386_iomap_set(pc98_systm_port[i]);
}

/*************************************************************************
 * load & unload
 *************************************************************************/
int
i386_bus_space_map_load(t, bsh, size, iat, flags)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
	bus_space_iat_t iat;
	u_int flags;
{
	register int i;

	if ((caddr_t) iat < (caddr_t) KERNBASE)
		panic("i386_bus_space_map_load: illegal bus iat");

	if (size > bsh->bsh_maxiatsz)
	{
		printf("i386_bus_space_map_load: map size too large\n");
		goto bad;
	}	

	i386_bus_space_map_unload(t, bsh);
	for (i = 0; i < bsh->bsh_maxiatsz; i ++)
	{
		if (i < size)
			bsh->bsh_iat[i] = iat[i];
		else
			bsh->bsh_iat[i] = 0;
		bsh->bsh_iat[i] += bsh->bsh_base;
	}				

	bsh->bsh_iatsz = size;
	switch (t->bs_tag)
	{
	default:
		break;

	case BUS_SPACE_IO:
		if (bsh->bsh_pbsh == NULL)
		{
			if (i386_iomap_alloc(t, bsh) != 0)
			{
				i386_bus_space_map_unload(t, bsh);
				goto bad;
			}
		}
		else
			i386_iomap_subregion(t, bsh, BMAP_LOAD);
		break;

	case BUS_SPACE_MEM:
		break;
	}

	bsh->bsh_bam = t->bs_ra;	/* relocate access */
	return 0;

bad:
	if (flags & BUS_SPACE_MAP_FAILFREE)
		bus_space_unmap(t, bsh, bsh->bsh_sz);
	return EINVAL;
}

void
i386_bus_space_map_unload(t, bsh)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
{

	switch (t->bs_tag)
	{
	default:
		break;

	case BUS_SPACE_IO:
		i386_iomap_free(t, bsh);
		break;

	case BUS_SPACE_MEM:
		break;
	}

	bsh->bsh_bam = t->bs_da;	/* default: direct access */
	bsh->bsh_iatsz = 0;
	bus_space_iat_init(bsh);
}	

/*************************************************************************
 * activate deactivate
 *************************************************************************/
int
i386_bus_space_map_deactivate(t, bsh, flags)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	u_int flags;
{

	if (bsh == NULL || bsh->bsh_pbsh != NULL)
		return 0;

	switch (t->bs_tag)
	{
	default:
		break;

	case BUS_SPACE_IO:
		i386_iomap_free(t, bsh);
		break;

	case BUS_SPACE_MEM:
		i386_mem_deactivate(t, bsh);
		break;
	}

	bsh->bsh_flags |= BUSIOH_DEACTIVATE;
	return 0;
}

int
i386_bus_space_map_activate(t, bsh, flags)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	u_int flags;
{
	int error;

	if (bsh == NULL || bsh->bsh_pbsh != NULL)
		return 0;

	switch (t->bs_tag)
	{
	default:
		error = EINVAL;
		break;

	case BUS_SPACE_IO:
		error = i386_iomap_alloc(t, bsh);
		break;

	case BUS_SPACE_MEM:
		error = i386_mem_activate(t, bsh);
		break;
	}

	if (error)
		return EINVAL;

	bsh->bsh_flags &= ~BUSIOH_DEACTIVATE;
	return 0;
}
/*************************************************************************
 * handle allocation
 *************************************************************************/
int
i386_bus_space_handle_alloc(t, bpa, size, bshp, pbsh)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	bus_space_handle_t *bshp;
	bus_space_handle_t pbsh;
{
	bus_space_handle_t bsh = NULL;
	int i;

	if (ioport_malloc_safe != 0)
	{
		bsh = (bus_space_handle_t)
		      malloc(sizeof(*bsh), M_DEVBUF, M_NOWAIT);
		if (bsh == NULL)
			return ENOMEM;
		bsh->bsh_flags = BUSIOH_INUSE;
	}
	else
	{
		for (i = 0; i < STATIC_IOH_NUM; i++)
		{
			bsh = &i386_bss_handle[i];
			if ((bsh->bsh_flags & BUSIOH_INUSE) == 0)
			{
				bsh->bsh_flags = (BUSIOH_INUSE | BUSIOH_STATIC);
				break;
			}
		}

		if (i == STATIC_IOH_NUM)
		{
			printf("i386_bus_space_load: short of static handle\n");
			return ENOMEM;
		}
	}

	bsh->bsh_maxiatsz = BUS_SPACE_IAT_MAXSIZE;
	bsh->bsh_iatsz = 0;
	bsh->bsh_pbase = bsh->bsh_base = bpa;
	bsh->bsh_sz = size;
	bsh->bsh_pbsh = pbsh;
	bus_space_iat_init(bsh);
	if (t->bs_tag == BUS_SPACE_MEM)
		bsh->bsh_pbase = pmap_extract(pmap_kernel(), bpa);

	bsh->bsh_bam = t->bs_da;		/* default: direct access */

	*bshp = bsh;
	return 0;
}

void
i386_bus_space_handle_free(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	size_t size;
{

	if (bsh->bsh_flags & BUSIOH_SYSTM)
		panic("i386_bus_space_handle_free:  free systm handle"); 

	if ((bsh->bsh_flags & BUSIOH_INUSE) == 0)
		panic("i386_bus_space_handle_free: freed bus io handle");

	if (bsh->bsh_flags & BUSIOH_STATIC)
		bsh->bsh_flags = 0;
	else
		free(bsh, M_DEVBUF);
}

/*************************************************************************
 * map
 *************************************************************************/
int
i386_memio_map(t, bpa, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	int error = 0;
	bus_addr_t tva;
	struct extent *ex;

	switch (t->bs_tag)
	{
	default:
		panic("i386_memio_map: bad bus space tag");
		break;
			
	case BUS_SPACE_IO:
		if (flags & BUS_SPACE_MAP_LINEAR)
			return (EOPNOTSUPP);

		error = i386_bus_space_handle_alloc(t, bpa, size, bshp, NULL);
		if (error != 0)
			return error;
		if (i386_iomap_alloc(t, *bshp) != 0)
		{
			i386_bus_space_handle_free(t, *bshp, size);
			error = EINVAL;
		}
		break;

	case BUS_SPACE_MEM:
		ex = iomem_ex;
		if (size == 0)
			size = NBPG;

		error = extent_alloc_region(ex, bpa, size,
			EX_NOWAIT | (ioport_malloc_safe ? EX_MALLOCOK : 0));
		if (error != 0)
			return (error);

		error = i386_mem_add_mapping(t, bpa, size,
			(flags & BUS_SPACE_MAP_CACHEABLE) != 0, &tva);
		if (error != 0)
			goto membad;

		error = i386_bus_space_handle_alloc(t, tva, size, bshp, NULL);
		if (error != 0)
		{
			i386_mem_remove_mapping(t, tva, size);
			goto membad;
		}
		break;
	}

	return error;

membad:
	if (extent_free(ex, bpa, size, EX_NOWAIT |
	    		(ioport_malloc_safe ? EX_MALLOCOK : 0)))
	{
		printf("i386_memio_map: pa 0x%lx, size 0x%lx\n",
			bpa, size);
		printf("i386_memio_map: can't free region\n");
	}
	return error;
}

int
_i386_memio_map(t, bpa, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	int error = 0;
	bus_addr_t tva;

	switch (t->bs_tag)
	{
	default:
		panic("_i386_memio_map: bad bus space tag");
		break;

	case BUS_SPACE_IO:
		if (flags & BUS_SPACE_MAP_LINEAR)
			return (EOPNOTSUPP);
		error = i386_bus_space_handle_alloc(t, bpa, size, bshp, NULL);
		break;

	case BUS_SPACE_MEM:
		if (size == 0)
			size = NBPG;
		error = i386_mem_add_mapping(t, bpa, size,
			(flags & BUS_SPACE_MAP_CACHEABLE) != 0, &tva);
		if (error != 0)
			break;

		error = i386_bus_space_handle_alloc(t, bpa, size, bshp, NULL);
		if (error != 0)
			i386_mem_remove_mapping(t, tva, size);
		break;
	}

	return error;			
}

int
i386_memio_alloc(t, rstart, rend, size, alignment, boundary, flags,
    bpap, bshp)
	bus_space_tag_t t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int flags;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	struct extent *ex;
	bus_addr_t tva;
	u_long bpa;
	int error;

	switch (t->bs_tag)
	{
	default:
		panic("i386_memio_alloc: bad bus space tag");
		break;

	case BUS_SPACE_IO:
		if (flags & BUS_SPACE_MAP_LINEAR)
			return (EOPNOTSUPP);

		bpa = rstart;
		error = i386_bus_space_map_prefer(t, &bpa, rend - size,
						  alignment, size);
		if (error != 0)
			return error;

		error = i386_bus_space_handle_alloc(t, bpa, size, bshp, NULL);
		if (error != 0)
			return error;

		if (i386_iomap_alloc(t, *bshp) != 0)
		{
			i386_bus_space_handle_free(t, *bshp, size);
			return EINVAL;
		}
		break;

	case BUS_SPACE_MEM:
		if (size == 0)
			size = NBPG;

		ex = iomem_ex;
		if (rstart < ex->ex_start || rend > ex->ex_end)
			panic("i386_memio_alloc: bad region start/end");

		error = extent_alloc_subregion(ex, rstart,
			rend, size, alignment, boundary, 
			EX_FAST | EX_NOWAIT |
			(ioport_malloc_safe ?  EX_MALLOCOK : 0), &bpa);
		if (error != 0)
			return (error);

		error = i386_mem_add_mapping(t, bpa, size, 
	    		(flags & BUS_SPACE_MAP_CACHEABLE) != 0, &tva);
		if (error != 0)
			goto membad;

		error = i386_bus_space_handle_alloc(t, tva, size, bshp, NULL);
		if (error != 0)
		{
			i386_mem_remove_mapping(t, tva, size);
			goto membad;
		}
		break;
	}

	*bpap = bpa;
	return 0;

membad:
	if (extent_free(iomem_ex, bpa, size, EX_NOWAIT |
			(ioport_malloc_safe ? EX_MALLOCOK : 0)))
	{
		printf("i386_memio_alloc: pa 0x%lx, size 0x%lx\n", bpa, size);
		printf("i386_memio_alloc: can't free region\n");
	}
	return error;
}

void
i386_memio_unmap(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	if ((bsh->bsh_flags & BUSIOH_DEACTIVATE) == 0)
		i386_bus_space_map_deactivate(t, bsh, 0);

	switch (t->bs_tag)
	{
	default:
		panic("i386_memio_unmap: bad bus space tag");
		break;

	case BUS_SPACE_IO:
		break;

	case BUS_SPACE_MEM:
		if (bsh->bsh_pbsh == NULL)
			i386_mem_remove_mapping(t, bsh->bsh_base, bsh->bsh_sz);
		break;
	}

	i386_bus_space_handle_free(t, bsh, bsh->bsh_sz);
}

void    
i386_memio_free(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	/* i386_memio_unmap() does all that we need to do. */
	i386_memio_unmap(t, bsh, bsh->bsh_sz);
}

int
i386_memio_subregion(t, pbsh, offset, size, tbshp)
	bus_space_tag_t t;
	bus_space_handle_t pbsh;
	bus_size_t offset, size;
	bus_space_handle_t *tbshp;
{
	int i, error = 0;
	bus_space_handle_t bsh;
	bus_addr_t pbase;

	pbase = pbsh->bsh_base + offset;
	switch (t->bs_tag)
	{
	case BUS_SPACE_IO:
		if (pbsh->bsh_iatsz > 0)
		{
			if (offset >= pbsh->bsh_iatsz || 
			    offset + size > pbsh->bsh_iatsz)
				return EINVAL;	
			pbase = pbsh->bsh_base;
		}
		break;

	case BUS_SPACE_MEM:
		if (pbsh->bsh_iatsz > 0)
			return EINVAL;
		if (offset > pbsh->bsh_sz || offset + size > pbsh->bsh_sz)
			return EINVAL;
		break;

	default:
		panic("i386_memio_subregion: bad bus space tag");
		break;
	}

	error = i386_bus_space_handle_alloc(t, pbase, size, &bsh, pbsh);
	if (error != 0)
		return error;

	switch (t->bs_tag)
	{
	case BUS_SPACE_IO:
		if (pbsh->bsh_iatsz > 0)
		{
			for (i = 0; i < size; i ++)
				bsh->bsh_iat[i] = pbsh->bsh_iat[i + offset];
			bsh->bsh_iatsz = size;
		}
		else if (pbsh->bsh_base > bsh->bsh_base ||
		         pbsh->bsh_base + pbsh->bsh_sz <
		         bsh->bsh_base + bsh->bsh_sz)
		{
			i386_bus_space_handle_free(t, bsh, size);
			return EINVAL;
		}
		i386_iomap_subregion(t, bsh, BMAP_LOAD);
		break;

	case BUS_SPACE_MEM:
		break;
	}

	if (pbsh->bsh_iatsz > 0)
		bsh->bsh_bam = t->bs_ra;	/* relocate access */
	*tbshp = bsh;
	return error;
}

/*************************************************************************
 * prefer range check
 *************************************************************************/
int i386_io_map_prefer 
	__P((bus_space_tag_t, bus_addr_t *, bus_addr_t, bus_size_t, bus_size_t));
int i386_mem_map_prefer 
	__P((bus_space_tag_t, bus_addr_t *, bus_addr_t, bus_size_t, bus_size_t));

static bus_addr_t io_boundary[] = {0x1000, 0x300, 0x100};
#define	NEC_UIOSP_MASK			0xff
#define	NEC_UIOSP_SIZE			0x20
#define	NEC_UIOSP_START			0xd0
#define	NEC_UIOSP_END			0xf0

int
i386_io_map_prefer(t, start, end, align, size)
	bus_space_tag_t t;
	bus_addr_t *start;
	bus_addr_t end;
	bus_size_t align, size;
{
	bus_addr_t saddr;
	int try;

	if (size == 0)
		return EINVAL;

	if (align == 0)
		align = 0x8;

	/* I: select NEC user io space */
	if (size <= NEC_UIOSP_SIZE)
	{
		for (saddr = *start; saddr <= end; saddr += align)
		{
			u_long ta;

			ta = saddr & NEC_UIOSP_MASK;
			if (ta < NEC_UIOSP_START || ta >= NEC_UIOSP_END)
				continue;

			ta = (saddr + size - 1) & NEC_UIOSP_MASK;
			if (ta < NEC_UIOSP_START || ta >= NEC_UIOSP_END)
				continue;

			if (i386_iomap_check_range(saddr, size) == 0)
			{
				*start = saddr;
				return 0;
			}
		}
	}

		    
	/* II: find io space */
	for (try = 0; try < sizeof(io_boundary) / sizeof(bus_addr_t); try ++)
	{
		for (saddr = *start; saddr <= end; saddr += align)
		{
			if (saddr < io_boundary[try])
				continue;
			if (try != 0 && saddr >= io_boundary[try - 1])
				break;
			if (i386_iomap_check_range(saddr, size) == 0)
			{
				*start = saddr;
				return 0;
			}
		}
	}

	return ENOSPC;
}

int
i386_mem_map_prefer(t, start, end, align, size)
	bus_space_tag_t t;
	bus_addr_t *start;
	bus_addr_t end;
	bus_size_t align, size;
{
	bus_addr_t saddr, caddr;
	int error;

	if (size == 0)
		return EINVAL;
	size = i386_round_page(size);

	if (align < NBPG)
		align = NBPG;
	else
		align = i386_round_page(align);

	saddr = i386_trunc_page(*start);
	if (saddr < CBUSHOLE_USPACE_START)
		saddr = CBUSHOLE_USPACE_START;

	for ( ; saddr <= end; saddr += align)
	{
		for (caddr = saddr; caddr < saddr + size; caddr += NBPG) 
		{
			if (is_fake_page(i386_btop(caddr)) != 0)
				break;

			if (caddr >= CBUSHOLE_USPACE_END &&
			    caddr < CBUSHOLE_BIOS_END) /* bios */
				break;
		}

		if (caddr != saddr + size)
			continue;

		error = extent_alloc_region(iomem_ex, saddr, size,
			EX_FAST | EX_NOWAIT |
			(ioport_malloc_safe ? EX_MALLOCOK : 0));
		if (error == 0)
		{
			extent_free(iomem_ex, saddr, size, EX_NOWAIT |
	    			    (ioport_malloc_safe ? EX_MALLOCOK : 0));
			*start = saddr;
			return 0;
		}
	}
	return ENOSPC;
}
		
int
i386_bus_space_map_prefer(t, start, end, align, size)
	bus_space_tag_t t;
	bus_addr_t *start;
	bus_addr_t end;
	bus_size_t align, size;
{

	switch (t->bs_tag)
	{
	case BUS_SPACE_IO:
		return i386_io_map_prefer(t, start, end, align, size);
	case BUS_SPACE_MEM:
		return i386_mem_map_prefer(t, start, end, align, size);
	}

	return EINVAL;
}

/*************************************************************************
 * io map control
 *************************************************************************/
static int
i386_iomap_check_range(sport, size)
	bus_addr_t sport;
	bus_size_t size;
{
#define	TYPEBIT(type)	(NBBY * sizeof(type))
	bus_addr_t soffs, eoffs, port, eport;
	u_int32_t mask;
	u_int idx;

	/* check validity */
 	eport = sport + size;
	if (sport >= IO_SPACE_MAXADDR || eport >= IO_SPACE_MAXADDR ||
	    sport >= eport)
		return EINVAL;

	/* calculate start mask */
	soffs = sport % TYPEBIT(mask);
	mask = ~((1 << soffs) - 1);
	sport = sport - soffs;

	/* start */
	for (port = sport; port < eport; port += TYPEBIT(mask))
	{
		if (port + TYPEBIT(mask) > eport)
		{
			eoffs = eport % TYPEBIT(mask); 
			mask &= ((1 << eoffs) - 1);
		}

		idx = port / TYPEBIT(mask);
		if ((i386_iomap_array[idx] & mask) != 0)
			return EBUSY;

		mask = ~0;	
	}
	return 0;
}

static int
i386_iomap_check(port)
	bus_addr_t port;
{
	int idx = port / TYPEBIT(u_int32_t);
	u_int32_t mask = 1 << (port % TYPEBIT(u_int32_t));

	return i386_iomap_array[idx] & mask;
}

static void
i386_iomap_set(port)
	bus_addr_t port;
{
	int idx = port / TYPEBIT(u_int32_t);
	u_int32_t mask = 1 << (port % TYPEBIT(u_int32_t));

	i386_iomap_array[idx] |= mask;
}

static void
i386_iomap_clr(port)
	bus_addr_t port;
{
	int idx = port / TYPEBIT(u_int32_t);
	u_int32_t mask = 1 << (port % TYPEBIT(u_int32_t));

	i386_iomap_array[idx] &= ~mask;
}

void
i386_iomap_subregion(t, bsh, set)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	int set;
{
	register int i;
	
	if (bsh->bsh_iatsz == 0)
	{
		bus_addr_t port = bsh->bsh_base;

		for (i = 0; i < bsh->bsh_sz; i ++)
		{
			if (port + i >= IO_SPACE_MAXADDR)
				break;
			if (set == BMAP_LOAD)
				i386_iomap_set(port + i);
			else
				i386_iomap_clr(port + i);
		}
	}
	else
	{
		for (i = 0; i < bsh->bsh_iatsz; i ++)
		{
			if (bsh->bsh_iat[i] >= IO_SPACE_MAXADDR)
				continue;
			if (set == BMAP_LOAD)
				i386_iomap_set(bsh->bsh_iat[i]);
			else
				i386_iomap_clr(bsh->bsh_iat[i]);
		}
	}
}

int
i386_iomap_scan(t, bsh, set)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	int set;
{
	int size, bit, i;
	u_char *s;

	s = (set == BMAP_LOAD) ? "busy" : "freed";		
	if (bsh->bsh_iatsz == 0)
	{
		bus_addr_t port = bsh->bsh_base;

		size = bsh->bsh_sz;
		for (i = 0; i < size; i ++)
		{
			if (port + i >= IO_SPACE_MAXADDR)
				return -1;

			bit = i386_iomap_check(port + i);
			if ((set == BMAP_LOAD && bit != 0) ||
			    (set == BMAP_UNLOAD && bit == 0))
			{
				printf("iomem_map: io port(D) 0x%lx %s\n",
					port + i, s);
				return -1;
			}
		}
	}
	else
	{
		size = bsh->bsh_iatsz;
		for (i = 0; i < size; i ++)
		{
			if (bsh->bsh_iat[i] >= IO_SPACE_MAXADDR)
				return -1;

			bit = i386_iomap_check(bsh->bsh_iat[i]);
			if ((set == BMAP_LOAD && bit != 0) ||
			    (set == BMAP_UNLOAD && bit == 0))
			{
				printf("iomem_map: io port(R) 0x%lx %s\n",
					bsh->bsh_iat[i], s);
				return -1;
			}
		}
	}
	return size;
}

int
i386_iomap_alloc(t, bsh)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
{
	int size;

	size = i386_iomap_scan(t, bsh, BMAP_LOAD);
	if (size < 0)
		return EBUSY;
	if (size > 0)
	{
		i386_iomap_subregion(t, bsh, BMAP_LOAD);
		bsh->bsh_flags |= BUSIOH_BITMAP;
	}
	return 0;
}

void
i386_iomap_free(t, bsh)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
{

	if ((bsh->bsh_flags & BUSIOH_BITMAP) == 0)
		return;

	(void) i386_iomap_scan(t, bsh, BMAP_UNLOAD);
	i386_iomap_subregion(t, bsh, BMAP_UNLOAD);
	bsh->bsh_flags &= ~BUSIOH_BITMAP;
}

/*************************************************************************
 * mem map control
 *************************************************************************/
int
i386_mem_deactivate(t, bsh)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
{

	if (extent_free(iomem_ex, bsh->bsh_pbase, bsh->bsh_sz, EX_NOWAIT |
	    		(ioport_malloc_safe ? EX_MALLOCOK : 0)))
		return EINVAL;
	return 0;
}
	
int
i386_mem_activate(t, bsh)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
{
	u_long bpa, va, eva, pa, endpa;
	pt_entry_t *pte;
	int error;

	bpa = bsh->bsh_pbase;
	pa = i386_trunc_page(bpa);
	endpa = i386_round_page(bpa + bsh->bsh_sz);
	va = i386_trunc_page(bsh->bsh_base);
	eva = i386_round_page(bsh->bsh_base + bsh->bsh_sz);

	error = extent_alloc_region(iomem_ex, bpa, bsh->bsh_sz,
		EX_NOWAIT | (ioport_malloc_safe ? EX_MALLOCOK : 0));
	if (error != 0)
		return error;

	pmap_remove(pmap_kernel(), va, eva);
	for (; pa < endpa; pa += NBPG, va += NBPG)
	{
		pmap_enter(pmap_kernel(), va, pa,
		    VM_PROT_READ | VM_PROT_WRITE, TRUE, 0);
		if (cpu_class != CPUCLASS_386) 
		{
			pte = kvtopte(va);
			*pte |= PG_N;	/* XXX */
#if defined(PMAP_NEW)
			pmap_update_pg(va);
#else
			pmap_update();
#endif
		}
	}
	return 0;
}

void
i386_mem_remove_mapping(t, sva, size)
	bus_space_tag_t t;
	bus_addr_t sva;
	bus_size_t size;
{
	bus_addr_t va, eva;

	va = i386_trunc_page(sva);
	eva = i386_round_page(sva + size);
#ifdef DIAGNOSTIC
	if (eva <= va)
		panic("i386_memio_unmap: overflow");
#endif

	uvm_km_free(kernel_map, va, eva - va);
}

int
i386_mem_add_mapping(t, bpa, size, cacheable, tva)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_addr_t *tva;
{
	u_long pa, endpa;
	pt_entry_t *pte;
	vaddr_t va;

	pa = i386_trunc_page(bpa);
	endpa = i386_round_page(bpa + size);
#ifdef DIAGNOSTIC
	if (endpa <= pa)
		panic("i386_mem_add_mapping: overflow");
#endif

	va = uvm_km_valloc(kernel_map, endpa - pa);
	if (va == 0)
		return ENOMEM;

	*tva = va + (bpa & PGOFSET);
	for (; pa < endpa; pa += NBPG, va += NBPG)
	{
		pmap_enter(pmap_kernel(), va, pa,
		    VM_PROT_READ | VM_PROT_WRITE, TRUE, 0);
		if (cpu_class != CPUCLASS_386)
		{
			pte = kvtopte(va);
			if (cacheable)
				*pte &= ~PG_N;
			else
				*pte |= PG_N;
#if defined(PMAP_NEW)
			pmap_update_pg(va);
#else
			pmap_update();
#endif
		}
	}

	return 0;
}
