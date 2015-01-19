/*	$NecBSD: syspmgr.h,v 3.6 1998/12/31 02:38:09 honda Exp $	*/
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

#ifndef _I386_SYSTMPORTMGR_H_
#define _I386_SYSTMPORTMGR_H_

typedef int syspmgr_tag_t;
typedef int syspmgr_channel_t;

#define	SYSTM_SYSPMGR_TAG	((syspmgr_tag_t) 0)

#define	SYSPMGR_PORTA		((syspmgr_channel_t) 0)
#define	SYSPMGR_PORTB		((syspmgr_channel_t) 1)
#define	SYSPMGR_PORTC		((syspmgr_channel_t) 2)
#define	SYSPMGR_PORTMC		((syspmgr_channel_t) 3)

#define	SYSPMGR_SPKR_ON		6
#define	SYSPMGR_SPKR_OFF	7
#define	SYSPMGR_MEMCHK_ON	8
#define	SYSPMGR_MEMCHK_OFF	9
#define	SYSPMGR_SHUT1_ON	10
#define	SYSPMGR_SHUT1_OFF	11
#define	SYSPMGR_PSTB_ON		12
#define	SYSPMGR_PSTB_OFF	13
#define	SYSPMGR_SHUT0_ON	14
#define	SYSPMGR_SHUT0_OFF	15

#ifdef	_KERNEL
void syspmgr_init __P((syspmgr_tag_t));
int syspmgr __P((syspmgr_tag_t, u_int));
int syspmgr_alloc_delayh __P((bus_space_tag_t *, bus_space_handle_t *));
int syspmgr_alloc_systmph __P((bus_space_tag_t *, bus_space_handle_t *, syspmgr_channel_t));
#endif	/* _KERNEL */
#endif /* _I386_SYSTMPORTMGR_H_ */
