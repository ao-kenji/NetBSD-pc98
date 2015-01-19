/*	$NecBSD: sndctrl.c,v 1.11 1998/03/14 07:01:59 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
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

/*
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>

#define	BLKSZ	2048
#define	UNK	((u_int) -1)

u_char *encode_table[] = {
	"none", "ulaw", "alaw", "pcm16", "pcm8", "adpcm", NULL,
};

u_int
get_encode(s)
	u_char *s;
{
	int no;

	for (no = 0; encode_table[no]; no ++)
		if (strcmp(encode_table[no], s) == 0)
			return no;

	return UNK;
}

void
usage(void)
{
	printf("usage: sndctrl -c chan -p prec -s speed -e coding -m mode\n");
	printf("               -b blksize -h hiwat -l lowat\n");
	printf("speed: 44100, 22050, 11025, 8000\n");
	printf("coding: ulaw, alaw, pcm16, pcm8, adpcm\n");
}

int
show_data(info)
	struct audio_info *info;
{
	u_char *mode;

	switch (info->mode & (AUMODE_RECORD | AUMODE_PLAY))
	{
	case AUMODE_RECORD:
		mode = "record";
		break;

	case AUMODE_PLAY:
		mode = "play";
		break;

	case AUMODE_PLAY | AUMODE_RECORD:
		mode = "duplex";
		break;
	}

	printf("/dev/sound status: prec(%d) chan(%d) speed(%d) coding(%s) mode(%s)\n",
		info->play.precision,
		info->play.channels,
		info->play.sample_rate,
		encode_table[info->play.encoding],
		mode);
	printf("                 : blksize(%d) hiwat(%d) lowat(%d)\n",
		info->blocksize, info->hiwat, info->lowat);

}

int
error_exit(s)
	char *s;
{
	printf("%s\n", s);
	usage();
	exit(1);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, fd, error, up;
	struct audio_info adata;
	u_char *chan = NULL;
	u_char *prec = NULL;
	u_char *encode = NULL;
	u_char *speed = NULL;
	u_char *blksize = NULL;
	u_char *hiwat = NULL;
	u_char *lowat = NULL;
	u_char *dvname = "/dev/sound";
	extern int opterr, optind;
	extern char *optarg;

	AUDIO_INITINFO(&adata);
	adata.mode = 0;

	opterr = up = 0;
	while ((ch = getopt(argc, argv, "f:c:p:e:s:m:b:l:h:")) != EOF)
	{

		switch (ch)
		{
		case 'b':
			blksize = optarg;
			break;

		case 'f':
			dvname = optarg;
			break;

		case 'l':
			lowat = optarg;
			break;

		case 'h':
			hiwat = optarg;
			break;

		case 'c':
			chan = optarg;
			break;

		case 'p':
			prec = optarg;
			break;

		case 'e':
			encode = optarg;
			break;

		case 's':
			speed = optarg;
			break;

		case 'm':
			if (strcmp(optarg, "record") == 0)
			{
				up = 1;
				adata.mode |= AUMODE_RECORD;
			}
			else if (strcmp(optarg, "play") == 0)
			{
				up = 1;
				adata.mode |= AUMODE_PLAY;
			}
			break;

		default:
			usage();
			exit(0);
		}
		opterr = 0;
	}

	argc -= optind;
	argv += optind;

	if (adata.mode == 0)
		adata.mode |= AUMODE_PLAY;

	fd = open(dvname, O_RDWR, 0);
	if (fd < 0)
	{
		perror(dvname);
		exit(1);
	}

	adata.blocksize = BLKSZ;

	if (encode)
	{
		up = 1;

		adata.play.encoding = get_encode(encode);
		if (adata.play.encoding == UNK)
			error_exit("invalid encoding");

		if (adata.play.encoding == AUDIO_ENCODING_PCM16)
			adata.play.precision = 16;
		else
			adata.play.precision = 8;
	}

	if (chan)
	{
		up = 1;
		adata.play.channels = atoi(chan);
		if (adata.play.channels != 1 && adata.play.channels != 2)
			error_exit("invalid channel: must 1 or 2");
	}

	if (prec)
	{
		up = 1;
		adata.play.precision = atoi(prec);
		if (adata.play.precision != 8 && adata.play.precision != 16)
			error_exit("invalid precision: must 8 or 16");
	}

	if (speed)
	{
		up = 1;
		adata.play.sample_rate = atoi(speed);
	}

	if (blksize)
	{
		up = 1;
		adata.blocksize = atoi(blksize);
	}

	if (lowat)
	{
		up = 1;
		adata.lowat = atoi(lowat);
	}

	if (hiwat)
	{
		up = 1;
		adata.hiwat = atoi(hiwat);
	}

	if (up)
	{
		adata.record = adata.play;
		error = ioctl(fd, AUDIO_SETINFO, &adata);
		if (error)
			perror("audio_setinfo");
	}

	AUDIO_INITINFO(&adata);
	error = ioctl(fd, AUDIO_GETINFO, &adata);
	if (error)
		perror("audio_getinfo");

	show_data(&adata);

	return 0;
}
