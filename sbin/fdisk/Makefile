#	$NetBSD: Makefile,v 1.10.2.2 1999/05/02 21:25:05 perry Exp $

.ifdef	ORIGINAL_CODE
SUBDIR=	mbr
.endif	# !PC-98

.if (${MACHINE} == "i386" || ${MACHINE} == "powerpc")
PROG=	fdisk 
SRCS=	fdisk.c
DPADD+=	${LIBUTIL}
LDADD+=	-lutil
.ifdef	ORIGINAL_CODE
SUBDIR+= mbr_bootsel
.endif	# !PC-98
.endif


MAN=	fdisk.8

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
