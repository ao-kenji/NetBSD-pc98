#	$NecBSD: Makefile.inc,v 1.14.2.1 1999/08/28 02:23:30 honda Exp $
#	$NetBSD: Makefile.inc,v 1.12 1999/02/05 03:01:52 cjs Exp $
#
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets
#

#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1995, 1996, 1997, 1998, 1999
#	NetBSD/pc98 porting staff. All rights reserved.
#

# Set this for any kernels beyond GENERIC you want to include
# in the distribution.
#EXTRA_KERNELS+=
.ifndef	ORIGINAL_CODE

LOCALTIME=	Japan		# override LOCALTIME defined in ../Makefile

.ifdef DESTDIR
# XXX: use etc.${MACHINE}/rc.local
# XXX: should be -rw-r--r--
# -rw-------
BIN3+=	etc.${MACHINE}/rc.local

# XXX: fix rc.local -rw-rw-r--
snap_md_pre: mtree_special clean_up_and_reinstall_config

mtree_special:
	-mtree -def mtree/special -p ${DESTDIR}/ -u

clean_up_and_reinstall_config:
.for i in pcmcia ipp
.if exists(${DESTDIR}/etc/${i}.conf)
	@rm ${DESTDIR}/etc/${i}.conf
.endif
.endfor	
	(cd ${.CURDIR}/../sys/arch/i386/util/sbin/pcsd && \
	 ${MAKE} install)
	(cd ${.CURDIR}/../sys/arch/i386/util/sbin/ippctrl && \
	 ${MAKE} install)

.endif	# DESTDIR check
.endif	# PC-98
