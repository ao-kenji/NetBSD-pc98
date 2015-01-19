/*	$NecBSD: busio.h,v 3.25.4.2 1999/08/21 07:25:45 kmatsuda Exp $	*/
/*	$NetBSD: bus.h,v 1.12 1997/10/01 08:25:15 fvdl Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
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
 *
 * This module support generic bus address relocation mechanism.
 * To reduce a function call overhead, we employ pascal call methods.
 */

#ifndef _I386_BUSIO_H_
#define _I386_BUSIO_H_

#ifndef __BUS_SPACE_COMPAT_OLDDEFS
#define	__BUS_SPACE_COMPAT_OLDDEFS
#endif

#ifdef BUS_SPACE_DEBUG
/*
 * Macros for sanity-checking the aligned-ness of pointers passed to
 * bus space ops.  These are not strictly necessary on the x86, but
 * could lead to performance improvements, and help catch problems
 * with drivers that would creep up on other architectures.
 */
#define	__BUS_SPACE_ALIGNED_ADDRESS(p, t)				\
	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	__BUS_SPACE_ADDRESS_SANITY(p, t, d)				\
({									\
	if (__BUS_SPACE_ALIGNED_ADDRESS((p), t) == 0) {			\
		printf("%s 0x%lx not aligned to %d bytes %s:%d\n",	\
		    d, (u_long)(p), sizeof(t), __FILE__, __LINE__);	\
	}								\
	(void) 0;							\
})

#define BUS_SPACE_ALIGNED_POINTER(p, t) __BUS_SPACE_ALIGNED_ADDRESS(p, t)
#else
#define	__BUS_SPACE_ADDRESS_SANITY(p,t,d)	(void) 0
#define BUS_SPACE_ALIGNED_POINTER(p, t) ALIGNED_POINTER(p, t)
#endif /* BUS_SPACE_DEBUG */

typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * address relocation table
 */
#define	BUS_SPACE_IAT_MAXSIZE	32
typedef	bus_addr_t *bus_space_iat_t;

extern bus_addr_t i386_bus_space_iat_1[];
extern bus_addr_t i386_bus_space_iat_2[];

#define	BUS_SPACE_IAT_1	((bus_space_iat_t) i386_bus_space_iat_1)
#define	BUS_SPACE_IAT_2	((bus_space_iat_t) i386_bus_space_iat_2)
#define	BUS_SPACE_IAT_SZ(IOTARRAY) (sizeof(IOTARRAY)/sizeof(bus_addr_t))

/*
 * bus space tag
 */
struct bus_space_handle;
typedef struct bus_space_handle *bus_space_handle_t;

#define	_PASCAL_CALL	__P((void))

#define	_BUS_SPACE_CALL_FUNCS_TAB(NAME,TYPE,BWN) \
	NAME##_space_read_##BWN##, 				\
	NAME##_space_read_multi_##BWN##, 			\
	NAME##_space_read_region_##BWN##,			\
	NAME##_space_write_##BWN##, 				\
	NAME##_space_write_multi_##BWN##, 			\
	NAME##_space_write_region_##BWN##,			\
	NAME##_space_set_multi_##BWN##,				\
	NAME##_space_set_region_##BWN##,			\
	NAME##_space_copy_region_##BWN

#define	_BUS_SPACE_CALL_FUNCS_PROTO(NAME,TYPE,BWN) \
	TYPE NAME##_space_read_##BWN _PASCAL_CALL;		\
	void NAME##_space_read_multi_##BWN _PASCAL_CALL;	\
	void NAME##_space_read_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_write_##BWN _PASCAL_CALL;		\
	void NAME##_space_write_multi_##BWN _PASCAL_CALL;	\
	void NAME##_space_write_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_set_multi_##BWN _PASCAL_CALL;		\
	void NAME##_space_set_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_copy_region_##BWN _PASCAL_CALL;

