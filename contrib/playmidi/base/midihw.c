/*	$NecBSD: midihw.c,v 1.9 1998/03/23 16:59:27 honda Exp $	*/
/*	$NetBSD$	*/

/* 
 * Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 * Copyright (c) 1996, 1997, 1998
 *	Noriyuki Hosobuchi. All rights reserved.
 *
 * The followings are supported:
 * 1) serial interface  + an external midi sequencer     	    OK
 * 2) mpu98 midi interface(UART mode) + an external midi sequencer  OK
 * 3) PCMCIA SCP-55					 	    OK
 * 4) PCMCIA QVISION MIDI CARD				 	    OK
 */

/*
#define	MPU_DEFAULT_DEVICE "cua1"
 */

#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "playmidi.h"

static int mpu98_open __P((int));
static int mpu98_output __P((int, u_char));
static u_char mpu98_input __P((int));

static int mpuQV_probe __P((int));
static int mpuQV_open __P((int));

static int mpuspc_probe __P((int));
static int mpuspc_open __P((int));
static int mpu401_output __P((int, u_char));
static u_char mpu401_input __P((int));

static int mputdk_output __P((int, u_char));
static int mputdk_open __P((int));

static int device_open __P((int));
static int device_output __P((int, u_char));
static u_char device_input __P((int));
static int device_flush __P((int));

static struct midi_hw midi_hw_select_array[] = {
	{
		"Generic RS232c",
		"cua",

		NULL,
		device_open,
		device_output,
		NULL,
		device_flush,
		0xe0d0,
	},

	{
		"Qvision midi",
		"qvision",

		mpuQV_probe,
		mpuQV_open,
		mpu98_output,
		mpu98_input,
		NULL,
		0xe0d0,
	},

#ifdef notyet
	{
		"TDK music EMU8000",
		"EMU",

		NULL,
		mputdk_open,
		mputdk_output,
		NULL,
		NULL,
		0x4330,
	},
#endif
};

/*******************************************************
 * Serial
 *******************************************************/
static u_char *ComBuff;
static int ComDataSize;
#define	COMBUFSIZE	0x10000

static int
device_open(dummy)
	int dummy;
{
	u_char device_dvname[FILENAME_MAX];
	u_char cmd_string[FILENAME_MAX + 256];
	int ComFd;
	
	sprintf(device_dvname, "/dev/%s", dvname);

	ComFd = open(device_dvname, O_RDWR, 0);
	if (ComFd < 0)
	{
		perror(device_dvname);
		exit(1);
	}

	sprintf(cmd_string, "/bin/stty -f %s 38400 raw", device_dvname);
	system(cmd_string);

	fcntl(ComFd, F_SETFL, O_NONBLOCK);

	ComBuff = malloc(COMBUFSIZE);
	if (ComBuff == NULL)
	{
		printf("malloc failed\n");	
		exit(1);
	};

	iobase = ComFd;
	return 0;
}

static int 
device_output(iobase, c)
	int iobase;
	u_char c;
{
	if (ComDataSize >= COMBUFSIZE)
		return EIO;

	ComBuff[ComDataSize ++] = c;
	return 0;
}

static int
device_flush(iobase)
	int iobase;
{
	int tmp;

	if (ComDataSize)
	{
		write(iobase, ComBuff, ComDataSize);
		ComDataSize = 0;
	}

	read(iobase, &tmp, 4);

	return 0;
}

/*******************************************************
 * MPU401
 *******************************************************/
static int 
mpu401_output(iobase, c)
	int iobase;
	u_char c;
{

	while (inb(iobase + mpu401_stat) & SR_WRDY)
		;	

	outb(iobase + mpu401_data, c);

	return 0;
}

static u_char
mpu401_input(iobase)
	int iobase;
{

	while (inb(iobase + mpu401_stat) & SR_RRDY)
		;	

	return (inb(iobase + mpu401_data));
}

/*******************************************************
 * MPU98
 *******************************************************/
static int 
mpu98_output(iobase, c)
	int iobase;
	u_char c;
{

	while (inb(iobase + mpu98_stat) & SR_WRDY)
		;	

	outb(iobase + mpu98_data, c);

	return 0;
}

static u_char
mpu98_input(iobase)
	int iobase;
{

	while (inb(iobase + mpu98_stat) & SR_RRDY)
		;	

	return (inb(iobase + mpu98_data));
}

