#	$NecBSD: Makefile,v 1.7.16.1 1999/08/15 01:11:15 kmatsuda Exp $
#	$NetBSD$
#
#
#       Makefile for Mtools
#
# check the Configure file for some examples of device-specific setups
# Berkeley flavors of Unix should include -DBSD in the CFLAGS.  Pick
# a lock method... either -DLOCKF, -DFLOCK, or -DFCNTL and put that
# string in the CFLAGS line below.

CFLAGS	= -O2 -fomit-frame-pointer -DBSD -DMULTISECTORSIZE -DPC98 -DSAFEABORT -DRELYTABLE -DSMARTNAME
BINDIR	= /usr/local/mtools
NOMAN	= YES
#MANDIR	= ${DESTDIR}/usr/local/man/cat	# XXX: don't delete `cat'

PROGS	= mattrib mcd mcopy mdel mdir mformat mlabel mmd mrd mread mren \
	mtype mwrite mkmanifest

SRCS	= buf_read.c buf_write.c devices.c dir_make.c dir_read.c \
	dir_write.c expand.c fat_free.c fat_read.c fat_write.c file_read.c \
	file_write.c init.c is_dir.c match.c mattrib.c mcd.c mcopy.c mdel.c \
	mdir.c mformat.c mkmanifest.c mlabel.c mmd.c mrd.c mread.c mren.c \
	mtype.c mwrite.c parse.c subdir.c

OBJS	= buf_read.o buf_write.o devices.o dir_make.o dir_read.o \
	dir_write.o expand.o fat_free.o fat_read.o fat_write.o file_read.o \
	file_write.o init.o is_dir.o match.o mattrib.o mcd.o mcopy.o mdel.o \
	mdir.o mformat.o mkmanifest.o mlabel.o mmd.o mrd.o mread.o mren.o \
	mtype.o mwrite.o parse.o subdir.o

MAN	= mattrib.1 mcd.1 mcopy.1 mdel.1 mdir.1 mformat.1 mkmanifest.1 \
	mlabel.1 mmd.1 mrd.1 mread.1 mren.1 mtools.1 mtype.1 mwrite.1

MATTRIB	= mattrib.o buf_read.o buf_write.o devices.o dir_read.o \
	dir_write.o expand.o fat_read.o init.o match.o parse.o subdir.o
MCD	= mcd.o buf_read.o devices.o dir_read.o expand.o fat_read.o \
	init.o is_dir.o match.o parse.o subdir.o
MCOPY	= mcopy.o
MDEL	= mdel.o buf_read.o buf_write.o devices.o dir_read.o dir_write.o \
	expand.o fat_free.o fat_read.o fat_write.o init.o match.o parse.o \
	subdir.o 
MDIR	= mdir.o buf_read.o devices.o dir_read.o expand.o fat_read.o \
	init.o is_dir.o match.o parse.o subdir.o
MFORMAT	= mformat.o devices.o dir_write.o expand.o
MLABEL	= mlabel.o buf_read.o buf_write.o devices.o dir_read.o \
	dir_write.o expand.o fat_read.o init.o parse.o
MMD	= mmd.o buf_read.o buf_write.o devices.o dir_make.o dir_read.o \
	dir_write.o expand.o fat_read.o fat_write.o init.o match.o parse.o \
	subdir.o
MRD	= mrd.o buf_read.o buf_write.o devices.o dir_read.o dir_write.o \
	expand.o fat_free.o fat_read.o fat_write.o init.o match.o parse.o \
	subdir.o
MREAD	= mread.o buf_read.o devices.o dir_read.o expand.o fat_read.o \
	file_read.o init.o match.o parse.o subdir.o
MREN	= mren.o buf_read.o buf_write.o devices.o dir_read.o dir_write.o \
	expand.o fat_read.o init.o match.o parse.o subdir.o
MTYPE	= mtype.o buf_read.o devices.o dir_read.o expand.o fat_read.o \
	file_read.o init.o match.o parse.o subdir.o
MWRITE	= mwrite.o buf_read.o buf_write.o devices.o dir_make.o \
	dir_read.o dir_write.o expand.o fat_free.o fat_read.o fat_write.o \
	file_write.o init.o is_dir.o match.o parse.o subdir.o
MKMANIFEST = mkmanifest.o

all:	$(PROGS)

mattrib: $(MATTRIB)
	cc $(LDFLAGS) $(MATTRIB) $(SHOBJ) -o mattrib $(SHLIB)

mcd:	$(MCD)
	cc $(LDFLAGS) $(MCD) $(SHOBJ) -o mcd $(SHLIB)

mcopy:	$(MCOPY)
	cc $(LDFLAGS) $(MCOPY) $(SHOBJ) -o mcopy $(SHLIB)

mdel:	$(MDEL)
	cc $(LDFLAGS) $(MDEL) $(SHOBJ) -o mdel $(SHLIB)

mdir:	$(MDIR)
	cc $(LDFLAGS) $(MDIR) $(SHOBJ) -o mdir $(SHLIB)

mformat: $(MFORMAT)
	cc $(LDFLAGS) $(MFORMAT) $(SHOBJ) -o mformat $(SHLIB)

mlabel:	$(MLABEL)
	cc $(LDFLAGS) $(MLABEL) $(SHOBJ) -o mlabel $(SHLIB)

mmd:	$(MMD)
	cc $(LDFLAGS) $(MMD) $(SHOBJ) -o mmd $(SHLIB)

mrd:	$(MRD)
	cc $(LDFLAGS) $(MRD) $(SHOBJ) -o mrd $(SHLIB)

mread:	$(MREAD)
	cc $(LDFLAGS) $(MREAD) $(SHOBJ) -o mread $(SHLIB)

mren:	$(MREN)
	cc $(LDFLAGS) $(MREN) $(SHOBJ) -o mren $(SHLIB)

mtype:	$(MTYPE)
	cc $(LDFLAGS) $(MTYPE) $(SHOBJ) -o mtype $(SHLIB)

mwrite:	$(MWRITE)
	cc $(LDFLAGS) $(MWRITE) $(SHOBJ) -o mwrite $(SHLIB)

mkmanifest:	$(MKMANIFEST)
	cc $(LDFLAGS) $(MKMANIFEST) $(SHOBJ) -o mkmanifest $(SHLIB)

beforeinstall: 
	-install -d -m 755 -o root -g wheel ${DESTDIR}$(BINDIR)

install:	$(PROGS)
	-install -d -m 755 -o root -g wheel ${DESTDIR}$(BINDIR)
	-install -s -c -o bin -g bin -m 755 $(PROGS) ${DESTDIR}$(BINDIR)

CLEANFILES= $(PROGS) $(OBJS)
.include <bsd.prog.mk>
