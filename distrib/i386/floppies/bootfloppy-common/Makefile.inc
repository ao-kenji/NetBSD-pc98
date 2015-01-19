#	$NecBSD: Makefile.inc,v 1.2 1999/08/07 11:11:12 kmatsuda Exp $
#	$NetBSD: Makefile.inc,v 1.18 1999/03/23 18:05:46 drochner Exp $

# TOP is assumed to be defined by Makefile including this one.

COMMONDIR=	${TOP}/bootfloppy-common

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
KERN?=		netbsd-xxx
MDEC=		${DESTDIR}/usr/mdec
STRIP?=		strip

LISTS=		${COMMONDIR}/list
RAMDISK	!=cd $(.CURDIR)/../ramdisk/; \
        printf "xxx: .MAKE\n\t@echo \$${.OBJDIR}/ramdisk.fs\n" | \
	${MAKE} -s -f-

.ifdef	ORIGINAL_CODE
DISKTYPE?=	floppy3
.else	# PC-98
DISKTYPE?=	floppy3ufs
.endif	# PC-98
DISKSIZE?=	2880

# Some reasonable values for the -i parameter to newfs are:
#
#   6144	1147k, 189 inodes free
#  16384	1159k,  93 inodes free
# on 1.44M:
# 204800	1407k,  27 inodes free
# (with a 4k blocksize, one cannot get fewer than ~32 inodes allocated)

INO_BYTES?=	204800

CLEANFILES+=	netbsd.ram.gz
.ifndef	ORIGINAL_CODE
CLEANFILES+=	${TMP_IMAGE}
.endif	# PC-98

all:	netbsd.ram.gz
.ifdef	ORIGINAL_CODE
	dd if=/dev/zero of=${IMAGE} count=${DISKSIZE}
	vnconfig -t ${DISKTYPE} -v -c ${VND_CDEV} ${IMAGE}
	disklabel -rw ${VND_CDEV} ${DISKTYPE}
.else	# PC-98
	dd if=/dev/zero of=${TMP_IMAGE} count=${DISKSIZE}
	vnconfig -t ${DISKTYPE} -v -c ${VND_CDEV} ${TMP_IMAGE}
	echo y | disklabel -rw ${VND_CDEV} ${DISKTYPE}
.endif	# PC-98
	newfs -B le -m 0 -o space -i ${INO_BYTES} -c 80 ${VND_RDEV} ${DISKTYPE}
.ifdef	ORIGINAL_CODE
	${MDEC}/installboot -v -f ${MDEC}/biosboot.sym ${VND_RDEV}
.else	# PC-98
	${MDEC}/installboot -v    ${MDEC}/biosboot.sym ${VND_RDEV}
.endif	# PC-98
	mount ${VND_DEV} ${MOUNT_POINT}
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	@echo "installing new bootblocks"
	-rm -f ${MOUNT_POINT}/boot
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

netbsd.ram.gz: ${KERN} ${RAMDISK}
	cp ${KERN} netbsd.tmp
	${DESTDIR}/usr/sbin/mdsetimage -v netbsd.tmp ${RAMDISK}
	${STRIP} netbsd.tmp
	gzip -9 netbsd.tmp
	mv netbsd.tmp.gz ${.TARGET}

clean cleandir distclean:
	/bin/rm -f *.core ${IMAGE} ${CLEANFILES}

.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
