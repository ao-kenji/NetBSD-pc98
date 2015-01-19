/*	$NecBSD: sertest.c,v 1.6 1998/02/08 08:01:31 kmatsuda Exp $	*/
/*	$NetBSD$	*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>

#define	BUFSZ	0x10000
u_char buffer[BUFSZ];

int
main(argc, argv)
	int argc;
	u_char **argv;
{
	int fd, i;
	u_char pathname[10000];
	u_char cmd[10000];

	if (argc != 3)
		exit(1);

	sprintf(pathname, "/dev/%s", argv[2]);
	fd = open(pathname, O_RDWR, 0);
	if (fd < 0)
		exit(1);

	sprintf(cmd, "stty -f %s 115200 raw\n", pathname);
	system(cmd);

	if (strcmp(argv[1], "out") == 0)
	{
		for (i = 0; i < BUFSZ; i ++)
			buffer[i] = i % 256;

		while (1)
		{
			write(fd, buffer, BUFSZ);
		};
	}
	else
	{
		int ind, count;

		ind = 0;
		while (1)
		{
			count = read(fd, buffer, BUFSZ);
			for (i = 0; i < count; i ++, ind ++)
			{
				if (ind >= 256)
					ind = 0;

				if (buffer[i] != ind)
				{
					printf("mis %d != %d", buffer[i], ind);
					fflush(stdout);
					ind = buffer[i];
				}
			}
		}
	}
}
