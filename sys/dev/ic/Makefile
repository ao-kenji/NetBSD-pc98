#	$NecBSD: Makefile,v 1.1 1999/07/24 01:28:56 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.3 1999/01/30 14:58:26 minoura Exp $
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1999
#	NetBSD/pc98 porting staff. All rights reserved.
# 

INCSDIR= /usr/include/dev/ic

INCS=	ad1848reg.h ahareg.h ahavar.h aic6360reg.h aic6360var.h \
	aic7xxxreg.h aic7xxxvar.h am7930reg.h am7990reg.h am7990var.h \
	bhareg.h bhavar.h bt431reg.h bt459reg.h bt463reg.h bt485reg.h \
	cd1190reg.h cd1400reg.h comreg.h comvar.h cs4231reg.h cyreg.h \
	cyvar.h dc21040reg.h dp8390reg.h dp8390var.h ds.h elink3reg.h \
	elink3var.h hayespreg.h i8042reg.h i82365reg.h i82365var.h \
	i8237reg.h i8253reg.h i82586reg.h i82586var.h i82595reg.h \
	ics2101reg.h ims332reg.h intersil7170.h interwavereg.h \
	interwavevar.h ispmbox.h ispreg.h ispvar.h lemacreg.h lemacvar.h \
	lptreg.h lptvar.h mb86960reg.h mb86960var.h mc146818reg.h \
	mc68450reg.h mc6845reg.h midwayreg.h midwayvar.h ncr5380reg.h \
	ncr5380var.h ncr53c9xreg.h ncr53c9xvar.h ne2000reg.h ne2000var.h \
	nec765reg.h ns16450reg.h ns16550reg.h opl3sa3.h pcdisplay.h \
	pcdisplayvar.h pdqreg.h pdqvar.h rrunnerreg.h rrunnervar.h \
	smc83c170reg.h smc83c170var.h smc90cx6reg.h smc91cxxreg.h \
	smc91cxxvar.h smc93cx6var.h st16650reg.h tms320av110reg.h \
	tms320av110var.h uhareg.h uhavar.h vgareg.h vgavar.h wdcreg.h wdcvar.h \
	z8530reg.h z8530sc.h

.ifndef	ORIGINAL_CODE
INCS+=	i8251reg.h wd33c93reg.h ym2203reg.h
.endif	# PC-98

.include <bsd.kinc.mk>
