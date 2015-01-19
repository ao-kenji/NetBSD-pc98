#	$NecBSD: Makefile,v 1.2 1998/03/24 10:37:56 honda Exp $
#	$NetBSD$

#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 

PROG=nplaymidi
BINDIR?= /usr/local/bin

SRCS= playmidi.c playevents.c readmidi.c io_ncurses.c
LDADD= -g -ltermcap -lcurses 
CFLAGS= -g

NOMAN = yes

.include <bsd.prog.mk>
