/*	$NecBSD: dir_write.c,v 1.5 1998/02/08 08:00:26 kmatsuda Exp $	*/
/*	$NetBSD$	*/

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include "msdos.h"

extern int fd, dir_dirty, clus_size, dir_entries;
extern long dir_chain[MAX_DIR_SECS];
extern unsigned char *dir_buf;

/*
 * Write a directory entry.  A simple cache is used instead of something
 * more elaborate.
 */

void
dir_write(num, dir)
int num;
struct directory *dir;
{
	unsigned char *offset;
#ifdef __NetBSD__
	void *memcpy();
#else
	char *memcpy();
#endif

	offset = dir_buf + (num * MDIR_SIZE);
	memcpy((char *) offset, (char *) dir, MDIR_SIZE);
	dir_dirty = 1;
	return;
}

/*
 * Write a partially filled directory buffer to disk.  Resets dir_dirty to
 * zero.
 */

void
dir_flush()
{
	int i, length;
	unsigned char *offset;
	void disk_write();

	if (fd < 0 || !dir_dirty)
		return;

#ifdef MULTISECTORSIZE
	length = dir_entries / (MSECTOR_SIZE / sizeof(struct directory));
#else
	length = dir_entries / 16;
#endif
	for (i = 0; i < length; i++) {
		offset = dir_buf + (i * MSECTOR_SIZE);
		disk_write(dir_chain[i], offset, MSECTOR_SIZE);
	}
	dir_dirty = 0;
	return;
}

#ifdef SMARTNAME
/*
 * Convert UNIX name to DOS name smartly.
 * Default renaming rules are:
 *	.tar.Z		-> .taz
 *	.tar.z		-> .tgz
 *	.tar.gz		-> .tgz
 * and
 *	.taz.ab		-> .tzb
 *	.tgz.ab		-> .tgb
 * or
 *	mt-2.0.7	-> mt-207
 */

static char
  rename_table[][2][8] = {
      /* for compressed or gzip-ed tar files */
      {".tar.Z",	".taz"},
      {".tar.z",	".tgz"},
      {".tar.gz",	".tgz"},
      /* for files split with GNU split */
      {".taz.a",	".tz"},
      {".tgz.a",	".tg" /* ".tz" is better? */},
      {"", ""}
  };

int preservename = 0;

char *
smart_dos_name(name, verbose)
 char *name;
 int   verbose;
{
    static char buf[MAX_PATH], bak[MAX_PATH];
    char *e, *s;
#ifdef __NetBSD__
    char *strstr(), *strcat(), *strchr(), *strrchr(), *strpbrk();
#else
    char *strstr(), *strcat(), *strchr(), *strrchr(), *strpbrk(), *strcmp();
#endif
    int i;

    strcpy(buf, name);
    /* Standard renaming rule for archived files. */
    for (i=0; rename_table[i][0][0]; i++) {
        s = strstr(buf, rename_table[i][0]);
        if (s) {
            strcpy(bak, s+strlen(rename_table[i][0]));
            strcpy(s, rename_table[i][1]);
            strcat(s, bak);
            if (verbose)
              fprintf(stderr, "Renamed %s to %s\n", name, buf);
        }
    }

    /* Change `.' at the beginning to `_'. */
    if (buf[0] == '.') buf[0] = '_';

    /* Truncate version number stripping dots. */
    do {
        strcpy(bak, buf);
        e = strrchr(buf, '.'); /* Check extension position. */
        s = strchr(buf, '.');
        if (NULL!=e && NULL!=s && s<e && *(s+1)>='0' && *(s+1)<='9') {
          strcpy(s, s+1);
          if (verbose)
            fprintf(stderr, "Truncated %s to %s\n", bak, buf);
      }
    } while (strcmp(bak, buf) != 0);

    /* Truncate `-', if needed. */
    s = strrchr(buf, '.');
    if ((NULL==s && strlen(buf) > 8) || (NULL!=s && s-buf > 8))
      do {
          strcpy(bak, buf);
          e = strrchr(buf, '.');
          s = strpbrk(buf, "-_");
          if (NULL!=e && NULL!=s && *(s+1)!='\0') {
              strcpy(s, s+1);
              if (verbose)
                fprintf(stderr, "Truncated %s to %s\n", bak, buf);
          }
      } while (strcmp(bak, buf) != 0);
    strcpy(name, buf);
}

#ifdef DEBUG_SMART
main(argc, argv)
 int argc;
 char *argv[];
{
    smart_dos_name(argv[1], 1);
}
#endif /* DEBUG_SMART */

#endif /* SMARTNAME */

