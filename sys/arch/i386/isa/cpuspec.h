/*	$NecBSD: cpuspec.h,v 1.16 1999/07/23 20:50:33 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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

#ifndef	_I386_CPUSPEC_H_
#define	_I386_CPUSPEC_H_

typedef void *cpu_data_t;

struct cpu_tag {
	u_int	ct_id;
	cpu_data_t ct_data;

#define	CPU_FLAGS_PG_TRAP	0x0001
#define	CPU_FLAGS_CF_PRER	0x0002
#define	CPU_FLAGS_CF_PREW	0x0004
#define	CPU_FLAGS_CF_POSTR	0x0008
#define	CPU_FLAGS_CF_POSTW	0x0010
	u_int	ct_flags;

#define	CPU_CC_OFF 0
#define	CPU_CC_ON  1
	int ct_ccs;
	int (*ct_cc) __P((struct cpu_tag *, u_int));

	void (*ct_cf_preR) __P((void));		/* XXX */
	void (*ct_cf_preW) __P((void));		/* XXX */
	void (*ct_cf_postR) __P((void));	/* XXX */
	void (*ct_cf_postW) __P((void));	/* XXX */
};

typedef struct cpu_tag *cpu_tag_t;

static __inline void cpu_cf_preRead __P((cpu_tag_t));
static __inline void cpu_cf_preWrite __P((cpu_tag_t));
static __inline void cpu_cf_postRead __P((cpu_tag_t));
static __inline void cpu_cf_postWrite __P((cpu_tag_t));
static __inline int cpu_cc __P((cpu_tag_t, u_int));
static __inline int cpu_flags __P((cpu_tag_t, u_int));

static __inline void
cpu_cf_preRead(ct)
	cpu_tag_t ct;
{
	if (ct->ct_flags & CPU_FLAGS_CF_PRER)
		(*ct->ct_cf_preR) ();	/* XXX */
}

static __inline void
cpu_cf_preWrite(ct)
	cpu_tag_t ct;
{
	if (ct->ct_flags & CPU_FLAGS_CF_PREW)
		(*ct->ct_cf_preW) ();	/* XXX */
}

static __inline void
cpu_cf_postRead(ct)
	cpu_tag_t ct;
{
	if (ct->ct_flags & CPU_FLAGS_CF_POSTR)
		(*ct->ct_cf_postR) ();	/* XXX */
}

static __inline void
cpu_cf_postWrite(ct)
	cpu_tag_t ct;
{
	if (ct->ct_flags & CPU_FLAGS_CF_POSTW)
		(*ct->ct_cf_postW) ();	/* XXX */
}
	
static __inline int
cpu_cc(ct, flags)
	cpu_tag_t ct;
	u_int flags;
{
	if (ct->ct_cc)
		return ((*ct->ct_cc) (ct, flags));

	return EINVAL;
}

static __inline int
cpu_flags(ct, flags)
	cpu_tag_t ct;
	u_int flags;
{

	return (ct->ct_flags & flags);
}

#define	CPU_CONTROL_CACHE_DISABLE	0
#define	CPU_CONTROL_CACHE_ENABLE	1
void CPU_control_cpu_init __P((void));
int CPU_control_WriteBack_enable __P((cpu_tag_t, u_int));
int CPU_control_AMD_K5_or_K6_enable __P((cpu_tag_t, u_int));
int CPU_control_cyrix_486DLC_enable __P((cpu_tag_t, u_int));
int CPU_control_cyrix_5x86_enable __P((cpu_tag_t, u_int));
int CPU_control_IDT_C6_enable __P((cpu_tag_t, u_int));
void CPU_control_check_cyrix_model __P((void));	/* i386/isa/cpu.c */

#ifdef	_KERNEL
extern cpu_tag_t curcpu;
#endif	/* _KERNEL */
#endif	/* !_I386_CPUSPEC_H_ */
