/*
 *	NetBSD領域確保・解放ツール
 *
 *	Copyright (C) by NetBSD/pc98 porting project
 *		written by Yoshio Kimura, 08/27/1995
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <dos.h>
#include "dospart.h"

#define DOSPART_READ	1
#define DOSPART_WRITE	0

struct dos_partition dp[NDOSPART];

struct free_area {
	int	count;
	unsigned short	scyl[NDOSPART];
	unsigned short	ecyl[NDOSPART];
	int dp_size[NDOSPART];
} freedp;

struct disk_geometry {
	unsigned char	*disk_type;
	unsigned char	daua;
	unsigned short	cylinders;
	unsigned char	heads;
	unsigned char	sectors;
	unsigned short	sectpercyl;
	int	disk_size;
} disk_parms;

char buffer[80];

void command_mode(void);
int get_daua(char *);
int	get_disk_params(void);
int partition_table(int);
void get_free_part(void);
void sort_part_table(void);
void print_part_table(void);
void set_netbsd_part(void);
void release_netbsd_part(void);
int answer(char *);
void help(void);
int usage(void);

int main(int argc, char *argv[])
{

	if (argc != 2)
		usage();

	/* コマンドラインオプションをDA/UAへ変換する */
	if (get_daua(argv[1])) {
		printf("指定されたディスクが存在しません。\n");
		exit(1);
	}

	/* 指定ディスクのgeometry情報を取得する */
	if (get_disk_params()) {
		printf("ディスクgeometryを取得できませんでした。\n");
		exit(1);
	}

	/* ＤＯＳパーティションテーブルの読み込み */
	if (partition_table(DOSPART_READ)) {
		printf("ＤＯＳパーティションテーブルの読み込みに失敗しました。\n");
		exit(1);
	}

	/* 未使用領域のサーチ */
	get_free_part();

	/* コマンドモード */
	command_mode();

	return 0;
}

void command_mode()
{

	printf("dospart Ver 1.00\n");
	printf("help 又は h と入力するとコマンドの解説を表示します。\n\n");

	for (;;) {
		printf("dospart> ");
		gets(buffer);

		/* コマンドの解釈 */
		if (!strcmpi(buffer, "list") || !strcmpi(buffer, "l"))
			print_part_table();
		else if (!strcmpi(buffer, "help") || !strcmpi(buffer, "h"))
			help();
		else if (!strcmpi(buffer, "set") || !strcmpi(buffer, "s"))
			set_netbsd_part();
		else if (!strcmpi(buffer, "release") || !strcmpi(buffer, "r"))
			release_netbsd_part();
		else if (!strcmpi(buffer, "write") || !strcmpi(buffer, "w"))
			partition_table(DOSPART_WRITE);
		else if (!strcmpi(buffer, "quit") || !strcmpi(buffer, "q"))
			return;
		else
			printf("コマンド名が違います。\n");
	}
}

/*
 *	コマンドラインオプションからDA/UAへの変換
 */
int get_daua(char *drive)
{
	unsigned char far *disk_equips;
	int unit;

	/* コマンドラインオプションの文字数検査 */
	if (strlen(drive) != 2)
		usage();

	/* ドライブ番号の取得 */
	unit = atoi(drive + 1);

	/* ディスクタイプの検査 */
	if (*drive == 'S' || *drive == 's') {
		if (unit < 0 || unit > 6)
			usage();
		disk_equips = (unsigned char far *)0x482;
		if (!(*disk_equips & (1 << unit)))
			return 1;
		disk_parms.daua = 0xa0 | (unsigned char)unit;
		disk_parms.disk_type = "SCSI固定ディスク";
	} else if (*drive == 'W' || *drive == 'w') {
		if (unit < 0 || unit > 3)
			usage();
		disk_equips = (unsigned char far *)0x55d;
		if (!(*disk_equips & (1 << unit)))
			return 1;
		disk_parms.daua = 0x80;
		disk_parms.disk_type = "固定ディスク";
	} else
		usage();

	return 0;
}

/*
 *	ディスクgeometryの取得
 */
int get_disk_params()
{
	union REGS regs;

	regs.h.ah = 0x84;							/* 新SENSE */
	regs.h.al = disk_parms.daua;

	/* DISK BIOSの呼び出し */
	int86(0x1b, &regs, &regs);

	if (regs.x.cflag)
		return 1;

	disk_parms.cylinders = regs.x.cx + 1;
	disk_parms.heads = regs.h.dh;
	disk_parms.sectors = regs.h.dl;
	disk_parms.sectpercyl = disk_parms.heads * disk_parms.sectors;
	disk_parms.disk_size = (long)(disk_parms.cylinders *
								   disk_parms.sectpercyl + 1024) / (1024 * 2);

	return 0;
}

