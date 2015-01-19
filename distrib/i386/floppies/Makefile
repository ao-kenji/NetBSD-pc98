#	$NecBSD: Makefile,v 1.19 1999/08/07 11:10:51 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.9.2.1 1999/07/06 23:50:32 perry Exp $
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999
#	NetBSD/pc98 porting staff. All rights reserved.
#

.ifdef	ORIGINAL_CODE
SUBDIR=	ramdisk fdset bootfloppy bootfloppy-big bootfloppy-tiny
.else	# PC-98
# XXX: Is fdset required ?
SUBDIR=	ramdisk       bootfloppy bootfloppy-small
.endif	# PC-98

.include <bsd.subdir.mk>
