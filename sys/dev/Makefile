#	$NecBSD: Makefile,v 1.3.2.1 1999/08/15 01:25:24 kmatsuda Exp $
#	$NetBSD: Makefile,v 1.3 1998/07/28 00:36:31 pk Exp $

SUBDIR=	eisa ata vme ic isa isapnp microcode ofw pci rcons sbus sun tc dec \
	scsipi pcmcia i2c mii ofisa pckbc wscons usb

KDIR=	/sys/dev
INCSDIR= /usr/include/dev

INCS=	auconv.h audio_if.h audiovar.h ccdvar.h clock_subr.h cons.h md.h \
	mulaw.h vndvar.h

.ifndef	ORIGINAL_CODE
INCS+=	packet_midi.h
.endif	# PC-98

.include <bsd.kinc.mk>