/*
 *	ＤＯＳパーティションテーブルの読み込み・書き込み
 */
int partition_table(int op)
{
	union REGS regs;
	struct SREGS sregs;

	regs.h.ah = op ? 0x06 : 0x05;				/* READ or WRITE */
	regs.h.al = disk_parms.daua;
	regs.x.bx = sizeof(dp);						/* 読み書きバイト数 */
	regs.x.bp = (unsigned short)&dp[0];			/* 読み書きアドレス */
	regs.x.cx = 0;								/* シリンダ番号 */
	regs.h.dh = 0;								/* ヘッド番号 */
	regs.h.dl = 1;								/* セクタ番号 */
	segread(&sregs);							/* 現在のセグメント */

	/* DISK BIOS呼び出し */
	int86x(0x1b, &regs, &regs, &sregs);

	if (regs.x.cflag)
		return 1;
	return 0;
}

/*
 *	未使用領域のサーチ
 */
void get_free_part()
{
	int i, c = 0;

	for (i = 0; i < NDOSPART; i++) {
		if (dp[i].dp_scyl == 0 && dp[i].dp_ecyl == 0) {
			if (i && dp[i - 1].dp_ecyl != 0)
				freedp.scyl[c] = dp[i - 1].dp_ecyl + 1;
			else
				freedp.scyl[c] = 1;
			freedp.ecyl[c] = disk_parms.cylinders - 1;
		} else if (dp[i].dp_scyl != dp[i - 1].dp_ecyl + 1) {
			freedp.scyl[c] = dp[i - 1].dp_ecyl + 1;
			freedp.ecyl[c] = dp[i].dp_scyl - 1;
		} else
			continue;
		freedp.dp_size[c] = (long)((freedp.ecyl[c] - freedp.scyl[c] + 1) *
							disk_parms.sectpercyl + 1024) / (1024 * 2);
		if (freedp.dp_size[c] == 0) {
			freedp.scyl[c] = freedp.ecyl[c] = 0;
			break;
		}
		if (freedp.ecyl[c] == disk_parms.cylinders - 1) {
			c++;
			break;
		}
		c++;
	}
	freedp.count = c;
}

/*
 *	ＤＯＳパーティションテーブルのソート
 */
void sort_part_table()
{
	int i;
	struct dos_partition dosdp;

	for (i = 1; i < NDOSPART; i++) {
		if (dp[i].dp_scyl != 0 && (dp[i].dp_scyl < dp[i - 1].dp_scyl ||
			dp[i - 1].dp_scyl == 0)) {
			dosdp = dp[i - 1];
			dp[i - 1] = dp[i];
			dp[i] = dosdp;
		}
	}

	/* 未使用領域の再構築 */
	get_free_part();
}

/*
 *	ＤＯＳパーティションテーブルの表示
 */
void print_part_table()
{
	int i;
	int disk_size;

	printf("\n%s #%1d\n", disk_parms.disk_type, disk_parms.daua & 0x0f);
	printf("    geometry: ");
	printf("%d MB, %d cyls, ", disk_parms.disk_size, disk_parms.cylinders);
	printf("%d heads, %d sectors\n\n", disk_parms.heads, disk_parms.sectors);

	printf("                                        シリンダ\n");
	printf("№     システム名         状  態      開始    終了  サイズ");
	printf("  ブート\n");
	for (i = 0; i < NDOSPART; i++) {
		if (!dp[i].dp_mid && !dp[i].dp_sid) break;
		strncpy(buffer, dp[i].dp_name, 16);
		buffer[16] = 0;
		printf("%2d  %16s    ", i + 1, buffer);
		if (dp[i].dp_sid & DOS_ACTIVE)
			printf("アクティブ    ");
		else
			printf(" スリープ     ");
		printf("%4d ～ %4d   ", dp[i].dp_scyl, dp[i].dp_ecyl);
		disk_size = (long)((dp[i].dp_ecyl - dp[i].dp_scyl + 1) * 
					        disk_parms.sectpercyl + 1024) / 2048;
		printf("%4d    ", disk_size);
		if (dp[i].dp_mid & DOS_BOOT)
			printf(" 可 \n");
		else
			printf("不可\n");
	}

	/* 未使用領域の表示 */
	for (i = 0; i < freedp.count; i++) {
		printf("    未使用の領域                      ");
		printf("%4d ～ %4d   ", freedp.scyl[i], freedp.ecyl[i]);
		printf("%4d\n", freedp.dp_size[i]);
	}
	printf("\n");
}

/*
 *	NetBSD/pc98パーティションの確保
 */
