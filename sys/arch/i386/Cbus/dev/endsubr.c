/*	$NecBSD: endsubr.c,v 1.6.6.4 1999/09/27 12:19:01 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
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

#include <sys/param.h>
#include <sys/systm.h>
#include <i386/Cbus/dev/endsubr.h>

struct end_vendor_macaddr {
	u_int8_t mac[3];
};

static struct end_vendor_macaddr end_valid_macaddr[] = {
	{{ 0x00, 0x00, 0x48 }}, {{ 0x00, 0x00, 0x65 }}, {{ 0x00, 0x00, 0xe8 }},
	{{ 0x00, 0x00, 0xf4 }}, {{ 0x00, 0x04, 0xac }}, {{ 0x00, 0x20, 0xcb }},
	{{ 0x00, 0x20, 0xe0 }}, {{ 0x00, 0x20, 0xe8 }}, {{ 0x00, 0x40, 0x26 }},
	{{ 0x00, 0x40, 0x33 }}, {{ 0x00, 0x40, 0x4c }}, {{ 0x00, 0x40, 0x95 }},
	{{ 0x00, 0x40, 0xc7 }}, {{ 0x00, 0x40, 0xf6 }}, {{ 0x00, 0x47, 0x43 }},
	{{ 0x00, 0x80, 0x19 }}, {{ 0x00, 0x80, 0x45 }}, {{ 0x00, 0x80, 0x4c }},
	{{ 0x00, 0x80, 0xc6 }}, {{ 0x00, 0x80, 0x98 }},
	{{ 0x00, 0x80, 0xc8 }}, {{ 0x00, 0x90, 0xfe }}, {{ 0x00, 0xa0, 0x0c }},
	{{ 0x00, 0xa0, 0xb0 }}, {{ 0x00, 0xc0, 0xf0 }}, {{ 0x08, 0x00, 0x00 }},
	{{ 0x08, 0x00, 0x17 }}, {{ 0x08, 0x00, 0x42 }}, {{ 0x08, 0x00, 0x5a }},
	{{ 0x00, 0x00, 0x4c }}, {{ 0x00, 0x80, 0x43 }},
	{{ 0x00, 0xe0, 0x98 }}, {{ 0x00, 0x40, 0x41 }}, {{ 0x00, 0x40, 0xb4 }},
	{{ 0xff, 0xff, 0xff }}	/* terminate */
};

int
end_lookup(ndp)
	u_int8_t *ndp;
{
	struct end_vendor_macaddr *macp;

	if (ndp[0] == 0xff || (ndp[0] | ndp[1] | ndp[2]) == 0 || ndp[1] & 1)
		return END_LOOKUP_INVALID;

	macp = &end_valid_macaddr[0];
	for ( ; macp->mac[0] != 0xff; macp ++)
		if (bcmp(ndp, macp->mac, 3) == 0)
			return 0;

	return END_LOOKUP_NOTFOUND;
}
