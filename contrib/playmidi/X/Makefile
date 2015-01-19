#	$NecBSD: Makefile,v 1.7 1998/03/14 07:01:40 kmatsuda Exp $
#	$NetBSD$

#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 

PROG= xplaymidi
BINDIR?= /usr/local/bin

SRCS= playmidi.c io_xaw.c playevents.c readmidi.c midihw.c
LDADD = -li386 -ltermcap -lcurses -L${DESTDIR}/usr/X11R6/lib -lXaw -lXmu -lXt -lX11 -lXext -lSM -lICE

NOMAN = yes

.PATH: ../base
CFLAGS= -I../base -I${DESTDIR}/usr/X11R6/include

.include <bsd.prog.mk>
