/*	$NecBSD: wavplay.c,v 1.16 1998/03/07 19:11:09 honda Exp $	*/
/*	$NetBSD$	*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/signal.h>
#include <errno.h>

int curpos, sigposted, endpos, loopon, silent, nomsg;
int DevFd;

/*
 * Fixed by various contributors:
 * 1) Little-endian handling
 * 2) Skip other kinds of file data
 * 3) Handle 16-bit formats correctly
 * 4) Not go into infinite loop
 */

#define	WAVE_FORMAT_UNKNOWN		(0x0000)
#define	WAVE_FORMAT_PCM			(0x0001)
#define	WAVE_FORMAT_ADPCM		(0x0002)
#define	WAVE_FORMAT_ALAW		(0x0006)
#define	WAVE_FORMAT_MULAW		(0x0007)
#define	WAVE_FORMAT_OKI_ADPCM		(0x0010)
#define	WAVE_FORMAT_DIGISTD		(0x0015)
#define	WAVE_FORMAT_DIGIFIX		(0x0016)
#define	IBM_FORMAT_MULAW		(0x0101)
#define	IBM_FORMAT_ALAW			(0x0102)
#define	IBM_FORMAT_ADPCM		(0x0103)

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *	Find out sampling rate,
 *	size and style of samples,
 *	mono/stereo/quad.
 */

void
fail(s)
	u_char *s;
{
	printf("%s\n", s);
}

unsigned long
rllong(fp)
	FILE *fp;
{
	unsigned char uc, uc2, uc3, uc4;

	uc  = getc(fp);
	uc2 = getc(fp);
	uc3 = getc(fp);
	uc4 = getc(fp);
	return ((long)uc4 << 24) | ((long)uc3 << 16) | ((long)uc2 << 8) | (long)uc;
}

unsigned short
rlshort(fp)
	FILE *fp;
{
	unsigned char uc, uc2;
	uc  = getc(fp);
	uc2 = getc(fp);
	return (uc2 << 8) | uc;
}

struct wavinfo {
	u_int fmt;
	u_int prec;
	u_int chan;
	u_int speed;
	u_int len;
};

int
wavstartread(fp, wi)
	FILE *fp;
	struct wavinfo *wi;
{
	char	magic[4];
	long	len;
	int	littlendian = 1;
	u_short enc;
	char	*endptr;
	char	c;

	endptr = (char *) &littlendian;

	fread(magic, 4, 1, fp);
	if (strncmp("RIFF", magic, 4))
	{
		fail("Sorry, not a RIFF file");
		return EINVAL;
	}

	len = rllong(fp);

	fread(magic, 4, 1, fp);
	if (strncmp("WAVE", magic, 4))
	{
		fail("Sorry, not a WAVE file");
		return EINVAL;
	}

	/* Skip to the next "fmt " or end of file */
	while (1) {
		if (feof(fp))
		{
			fail("Sorry, missing fmt spec");
			return EINVAL;
		}
		fread(magic, 4, 1, fp);
		if (! strncmp("fmt ", magic, 4))
			break;
		for (len = rllong(fp); len > 0; len--)
			fread(&c, 1, 1, fp);
	}

	len = rllong(fp);
	enc = rlshort(fp);
	wi->chan = rlshort(fp);
	wi->speed = rllong(fp);
	rllong(fp);	/* Average bytes/second */
	rlshort(fp);	/* Block align */
	wi->prec = rlshort(fp);
	switch (enc)
	{
		case WAVE_FORMAT_UNKNOWN:
			wi->fmt = -1;
			break;
		case WAVE_FORMAT_PCM:	/* this one, at least, I can handle */
			if (wi->prec == 8)
				wi->fmt = AUDIO_ENCODING_ULINEAR;
			else
				wi->fmt = AUDIO_ENCODING_SLINEAR_LE;
			break;
		case WAVE_FORMAT_ADPCM:
			wi->fmt = AUDIO_ENCODING_ADPCM;
			break;
		case WAVE_FORMAT_ALAW:
			wi->fmt = AUDIO_ENCODING_ALAW;
			break;
		case WAVE_FORMAT_MULAW:
			wi->fmt = AUDIO_ENCODING_ULAW;
			break;
		default:
			fail("Sorry, don't understand format");
			return EINVAL;
	}

	len -= 16;
	while (--len >= 0)
		getc(fp);

	fread(magic, 4, 1, fp);
	if (strncmp("fact", magic, 4) == 0)
	{
		len = rllong(fp);
		while (--len >= 0)
			getc(fp);
		fread(magic, 4, 1, fp);
	}

	if (strncmp("data", magic, 4) == 0)
	{
		wi->len = rllong(fp);
		return 0;
	}

	fail("Sorry, missing data portion");
	return EINVAL;
}

