#	$NecBSD: Makefile,v 1.15 1999/08/05 08:52:47 kmatsuda Exp $
#	$NetBSD$

# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1995, 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.

PROG=		kcon
DEVICE=		/dev/ttyv0
.if	defined(ORIGINAL_CODE)
CFLAGS+= 	-I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"
.else
CFLAGS+= 	-I${.CURDIR}/../keycap -I${.CURDIR}/../../../vsc -I${.CURDIR}/../../../../.. -DKEYB_DEVICE=\"${DEVICE}\"
.endif

MKMAN=	no

KEYCAPOBJDIR!=	cd ${.CURDIR}/../keycap; make print-objdir

LDADD=		-L${KEYCAPOBJDIR} -lkeycap

.include <bsd.prog.mk>
