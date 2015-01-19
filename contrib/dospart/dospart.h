/*
 *	NetBSD/pc98領域確保・解放ツール
 *
 *	Copyright (C) by NetBSD/pc98 porting project
 *		written by Yoshio Kimura, 08/30/1995
 */

#define	DOSBBSECTOR		1
#define NDOSPART		16

struct dos_partition {
	unsigned char	dp_mid;
	unsigned char	dp_sid;
	unsigned char	dp_dum1;
	unsigned char	dp_dum2;
	unsigned char	dp_ipl_sct;
	unsigned char	dp_ipl_head;
	unsigned short	dp_ipl_cyl;
	unsigned char	dp_ssect;
	unsigned char	dp_shd;
	unsigned short	dp_scyl;
	unsigned char	dp_esect;
	unsigned char	dp_ehd;
	unsigned short	dp_ecyl;
	unsigned char	dp_name[16];
};

#define	DOS_BOOT		0x80
#define DOS_ACTIVE		0x80
#define DOSMID_386BSD	0x14
#define DOSSID_386BSD	0x44
#define DOSMID_NetBSD	DOSMID_386BSD
#define DOSSID_NetBSD	(DOSSID_386BSD | DOS_ACTIVE)
