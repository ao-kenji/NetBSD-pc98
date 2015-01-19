/*	$NecBSD: bios_disk.S,v 1.2 1999/08/02 05:42:38 kmatsuda Exp $	*/
/*	$NetBSD: bios_disk.S,v 1.7 1999/03/30 22:35:21 fvdl Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* extracted from netbsd:sys/arch/i386/boot/bios.S */
#ifndef	ORIGINAL_CODE
/* extracted from netbsd_pc98:sys/arch/i386/boot/bios.S */
#endif	/* PC-98 */

#include <machine/asm.h>

#define	addr32	.byte 0x67
#define	data32	.byte 0x66

#ifdef	ORIGINAL_CODE
/*
# BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
#	Call with	%ah = 0x2
#			%al = number of sectors
#			%ch = cylinder
#			%cl = sector
#			%dh = head
#			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
#			%es:%bx = segment:offset of buffer
#	Return:		
#			%al = 0x0 on success; err code on failure
*/
ENTRY(biosread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push	%edi

	movb	16(%ebp), %dh
	movw	12(%ebp), %cx
	xchgb	%ch, %cl	# cylinder; the highest 2 bits of cyl is in %cl
	rorb	$2, %cl
	movb	20(%ebp), %al
	orb	%al, %cl
	incb	%cl		# sector; sec starts from 1, not 0
	movb	8(%ebp), %dl	# device
	movl	28(%ebp), %ebx	# offset
				# prot_to_real will set %es to BOOTSEG

	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	$0x2, %ah	# subfunction
	addr32
	movb	24(%ebp), %al	# number of sectors
	int	$0x13
	setc	%bl

	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	xorl	%eax, %eax
	movb	%bl, %al	# return value in %ax

	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	popl	%ebx
	popl	%ebp
	ret
#else	/* PC-98 */
/*
# BIOS call "INT 0x1B Function 0xn6" to read sectors from disk into memory
#	Call with	%ah = 0xd6(for floppy disk) or 0x06(for hard disk)
#			%al = DA/UA
#			%bx = data length
#			%ch = sector size(for floppy) or cylinder(for hard)
#			%cl = cylinder
#			%dh = head
#			%dl = sector
#			%es:%bp = segment:offset of buffer
#	Return:		
#			%al = 0x0 on success; err code on failure
*/
ENTRY(biosread)
	pushl	%ebp
	mov	%esp, %ebp
	pushl	%ebx
	push	%ecx
	push	%edx
	push	%es
	push	%esi
	push	%edi

	movb	0x14(%ebp), %dl		# sector
	movb	0x10(%ebp), %dh		# head
	movw	0x0c(%ebp), %cx		# cylinder
	movb	0x08(%ebp), %al		# DA/UA
	movb	$0x06, %ah
	andb	$0xf0, %al
	cmpb	$0x30, %al
	jz	9f
	cmpb	$0x90, %al
	jz	9f
	jmp	1f

9:
	incb	%dl
	movb	$0x02, %ch
	movb	$0xd6, %ah

1:
	movb	0x08(%ebp), %al
	movl	%eax, %ebx
				# prot_to_real will set %es to BOOTSEG

	call	_C_LABEL(prot_to_real)	# enter real mode
	data32
	mov	%ebx, %eax
	xor	%ebx, %ebx
	addr32
	movb	0x18(%ebp), %bl		# number of sectors
	data32
	shl	$9, %ebx
	data32
	push	%ebx
	addr32
	data32
	mov	0x1c(%ebp), %ebx
	data32
	mov	%ebx, %ebp
	data32
	pop	%ebx

	sti
	int	$0x1b
	cli
	jc	1f
	xor	%eax, %eax
1:
	data32
	mov	%eax, %ebx	# save return value (actually movw %ax, %bx)

	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	xor	%eax, %eax
	movb	%bh, %al	# return value in %ax

	pop	%edi
	pop	%esi
	pop	%es
	pop	%edx
	pop	%ecx
	popl	%ebx
	popl	%ebp
	ret
#endif	/* PC-98 */

/*
#
# get_diskinfo():  return a word that represents the
#	max number of sectors, heads and cylinders for this device
#
*/

#ifdef	ORIGINAL_CODE
ENTRY(get_diskinfo)
	pushl	%ebp
	movl	%esp, %ebp
	push	%es
	pushl	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push	%edi

	movb	8(%ebp), %dl		# diskinfo(drive #)

	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	$0x08, %ah		# ask for disk info
	int	$0x13
	jnc	ok

	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.  Initialize all the registers for
	 * documentation, although we only need head and sector counts.
	 */
#	subb	%ah, %ah		# %ax = 0
#	movb	%ah, %bh		# %bh = 0
#	movb	$2, %bl			# %bl bits 0-3 = drive type, 2 = 1.2M
	movb	$79, %ch		# max track
	movb	$15, %cl		# max sector
	movb	$1, %dh			# max head
#	movb	$1, %dl			# # floppy drives installed
	# es:di = parameter table
	# carry = 0

ok:
	data32
	call	_C_LABEL(real_to_prot)	# back to protected mode

	/* form a longword representing all this gunk */
	shll	$8, %ecx
	movb	%dh, %cl

	movl	%ecx, %eax

	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	popl	%ebx
	pop	%es
	popl	%ebp
	ret
#else	/* PC-98 */
ENTRY(get_diskinfo)
	pushl	%ebp
	mov	%esp, %ebp
	push	%es
	pushl	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push	%edi

	movb	0x8(%ebp), %dl		# diskinfo(drive #)
	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	%dl, %al		# ask for disk info
	andb	$0xf0, %al
	cmpb	$0x90, %al
	jz	fdd0
	cmpb	$0x30, %al
	jz	fdd1

	movb	%dl, %al
	movb	$0x84, %ah

	sti
	int	$0x1b
	cli

	jnc	ok

	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.  Initialize all the registers for
	 * documentation, although we only need head and sector counts.
	 */
fdd0:
#	subb	%ah, %ah		# %ax = 0
#	movb	%al, %al
#	movb	%ah, %bh		# %bh = 0
#	movb	$2, %bl			# %bl bits 0-3 = drive type, 2 = 1.2M
	movb	$0, %ch			# movw ?
	movb	$79, %cl		# max track
	movb	$15, %dl		# max sector
	movb	$2, %dh			# max head
#	movb	$1, %cl			# # floppy drives installed
	jmp	ok

fdd1:
#	subb	%ah, %ah		# %ax = 0
#	movb	%al, %al
#	movb	%ah, %bh		# %bh = 0
#	movb	$2, %bl			# %bl bits 0-3 = drive type, 2 = 1.2M
	movb	$0, %ch			# movw ?
	movb	$79, %cl		# max track
	movb	$18, %dl		# max sector
	movb	$2, %dh			# max head
#	movb	$1, %dl			# # floppy drives installed

ok:

	data32
	call	_C_LABEL(real_to_prot)	# back to protected mode

	/* form a longword representing all this gunk */
	shll	$16, %ecx		# max cylinder
	movb	%dl, %ch		# max sector (and # sectors)
	movb	%dh, %cl		# max head

	movl	%ecx, %eax

	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	popl	%ebx
	pop	%es
	popl	%ebp
	ret
#endif	/* PC-98 */

#ifndef	ORIGINAL_CODE
/* XXX: not yet (at any rate, is it valid for PC-98?) */
#else	/* !PC-98 */
/*
# int13_extension: check for availibility of int13 extensions.
*/

ENTRY(int13_extension)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi

	movb	8(%ebp), %dl		# drive #
	movw	$0x55aa, %bx

	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	$0x41, %ah		# ask for disk info
	int	$0x13
	setnc	%dl

	data32
	CALL	_C_LABEL(real_to_prot)	# switch back

	xorl	%eax, %eax
	movb	%dl, %al	# return value in %ax

	cmpw	$0xaa55, %bx
	sete	%dl
	andb	%dl, %al

	andb	%cl, %al

	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/*
# BIOS call "INT 0x13 Function 0x42" to read sectors from disk into memory
#	Call with	%ah = 0x42
#			%ds:%si = parameter block
#			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
#	Return:		
#			%al = 0x0 on success; err code on failure
*/
ENTRY(biosextread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push	%edi

	movb	8(%ebp), %dl	# device
	movl	12(%ebp), %esi	# parameter block

	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	$0x42, %ah	# subfunction
	int	$0x13
	setc	%bl

	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	xorl	%eax, %eax
	movb	%bl, %al	# return value in %ax

	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	popl	%ebx
	popl	%ebp
	ret

ENTRY(int13_getextinfo)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	push	%ecx
	push	%edx
	push	%esi
	push	%edi

	movb	8(%ebp), %dl	# device
	movl	12(%ebp), %esi	# parameter block
	movl	$0x01a, (%esi)	# length (v 1.x)

	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	$0x48, %ah	# subfunction
	int	$0x13
	setc	%bl

	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	xorl	%eax, %eax
	movb	%bl, %al	# return value in %ax

	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx
	popl	%ebx
	popl	%ebp
	ret
#endif	/* !PC-98 */
