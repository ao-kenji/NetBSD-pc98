#	$NecBSD: Makefile,v 1.4.2.1 1999/08/28 04:55:18 honda Exp $
#	$NetBSD: Makefile,v 1.11 1999/03/08 21:44:48 drochner Exp $

S?=	${.CURDIR}/../../../../

LIB= i386
MKPIC=no
MKPROFILE=no

I386_INCLUDE_DISK?= yes
I386_INCLUDE_DOS?= no
.ifdef	ORIGINAL_CODE
I386_INCLUDE_BUS?= no
.else	# PC-98 & debug
I386_INCLUDE_BUS?= no		# yes if bus debug
.endif	# PC-98 & debug

CPPFLAGS= -I$S/lib/libsa ${I386CPPFLAGS} ${I386MISCCPPFLAGS}
.ifdef	ORIGINAL_CODE
#CPPFLAGS+= -DDISK_DEBUG
#CPPFLAGS+= -DNO_DISKLABEL
#CPPFLAGS+= -DSAVE_MEMORY
.else	# !PC-98 (debug)
CPPFLAGS+= -DDISK_DEBUG
#CPPFLAGS+= -DDEBUG
#CPPFLAGS+= -DNO_DISKLABEL
#CPPFLAGS+= -DSAVE_MEMORY
.endif	# !PC-98 (debug)

.ifdef	ORIGINAL_CODE
SRCS= pcio.c conio.S comio.S comio_direct.c
SRCS+= getsecs.c biosgetrtc.S biosdelay.S biosreboot.S gatea20.c
SRCS+= biosmem.S getextmemx.c biosmemx.S
SRCS+= pread.c menuutils.c parseutils.c
SRCS+= bootinfo.c bootinfo_biosgeom.c bootinfo_memmap.c
SRCS+= loadfile.c startprog.S netbsd_opts.c panic.c
.else	# PC-98
#	kill biosdeley.S, because PC-98 does not have bios function for delay
#	add delay.c
#	kill biosreboot.S, add reboot.c
#	add conputc.c
#
#	(REQUIRED)
SRCS= pcio.c conio.S                       
#SRCS+= comio.S		(depend on SUPPORT_SERIAL)
SRCS+= seri_direct.c
#SRCS+= biosgetrtc.S	(biosgetrtc())
#SRCS+= getsecs.c	(getsecs())
#		bcd2dec() -> getsecs() <- biosgetrct()
#		getsecs() -> lib/netif/netif_small.c (depend on USE_NETIF)
#			  -> libsa/tftp.c (depend on USE_NETIF)
#	(REQUIRED)
SRCS+= gatea20.c
SRCS+= biosparam.S
#SRCS+= biosmem.S
SRCS+= bios_mem.c
#SRCS+= getextmemx.c	(depend on !CONSERVATIVE_MEMDETECT)
#SRCS+= biosmemx.S	(depend on !CONSERVATIVE_MEMDETECT)
SRCS+= delay.c reboot.c conputc.c
SRCS+= pread.c menuutils.c parseutils.c
SRCS+= bootinfo.c
SRCS+= bootinfo_biosgeom.c	#(depend on PASS_BIOSGEOM)
SRCS+= bootinfo_bootflags.c
#		bi_getbiosgeom() -> exec_netbsd() in exec.c
#SRCS+= bootinfo_memmap.c	#(depend on PASS_MEMMAP, not yet done)
#		bi_getmemmap() -> exec_netbsd() in exec.c
SRCS+= loadfile.c startprog.S netbsd_opts.c panic.c
.endif	# PC-98
.if (${I386_INCLUDE_DISK} == "yes")
SRCS+= biosdisk.c biosdisk_ll.c bios_disk.S
.endif
.if (${I386_INCLUDE_DOS} == "yes")
SRCS+= dosfile.c dos_file.S
.endif
.if (${I386_INCLUDE_DISK} == "yes") || (${I386_INCLUDE_DOS} == "yes")
SRCS+= diskbuf.c
.endif
.if (${I386_INCLUDE_BUS} == "yes")
SRCS+= biospci.c bios_pci.S isapnp.c isadma.c
.endif

.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `lorder ${OBJS} | tsort`
