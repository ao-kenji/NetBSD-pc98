#	$NecBSD: Makefile,v 1.1 1999/08/07 11:11:15 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.2 1999/03/23 18:10:59 drochner Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"

IMAGE=	boot-tiny.fs
KERN?=	${.CURDIR}/../../../../sys/arch/i386/compile/INSTALL_TINY/netbsd
.ifdef	ORIGINAL_CODE
DISKTYPE=	floppy5
.else	# PC-98
DISKTYPE=	floppy5ufs
.endif	# PC-98
DISKSIZE=	2400

.include "${TOP}/bootfloppy-common/Makefile.inc"
