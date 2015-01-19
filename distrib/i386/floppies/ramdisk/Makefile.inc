#	$NecBSD: Makefile.inc,v 1.2 1999/08/07 11:12:00 kmatsuda Exp $
#	$NetBSD: Makefile.inc,v 1.7 1999/03/04 20:32:19 wrstuden Exp $

# TOP is assumed to be defined by Makefile including this one.

CBIN=		ramdiskbin
COMMONDIR=	${TOP}/ramdisk-common

MOUNT_POINT?=	/mnt
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

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
CRUNCHCONF=	${COMMONDIR}/${CBIN}.conf
MTREE=		${COMMONDIR}/mtree.conf

.ifdef	ORIGINAL_CODE
DISKTYPE=	floppy3
.else	# PC-98
DISKTYPE=	floppy3ufs

INO_BYTES?=	3045	#4069, 5120, 6144
.endif	# PC-98

all: ${AUXTARGETS} ${CBIN} 
.ifdef	ORIGINAL_CODE
	dd if=/dev/zero of=${IMAGE} count=2880
	vnconfig -t ${DISKTYPE} -v -c ${VND_CDEV} ${IMAGE}
	disklabel -rw ${VND_CDEV} ${DISKTYPE}
	newfs -B le -m 0 -o space -i 5120 -c 80 ${VND_RDEV} ${DISKTYPE}
.else	# PC-98
	dd if=/dev/zero of=${TMP_IMAGE} count=2880
	vnconfig -t ${DISKTYPE} -v -c ${VND_CDEV} ${TMP_IMAGE}
	disklabel -rw ${VND_CDEV} ${DISKTYPE}
	newfs -B le -m 0 -o space -i ${INO_BYTES} -c 80 ${VND_RDEV} ${DISKTYPE}
.endif	# PC-98
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_CDEV}
.ifndef	ORIGINAL_ODE
	mv ${TMP_IMAGE} ${IMAGE}
.endif	# PC-98

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND_DEV}
.ifdef	ORIGINAL_ODE
	-/bin/rm -f ${IMAGE}
.else	# PC-98
	-/bin/rm -f ${TMP_IMAGE} ${IMAGE}
.endif	# PC-98

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${TOP}/../../.. -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

clean cleandir distclean:
.ifdef	ORIGINAL_CODE
	/bin/rm -f ${AUXCLEAN} *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c
.else	# PC-98
	/bin/rm -f ${AUXCLEAN} *.core ${IMAGE} ${TMP_IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c
.endif	# PC-98

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
