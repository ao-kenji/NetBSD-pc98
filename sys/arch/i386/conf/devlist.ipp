#	$NecBSD: devlist.ipp,v 3.29.2.1 1999/12/12 00:36:30 kmatsuda Exp $
#	$NetBSD$
#
# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.
#
#	PnP device list
#
#############################################################
# PNP CBUS BOARD SECTION
#############################################################
BUS		PISA
PROTOCOL	IPPI	IPPI

#############################################################
# scsi (ct)
#############################################################
#	ID		LD	CLASS	DEV
# SMIT LHA-301	
DEVICE	0xb1e70001	0	0	ct

# IODATA SC-98III/Qvision WaveSMIT/TAXAN SC55BX6
DEVICE	0xdc540111	0	0	ct

# MELCO IFC-NN
DEVICE	0xb4ac2041	0	0	ct

#############################################################
# scsi (aic)
#############################################################
# NEC PC-9801-100
DEVICE	0xb8a380a1	0	0	aic
DVCFG	0x10000
# Adaptec AHA-1030P 
DEVICE	0x84903010	0	0	aic
DVCFG	0x10000

#############################################################
# ide
#############################################################
#	ID		LD	CLASS	DEV
# IDE98
DEVICE	0xa5e46001	0	0	wdc

#############################################################
# serial devices
#
DEFINE	DVCFG_SER_GENERIC	0x0
DEFINE	DVCFG_SER_RSA98		0x180000
#############################################################
#	ID		LD	CLASS	DEV
# NEC internal modem
DEVICE	0xb8a38241	0	0	ser
DEVICE	0xbf2f8011	0	0	ser

# RSA98III family
DEVICE	0xa5e40071	0	0	ser
MASKS	0xffffff8f
DVCFG	DVCFG_SER_RSA98

# RSA98III/S
#DEVICE	0xa5e40021	0	0	ser
#DVCFG	DVCFG_SER_RSA98
## RSA98III/SB
#DEVICE	0xa5e40031	0	0	ser
#DVCFG	DVCFG_SER_RSA98
## RSA98III(ch1)
#DEVICE	0xa5e40041	0	0	ser
#DVCFG	DVCFG_SER_RSA98
## RSA98III(ch2)
#DEVICE	0xa5e40051	0	0	ser
#DVCFG	DVCFG_SER_RSA98

##############################################################
# ether adapter(ne)
#############################################################
#	ID		LD	CLASS	DEV
# D-LINK
DEVICE	0x91839183	0	0	ne

# LA-98-25T
DEVICE	0x8689f428	0	0	ne

# corega 98-T
DEVICE	0x8689f454	0	0	ne

# LA-98-T
DEVICE	0x8986f428	0	0	ne

#############################################################
# ether adapter(mbe)
#############################################################
#	ID		LD	CLASS	DEV
# C-NET(98)P2
DEVICE	0x0dee0101	0	0	mbe
DVCFG	0x40000

#############################################################
# ether adapter(sn)
#############################################################
#	ID		LD	CLASS	DEV
# NEC PC-9801-103
DEVICE	0xb8a38051	0	0	sn

# NEC PC-9801-104
DEVICE	0xb8a38061	0	0	sn

#############################################################
# pcm sound devices(pcm)
#############################################################
#	ID		LD	CLASS	DEV
#DEVICE	0xdc540211	0	0	pcm

#############################################################
# game
#############################################################
#	ID		LD	CLASS	DEV
#DEVICE	0xdc540211	0	0	joy

#############################################################
# sound + game
#############################################################
#	ID		LD	CLASS	DEV
DEVICE	0xdc540211	0	0	qws

#############################################################
# pc card adapter
#############################################################
#	ID		LD	CLASS	DEV
DEVICE	0xb8a38121	0	0	gipc

#############################################################
# midi 
#############################################################
#	ID		LD	CLASS	DEV
#Board Vendor ID: RJA0051(0x510041c9)
DEVICE	0xc9410051	0	0	mpu
