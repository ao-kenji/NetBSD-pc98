/*
 *	NetBSD�̈�m�ہE����c�[��
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

	/* �R�}���h���C���I�v�V������DA/UA�֕ϊ����� */
	if (get_daua(argv[1])) {
		printf("�w�肳�ꂽ�f�B�X�N�����݂��܂���B\n");
		exit(1);
	}

	/* �w��f�B�X�N��geometry�����擾���� */
	if (get_disk_params()) {
		printf("�f�B�X�Ngeometry���擾�ł��܂���ł����B\n");
		exit(1);
	}

	/* �c�n�r�p�[�e�B�V�����e�[�u���̓ǂݍ��� */
	if (partition_table(DOSPART_READ)) {
		printf("�c�n�r�p�[�e�B�V�����e�[�u���̓ǂݍ��݂Ɏ��s���܂����B\n");
		exit(1);
	}

	/* ���g�p�̈�̃T�[�` */
	get_free_part();

	/* �R�}���h���[�h */
	command_mode();

	return 0;
}

void command_mode()
{

	printf("dospart Ver 1.00\n");
	printf("help ���� h �Ɠ��͂���ƃR�}���h�̉����\�����܂��B\n\n");

	for (;;) {
		printf("dospart> ");
		gets(buffer);

		/* �R�}���h�̉��� */
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
			printf("�R�}���h�����Ⴂ�܂��B\n");
	}
}

/*
 *	�R�}���h���C���I�v�V��������DA/UA�ւ̕ϊ�
 */
int get_daua(char *drive)
{
	unsigned char far *disk_equips;
	int unit;

	/* �R�}���h���C���I�v�V�����̕��������� */
	if (strlen(drive) != 2)
		usage();

	/* �h���C�u�ԍ��̎擾 */
	unit = atoi(drive + 1);

	/* �f�B�X�N�^�C�v�̌��� */
	if (*drive == 'S' || *drive == 's') {
		if (unit < 0 || unit > 6)
			usage();
		disk_equips = (unsigned char far *)0x482;
		if (!(*disk_equips & (1 << unit)))
			return 1;
		disk_parms.daua = 0xa0 | (unsigned char)unit;
		disk_parms.disk_type = "SCSI�Œ�f�B�X�N";
	} else if (*drive == 'W' || *drive == 'w') {
		if (unit < 0 || unit > 3)
			usage();
		disk_equips = (unsigned char far *)0x55d;
		if (!(*disk_equips & (1 << unit)))
			return 1;
		disk_parms.daua = 0x80;
		disk_parms.disk_type = "�Œ�f�B�X�N";
	} else
		usage();

	return 0;
}

/*
 *	�f�B�X�Ngeometry�̎擾
 */
