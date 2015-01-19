/*	$NecBSD: mktbl.c,v 1.2 1999/04/15 01:36:52 kmatsuda Exp $	*/
/*	$NetBSD$	*/

#include <stdio.h>
#include <fcntl.h>

#define 	DEV_BSIZE	512

int
main(argc, argv)
	int argc;
	u_char **argv;
{
	int fd, size, i;
	u_char buf[DEV_BSIZE];

	fd = open(argv[1], O_RDONLY, 0);
	if (fd < 0)
		return 1;

	size = read(fd, buf, DEV_BSIZE);
	if (size < 0)
		return 1;
	
	printf("{");
	for (i = 1; i <= size ; i ++)
	{
		printf("0x%02x, ", buf[i - 1]);
		if ((i % 8) == 0)
			printf("\n");	
	}
	printf("};\n");

	return 0;
}