void set_netbsd_part()
{
	unsigned char boot, active;
	unsigned short scyl, ecyl;
	int i, j, part_size;

	/* 確保容量の設定 */
	do {
		printf("\nNetBSD/pc98 領域の確保容量(MB) > ");
		gets(buffer);
		part_size = atoi(buffer);
	} while (!part_size);

	/* 開始シリンダと終了シリンダの設定 */
	for (i = 0; i < freedp.count; i++)
		if (freedp.dp_size[i] >= part_size) break;
	if (freedp.count == i)
		printf("指定された容量を確保できるだけの領域がありません。\n");

	for (j = 0; j < NDOSPART; j++)
		if (!dp[j].dp_mid && !dp[j].dp_sid) break;

	boot = DOSMID_NetBSD;
	active = DOSSID_NetBSD;
	scyl = freedp.scyl[i];
	if (freedp.dp_size[i] == part_size)
		ecyl = freedp.ecyl[i];
	else {
		ecyl = scyl + (long)(part_size * 2048 + disk_parms.sectpercyl / 2) /
					disk_parms.sectpercyl - 1;
	}
	if (ecyl > disk_parms.cylinders - 1)
		ecyl = disk_parms.cylinders - 1;

	/* ブート可・不可の設定 */
	if (!answer("確保領域からブート可能としますか"))
				boot |= DOS_BOOT;
	else
				boot &= ~DOS_BOOT;

	/* 設定の表示と確認 */
	printf("\n                                        シリンダ\n");
	printf("   システム名         状  態      開始    終了  サイズ");
	printf("  ブート\n");
	printf("NetBSD/pc98         アクティブ    ");
	printf("%4d ～ %4d   ", scyl, ecyl);
	part_size = (long)((ecyl - scyl + 1) * disk_parms.sectpercyl + 1024)
				/ 2048;
	printf("%4d    ", part_size);
	if (boot & DOS_BOOT)
		printf(" 可 \n\n");
	else
		printf("不可\n\n");
	if (!answer("領域の設定は上記の通りです。よろしいですか")) {
		dp[j].dp_mid = boot;
		dp[j].dp_sid = active;
		dp[j].dp_ipl_cyl = dp[j].dp_scyl = scyl;
		dp[j].dp_ecyl = ecyl;
		strncpy(dp[j].dp_name, "NetBSD/pc98     ", 16);
		sort_part_table();
		printf("write コマンドで結果を反映させてください。\n");
	} else
		printf("領域確保を取り止めました。\n");
}

/*
 *	NetBSD/pc98パーティションの解放
 */
void release_netbsd_part()
{
	int i, j;

	if (!answer("NetBSD/pc98 領域を解放してよろしいですか")) {
		for (i = 0; i < NDOSPART; i++) {
			if ((dp[i].dp_mid & ~DOS_BOOT) == DOSMID_NetBSD &&
				 dp[i].dp_sid == DOSSID_NetBSD) {
				dp[i].dp_mid = dp[i].dp_sid = 0;
				dp[i].dp_ipl_cyl = dp[i].dp_scyl = dp[i].dp_ecyl = 0;
				for (j = 0; j < 16; j++)
					dp[i].dp_name[j] = 0;
			}
		}
		sort_part_table();
		printf("write コマンドで結果を反映させてください。\n");
	} else
		printf("解放を取り止めました。\n");
}

/*
 *	質問の表示
 */
int answer(char *msg)
{

	while (1) {
		printf("%s(y/n) ", msg);
		gets(buffer);

		if (strlen(buffer) == 1) {
			switch(buffer[0]) {
			case 'Y':
			case 'y':
				return 0;
				break;
			case 'N':
			case 'n':
				return 1;
				break;
			default:
				break;
			}
		}
	}
}
/*
 *	コマンド解説の表示
 */
void help()
{
	printf("\n使用可能なコマンドは、以下の通りです。\n");
	printf("      list or l     ＤＯＳパーティションテーブルの表示\n");
	printf("      set or s      NetBSD/pc98 領域の確保\n");
	printf("      release or r  NetBSD/pc98 領域の解放\n");
	printf("      write or w    ＤＯＳパーティションテーブルの書き込み\n");
	printf("      quit or q     dospart の終了\n");
	printf("      help or h     コマンド解説の表示\n");
	printf("注意：set やrelease で領域の確保・解放を行った後に必ず、write\n");
	printf("      コマンドを実行してＤＯＳパーティションテーブルを書き替\n");
	printf("      えてください。\n\n");
}

int usage()
{
	printf("usage: dospart <drive>\n");
	printf("       <drive> は、\n");
	printf("           SCSIハードディスクの場合、s0 ～ s6");
	printf("（数字はSCSI ID に対応）\n");
	printf("           IDE ハードディスクの場合、w0 ～ w3\n");
	printf("       と指定してください。（省略不可！）\n");
	exit(1);
}
