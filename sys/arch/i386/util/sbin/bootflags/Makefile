# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1999 NetBSD/pc98 porting staff.
#  All rights reserved.
# 

PROG=	bootflags
SRCS=	bootflags.c keycap.c
MKMAN=	no
CONFDIR=	${DESTDIR}/etc
CONFMODE=	444
BOOTFLAGSCONF=	bootflags.conf

beforeinstall:
	@if [ ! -e ${CONFDIR}/${BOOTFLAGSCONF} ]; then \
		echo installing ${BOOTFLAGSCONF} to ${CONFDIR} ; \
		install -c -m ${CONFMODE} ${.CURDIR}/${BOOTFLAGSCONF} ${CONFDIR} ; \
	fi

.include <bsd.prog.mk>
