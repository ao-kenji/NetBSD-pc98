#	$NecBSD: Makefile,v 1.9 1998/03/27 17:27:20 honda Exp $
#	$NetBSD: Makefile,v 1.8 1994/10/27 04:14:23 cgd Exp $
#	@(#)Makefile	7.3 (Berkeley) 6/9/91

NOPROG=	noprog
NOMAN=	noman

SUBDIR=	curses nplaymidi recmidi
#
#
# XXX: check more strict:
# check X11 library. If someone exists, make X11 version.
#
.if exists(${DESTDIR}/usr/X11R6/lib/libX11.a)
SUBDIR+=	X
.endif

.include <bsd.subdir.mk>