/*
 * Convert a Unix filename to a legal MSDOS name.  Returns a pointer to
 * a static area.  Will truncate file and extension names, will
 * substitute the letter 'X' for any illegal character in the name.
 */

unsigned char *
dos_name(filename, verbose)
char *filename;
int verbose;
{
	static char *dev[9] = {"CON", "AUX", "COM1", "COM2", "PRN", "LPT1",
	"LPT2", "LPT3", "NUL"};
	char *s, buf[MAX_PATH], *name, *ext, *strcpy(), *strpbrk(), *strrchr();
#ifdef SMARTNAME
        char *smart_dos_name();
#endif
	static unsigned char ans[13];
	int dot, modified, len;
	register int i;

	strcpy(buf, filename);
	name = buf;
					/* skip drive letter */
	if (buf[0] && buf[1] == ':')
		name = &buf[2];
					/* zap the leading path */
	if (s = strrchr(name, '/'))
		name = s + 1;
	if (s = strrchr(name, '\\'))
		name = s + 1;

#ifdef SMARTNAME
        if (!preservename) smart_dos_name(name, verbose);
#endif /* SMARTNAME */
	ext = "";
	dot = 0;
	len = strlen(name);
	for (i = 0; i < len; i++) {
		s = name + len -i -1;
		if (*s == '.' && !dot) {
			dot = 1;
			*s = '\0';
			ext = s +1;
		}
		if (islower(*s))
			*s = toupper(*s);
	}
	if (*name == '\0') {
		name = "X";
		if (verbose)
			printf("\"%s\" Null name component, using \"%s.%s\"\n", filename, name, ext);
	}
	for (i = 0; i < 9; i++) {
		if (!strcmp(name, dev[i])) {
			*name = 'X';
			if (verbose)
				printf("\"%s\" Is a device name, using \"%s.%s\"\n", filename, name, ext);
		}
	}
	if (strlen(name) > 8) {
		*(name + 8) = '\0';
		if (verbose)
			printf("\"%s\" Name too long, using, \"%s.%s\"\n", filename, name, ext);
	}
	if (strlen(ext) > 3) {
		*(ext + 3) = '\0';
		if (verbose)
			printf("\"%s\" Extension too long, using \"%s.%s\"\n", filename, name, ext);
	}
	modified = 0;
	while (s = strpbrk(name, "^+=/[]:',?*\\<>|\". ")) {
		modified++;
		*s = 'X';
	}
	while (s = strpbrk(ext, "^+=/[]:',?*\\<>|\". ")) {
		modified++;
		*s = 'X';
	}
	if (modified && verbose)
		printf("\"%s\" Contains illegal character(s), using \"%s.%s\"\n", filename, name, ext);

	sprintf((char *) ans, "%-8.8s%-3.3s", name, ext);
	return(ans);
}

/*
 * Make a directory entry.  Builds a directory entry based on the
 * name, attribute, starting cluster number, and size.  Returns a pointer
 * to a static directory structure.
 */

struct directory *
mk_entry(filename, attr, fat, size, date)
unsigned char *filename;
unsigned char attr;
unsigned int fat;
long size, date;
{
	int i;
	char *strncpy();
	static struct directory ndir;
	struct tm *now, *localtime();
	unsigned char hour, min_hi, min_low, sec;
	unsigned char year, month_hi, month_low, day;

	now = localtime(&date);
	strncpy((char *) ndir.name, (char *) filename, 8);
	strncpy((char *) ndir.ext, (char *) filename + 8, 3);
	ndir.attr = attr;
	for (i = 0; i < 10; i++)
		ndir.reserved[i] = '\0';
	hour = now->tm_hour << 3;
	min_hi = now->tm_min >> 3;
	min_low = now->tm_min << 5;
	sec = now->tm_sec / 2;
	ndir.time[1] = hour + min_hi;
	ndir.time[0] = min_low + sec;
	year = (now->tm_year - 80) << 1;
	month_hi = (now->tm_mon + 1) >> 3;
	month_low = (now->tm_mon + 1) << 5;
	day = now->tm_mday;
	ndir.date[1] = year + month_hi;
	ndir.date[0] = month_low + day;
	ndir.start[1] = fat / 0x100;
	ndir.start[0] = fat % 0x100;
	ndir.size[3] = size / 0x1000000L;
	ndir.size[2] = (size % 0x1000000L) / 0x10000L;
	ndir.size[1] = ((size % 0x1000000L) % 0x10000L) / 0x100;
	ndir.size[0] = ((size % 0x1000000L) % 0x10000L) % 0x100;
	return(&ndir);
}
