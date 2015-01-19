#	$NetBSD: Makefile,v 1.106 1999/03/27 03:22:49 perry Exp $
#	from: @(#)Makefile	5.20 (Berkeley) 6/12/93
#
# not yet done: catman

# XXX Temporary for NO_SENDMAIL
.include <bsd.own.mk>

.ifdef	ORIGINAL_CODE
SUBDIR=	ac accton amd apm apmd arp bad144 bind bootp \
	chown chroot chrtbl config cron dbsym dev_mkdb \
	dhcp diskpart dumpfs dumplfs edquota eeprom eshconfig greconfig \
	grfconfig grfinfo gspa hilinfo inetd iostat ipf iteconfig kgmon \
	kvm_mkdb lastlogin lpr mailwrapper map-mbone mbrlabel \
	mdconfig mdsetimage \
	mopd mountd mrinfo mrouted mtrace mtree \
	netgroup_mkdb nfsd nfsiod pkg_install portmap pppd pstat \
	pwd_mkdb quot quotacheck quotaon rarpd rbootd rdate \
	repquota rmt rpc.bootparamd rpc.lockd rpc.pcnfsd \
	rpc.statd rpc.yppasswdd rwhod sa screenblank \
	sliplogin slstats spray sup sysctl syslogd tcpdchk \
	tcpdmatch tcpdump timed traceroute trpt trsp update \
	usbd usbdevs videomode vipw vnconfig wsconscfg wsfontload xntp \
	ypbind yppoll ypserv ypset \
	zdump zic
.else
#	kill apm, apmd, chrtbl, mbrlabel
SUBDIR=	ac accton amd arp bad144 bind bootp \
	chown chroot config cron dbsym dev_mkdb \
	dhcp diskpart dumpfs dumplfs edquota eeprom eshconfig greconfig \
	grfconfig grfinfo gspa hilinfo inetd iostat ipf iteconfig kgmon \
	kvm_mkdb lastlogin lpr mailwrapper map-mbone \
	mdconfig mdsetimage \
	mopd mountd mrinfo mrouted mtrace mtree \
	netgroup_mkdb nfsd nfsiod pkg_install portmap pppd pstat \
	pwd_mkdb quot quotacheck quotaon rarpd rbootd rdate \
	repquota rmt rpc.bootparamd rpc.lockd rpc.pcnfsd \
	rpc.statd rpc.yppasswdd rwhod sa screenblank \
	sliplogin slstats spray sup sysctl syslogd tcpdchk \
	tcpdmatch tcpdump timed traceroute trpt trsp update \
	usbd usbdevs videomode vipw vnconfig wsconscfg wsfontload xntp \
	ypbind yppoll ypserv ypset \
	zdump zic
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=	sendmail
.endif

.if defined(BUILD_POSTFIX)
SUBDIR+=	postfix
.endif

.include <bsd.subdir.mk>
