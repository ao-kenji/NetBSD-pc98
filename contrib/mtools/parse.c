/*	$NecBSD: parse.c,v 1.9 1998/03/14 07:01:38 kmatsuda Exp $	*/
/*	$NetBSD$	*/

#include <stdio.h>
#include <ctype.h>
#include "msdos.h"

extern char *mcwd;

/*
 * Get name component of filename.  Translates name to upper case.  Returns
 * pointer to static area.
 */

char *
get_name(filename)
char *filename;
{
	char *s, *temp, *strcpy(), *strrchr(), buf[MAX_PATH];
	static char ans[13];

	strcpy(buf, filename);
	temp = buf;
					/* skip drive letter */
	if (buf[0] && buf[1] == ':')
		temp = &buf[2];
					/* find the last separator */
	if (s = strrchr(temp, '/'))
		temp = s + 1;
	if (s = strrchr(temp, '\\'))
		temp = s + 1;
					/* xlate to upper case */
	for (s = temp; *s; ++s) {
		if (islower(*s))
			*s = toupper(*s);
	}

	strcpy(ans, temp);
	return(ans);
}

/*
 * Get the path component of the filename.  Translates to upper case.
 * Returns pointer to a static area.  Doesn't alter leading separator,
 * always strips trailing separator (unless it is the path itself).
 */

char *
get_path(filename)
char *filename;
{
	char *s, *end, *begin, *strcpy(), *strrchr(), buf[MAX_PATH];
	char drive, *strcat();
	static char ans[MAX_PATH];
	int has_sep;

	strcpy(buf, filename);
	begin = buf;
					/* skip drive letter */
	drive = '\0';
	if (buf[0] && buf[1] == ':') {
		drive = (islower(buf[0])) ? toupper(buf[0]) : buf[0];
		begin = &buf[2];
	}
					/* if absolute path */
	if (*begin == '/' || *begin == '\\')
		ans[0] = '\0';
	else {
		if (!drive || drive == *mcwd)
			strcpy(ans, mcwd + 2);
		else
			strcpy(ans, "/");
	}
					/* find last separator */
	has_sep = 0;
	end = begin;
	if (s = strrchr(end, '/')) {
		has_sep++;
		end = s;
	}
	if (s = strrchr(end, '\\')) {
		has_sep++;
		end = s;
	}
					/* zap the trailing separator */
	*end = '\0';
					/* translate to upper case */
	for (s = begin; *s; ++s) {
		if (islower(*s))
			*s = toupper(*s);
		if (*s == '\\')
			*s = '/';
	}
					/* if separator alone, put it back */
	if (!strlen(begin) && has_sep)
		strcat(ans, "/");

	strcat(ans, begin);
	return(ans);
}

/*
 * get the drive letter designation
 */

#ifdef	ORIGINAL_CODE
char
get_drive(filename)
char *filename;
{
	if (*filename && *(filename + 1) == ':') {
		if (islower(*filename))
			return(toupper(*filename));
		else
			return(*filename);
	}
	else
		return(*mcwd);
}
#else	/* PC-98 */
/*
 * get the drive letter designation
 */
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <machine/fdio.h>
#define	DEFAULTDIR	'A'

char
get_cur_mode(drive)
	int drive;
{
	char pathname[FILENAME_MAX];
	int  fd, error, mode;
	char c = 'A';

	sprintf(pathname, "/dev/rfd%da", drive);
	fd=open(pathname, O_RDONLY, 0777);
	if (fd < 0)
	{
		return -1;
	}

	error = ioctl(fd, FDMIOC, &mode);
	if (error)
	{
		perror("FDMIOC");
		close(fd);
		return -1;
	}
	error = ioctl(fd, FDIOCGDENSITY, &mode);
	if (error < 0)
	{
		perror("FDIOCGDENSITY");
		close(fd);
		return -1;
	}

	c += drive * 5;
	c += mode;
	close(fd);
	return c;
}

char
get_drive(filename)
	char *filename;
{
	char c = DEFAULTDIR;

	if (*filename && *(filename + 1) == ':') {
		if (islower(*filename))
			c = toupper(*filename);
		else
			c = *filename;
	}
	if ((c = get_cur_mode(c - 'A')) < 0)
		 c = DEFAULTDIR;
	return c;
}
#endif	/* PC-98 */