#define	_BUS_SPACE_CALL_FUNCS(NAME,TYPE,BWN) \
	TYPE (*##NAME##_read_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_read_multi_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_read_region_##BWN) _PASCAL_CALL;	\
	void (*##NAME##_write_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_write_multi_##BWN) _PASCAL_CALL;	\
	void (*##NAME##_write_region_##BWN) _PASCAL_CALL;	\
	void (*##NAME##_set_multi_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_set_region_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_copy_region_##BWN) _PASCAL_CALL;	

struct bus_space_access_methods {
	/* 8 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int8_t,1)

	/* 16 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int16_t,2)

	/* 32 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int32_t,4)
};

struct bus_space_tag {
	u_int	bs_id;			/* bus id */

#define	BUS_SPACE_IO	0
#define	BUS_SPACE_MEM	1
	u_int	bs_tag;			/* bus space flags */
	void	*bs_busc;		/* bus private data pointer */

 	int (*bs_map) __P((struct bus_space_tag *, bus_addr_t, bus_size_t, \
			   int, bus_space_handle_t *));
 	void (*bs_unmap) __P((struct bus_space_tag *, bus_space_handle_t, \
			     bus_size_t));
 	void (*bs_free) __P((struct bus_space_tag *, bus_space_handle_t, \
			     bus_size_t));
	int (*bs_subregion) __P((struct bus_space_tag *, bus_space_handle_t, \
				 bus_size_t, bus_size_t, bus_space_handle_t *));
	int (*bs_map_load) __P((struct bus_space_tag *, bus_space_handle_t,\
				bus_size_t, bus_space_iat_t, u_int));
	void (*bs_map_unload) __P((struct bus_space_tag *, bus_space_handle_t));
	int (*bs_map_activate) __P((struct bus_space_tag *, \
				    bus_space_handle_t, u_int));
	int (*bs_map_deactivate) __P((struct bus_space_tag *, \
				      bus_space_handle_t, u_int));
	int (*bs_map_prefer) __P((struct bus_space_tag *, bus_addr_t *, \
				  bus_addr_t, bus_size_t, bus_size_t));
	int (*bs_vop) __P((struct bus_space_tag *, bus_space_handle_t, u_int));

	struct bus_space_access_methods bs_da;	/* direct access */
	struct bus_space_access_methods bs_ra;	/* relocate access */
#if	0
	struct bus_space_access_methods bs_ida;	/* indexed direct access */
#endif
};

typedef struct bus_space_tag *bus_space_tag_t;

extern struct bus_space_tag SBUS_io_space_tag;
extern struct bus_space_tag SBUS_mem_space_tag;

#define	I386_BUS_SPACE_IO	(&SBUS_io_space_tag)
#define	I386_BUS_SPACE_MEM	(&SBUS_mem_space_tag)

/*
 * bus space handle
 */
struct bus_space_handle {
	bus_addr_t bsh_base;
	bus_addr_t bsh_iat[BUS_SPACE_IAT_MAXSIZE];
	struct bus_space_access_methods bsh_bam;

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUSIOH_INUSE			0x010000
#define	BUSIOH_STATIC			0x020000
#define	BUSIOH_DEACTIVATE		0x040000
#define	BUSIOH_SYSTM			0x080000
#define	BUSIOH_BITMAP			0x100000
	u_int bsh_flags;

	bus_addr_t bsh_pbase;
	size_t bsh_sz;

	struct bus_space_handle *bsh_pbsh;

	void *bsh_cookies;

	size_t bsh_maxiatsz;
	size_t bsh_iatsz;
};

extern bus_space_handle_t i386_bus_space_systm_handle;
#define	BUS_SPACE_SYSTM_HANDLE (i386_bus_space_systm_handle)

 /*
  * Initialize a bus space layer
  */
void	i386_bus_space_init __P((void));
#define	bus_space_init	i386_bus_space_init

/*
 *	int bus_space_map  __P((bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flag, bus_space_handle_t *bshp));
 *
 * Map a region of bus space.
 */
int	i386_memio_map __P((bus_space_tag_t t, bus_addr_t addr,
	    bus_size_t size, int flag, bus_space_handle_t *bshp));
/* like map, but without extent map checking/allocation */
int	_i386_memio_map __P((bus_space_tag_t t, bus_addr_t addr,
	    bus_size_t size, int flag, bus_space_handle_t *bshp));
static __inline int bus_space_map  __P((bus_space_tag_t, bus_addr_t, \
	    bus_size_t, int, bus_space_handle_t *));

static __inline int
bus_space_map(t, a, s, f, hp)
	bus_space_tag_t t;
	bus_addr_t a;
	bus_size_t s;
	int f;
	bus_space_handle_t *hp;
{

	return ((*t->bs_map) (t, a, s, f, hp));
}

/*
 *	int bus_space_unmap __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Unmap a region of bus space.
 */
static __inline int bus_space_unmap __P((bus_space_tag_t, bus_space_handle_t,\
				       bus_size_t));
void	i386_memio_unmap __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));

static __inline int
bus_space_unmap(t, h, s)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t s;		
{

	(*t->bs_unmap) (t, h, s);
	return 0;
}

/*
 *	int bus_space_subregion __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp));
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */
static __inline int bus_space_subregion __P((bus_space_tag_t, \
	bus_space_handle_t, bus_size_t, bus_size_t, bus_space_handle_t *));
int	i386_memio_subregion __P((bus_space_tag_t t, bus_space_handle_t bsh, \
	 bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp));

static __inline int
bus_space_subregion(t, h, o, s, nhp)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	bus_size_t s;
	bus_space_handle_t *nhp;
{
	
	return ((*t->bs_subregion) (t, h, o, s, nhp));
}

/*
 *	int bus_space_alloc __P((bus_space_tag_t t, bus_addr_t rstart,
 *	    bus_addr_t rend, bus_size_t size, bus_size_t align,
 *	    bus_size_t boundary, int flag, bus_addr_t *addrp,
 *	    bus_space_handle_t *bshp));
 *
 * Allocate a region of bus space.
 */
int	i386_memio_alloc __P((bus_space_tag_t t, bus_addr_t rstart,
	    bus_addr_t rend, bus_size_t size, bus_size_t align,
	    bus_size_t boundary, int flag, bus_addr_t *addrp,
	    bus_space_handle_t *bshp));

#define bus_space_alloc(t, rs, re, s, a, b, f, ap, hp)			\
	i386_memio_alloc((t), (rs), (re), (s), (a), (b), (f), (ap), (hp))

/*
 *	int bus_space_free __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Free a region of bus space.
 */
void	i386_memio_free __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));
static __inline int bus_space_free \
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t));

