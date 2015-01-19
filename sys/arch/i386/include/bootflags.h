/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1999
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
#ifndef _I386_BOOTFLAGS_H_
#define _I386_BOOTFLAGS_H_
#define	BOOTFLAGS_BIOS_OFFSET		0xffc
#define	BOOTFLAGS_DISK_OFFSET		0x10
#define	BOOTFLAGS_SIGNATURE		0x12345678

#define	BOOTFLAGS_FLAGS_ZERO		0x0
#define	BOOTFLAGS_FLAGS_BIOSGEOM	0x00000001
#define	BOOTFLAGS_FLAGS_VERBOSE		0x00000002
#define	BOOTFLAGS_FLAGS_DEVICES		0x00000004

#if	defined(_KERNEL) && !defined(_LOCORE)
#define	BTINFO_BOOTFLAGS	98

struct biosboot_header {
	u_int8_t	bh_jmp[2];
	u_int16_t	bh_scyl;
	u_int8_t	bh_string[8];
	u_int32_t	bh_signature;
	u_int32_t	bh_bootflags;
} __attribute__((packed));

#define	BOOTFLAGS_BTINFO_MAXENTRY	8	
struct bootflags_info_header {
	u_int32_t bi_signature;
	int bi_nentry;
};

struct bootflags_btinfo_header {
	struct btinfo_common bb_common;
	struct bootflags_info_header bb_bi;

	u_int32_t bb_bootflags;
};

struct bootflags_device {
	u_long bd_type;
	u_long bd_flags;
	u_char bd_name[16];
	u_long bd_loc[8];
};
#endif	/* _KERNEL && !_LOCORE */
#endif /* _I386_BOOTFLAGS_H_ */
