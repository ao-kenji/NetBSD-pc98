#	$NecBSD: Makefile,v 3.1 1999/07/24 01:28:59 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.1 1998/06/12 23:22:50 cgd Exp $
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1999
#	NetBSD/pc98 porting staff. All rights reserved.
# 

INCSDIR= /usr/include/dev/isa

INCS=	ad1848var.h ariareg.h com_multi.h cs4231var.h elink.h espvar.h \
	gusreg.h ics2101var.h if_aireg.h if_ecreg.h if_efreg.h if_egreg.h \
	if_elreg.h if_fereg.h if_ixreg.h if_levar.h if_wereg.h isadmareg.h \
	isadmavar.h isareg.h isavar.h madreg.h mcdreg.h pasreg.h \
	pcdisplayvar.h pckbcvar.h pcppireg.h pcppivar.h pssreg.h satlinkio.h \
	satlinkreg.h sbdspvar.h sbreg.h sbvar.h spkrio.h vga_isavar.h \
	wdsreg.h wssreg.h wssvar.h wtreg.h ymvar.h

.ifndef	ORIGINAL_CODE
INCS+=	ccbque.h pisaif.h pisavar.h
.endif	# PC-98

.include <bsd.kinc.mk>