static __inline int
bus_space_free(t, h, s)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t s;		
{

	(*t->bs_free) (t, h, s);
	return 0;
}

/*
 * Mapping load and unload, activate and deactivate.
 */
/* flags definition */
#define	BUS_SPACE_MAP_FAILFREE	0x01
#define	BUS_SPACE_MAP_INDDA	0x02

int i386_bus_space_map_load 
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t,\
	     bus_space_iat_t, u_int));
void i386_bus_space_map_unload
	__P((bus_space_tag_t, bus_space_handle_t));
int i386_bus_space_map_activate
	__P((bus_space_tag_t, bus_space_handle_t, u_int));
int i386_bus_space_map_deactivate
	__P((bus_space_tag_t, bus_space_handle_t, u_int));
static __inline int bus_space_map_load 
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t,\
	     bus_space_iat_t, u_int));
static __inline void bus_space_map_unload
	__P((bus_space_tag_t, bus_space_handle_t));
static __inline int bus_space_map_activate
	__P((bus_space_tag_t, bus_space_handle_t, u_int));
static __inline int bus_space_map_deactivate
	__P((bus_space_tag_t, bus_space_handle_t, u_int));

static __inline int
bus_space_map_load(t, bsh, size, iat, flags)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
	bus_space_iat_t iat;
	u_int flags;
{

	return ((*t->bs_map_load) (t, bsh, size, iat, flags));
}

static __inline void
bus_space_map_unload(t, bsh)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
{

	(*t->bs_map_unload) (t, bsh);
}	

static __inline int
bus_space_map_activate(t, bsh, flags)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	u_int flags;
{

	return ((*t->bs_map_activate) (t, bsh, flags));
}

static __inline int
bus_space_map_deactivate(t, bsh, flags)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	u_int flags;
{

	return ((*t->bs_map_deactivate) (t, bsh, flags));
}

/*
 * Prefer address select
 */
int i386_bus_space_map_prefer 
	__P((bus_space_tag_t, bus_addr_t *, bus_addr_t, bus_size_t, bus_size_t));
static __inline int bus_space_map_prefer
	__P((bus_space_tag_t, bus_addr_t *, bus_addr_t, bus_size_t, bus_size_t));

static __inline int
bus_space_map_prefer(t, start, end, align, size)
	bus_space_tag_t t;
	bus_addr_t *start;
	bus_addr_t end;
	bus_size_t align, size;
{

	if (t->bs_map_prefer == NULL)
		return 0;

	return ((*t->bs_map_prefer) (t, start, end, align, size));
}

/*
 * Access methods for bus resources and address space.
 */
