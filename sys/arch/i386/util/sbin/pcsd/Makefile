#	$NecBSD: Makefile,v 1.11.2.2 1999/08/23 07:51:46 kmatsuda Exp $
#	$NetBSD$
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1995, 1996 NetBSD/pc98 porting staff.
#  All rights reserved.
# 

PROG=	pcsd
SRCS=	pcsd.c keycap.c
MKMAN=	no
LDADD=	-L${.CURDIR}/../../lib/libpcmcia -lpcmcia
CFLAGS+= -g	-I${.CURDIR}/../../../..
LDFLAGS+= -g

PCSDCONF=	pcmcia.conf
CONFDIR=	${DESTDIR}/etc
CONFMODE=	444
EXECCONF=	etherstart modemstart
ECONFMODE=	744
EXECUTIL=	pcsd.restart pcsd.kill
EXECUTILDIR=	${DESTDIR}/usr/sbin
EUTILMODE=	744

beforeinstall:
	@if [ ! -e ${CONFDIR}/${PCSDCONF} ]; then \
		echo installing ${PCSDCONF} to ${CONFDIR} ; \
		install -c -m ${CONFMODE} ${.CURDIR}/${PCSDCONF} ${CONFDIR} ; \
	fi
	@for i in ${EXECCONF}; do \
		if [ ! -e ${CONFDIR}/$$i ]; then \
			echo installing $$i to ${CONFDIR} ; \
			install -c -m ${ECONFMODE} ${.CURDIR}/$$i ${CONFDIR} ; \
		fi; \
	done
	@for i in ${EXECUTIL}; do \
		echo installing $$i to ${EXECUTILDIR} ; \
		install -c -m ${EUTILMODE} ${.CURDIR}/$$i ${EXECUTILDIR} ; \
	done

.include <bsd.prog.mk>
