#	$NecBSD: Makefile,v 1.2 1999/08/05 08:53:11 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.3 1999/03/14 05:50:39 sommerfe Exp $
#
# XXX: STILL BROKEN (PC-98 only?)

S=	${.CURDIR}/../../../../

BASE= biosboot_ser
PROG= ${BASE}.sym
MKMAN=	no
NEWVERSWHAT=	"BIOS Boot"

SRCS= main.c devopen.c conf.c exec.c

CLEANFILES+= ${BSSTART}

CPPFLAGS+= -DCOMPAT_OLDBOOT -DCOMPAT_386BSD_MBRPART


#Sample use of serial line debugger
.ifdef	ORIGINAL_CODE
#CPPFLAGS+= -DSUPPORT_SERIAL=CONSDEV_COM0KBD
#	or
CPPFLAGS+= -DSUPPORT_SERIAL=CONSDEV_AUTO -DDIRECT_SERIAL
#	and maybe
#CPPFLAGS+= -DDIRECT_SERIAL -DCOMCONS_KEYPRESS -DCONSPEED=57600
.else	# PC-98
# XXX: should -DSUPPORT_SERIAL=CONSDEV_AUTO
CPPFLAGS+= -DSUPPORT_SERIAL
# If your machine has old I8251,
CPPFLAGS+= -DDIRECT_SERIAL -DCONSPEED=38400
# If your machine has I8251AF,
#CPPFLAGS+= -DDIRECT_SERIAL -DCONSPEED=115200
.endif	# PC-98
CPPFLAGS+= -DPASS_BIOSGEOM
#uncomment if there are problems with memory detection
#CPPFLAGS+= -DCONSERVATIVE_MEMDETECT
#increase MAXFLENTRIES if "installboot" complains about
# "not enough fragment space in bootcode" (default: 10)
#CPPFLAGS+= -DMAXFLENTRIES=15
#set PRIM_LOADSZ to <=9 to make a 720k boot floppy (default: 15)
.ifdef	ORIGINAL_CODE
#CPPFLAGS+= -DPRIM_LOADSZ=9
.else	# PC-98
CPPFLAGS+= -DPRIM_LOADSZ=16
.endif	# PC-98

#CFLAGS= -O2 -fomit-frame-pointer -fno-defer-pop
CFLAGS+= -Wall

# XXX should go into library
SRCS+= ls.c
.PATH: ${.CURDIR}/../libsa ${.CURDIR}/../biosboot

SAMISCCPPFLAGS+= -DHEAP_START=0x10000 -DHEAP_LIMIT=0x40000
SAMISCMAKEFLAGS= SA_USE_CREAD=yes SA_INCLUDE_NET=no

VERSIONFILE= ${.CURDIR}/../biosboot/version

.include "../Makefile.booters"
