/*	$NecBSD: conio.S,v 1.1 1998/03/20 03:05:00 kmatsuda Exp $	*/
/*	$NetBSD: conio.S,v 1.1.1.1 1997/03/14 02:40:32 perry Exp $	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/* PC console handling
  originally from: FreeBSD:sys/i386/boot/netboot/start2.S
 */
#ifndef	ORIGINAL_CODE
/* extracted from netbsd_pc98:sys/arch/i386/boot/bios.S */
#endif	/* PC-98 */

#include <machine/asm.h>

#define	data32	.byte 0x66

	.text

#ifdef	ORIGINAL_CODE
/**************************************************************************
PUTC - Print a character
**************************************************************************/
ENTRY(conputc)
	push	%ebp
	mov	%esp,%ebp
	push	%ecx
	push	%ebx
	push	%esi
	push	%edi

	movb	8(%ebp),%cl

	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	%cl,%al
	data32
	mov	$1,%ebx
	movb	$0x0e,%ah
	int	$0x10

	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ecx
	pop	%ebp
	ret
#endif	/* !PC-98 */

/**************************************************************************
GETC - Get a character
**************************************************************************/
#ifdef	ORIGINAL_CODE
ENTRY(congetc)
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi

	call	_C_LABEL(prot_to_real)	# enter real mode

	movb	$0x0,%ah
	int	$0x16
	movb	%al,%bl

	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	xor	%eax,%eax
	movb	%bl,%al

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
#else	/* PC-98 */
ENTRY(congetc)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx		# save %ebx
	push	%esi
	push	%edi

	call	_C_LABEL(prot_to_real)

	movb	$0x0, %ah
	sti
	int	$0x18
	cli
	movb	%al, %bl	# real_to_prot uses %eax

	data32
	call	_C_LABEL(real_to_prot)

	xor	%eax, %eax
	movb	%bl, %al

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
#endif	/* PC-98 */

/**************************************************************************
ISKEY - Check for keyboard interrupt
**************************************************************************/
#ifdef	ORIGINAL_CODE
ENTRY(coniskey)
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi

	call	_C_LABEL(prot_to_real)	# enter real mode

	xor	%ebx,%ebx
	movb	$0x1,%ah
	int	$0x16
	data32
	jz	1f
	movb	%al,%bl
1:

	data32
	call	_C_LABEL(real_to_prot) # back to protected mode

	xor	%eax,%eax
	movb	%bl,%al

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
#else	/* PC-98 */
ENTRY(coniskey)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%esi
	push	%edi

	call	_C_LABEL(prot_to_real)		# enter real mode

	xor	%ebx, %ebx
	movb	$0x1, %ah
	sti
	int	$0x18
	cli
	andb	%bh, %bh
	data32
	jz	nochar
	movb	%al, %bl

nochar:
	data32
	call	_C_LABEL(real_to_prot)

	xor	%eax, %eax
	movb	%bl, %al

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
#endif	/* PC-98 */