#define	_BUS_ACCESS_METHODS_PROTO(TYPE,BWN) \
	static __inline TYPE bus_space_read_##BWN 			\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t offset));	\
	static __inline void bus_space_read_multi_##BWN			\
	__P((bus_space_tag_t, bus_space_handle_t,			\
	     bus_size_t, TYPE *, size_t));				\
	static __inline void bus_space_read_region_##BWN		\
	__P((bus_space_tag_t, bus_space_handle_t,			\
	     bus_size_t, TYPE *, size_t));				\
	static __inline void bus_space_write_##BWN			\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t, TYPE));	\
	static __inline void bus_space_write_multi_##BWN		\
	__P((bus_space_tag_t, bus_space_handle_t,			\
	     bus_size_t, TYPE *, size_t));				\
	static __inline void bus_space_write_region_##BWN		\
	__P((bus_space_tag_t, bus_space_handle_t,			\
	     bus_size_t, TYPE *, size_t));				\
	static __inline void bus_space_set_multi_##BWN			\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t, TYPE, size_t));\
	static __inline void bus_space_set_region_##BWN			\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t, TYPE, size_t));\
	static __inline void bus_space_copy_region_##BWN		\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	     bus_space_handle_t, bus_size_t, size_t));

_BUS_ACCESS_METHODS_PROTO(u_int8_t,1)
_BUS_ACCESS_METHODS_PROTO(u_int16_t,2)
_BUS_ACCESS_METHODS_PROTO(u_int32_t,4)

/*
 * read methods
 */
#define	_BUS_SPACE_READ(TYPE,BWN)				\
static __inline TYPE						\
bus_space_read_##BWN##(tag, bsh, offset)			\
	bus_space_tag_t tag;					\
	bus_space_handle_t bsh;					\
	bus_size_t offset;					\
{								\
	register TYPE result;					\
								\
	__asm __volatile("call *%1"  				\
			:"=a" (result)				\
			:"o" (bsh->bsh_bam.bs_read_##BWN),	\
			 "b" (bsh),				\
			 "d" (offset)				\
			:"%eax", "%edx");			\
								\
	return result;						\
}

_BUS_SPACE_READ(u_int8_t,1)
_BUS_SPACE_READ(u_int16_t,2)
_BUS_SPACE_READ(u_int32_t,4)

/*
 * write methods
 */
#define	_BUS_SPACE_WRITE(TYPE,BWN)				\
static __inline void						\
bus_space_write_##BWN##(tag, bsh, offset, val)			\
	bus_space_tag_t tag;					\
	bus_space_handle_t bsh;					\
	bus_size_t offset;					\
	TYPE val;						\
{								\
								\
	__asm __volatile("call *%0":  				\
			:"o" (bsh->bsh_bam.bs_write_##BWN),	\
			 "a" (val),				\
			 "b" (bsh),				\
			 "d" (offset)				\
			:"%edx");				\
}								

_BUS_SPACE_WRITE(u_int8_t,1)
_BUS_SPACE_WRITE(u_int16_t,2)
_BUS_SPACE_WRITE(u_int32_t,4)

/*
 * multi read
 */
#define	_BUS_SPACE_READ_MULTI(TYPE,BWN)					\
static __inline void							\
bus_space_read_multi_##BWN##(tag, bsh, offset, buf, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE *buf;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%0": 					\
			:"o" (bsh->bsh_bam.bs_read_multi_##BWN),	\
			 "b" (bsh),					\
			 "c" (cnt),					\
			 "d" (offset),					\
			 "D" (buf)					\
			:"%ecx", "%edx", "%edi", "memory");		\
}

_BUS_SPACE_READ_MULTI(u_int8_t,1)
_BUS_SPACE_READ_MULTI(u_int16_t,2)
_BUS_SPACE_READ_MULTI(u_int32_t,4)

/*
 * multi write
 */
#define	_BUS_SPACE_WRITE_MULTI(TYPE,BWN)				\
static __inline void							\
bus_space_write_multi_##BWN##(tag, bsh, offset, buf, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE *buf;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%0": 					\
			:"o" (bsh->bsh_bam.bs_write_multi_##BWN),	\
			 "b" (bsh),					\
			 "c" (cnt),					\
			 "d" (offset),					\
			 "S" (buf)					\
			:"%ecx", "%edx", "%esi");			\
}

_BUS_SPACE_WRITE_MULTI(u_int8_t,1)
_BUS_SPACE_WRITE_MULTI(u_int16_t,2)
_BUS_SPACE_WRITE_MULTI(u_int32_t,4)

/*
 * region read
 */
#define	_BUS_SPACE_READ_REGION(TYPE,BWN)				\
static __inline void							\
bus_space_read_region_##BWN##(tag, bsh, offset, buf, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE *buf;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%0": 					\
			:"o" (bsh->bsh_bam.bs_read_region_##BWN),	\
			 "b" (bsh),					\
			 "c" (cnt),					\
			 "d" (offset),					\
			 "D" (buf)					\
			:"%ecx", "%edx", "%edi", "memory");		\
}

