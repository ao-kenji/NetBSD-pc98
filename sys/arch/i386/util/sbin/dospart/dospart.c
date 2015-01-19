/*	$NecBSD: dospart.c,v 1.3 1998/03/14 07:11:05 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <paths.h>

struct dos_partition dos_partitions[NDOSPART];
char part_table[DEV_BSIZE];
char *dkname;
char *specname;
char namebuf[DEV_BSIZE];
char dp_netbsd[] = "NetBSD/pc98     ";
char dp_msdos[] = "MS-DOS 5.00     ";
int mes = 0;

int usage __P((void));
int Perror __P((char *));
int display_dospart __P((int));
void select_dospart __P((int, int, int));
int main __P((int, char *[]));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	int fp, ch, maxpart, flag, op = 0;

	while ((ch = getopt (argc, argv, "esr")) != EOF)
		switch(ch) {
		case 'e':
			mes = 1;
			break;
		case 's':
			op = 1;
			break;
		case 'r':
			op = 2;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();

	dkname = argv[0];
	if (dkname[0] != '/') {
		(void)sprintf(namebuf, "%sr%sd", _PATH_DEV, dkname);
		specname = namebuf;
	} else
		specname = dkname;
	fp = open(specname, O_RDWR);
	if (fp < 0)
		Perror(specname);

	lseek(fp, (off_t)DOSBBSECTOR * DEV_BSIZE + DOSPARTOFF, SEEK_SET);
	if (read(fp, part_table, sizeof(part_table)) < sizeof(part_table)) {
		if (mes == 0)
			Perror("DOSパーティションテーブルの読み込みに失敗しました");
		else
			Perror("Cannot read DOS partition table");
		exit(1);
	}
	bcopy(part_table, dos_partitions, sizeof(dos_partitions));

	maxpart = display_dospart(fp);

	if (op) {
		select_dospart(fp, op, maxpart);
		bcopy(dos_partitions, part_table, sizeof(dos_partitions));
		lseek(fp, (off_t)DOSBBSECTOR * DEV_BSIZE + DOSPARTOFF, SEEK_SET);
		flag = 1;
		if (ioctl(fp, DIOCWLABEL, &flag) < 0)
			fprintf(stderr, "error: ioctl DIOCWLABEL\n");
		if (write(fp, part_table, sizeof(part_table)) < sizeof(part_table)) {
			if (mes == 0)
				Perror("DOSパーティションテーブルの書き込みに失敗しました");
			else
				Perror("Cannot write DOS partition table");
			exit(1);
		}
		flag = 0;
		ioctl(fp, DIOCWLABEL, &flag);
	}
	close(fp);
	exit(0);
}

int
display_dospart(fp)
	int fp;
{
	struct dos_partition *dosdp;
	struct disklabel lp;
	char dp_name[17];
	unsigned long p_size;
	int i;

	ioctl(fp, DIOCGDINFO, &lp);

	dosdp = dos_partitions;
	if (mes == 0)
		printf("デバイスファイル名: %s\n\n", specname);
	else
		printf("Device file name: %s\n\n", specname);
	if (mes == 0)
		printf("番号  システム名          状  態    From(cyl)   To(cyl)   サイズ  ブート\n");
	else
		printf("   #  System Name         State     From(cyl)   To(cyl)     Size  Boot\n");
	for (i = 0; i < NDOSPART; i++, dosdp++) {
		if (!dosdp->dp_mid && !dosdp->dp_sid) break;

		strncpy(dp_name, dosdp->dp_name, 16);
		dp_name[17] = 0;
		printf("  %2d  %s  ", i + 1, dp_name);
		if (dosdp->dp_sid & 0x80)
			if (mes == 0)
				printf("アクティブ");
			else
				printf("  active  ");
		else
			if (mes == 0)
				printf(" スリープ ");
			else
				printf("  sleep   ");
		printf("  %8d  %8d", dosdp->dp_scyl, dosdp->dp_ecyl);
		p_size = (dosdp->dp_ecyl - dosdp->dp_scyl + 1) *
						lp.d_secpercyl / (1024 * 2);
		printf("%8ldMB ", p_size);
		if (dosdp->dp_mid & 0x80)
			if (mes == 0)
				printf(" 可\n");
			else
				printf(" On\n");
		else
			if (mes == 0)
				printf(" 不可\n");
			else
				printf(" Off\n");
	}
	return i;
}

void
select_dospart(fp, op, maxpart)
	int fp, op, maxpart;
{
	int ans;

	if (op == 1)
		if (mes == 0)
			printf("\n指定されたパーティションをNetBSDに置き換えます\n");
		else
			printf("\nset NetBSD partition\n");
	else
		if (mes == 0)
			printf("\n指定されたパーティションをMS-DOSに戻します\n");
		else
			printf("\nset DOS partition\n");
	if (mes == 0)
		printf("どのパーティション番号ですか？ ");
	else
		printf("which partition ? ");

	do {
		ans = getchar();
		if (ans != EOF && ans != '\n')
			while(getchar() != '\n') {}
	} while (ans < '1' || ans > (0x30 + maxpart));
	if (op == 1) {
		dos_partitions[ans - '1'].dp_mid = DOSMID_NETBSD;
		dos_partitions[ans - '1'].dp_sid = DOSSID_NETBSD;
		strncpy(dos_partitions[ans - '1'].dp_name, dp_netbsd, 16);
	} else {
		dos_partitions[ans - '1'].dp_mid = 0x20;
		dos_partitions[ans - '1'].dp_sid = 0xa1;
		strncpy(dos_partitions[ans - '1'].dp_name, dp_msdos, 16);
	}
}

int
Perror(str)
	char *str;
{

	fputs("dospart: ", stderr); perror(str);
	exit(4);
}

int
usage(void)
{

	fprintf(stderr,"%s\n\t%s\n%s\n\t%s\n%s\n\t%s\n",
"usage: dospart disk",
		"(to display Boot partitions table)",
"or dospart -r disk",
		"(to restore NetBSD partition to MS-DOS partition)",
"or dospart -s disk",
		"(to replace MS-DOS partition with NetBSD partition)");
	exit(1);
}
