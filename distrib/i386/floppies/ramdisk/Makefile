#	$NecBSD: Makefile,v 1.2 1999/08/07 11:12:00 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.11.2.2 1999/06/22 16:51:43 perry Exp $

TOP=		${.CURDIR}/..
WARNS=1

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk.fs

AUXTARGETS=	install.sh upgrade.sh start.sh
.ifdef	ORIGINAL_CODE
AUXDEPENDS= 	dot.profile dot.hdprofile disktab.preinstall \
		termcap.mini termcap.pc3
.else	# PC-98
AUXDEPENDS= 	dot.profile dot.hdprofile disktab.preinstall \
		termcap.mini termcap.vsc
.endif	# PC-98
AUXCLEAN=	${AUXTARGETS}

CBIN=		ramdiskbin

MOUNT_POINT?=	/mnt
# DEV/RDEV file system device, CDEV/RDEV vnconfig device
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CDEV=	/dev/${VND}d
VND_CRDEV=	/dev/r${VND}d
IMAGE?=		xxx.fs
.ifndef	ORIGINAL_CODE
TMP_IMAGE?=	/tmp/tmp-${IMAGE}
.endif	# PC-98
MDEC=		${DESTDIR}/usr/mdec

LISTS=		list
CRUNCHCONF=	${CBIN}.conf
MTREE=		mtree.conf

.ifdef	ORIGINAL_CODE
DISKTYPE=	floppy3
.else	# PC-98
DISKTYPE=	floppy3ufs

INO_BYTES?=	3045	#4069, 5120, 6144
.endif	# PC-98

install.sh: install.tmpl
	sed "s/@@VERSION@@/${VER}/" < ${.ALLSRC} > ${.TARGET}

upgrade.sh: upgrade.tmpl
	sed "s/@@VERSION@@/${VER}/" < ${.ALLSRC} > ${.TARGET}

start.sh: start.tmpl
	sed "s/@@VERSION@@/${VER}/" < ${.ALLSRC} > ${.TARGET}

all: ${AUXTARGETS} ${CBIN} ${AUXDEPENDS} ${MTREE} ${LISTS}
.ifdef	ORIGINAL_CODE
	dd if=/dev/zero of=${IMAGE} count=2880
	vnconfig -t ${DISKTYPE} -v -c ${VND_CDEV} ${IMAGE}
	disklabel -rw ${VND_CDEV} ${DISKTYPE}
	newfs -B le -m 0 -o space -i 5120 -c 80 ${VND_RDEV} ${DISKTYPE}
.else	# PC-98
	dd if=/dev/zero of=${TMP_IMAGE} count=2880
	vnconfig -t ${DISKTYPE} -v -c ${VND_CDEV} ${TMP_IMAGE}
	echo y | disklabel -rw ${VND_CDEV} ${DISKTYPE}
	newfs -B le -m 0 -o space -i ${INO_BYTES} -c 80 ${VND_RDEV} ${DISKTYPE}
.endif	# PC-98
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${.CURDIR}/${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${.CURDIR}/${LISTS}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_CDEV}
.ifndef	ORIGINAL_CODE
	mv ${TMP_IMAGE} ${IMAGE}
.endif	# PC-98

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND_DEV}
.ifdef	ORIGINAL_CODE
	-/bin/rm -f ${IMAGE}
.else	# PC-98
	-/bin/rm -f ${TMP_IMAGE} ${IMAGE}
.endif	# PC-98

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${TOP}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

# This is listed in ramdiskbin.conf but is built here.
${CBIN}: libhack.o

# Use stubs to eliminate some large stuff from libc
HACKSRC=${TOP}/../../utils/libhack
.include "${HACKSRC}/Makefile.inc"

# turn off small gethostby* temporarily
HACKOBJS:= getcap.o getgrent.o getnet.o getnetgr.o getpwent.o setlocale.o yplib.o

.ifndef	ORIGINAL_CODE
# This is listed in ramdiskbin.conf but is built here.
${CBIN}: libcurses.o	# x_libcurses for multibytes

CURSESSRC=${TOP}/../../utils/x_libcurses
.include "${CURSESSRC}/Makefile.inc"
.endif	# PC-98

clean cleandir distclean:
	/bin/rm -f ${AUXCLEAN} *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include <bsd.sys.mk>
