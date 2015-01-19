#	$NecBSD: Makefile,v 1.10 1999/08/05 08:53:28 kmatsuda Exp $
#	$NetBSD$
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1996, 1997 NetBSD/pc98 porting staff.
#  All rights reserved.
# 

PROG=	ippctrl
SRCS=	ippctrl.c keycap.c ippres.c
MKMAN=	no
IPPCONF=	ipp.conf
CONFDIR=	${DESTDIR}/etc
CONFMODE=	444
CFLAGS+=	-g -I${.CURDIR}/../../../..
LDFLAGS+=	-g
.PATH: ${.CURDIR}/../../../Cbus/pnp

BINOWN=	root
BINGRP=	operator
BINMODE=4550

beforeinstall:
	@if [ ! -e ${CONFDIR}/${IPPCONF} ]; then \
		echo installing ${IPPCONF} to ${CONFDIR} ; \
		install -c -m ${CONFMODE} ${.CURDIR}/${IPPCONF} ${CONFDIR} ; \
	fi

.include <bsd.prog.mk>
