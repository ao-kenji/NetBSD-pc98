#	$NecBSD: Makefile,v 1.2 1998/03/30 23:57:45 honda Exp $
#	$NetBSD$

#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 

PROG=recmidi
BINDIR?= /usr/local/bin

SRCS= recmidi.c io_ncurses.c
LDADD= -g -ltermcap -lcurses 
CFLAGS= -g

CFLAGS= -g
LDFLAGS= -g
NOMAN = yes

.include <bsd.prog.mk>
