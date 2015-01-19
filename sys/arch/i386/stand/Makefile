#	$NecBSD: Makefile,v 1.2 1999/08/02 05:41:42 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.10 1999/03/14 05:50:38 sommerfe Exp $

SUBDIR=		biosboot installboot
.ifdef	ORIGINAL_CODE
SUBDIR+=	dosboot
#SUBDIR+=	genprom netboot
.endif	# !PC-98

.include <bsd.subdir.mk>
