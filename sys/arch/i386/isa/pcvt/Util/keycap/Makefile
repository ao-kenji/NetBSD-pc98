#	$NecBSD: Makefile,v 1.13 1999/08/05 08:52:50 kmatsuda Exp $
#	$NetBSD$

# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1995, 1996, 1997, 1998, 1999
#	NetBSD/pc98 porting staff. All rights reserved.

LIB	=	keycap
CAPDIR	=	/usr/share/pcvt
CAPPATH	=	$(CAPDIR)/keycap.pcvt
KEYCAPSRC=	keycap.src
CPPFLAGS+=	-DKEYCAP_PATH=\"$(CAPPATH)\"
SRCS	=	keycap.c

MKMAN=	no
MKLINT=	no

beforeinstall:
	@if [ ! -d ${DESTDIR}${CAPDIR} ]; then mkdir ${DESTDIR}${CAPDIR};fi
	@${ECHO} "Saving old keycap database"
	-mv -f ${DESTDIR}${CAPPATH} ${DESTDIR}${CAPPATH}.BAK
	${INSTALL} -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
	${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}

.include <bsd.lib.mk>
