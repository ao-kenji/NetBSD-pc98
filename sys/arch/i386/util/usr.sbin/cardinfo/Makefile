#	$NecBSD: Makefile,v 1.10 1999/08/05 08:53:42 kmatsuda Exp $
#	$NetBSD$
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1995, 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 

PROG=	cardinfo
MKMAN=	no
LDADD= -L${.CURDIR}/../../lib/libpcmcia -lpcmcia
LDFLAGS+= -g
CFLAGS+= -g	-I${.CURDIR}/../../../..

.include <bsd.prog.mk>
