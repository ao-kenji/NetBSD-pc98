/*	$NecBSD: vm86bios.h,v 3.14.2.1 1999/08/14 14:40:46 honda Exp $	*/
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

struct vm86_call {
	void *vc_id;
#define	VM86BIOS_ASYNC	0x01
	u_int vc_flags;

	int vc_error;
	int vc_intno;
	struct sigcontext vc_reg;
};

#define VM86BIOS_IOC_WAIT	_IO('V', 1)
#define	VM86BIOS_IOG_TASK	_IOR('V', 2, struct vm86_call)
#define	VM86BIOS_IOC_TASKDONE	_IOW('V', 3, struct vm86_call)
#define	VM86BIOS_IOC_USRREQ	_IOWR('V', 4, struct vm86_call)
#define	VM86BIOS_IOC_CONNECT	_IO('V', 5)

#define	VM86BIOS_BMAPBASE 0xa0000
#define	VM86BIOS_BMAPSIZE 0x60000
#define	VM86BIOS_IMAPBASE 0x0
#define	VM86BIOS_IMAPSIZE 0x1000

#ifdef _KERNEL
#define	VM86BIOS_KIO_SWAITOK	0x0001
void vm86biosstart __P((struct vm86_call *, void (*) __P((void *)), void *, int));
void kthread_vm86biosd  __P((void *));
#endif
