#	$NecBSD: devlist.awk,v 3.8 1998/09/11 13:02:45 honda Exp $
#	$NetBSD$

# [NetBSD for NEC PC-98 series]
#  Copyright (c) 1997, 1998
#	NetBSD/pc98 porting staff. All rights reserved.

BEGIN {
	printf("/* this file is automatically generated */\n");
	printf("#include <machine/devlist.h>\n\n");
	ndev = -1;
	nprot = 0;
	nlist = 0;
	FS="\t+";
}

END {
	ndev ++;

	for (i = 0; i < ndev; i ++)
	{
		for (j = 0; j < nlist; j ++)
		{
			if (DEVICE_LIST[j] == DEVICE_NAME[i])
			{
				break;
			}
		}

		if (j < nlist)
		{
			continue;
		}

		DEVICE_LIST[nlist] = DEVICE_NAME[i];
		DEVICE_LIST_BUS[nlist] = DEVICE_BUS[i];
		nlist ++
	}

	printf("\n\n");
	for (i = 0; i < nprot; i ++)
	{
		DEVICE_FILE = tolower(SLOT_TAG[i])".h";
		printf("#include \"%s\"\n", DEVICE_FILE);	
	}

	printf("\n\n");
	for (i = 0; i < nlist; i ++)
	{
		DEVICE_FILE = DEVICE_LIST[i]"_"tolower(DEVICE_LIST_BUS[i])".h";
		printf("#include \"%s\"\n", DEVICE_FILE);	
	}

	printf("\n\n");
	for (i = 0; i < nlist; i ++)
	{
		DEVICE_N = "N"toupper(DEVICE_LIST[i])"_"DEVICE_LIST_BUS[i];

		printf("#if %s > 0\n", DEVICE_N);
		printf("\textern struct cfdriver %s_cd;\n", DEVICE_LIST[i]);
		printf("#endif\n");
	}

	printf("\n\n");
	printf("struct devlist devlist[] = {\n")
	for (i = 0; i < ndev; i ++)
	{
		DEVICE_N = "N"toupper(DEVICE_NAME[i])"_"DEVICE_BUS[i];
		printf("#if %s > 0 && N%s > 0\n", DEVICE_N, DEVICE_SLOT_TAG[i]);
		printf("\t{\"%s\", %s, %s, %s, %s, &%s_cd, %s, %s, %s, %s,\n", \
			DEVICE_PROTOCOL[i],\
			DEVICE_ID[i],\
			DEVICE_IDMASK[i],\
			DEVICE_LD[i],\
			DEVICE_CLASS[i],\
			DEVICE_NAME[i],\
			DEVICE_FLAGS[i],\
			DEVICE_BCFG[i],\
			DEVICE_KEY0[i],\
			DEVICE_KEY1[i])
		if (DEVICE_SKEY[i] == 0)
		{	
			printf("\t 0},\n");
		}
		else
		{
			printf("\t \"%s\"},\n", DEVICE_SKEY[i]);
		}
			
		printf("#endif\n")
	}

	printf("\t{}\n");

	printf("};\n")
}

/^BUSCFG/ {
	DEVICE_BCFG[ndev] = $2
	next;
}

/^MASKS/ {
	DEVICE_IDMASK[ndev] = $2
	next;
}

/^BUS/ {
	CURRENT_BUS = $2
	next;
}

/^PROTOCOL/ {
	CURRENT_BUS_PROTOCOL = $2
	CURRENT_BUS_SLOT_TAG = $3
	SLOT_TAG[nprot] = $3
	nprot ++
	next;
}

/^DVCFG/ {
	DEVICE_FLAGS[ndev] = $2
	next;
}

/^IOKEY/ {
	DEVICE_KEY0[ndev] = $2
	DEVICE_KEY1[ndev] = $3
	next;
}

/^SKEY/	{
	DEVICE_SKEY[ndev] = $2
	next;
}

/^DEVICE/ {
	ndev ++
	DEVICE_BUS[ndev] = CURRENT_BUS
	DEVICE_PROTOCOL[ndev] = CURRENT_BUS_PROTOCOL
	DEVICE_SLOT_TAG[ndev] = CURRENT_BUS_SLOT_TAG
	DEVICE_ID[ndev] = $2
	DEVICE_LD[ndev] = $3
	DEVICE_CLASS[ndev] = $4
	DEVICE_NAME[ndev] = $5

	DEVICE_IDMASK[ndev] = "(u_long) -1";
	DEVICE_BCFG[ndev] = 0
	DEVICE_FLAGS[ndev] = 0
	DEVICE_KEY0[ndev] = 0
	DEVICE_KEY1[ndev] = 0
	DEVICE_SKEY[ndev] = 0

	next;
}
