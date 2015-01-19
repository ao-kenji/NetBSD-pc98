#	$NecBSD: Makefile,v 1.11 1999/08/05 08:52:56 kmatsuda Exp $
#	$NetBSD$

# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1995, 1996, 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
# 

DEMOS=	jis.90 test.njis
MKMAN=	no

all: $(DEMOS)

install: ${DEMOS}
	@echo "to look at the demos, execute:"
	@echo "        \"cat <filename>\""

.include <bsd.prog.mk>

CLEANFILES+= 	${DEMOS}

# this seems to be the lowest common denominator

jis.90: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz

test.njis: ${.CURDIR}/$@.gz.uu
	uudecode ${.CURDIR}/$@.gz.uu
	gunzip -f $@.gz
	rm -f $@.gz
