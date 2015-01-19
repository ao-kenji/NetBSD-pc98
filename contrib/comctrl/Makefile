#	$NecBSD: Makefile,v 1.9 1998/03/14 07:01:20 kmatsuda Exp $
#	$NetBSD$
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1995, 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 

PROG=	comctrl
NOMAN=	noman
BINDIR?=	/sbin
LDSTATIC=	-static
CFLAGS+=	-I${.CURDIR}/../../sys/arch
COMCONF=	com.conf
CONFDIR=	${DESTDIR}/etc
CONFMODE=	444
EXECCONF=	comstart
ECONFMODE=	744

beforeinstall:
	@if [ ! -e ${CONFDIR}/${COMCONF} ]; then \
		echo installing ${COMCONF} to ${CONFDIR} ; \
		install -c -m ${CONFMODE} ${.CURDIR}/${COMCONF} ${CONFDIR} ; \
	fi
	@for i in ${EXECCONF}; do \
		if [ ! -e ${CONFDIR}/$$i ]; then \
			echo installing $$i to ${CONFDIR} ; \
			install -c -m ${ECONFMODE} ${.CURDIR}/$$i ${CONFDIR} ; \
		fi; \
	done

.include <bsd.prog.mk>
