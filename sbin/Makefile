#	$NetBSD: Makefile,v 1.56 1999/03/25 04:08:29 explorer Exp $
#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck ncheck

.ifdef	ORIGINAL_CODE
SUBDIR=	atactl badsect bim ccdconfig disklabel dmesg edlabel fastboot fdisk \
	fsck fsirand ifconfig init ldconfig lmcctl mknod modload modunload \
	mount nologin ping reboot raidctl rndctl route routed savecore \
	scsictl shutdown slattach swapctl ttyflags umount wsconsctl
.else	# PC-98	XXX: kill: atactl
SUBDIR=	       badsect bim ccdconfig disklabel dmesg edlabel fastboot fdisk \
	fsck fsirand ifconfig init ldconfig lmcctl mknod modload modunload \
	mount nologin ping reboot raidctl rndctl route routed savecore \
	scsictl shutdown slattach swapctl ttyflags umount wsconsctl
.endif	# PC-98

# support for various file systems
SUBDIR+= mount_ados
SUBDIR+= mount_cd9660
SUBDIR+= mount_ext2fs	fsck_ext2fs
SUBDIR+= mount_fdesc
SUBDIR+= mount_filecore
SUBDIR+= mount_ffs	newfs fsck_ffs fsdb dump restore clri tunefs
SUBDIR+= mount_kernfs
SUBDIR+= mount_lfs	newfs_lfs fsck_lfs
# mount_mfs -> newfs
.ifdef	ORIGINAL_CODE
SUBDIR+= mount_msdos	newfs_msdos fsck_msdos
.else	# PC-98	XXX: temporary, until we will update fsck_msdos, newfs_msdos.
SUBDIR+= mount_msdos
.endif	# PC-98
SUBDIR+= mount_nfs
SUBDIR+= mount_null
SUBDIR+= mount_portal
SUBDIR+= mount_procfs
SUBDIR+= mount_umap
SUBDIR+= mount_union

.include <bsd.subdir.mk>
