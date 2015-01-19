#	$NecBSD: Makefile,v 1.3 1999/08/05 08:53:16 kmatsuda Exp $
#	$NetBSD$
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
#

NOPROG=	noprog
MKMAN=	no

OBJS=	start.o

all: boot

boot: ${OBJS} mktbl.c
	gcc -o mktbl mktbl.c
	${LD} -Bstatic -e start -N -T 0 -o boot ${OBJS}
	@strip boot
	@sh ${.CURDIR}/rmaouthdr boot boot.tmp
	./mktbl boot.tmp > boot-helper.tbl

.include <bsd.prog.mk>
