#	$NecBSD: Makefile,v 1.15 1999/08/05 08:52:52 kmatsuda Exp $
#	$NetBSD$

#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1994, 1995, 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 

SUBDIR= 	clock loadmaru vsc screenmode loadascii \
		../../pcvt/Util/keycap ../../pcvt/Util/kcon \
		../../pcvt/Util/vttest

.if make(clean) || make(cleandir) || make(obj)
SUBDIR+=	demo
.endif

.include <bsd.subdir.mk>
