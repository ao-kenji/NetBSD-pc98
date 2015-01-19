#	$NecBSD: Makefile,v 1.1 1999/08/07 11:13:11 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.19 1998/02/18 22:08:12 jtc Exp $
#	@(#)Makefile	8.2 (Berkeley) 1/2/94

CPPFLAGS+=#-DTFILE=\"/dev/ttyp0\"
CPPFLAGS+=-D_CURSES_PRIVATE -I${.CURDIR}
CPPFLAGS+=-D_X_LIBCURSES
LIB=	curses
XSRCS=	addbytes.c delch.c move.c refresh.c
LSRCS=	           addch.c addnstr.c box.c clear.c clrtobot.c clrtoeol.c \
	cr_put.c ctrace.c cur_hash.c curses.c         deleteln.c delwin.c \
	erase.c fullname.c getch.c getstr.c id_subwins.c idlok.c initscr.c \
	insch.c insertln.c longname.c        mvwin.c newwin.c overlay.c \
	overwrite.c printw.c putchar.c           scanw.c scroll.c setterm.c \
	standout.c toucholap.c touchwin.c tscroll.c tstp.c tty.c unctrl.c
SRCS=	${XSRCS} ${LSRCS}
MKMAN=	no
MKPIC=	no
MKPROFILE=	no
MKLINT=	no

CPPFLAGS+=-DCM_N -DCM_GT -DCM_B -DCM_D
SRCS+=	termcap.c tgoto.c tputs.c
.PATH: ${.CURDIR}/../../../lib/libterm

CURSES=	${.CURDIR}/../../../lib/libcurses
MAKE_SYMLINK?=	ln -sf

.for file in ${LSRCS}
${file}:	${CURSES}/${file}
	${MAKE_SYMLINK} $> $@
.endfor

.include <bsd.lib.mk>