int
audio_play(fd, fp, wi)
	int fd;
	FILE *fp;
	struct wavinfo *wi;
{
#define BLKSZ 2048
	struct audio_info adata, odata;
	u_char buf[BLKSZ];
	int count, error;
	int update = 0;
	int len = wi->len;

	if (len <= 0)
		return 0;

	AUDIO_INITINFO(&odata);
	error = ioctl(fd, AUDIO_GETINFO, &odata);
	if (error)
	{
		perror("audio_setinfo");
		return error;
	}

	AUDIO_INITINFO(&adata);
	if (odata.mode != (AUMODE_PLAY | AUMODE_PLAY_ALL))
	{
		update = 1;
		adata.mode = AUMODE_PLAY | AUMODE_PLAY_ALL;
	}

	if (odata.blocksize != BLKSZ)
	{
		update = 1;
		adata.blocksize = BLKSZ;
	}

	if (odata.play.channels != wi->chan)
	{
		update = 1;
		adata.play.channels = wi->chan;
	}

	if (odata.play.precision != wi->prec)
	{
		update = 1;
		adata.play.precision = wi->prec;
	}

	if (odata.play.sample_rate != wi->speed)
	{
		update = 1;
		adata.play.sample_rate = wi->speed;
	}

	if (odata.play.encoding != wi->fmt)
	{
		update = 1;
		adata.play.encoding = wi->fmt;
	}

	if (update)
	{
		adata.record = adata.play;
		error = ioctl(fd, AUDIO_SETINFO, &adata);
		if (error)
		{
			perror("audio_setinfo");
			return error;
		}
	}

	while (1)
	{
		count = fread(buf, 1, BLKSZ, fp);
		if (count <= 0)
			return 0;

		if (sigposted)
			return 0;

		write(fd, buf, count);
		len -= count;
		if (len <= 0 || count != BLKSZ)
			return 0;

		if (sigposted)
			return 0;
	}
}

void
sigint()
{
	u_char ch;

	ioctl(DevFd, AUDIO_FLUSH, 0);

	sigposted = 1;

	if (silent)
		exit(0);

	system("stty raw");
	printf("\r");
	while (1)
	{
		printf("current posision(%d in 0 ... %d) :: cmd(q,n,p,c): ",
			curpos, endpos - 1);
		ch = getchar();
		printf("%c\r", ch);
		switch (ch)
		{
		case 'q':
		case '3':
			printf("\n\r");
			system("stty sane");
			exit(0);

		case 'c':
		case '\n':
		case '\r':
			system("stty sane");
			return;

		case 'n':
			curpos ++;
			if (curpos >= endpos)
				curpos = endpos - 1;
			break;

		case 'p':
			curpos --;
			if (curpos < 0)
				curpos = 0;
			break;

		case 'l':
			loopon ^= 1;
			break;
		}
	}
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int fd, i;
	FILE *fp;
	struct wavinfo winfo;
	u_char *target, *dvname = "/dev/sound";
	int ch;
	extern int opterr, optind;
	extern char *optarg;

	signal(SIGINT, sigint);
	siginterrupt(SIGINT, 1);

	opterr = 0;
	while ((ch = getopt(argc, argv, "f:s")) != EOF)
	{

		switch (ch)
		{
		case 'f':
			dvname = optarg;
			break;

		case 's':
			nomsg = 1;
			break;
		}
		opterr = 0;
	}

	argc -= optind;
	argv += optind;
	fd = open(dvname, O_RDWR, 0);
	if (fd < 0)
	{
		perror(dvname);
		exit(1);
	}
	DevFd = fd;

	endpos = argc;
	for (curpos = 0; curpos < argc; )
	{
		sigposted = 0;

		if (strcmp(argv[curpos], "-") == 0)
		{
			silent = 1;
			target = "/dev/stdin";
		}
		else
		{
			silent = 0;
			target = argv[curpos];
		}

		fp = fopen(target, "r");
		if (fp == NULL)
		{
			perror(argv[curpos]);
			curpos ++;
			continue;
		}

		if (wavstartread(fp, &winfo))
		{
			curpos ++;
			goto resume;
		}

		if (nomsg == 0)
		printf("<%d> %s: fmt(%d) prec(%d) chan(%d) speed(%d) len(%d)\n",
			curpos, argv[curpos],
			winfo.fmt, winfo.prec, winfo.chan, winfo.speed,
			winfo.len);

		audio_play(fd, fp, &winfo);
		if (sigposted == 0)
			curpos ++;
		if (loopon && curpos >= argc)
			curpos = 1;

resume:
		fclose(fp);
		ioctl(fd, AUDIO_DRAIN, 0);
		ioctl(fd, AUDIO_FLUSH, 0);
		sleep(1);
	}
}