static int
mpu98_open(iobase)
	int iobase;
{
	outb(iobase + mpu98_cmd, 0xff);		/* reset */
	while ((inb(iobase + mpu98_stat) & SR_RRDY) == 0){
		inb(iobase + mpu98_data);
	}

	while (inb(iobase + mpu98_stat) & SR_WRDY){
		;
	}

	outb(iobase + mpu98_cmd, 0x3f);		/* Goto UART mode */
	while(mpu98_input(iobase) != 0xfe){	/* ACK */
		;
	}

	return 0;
}

/*******************************************************
 * QVision
 *******************************************************/
static int
qv_wait_busy(iobase)
	int iobase;
{
	while(inb(iobase + qv_stat) & SR_BUSY)
		;

	return 0;
}

static int
mpuQV_probe(iobase)
	int iobase;
{
	u_char tval = inb(iobase + mpu98_stat);

	if (tval == 0xff)
		return ENODEV;

	return 0;
}

static int
mpuQV_open(iobase)
	int iobase;
{

	outb(iobase + qv_cmd, 0x85);
	qv_wait_busy(iobase);

	outb(iobase + qv_cmd, 0x02);
	qv_wait_busy(iobase);

	outb(iobase + qv_cmd, 0x0);
	qv_wait_busy(iobase);

	outb(iobase + qv_cmd, 0x7e);
	qv_wait_busy(iobase);

	return 0;
}

/*******************************************************
 * TDK
 *******************************************************/
#ifdef	notyet
static int
mputdk_output(iobase, c)
	int iobase;
	u_char c;
{
	u_char stat;

	outb(iobase + 0x2, 0x2);
	while (((stat = inb(iobase + 0x04)) & 0x02))
	{
		printf("out %x %x\n", c, stat);
	}

	while (((stat = inb(iobase + 0x0a)) & 0x01))
	{
		printf("out %x %x\n", c, stat);
	}

	outb(iobase, c);
	outb(iobase + 0x2, 0x0);
	return 0;
}

static int
mputdk_open(iobase)
	int iobase;
{
	int i;

	inb(iobase);
	inb(iobase + 0x0a);
	inb(iobase + 0x0c);
	inb(iobase + 0x04);

	outb(iobase + 6, 0x80);
	for (i = 0; i < 0x10000; i ++);

	outb(iobase, 0x05);
	for (i = 0; i < 0x10000; i ++);

	outb(iobase + 2, 0x0);
	for (i = 0; i < 0x10000; i ++);
	
	outb(iobase + 6, 0x0);
	for (i = 0; i < 0x10000; i ++);
	
	outb(iobase + 6, 0x3);
	for (i = 0; i < 0x10000; i ++);

	outb(iobase + 8, 0x0);
	for (i = 0; i < 0x10000; i ++);

	outb(iobase + 4, 0x7);
	for (i = 0; i < 0x10000; i ++);

	outb(iobase + 4, 0x1);
	for (i = 0; i < 0x10000; i ++);

	inb(iobase + 0x0a);

	outb(iobase + 0x2, 0x0);
	for (i = 0; i < 0x10000; i ++);
}
#endif

/*******************************************************
 * hw select
 *******************************************************/
int
midi_hw_select(void)
{
	int i;
	struct midi_hw *hwt;

#ifdef	MPU_DEFAULT_DEVICE
	if (dvname[0] == 0)
		strcpy(dvname, MPU_DEFAULT_DEVICE);
#endif

	for (i = 0; dvname[i] != 0 ; i ++)
		dvname[i] = (u_char) tolower((u_int) dvname[i]);

	for (i = 0; i < sizeof(midi_hw_select_array) / sizeof(struct midi_hw); i ++)
	{
		hwt = &midi_hw_select_array[i];
		if (strncmp(dvname, hwt->hw_dvname, 3) != 0)
			continue;

		if (hwt->hw_probe == NULL || 
		    ((*hwt->hw_probe)(hwt->hw_dfiobase)) == 0)
		{
			hw = hwt;
			iobase = hw->hw_dfiobase;
			return 0;
		}
	}

	printf("\nYOU MUST SPECIFY mpu hardware with -f option\n");
	printf("MPU hardware: cua?, qvision\n");
	return EINVAL;
}
