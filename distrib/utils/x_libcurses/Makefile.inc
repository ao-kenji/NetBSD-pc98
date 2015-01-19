#	$NecBSD: Makefile.inc,v 1.1 1999/08/07 11:13:11 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.19 1998/02/18 22:08:12 jtc Exp $
#	@(#)Makefile	8.2 (Berkeley) 1/2/94

CPPFLAGS+=#-DTFILE=\"/dev/ttyp0\"
CPPFLAGS+=-D_CURSES_PRIVATE
CPPFLAGS+=-D_X_LIBCURSES

XSRCS=	addbytes.c delch.c move.c refresh.c
LSRCS=	           addch.c addnstr.c box.c clear.c clrtobot.c clrtoeol.c \
	cr_put.c ctrace.c cur_hash.c curses.c         deleteln.c delwin.c \
	erase.c fullname.c getch.c getstr.c id_subwins.c idlok.c initscr.c \
	insch.c insertln.c longname.c        mvwin.c newwin.c overlay.c \
	overwrite.c printw.c putchar.c           scanw.c scroll.c setterm.c \
	standout.c toucholap.c touchwin.c tscroll.c tstp.c tty.c unctrl.c
#SRCS=	${XSRCS} ${LSRCS}

#CURSES=	${.CURDIR}/../../../lib/libcurses
CURSES=	${DESTDIR}/usr/src/lib/libcurses
MAKE_SYMLINK?=	ln -sf

OBJS=	addbytes.o delch.o move.o refresh.o \
	           addch.o addnstr.o box.o clear.o clrtobot.o clrtoeol.o \
	cr_put.o ctrace.o cur_hash.o curses.o         deleteln.o delwin.o \
	erase.o fullname.o getch.o getstr.o id_subwins.o idlok.o initscr.o \
	insch.o insertln.o longname.o        mvwin.o newwin.o overlay.o \
	overwrite.o printw.o putchar.o           scanw.o scroll.o setterm.o \
	standout.o toucholap.o touchwin.o tscroll.o tstp.o tty.o unctrl.o

.for file in ${LSRCS}
${file}:	${CURSES}/${file}
	${MAKE_SYMLINK} $> $@
.endfor

all: libcurses.o

CLEANFILES+=  libcurses.o
libcurses.o : $(OBJS)
	$(LD) -r -o $@ $(OBJS)

.PATH: ${CURSESSRC}
