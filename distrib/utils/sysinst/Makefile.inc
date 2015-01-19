#	$NecBSD: Makefile.inc,v 1.2 1999/08/07 11:12:33 kmatsuda Exp $
#	$NetBSD: Makefile.inc,v 1.4.2.1 1999/06/24 22:44:05 cgd Exp $
#
# Makefile for install

PROG=sysinst

.ifdef	ORIGINAL_CODE
LDADD=-lcurses -ltermcap -lutil
.else	# PC-98
# XXX: use with x_libcurses
LDADD=-lcurses -ltermcap -lutil
.endif	# PC-98

.if exists(${.CURDIR}/../../../../sys/conf/osrelease.sh)
VER != sh ${.CURDIR}/../../../../sys/conf/osrelease.sh
.endif

.if exists(${.CURDIR}/../../../../../sys/conf/osrelease.sh)
VER != sh ${.CURDIR}/../../../../../sys/conf/osrelease.sh
.endif

CPPFLAGS+= -I. -I${.CURDIR}/../.. -I${.CURDIR} \
	 -DREL=\"${VER}\" -DMACH=\"${MACHINE}\"

MKMAN=no

WARNS=1

CLEANFILES= menu_defs.c menu_defs.h menus.def msg_defs.c msg_defs.h msg.def

.ifdef	ORIGINAL_CODE
LANG?=en
.else	# PC-98
LANG?=ja
.endif	# PC-98

.PATH: ${.CURDIR}/../..

MENUC=menuc
MSGC=msgc

.include <bsd.prog.mk>

menu_defs.c menu_defs.h: menus.def
	${MENUC} menus.def

msg_defs.c msg_defs.h: msg.def
	${MSGC} msg.def

msg.def: msg.mi.${LANG} msg.md.${LANG}
	sed "s/@@VERSION@@/${VER}/" ${.ALLSRC} > ${.TARGET}

menus.def: menus.mi.${LANG} menus.md.${LANG}
	sed "s/@@VERSION@@/${VER}/" ${.ALLSRC} > ${.TARGET}

menu_defs.c: msg_defs.h

disks.o install.o main.o net.o upgrade.o util.o: msg_defs.h menu_defs.h
