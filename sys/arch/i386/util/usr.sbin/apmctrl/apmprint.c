/*	$NecBSD: apmprint.c,v 1.7 1998/03/14 07:11:40 kmatsuda Exp $	*/
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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <machine/apmvar.h>

#include "apmprint.h"

static u_char *bs[BSTATE_MAX] =
	{ "valid", "none", "charge", "abnormal", "available" };

void
apm_pow_str(buf, binfo)
	u_char *buf;
	struct apm_battery *binfo;
{

	if (binfo->state)
		sprintf(buf, "%s", bs[binfo->state]);
	else if (binfo->life == 0)
		sprintf(buf, "%s", "empty");
	else
		sprintf(buf, "%d%%", binfo->life);
}

void
apm_sprint_power(info, buf)
	struct apm_power_info *info;
	u_char *buf;
{
	u_char tbuf0[128];
	u_char tbuf1[128];
	u_char *ac_s = info->ac_state ? "on" : "off";
	u_char *cpu_s = info->cpu_state == CPU_CTRL_ON ? "low" : "high";

	apm_pow_str(tbuf0, &info->pack0);
	apm_pow_str(tbuf1, &info->pack1);
	sprintf(buf, "A/C(%s) 1st(%s) 2nd(%s) cpu(%s)",
	       ac_s, tbuf0, tbuf1, cpu_s);
}

int
apm_get_power(fd, ainfo)
	int fd;
	struct apm_power_info *ainfo;
{

	return ioctl(fd, APM_IOG_POWER, ainfo);
}

int
apm_crit_exec(ainfo)
	struct apm_power_info *ainfo;
{

	if (ainfo->ac_state)
		return 0;

	if (ainfo->pack0.life + ainfo->pack1.life > 20)
		return 0;

	if ((ainfo->pack0.state == BSTATE_VALID ||
	     ainfo->pack0.state == BSTATE_NONE) &&
	    (ainfo->pack1.state == BSTATE_VALID ||
	     ainfo->pack1.state == BSTATE_NONE))
		return 1;

	return 0;
}