_BUS_SPACE_READ_REGION(u_int8_t,1)
_BUS_SPACE_READ_REGION(u_int16_t,2)
_BUS_SPACE_READ_REGION(u_int32_t,4)

/*
 * region write
 */
#define	_BUS_SPACE_WRITE_REGION(TYPE,BWN)				\
static __inline void							\
bus_space_write_region_##BWN##(tag, bsh, offset, buf, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE *buf;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%0": 					\
			:"o" (bsh->bsh_bam.bs_write_region_##BWN),	\
			 "b" (bsh),					\
			 "c" (cnt),					\
			 "d" (offset),					\
			 "S" (buf)					\
			:"%ecx", "%edx", "%esi");			\
}

_BUS_SPACE_WRITE_REGION(u_int8_t,1)
_BUS_SPACE_WRITE_REGION(u_int16_t,2)
_BUS_SPACE_WRITE_REGION(u_int32_t,4)

/*
 * multi set
 */
#define	_BUS_SPACE_SET_MULTI(TYPE,BWN)					\
static __inline void							\
bus_space_set_multi_##BWN##(tag, bsh, offset, val, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE val;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%0": 					\
			:"o" (bsh->bsh_bam.bs_set_multi_##BWN),		\
			 "a" (val),					\
			 "b" (bsh),					\
			 "c" (cnt),					\
			 "d" (offset)					\
			:"%ecx", "%edx");				\
}

_BUS_SPACE_SET_MULTI(u_int8_t,1)
_BUS_SPACE_SET_MULTI(u_int16_t,2)
_BUS_SPACE_SET_MULTI(u_int32_t,4)

/*
 * region set
 */
#define	_BUS_SPACE_SET_REGION(TYPE,BWN)					\
static __inline void							\
bus_space_set_region_##BWN##(tag, bsh, offset, val, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE val;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%0": 					\
			:"o" (bsh->bsh_bam.bs_set_region_##BWN),	\
			 "a" (val),					\
			 "b" (bsh),					\
			 "c" (cnt),					\
			 "d" (offset)					\
			:"%ecx", "%edx");				\
}

_BUS_SPACE_SET_REGION(u_int8_t,1)
_BUS_SPACE_SET_REGION(u_int16_t,2)
_BUS_SPACE_SET_REGION(u_int32_t,4)

/*
 * copy
 */
#define	_BUS_SPACE_COPY_REGION(BWN)					\
static __inline void							\
bus_space_copy_region_##BWN##(tag, sbsh, src, dbsh, dst, cnt)		\
	bus_space_tag_t tag;						\
	bus_space_handle_t sbsh;					\
	bus_size_t src;							\
	bus_space_handle_t dbsh;					\
	bus_size_t dst;							\
	size_t cnt;							\
{									\
	extern void panic __P((const char *, ...));			\
									\
	if (dbsh->bsh_bam.bs_copy_region_1 != sbsh->bsh_bam.bs_copy_region_1) \
		panic("bus_space_copy_region: funcs mismatch (ENOSUPPORT)");\
									\
	__asm __volatile("call *%0": 					\
			:"o" (dbsh->bsh_bam.bs_copy_region_##BWN),	\
			 "a" (sbsh),					\
			 "b" (dbsh),					\
			 "c" (cnt),					\
			 "S" (src),					\
			 "D" (dst)					\
			:"%ecx", "%esi", "%edi");			\
}

_BUS_SPACE_COPY_REGION(1)
_BUS_SPACE_COPY_REGION(2)
_BUS_SPACE_COPY_REGION(4)

#ifdef __BUS_SPACE_COMPAT_OLDDEFS
/* compatibility definitions; deprecated */
#define	bus_space_copy_1(t, h1, o1, h2, o2, c)				\
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_2(t, h1, o1, h2, o2, c)				\
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_4(t, h1, o1, h2, o2, c)				\
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_8(t, h1, o1, h2, o2, c)				\
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#endif


/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags));
 *
 * Note: the i386 does not currently require barriers, but we must
 * provide the flags to MI code.
 */
#define	bus_space_barrier(t, h, o, l, f)	\
	((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)))
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

#ifdef __BUS_SPACE_COMPAT_OLDDEFS
/* compatibility definitions; deprecated */
#define	BUS_BARRIER_READ	BUS_SPACE_BARRIER_READ
#define	BUS_BARRIER_WRITE	BUS_SPACE_BARRIER_WRITE
#endif

#endif /* _I386_BUSIO_H_ */
