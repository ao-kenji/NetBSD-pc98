#	$NecBSD: Makefile,v 1.6 1998/03/14 07:01:44 kmatsuda Exp $
#	$NetBSD$

#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 

PROG=playmidi
BINDIR?= /usr/local/bin

SRCS= playmidi.c io_ncurses.c playevents.c readmidi.c midihw.c
LDADD= -li386 -ltermcap -lcurses 
CFLAGS= -I../base
#LDFLAGS= -g

NOMAN = yes

.PATH: ../base

.include <bsd.prog.mk>
