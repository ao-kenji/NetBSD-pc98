#	$NecBSD: devlist-sed.awk,v 3.2 1999/04/15 01:08:42 kmatsuda Exp $
#	$NetBSD$

# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1998
#	NetBSD/pc98 porting staff. All rights reserved.

BEGIN {
	FS="\t+";
	ndef = 0;
}

/^DEFINE/ {
	printf("s/%s/%s/g\n", $2, $3);
	ndef ++;
}
