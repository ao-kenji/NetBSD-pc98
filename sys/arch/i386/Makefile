#	$NecBSD: Makefile,v 1.17 1999/08/05 08:52:21 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.16 1998/11/15 20:36:30 perry Exp $
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1996, 1997, 1998, 1999
#	NetBSD/pc98 porting staff. All rights reserved.
# 

# Makefile for i386 tags file and boot blocks

SYSDIR?=	/sys

TI386=	${SYSDIR}/arch/i386/tags
.ifdef	ORIGINAL_CODE
SI386=	${SYSDIR}/arch/i386/i386/*.[ch] ${SYSDIR}/arch/i386/include/*.h \
	${SYSDIR}/arch/i386/eisa/*.[ch] ${SYSDIR}/arch/i386/isa/*.[ch] \
	${SYSDIR}/arch/i386/pci/*.[ch]
AI386=	${SYSDIR}/arch/i386/i386/*.s ${SYSDIR}/arch/i386/isa/*.s
.else	# PC-98
SI386=	${SYSDIR}/arch/i386/i386/*.[ch] ${SYSDIR}/arch/i386/include/*.h \
	${SYSDIR}/arch/i386/eisa/*.[ch] ${SYSDIR}/arch/i386/isa/*.[ch] \
	${SYSDIR}/arch/i386/pci/*.[ch] \
	${SYSDIR}/arch/i386/Cbus/*.[ch] \
	${SYSDIR}/arch/i386/Cbus/dev/*.[ch] \
	${SYSDIR}/arch/i386/Cbus/dev/atapi/*.[ch] \
	${SYSDIR}/arch/i386/Cbus/dev/ct/*.[ch] \
	${SYSDIR}/arch/i386/Cbus/dev/scsi-atapi/*.[ch] \
	${SYSDIR}/arch/i386/Cbus/dev/serial/*.[ch] \
	${SYSDIR}/arch/i386/Cbus/pccs16/*.[ch] \
	${SYSDIR}/arch/i386/Cbus/pnp/*.[ch] \
	${SYSDIR}/arch/i386/compat/dev/*.[ch] \
	${SYSDIR}/arch/i386/isa/vsc/*.[ch] \
	${SYSDIR}/arch/i386/pccs/*.[ch] \
	${SYSDIR}/arch/i386/systm/*.[ch]
AI386=	${SYSDIR}/arch/i386/i386/*.s ${SYSDIR}/arch/i386/isa/*.s \
	${SYSDIR}/arch/i386/systm/*.s
.endif	# PC-98

# Directories in which to place tags links
.ifdef	ORIGINAL_CODE
DI386=	i386 eisa isa include pci
.else	# PC-98
DI386=	i386 isa include pccs pci systm
.endif	# PC-98

.include "../../kern/Make.tags.inc"

tags:
	-ctags -wdtf ${TI386} ${SI386} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AI386} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TI386}
	sort -o ${TI386} ${TI386}

links:
	-for i in ${DI386}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done


SUBDIR=	include stand isa/pcvt
.ifndef	ORIGINAL_CODE
SUBDIR+=	isa/vsc util
.endif	# PC-98

.include <bsd.subdir.mk>