int get_disk_params()
{
	union REGS regs;

	regs.h.ah = 0x84;							/* �VSENSE */
	regs.h.al = disk_parms.daua;

	/* DISK BIOS�̌Ăяo�� */
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
 *	�c�n�r�p�[�e�B�V�����e�[�u���̓ǂݍ��݁E��������
 */
int partition_table(int op)
{
	union REGS regs;
	struct SREGS sregs;

	regs.h.ah = op ? 0x06 : 0x05;				/* READ or WRITE */
	regs.h.al = disk_parms.daua;
	regs.x.bx = sizeof(dp);						/* �ǂݏ����o�C�g�� */
	regs.x.bp = (unsigned short)&dp[0];			/* �ǂݏ����A�h���X */
	regs.x.cx = 0;								/* �V�����_�ԍ� */
	regs.h.dh = 0;								/* �w�b�h�ԍ� */
	regs.h.dl = 1;								/* �Z�N�^�ԍ� */
	segread(&sregs);							/* ���݂̃Z�O�����g */

	/* DISK BIOS�Ăяo�� */
	int86x(0x1b, &regs, &regs, &sregs);

	if (regs.x.cflag)
		return 1;
	return 0;
}

/*
 *	���g�p�̈�̃T�[�`
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
 *	�c�n�r�p�[�e�B�V�����e�[�u���̃\�[�g
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

	/* ���g�p�̈�̍č\�z */
	get_free_part();
}

/*
 *	�c�n�r�p�[�e�B�V�����e�[�u���̕\��
 */
void print_part_table()
{
	int i;
	int disk_size;

	printf("\n%s #%1d\n", disk_parms.disk_type, disk_parms.daua & 0x0f);
	printf("    geometry: ");
	printf("%d MB, %d cyls, ", disk_parms.disk_size, disk_parms.cylinders);
	printf("%d heads, %d sectors\n\n", disk_parms.heads, disk_parms.sectors);

	printf("                                        �V�����_\n");
	printf("��     �V�X�e����         ��  ��      �J�n    �I��  �T�C�Y");
	printf("  �u�[�g\n");
	for (i = 0; i < NDOSPART; i++) {
		if (!dp[i].dp_mid && !dp[i].dp_sid) break;
		strncpy(buffer, dp[i].dp_name, 16);
		buffer[16] = 0;
		printf("%2d  %16s    ", i + 1, buffer);
		if (dp[i].dp_sid & DOS_ACTIVE)
			printf("�A�N�e�B�u    ");
		else
			printf(" �X���[�v     ");
		printf("%4d �` %4d   ", dp[i].dp_scyl, dp[i].dp_ecyl);
		disk_size = (long)((dp[i].dp_ecyl - dp[i].dp_scyl + 1) * 
					        disk_parms.sectpercyl + 1024) / 2048;
		printf("%4d    ", disk_size);
		if (dp[i].dp_mid & DOS_BOOT)
			printf(" �� \n");
		else
			printf("�s��\n");
	}

	/* ���g�p�̈�̕\�� */
	for (i = 0; i < freedp.count; i++) {
		printf("    ���g�p�̗̈�                      ");
		printf("%4d �` %4d   ", freedp.scyl[i], freedp.ecyl[i]);
		printf("%4d\n", freedp.dp_size[i]);
	}
	printf("\n");
}

/*
 *	NetBSD/pc98�p�[�e�B�V�����̊m��
 */
void set_netbsd_part()
{
	unsigned char boot, active;
	unsigned short scyl, ecyl;
	int i, j, part_size;

	/* �m�ۗe�ʂ̐ݒ� */
	do {
		printf("\nNetBSD/pc98 �̈�̊m�ۗe��(MB) > ");
		gets(buffer);
		part_size = atoi(buffer);
	} while (!part_size);

	/* �J�n�V�����_�ƏI���V�����_�̐ݒ� */
	for (i = 0; i < freedp.count; i++)
		if (freedp.dp_size[i] >= part_size) break;
	if (freedp.count == i)
		printf("�w�肳�ꂽ�e�ʂ��m�ۂł��邾���̗̈悪����܂���B\n");

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

	/* �u�[�g�E�s�̐ݒ� */
	if (!answer("�m�ۗ̈悩��u�[�g�\�Ƃ��܂���"))
				boot |= DOS_BOOT;
	else
				boot &= ~DOS_BOOT;

	/* �ݒ�̕\���Ɗm�F */
	printf("\n                                        �V�����_\n");
	printf("   �V�X�e����         ��  ��      �J�n    �I��  �T�C�Y");
	printf("  �u�[�g\n");
	printf("NetBSD/pc98         �A�N�e�B�u    ");
	printf("%4d �` %4d   ", scyl, ecyl);
	part_size = (long)((ecyl - scyl + 1) * disk_parms.sectpercyl + 1024)
				/ 2048;
	printf("%4d    ", part_size);
	if (boot & DOS_BOOT)
		printf(" �� \n\n");
	else
		printf("�s��\n\n");
	if (!answer("�̈�̐ݒ�͏�L�̒ʂ�ł��B��낵���ł���")) {
		dp[j].dp_mid = boot;
		dp[j].dp_sid = active;
		dp[j].dp_ipl_cyl = dp[j].dp_scyl = scyl;
		dp[j].dp_ecyl = ecyl;
		strncpy(dp[j].dp_name, "NetBSD/pc98     ", 16);
		sort_part_table();
		printf("write �R�}���h�Ō��ʂ𔽉f�����Ă��������B\n");
	} else
		printf("�̈�m�ۂ����~�߂܂����B\n");
}

/*
 *	NetBSD/pc98�p�[�e�B�V�����̉��
 */
void release_netbsd_part()
{
	int i, j;

	if (!answer("NetBSD/pc98 �̈��������Ă�낵���ł���")) {
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
		printf("write �R�}���h�Ō��ʂ𔽉f�����Ă��������B\n");
	} else
		printf("��������~�߂܂����B\n");
}

/*
 *	����̕\��
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
 *	�R�}���h����̕\��
 */
void help()
{
	printf("\n�g�p�\�ȃR�}���h�́A�ȉ��̒ʂ�ł��B\n");
	printf("      list or l     �c�n�r�p�[�e�B�V�����e�[�u���̕\��\n");
	printf("      set or s      NetBSD/pc98 �̈�̊m��\n");
	printf("      release or r  NetBSD/pc98 �̈�̉��\n");
	printf("      write or w    �c�n�r�p�[�e�B�V�����e�[�u���̏�������\n");
	printf("      quit or q     dospart �̏I��\n");
	printf("      help or h     �R�}���h����̕\��\n");
	printf("���ӁFset ��release �ŗ̈�̊m�ہE������s������ɕK���Awrite\n");
	printf("      �R�}���h�����s���Ăc�n�r�p�[�e�B�V�����e�[�u����������\n");
	printf("      ���Ă��������B\n\n");
}

int usage()
{
	printf("usage: dospart <drive>\n");
	printf("       <drive> �́A\n");
	printf("           SCSI�n�[�h�f�B�X�N�̏ꍇ�As0 �` s6");
	printf("�i������SCSI ID �ɑΉ��j\n");
	printf("           IDE �n�[�h�f�B�X�N�̏ꍇ�Aw0 �` w3\n");
	printf("       �Ǝw�肵�Ă��������B�i�ȗ��s�I�j\n");
	exit(1);
}
