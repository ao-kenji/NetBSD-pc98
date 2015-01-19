#	$NecBSD: Makefile.inc,v 1.16 1999/08/05 08:52:52 kmatsuda Exp $
#	$NetBSD: Makefile.inc,v 1.10 1998/11/01 16:37:06 hubertf Exp $

#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1995, 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 


ECHO	?=	echo

BINDIR	?=	/usr/local/bin
FONTDIR	=	/usr/share/pcvt/fonts
LIBDIR	=	/usr/local/lib
LIBMODE	=	644

INCDIR= ${DESTDIR}/usr/include/machine

${PROG}: ${INCDIR}/pcvt_ioctl.h

MAN3EXT	=	3
MAN4EXT	=	4
MAN5EXT	=	5
MAN8EXT	=	8
MANDIR	=	/usr/local/man

MAN = ${MAN1} ${MAN3} ${MAN4} ${MAN5} ${MAN8}

.if	empty(MAN) && defined(PROG)
MAN+=	${PROG}.1
.endif
