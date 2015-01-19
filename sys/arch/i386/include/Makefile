#	$NecBSD: Makefile,v 3.1.4.3 1999/08/28 09:12:47 honda Exp $
#	$NetBSD: Makefile,v 1.6 1999/03/10 08:13:01 mycroft Exp $
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1999
#	NetBSD/pc98 porting staff. All rights reserved.
# 

KDIR=	/sys/arch/i386/include
INCSDIR= /usr/include/i386

INCS=	ansi.h aout_machdep.h apmvar.h asm.h bioscall.h bootinfo.h bswap.h \
	byte_swap.h bus.h cdefs.h conf.h cpu.h cpufunc.h cputypes.h \
	db_machdep.h disklabel.h elf_machdep.h endian.h float.h frame.h \
	freebsd_machdep.h gdt.h ibcs2_machdep.h ieee.h ieeefp.h intr.h \
	joystick.h \
	kcore.h limits.h mouse.h npx.h param.h pcb.h pccons.h pio.h pmap.h \
	pmap.new.h proc.h profile.h psl.h pte.h ptrace.h reg.h segments.h \
	setjmp.h signal.h specialreg.h spkr.h stdarg.h svr4_machdep.h \
	sysarch.h trap.h tss.h types.h varargs.h vm86.h vmparam.h
.ifndef	ORIGINAL_CODE
INCS+=	busdma.h busio.h devlist.h diskutil.h dvcfg.h fdio.h pcvt_ioctl.h \
	physio_proc.h rtcvar.h slot_device.h syspmgr.h systmbusvar.h \
	timervar.h vm86bios.h vpdio.h vscio.h bootflags.h vm_interface.h \
	sysinfo.h
.endif	# PC-98

.include <bsd.kinc.mk>
