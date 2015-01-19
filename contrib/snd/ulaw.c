/*	$NecBSD: ulaw.c,v 1.14 1998/03/14 07:01:54 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 * 44.1khz ulaw sterio
 * N. Honda.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>

#define MAXBLKS 512
#define	BLKSZ	2048
#define DEBUG 0

struct rb {
	u_int top, tail;
	u_int nblks, maxblks;
	u_int blksize;
	u_int hiwat, lowat;
	u_char *data[MAXBLKS];
	int size[MAXBLKS];
};

/**********************************************************
 * Util
 **********************************************************/
struct rb *
rb_alloc(size, maxblks)
	int size, maxblks;
{
	int i;
	struct rb *rb;
	u_char *pt;

	if (maxblks > MAXBLKS)
		maxblks = MAXBLKS;

	rb = (void *)malloc(sizeof(*rb));
	rb->nblks = rb->top = rb->tail = 0;
	rb->maxblks = maxblks;
	rb->hiwat = (maxblks  * 2) / 3;
	rb->lowat = (maxblks * 1) / 3;
	rb->blksize = size;
	pt = (void *)malloc(maxblks * size);
	for (i = 0; i < maxblks; i ++)
	{
		rb->size[i] = 0;
		rb->data[i] = pt;
		pt += size;
	}
	return rb;
}

void
setup_ai(ai)
	struct audio_info *ai;
{

	AUDIO_INITINFO(ai);
	ai->mode = AUMODE_PLAY;
	ai->play.sample_rate = 44100;
	ai->play.channels = 2;
	ai->play.encoding = AUDIO_ENCODING_ULAW;
	ai->play.precision = 8;
	ai->play.pause = 1;
#ifdef	TESTMODE
	ai->hiwat = 12;
	ai->lowat = 1;
#endif	/* TESTMODE */
	ai->blocksize = BLKSZ;
	ai->record = ai->play;
}

void
pause(fd, on, ai)
	int fd, on;
	struct audio_info *ai;
{

	AUDIO_INITINFO(ai);
	ai->play.pause = on;
	ioctl(fd, AUDIO_SETINFO, ai);
}

void
output(fd, rb)
	int fd;
	struct rb *rb;
{
	static struct timeval tout;
	fd_set wfd;
	int no;

	if (rb->nblks == 0)
		return;

	do
	{
		no = (rb->top ++) & (rb->maxblks - 1);
		write(fd, rb->data[no], rb->size[no]);
#if DEBUG > 1
		printf("w%d", rb->nblks);
		fflush(stdout);
#endif
		rb->nblks --;
		FD_ZERO(&wfd);
		FD_SET(fd, &wfd);
		if (select(0x20, NULL, &wfd, NULL, &tout) < 0)
			exit(1);
	}
	while (FD_ISSET(fd, &wfd) && rb->nblks);
}

int
input(fd, rb)
	int fd;
	struct rb *rb;
{
	int no;

	if (rb->nblks >= rb->maxblks)
		return 0;

	no = (rb->tail ++) & (rb->maxblks - 1);
	rb->size[no] = read(0, rb->data[no], rb->blksize);

#if DEBUG > 1
	printf("r");
	fflush(stdout);
#endif

	if (rb->size[no] <= 0)
		return 1;

	rb->nblks ++;
	return 0;
}

/**********************************************************
 * Main
 **********************************************************/
int
main(void)
{
	struct audio_info ai;
	struct rb *rb;
	fd_set rfd, wfd, efd;
	int fd, i, complete = 0, otriger = 1;

	/* open */
	fd = open("/dev/audio", O_RDWR, 0);
	if (fd < 0)
		exit(0);

	/* alloc ring bufs */
	rb = rb_alloc(BLKSZ, MAXBLKS);

	/* setup audio */
	setup_ai(&ai);
	ioctl(fd, AUDIO_SETINFO, &ai);

#if	DEBUG > 0
	ioctl(fd, AUDIO_GETINFO, &ai);
	printf("ulaw: blk %d hi %d lo %d\n", ai.blocksize, ai.hiwat, ai.lowat);
#endif

	/* flush */
	ioctl(fd, AUDIO_FLUSH, 0 );

	/* fill out buffers */
#if	1
	for (i = 0; i < rb->maxblks && complete == 0; i ++)
		complete = input(fd, rb);
#else
	for (i = 0; i < rb->maxblks && i < 0x10 && complete == 0; i ++)
		complete = input(fd, rb);
#endif

	if (rb->nblks == 0)
		exit(1);

	/* start output */
	output(fd, rb);
	pause(fd, 0, &ai);
	FD_ZERO(&efd);

	/* loop */
	while (complete == 0  || rb->nblks > 0)
	{

		/* select */
		if (complete == 0 && rb->nblks < rb->lowat)
		{
			if (otriger)
				pause(fd, 1, &ai);
			otriger = 0;
		}

		if (complete || rb->nblks >= rb->hiwat)
		{
			if (otriger == 0)
				pause(fd, 0, &ai);
			otriger = 1;
		}

		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		if (rb->nblks < rb->maxblks && complete == 0)
			FD_SET(0,&rfd);
		if (otriger)
			FD_SET(fd,&wfd);

	 	if (select(0x20, &rfd, &wfd, &efd, NULL) < 0)
			exit(1);

		/* output */
		if (FD_ISSET(fd, &wfd))
			output(fd, rb);
		/* input */
		if (FD_ISSET(0, &rfd))
			complete = input(fd, rb);
	}

	exit(0);
}
