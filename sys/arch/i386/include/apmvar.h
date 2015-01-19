/*	$NecBSD: apmvar.h,v 1.19 1998/03/14 07:07:40 kmatsuda Exp $	*/
/*	$NetBSD$	*/

#ifndef	ORIGINAL_CODE
/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
#endif	/* PC-98 */
/*
 *  Copyright (c) 1995 John T. Kohl
 *  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
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
 * 
 */

#ifndef __I386_APM_H__
#define __I386_APM_H__

#define APM_BIOS_FNCODE	0x9a
#define APM_SYSTEM_BIOS	0x1f

#define	APM_REG_HIGH(reg)	(((reg) >> NBBY) & ((1 << NBBY) - 1))
#define	APM_REG_LOW(reg)	((reg) & ((1 << NBBY) - 1))

#define	APM_MAJOR_VERS(info)	(APM_REG_HIGH(info))
#define	APM_MINOR_VERS(info)	(APM_REG_LOW(info))

/* support bits */
#define APM_16BIT_SUPPORTED	0x0001
#define APM_32BIT_SUPPORTED	0x0002
#define APM_IDLE_SLOWS		0x0004
#define APM_BIOS_PM_DISABLED	0x0008
#define APM_BIOS_PM_DISENGAGED	0x0010

/* error code */
#define	APM_ERR_CODE(code)	(APM_REG_HIGH(code))
#define	APM_ERR_PM_DISABLED	0x01
#define	APM_ERR_REALALREADY	0x02
#define	APM_ERR_NOTCONN		0x03
#define	APM_ERR_16ALREADY	0x05
#define	APM_ERR_16NOTSUPP	0x06
#define	APM_ERR_32ALREADY	0x07
#define	APM_ERR_32NOTSUPP	0x08
#define	APM_ERR_UNRECOG_DEV	0x09
#define	APM_ERR_ERANGE		0x0A
#define	APM_ERR_NOTENGAGED	0x0B
#define APM_ERR_UNABLE		0x60
#define APM_ERR_NOEVENTS	0x80
#define	APM_ERR_NOT_PRESENT	0x86

/* device code */
#define APM_DEV_APM_BIOS	0x0000
#define APM_DEV_ALLDEVS		0x0001
#define	APM_DEV_DISPLAY		0x0100
#define	APM_DEV_FLOPPY		0x0200
#define	APM_DEV_HD		0x0300
#define	APM_DEV_PRINTER		0x0400
#define	APM_ALL_UNITS		0x00ff

/* apm functions */
#define	APM_INSTALLATION_CHECK	0x00
#define	APM_REALMODE_CONNECT	0x01
#define	APM_16BIT_CONNECT	0x02
#define	APM_32BIT_CONNECT	0x03
#define APM_DISCONNECT		0x04
#define APM_CPU_IDLE		0x05
#define APM_CPU_BUSY		0x06

#define APM_SET_PWR_STATE	0x07
#define	APM_SYS_READY		0x0000
#define	APM_SYS_STANDBY		0x0001
#define	APM_SYS_SUSPEND		0x0002
#define	APM_SYS_OFF		0x0003

#define APM_PWR_MGT_ENABLE	0x08
#define	APM_MGT_DISABLE	0x0
#define	APM_MGT_ENABLE	0x1

#define APM_SYSTEM_DEFAULTS	0x09

#define APM_POWER_STATUS	0x0a
#define	APM_AC_MASK		0x01
#define	APM_AC_OFF		0x00
#define	APM_AC_ON		0x01
#define	APM_BATT_SPMASK		0x10
#define	APM_BATT_NONE		0x10
#define	APM_BATT_EMPTY		0x11
#define	APM_BATT_CHARGE		0x12
#define	APM_BATT_ABNORMAL	0x14
#define	APM_BATT_FULL		0x17
#define	APM_BATT_INVALID	0x08
#define	BATT_MASK		0x1f
#define	BATT1_LIFE(code)	(code & BATT_MASK)
#define	BATT2_LIFE(code)	((code >> NBBY) & BATT_MASK)

#define	APM_GET_PM_EVENT	0x0b
#define	APM_EVENT_CODE(code)	(APM_REG_LOW(code))
#define	APM_EVENT_NULL		0x00
#define	APM_EVENT_POWER		0x02
#define	APM_EVENT_MENU		0x04

#define	APM_PWR_MGT_ENGAGE	0x0f
#define	APM_MGT_DISENGAGE	0x0
#define	APM_MGT_ENGAGE		0x01

/* apm new functions (NEC) */
#define	APM_NEC_POWER		0x33

#define	APM_NEC_NEWPSTAT	0x3a
#define	BATTN1_LIFE(data)	(APM_REG_LOW(data))
#define	BATTN2_LIFE(data)	(APM_REG_HIGH(data))
#define	BATT_UNKNOWN		0xff

#define	APM_NEC_CONNECT		0x3e
#define	APM_NEC_CONNECT_ID	0x101

#define	APM_NECSMM_PORT		(0x6b8e)
#define	APM_NECSMM_EN		0x10
#define	APM_NECSMM_PORTSZ	0x01

/* structures */
#ifndef	_LOCORE
struct apm_reg_args {
	u_int32_t eax;
	u_int32_t ebx;
	u_int32_t ecx;
	u_int32_t edx;
};

struct apm_event_info {
#define	APM_STANDBY_REQ		0x0001
#define	APM_SUSPEND_REQ		0x0002
#define	APM_NORMAL_RESUME	0x0003
#define	APM_CRIT_RESUME		0x0004
#define	APM_BATTERY_LOW		0x0005
#define	APM_POWER_CHANGE	0x0006
#define	APM_UPDATE_TIME		0x0007
#define	APM_CRIT_SUSPEND_REQ	0x0008
#define	APM_USER_STANDBY_REQ	0x0009
#define	APM_USER_SUSPEND_REQ	0x000A
#define	APM_SYS_STANDBY_RESUME	0x000B
	u_int av_type;
	u_int av_seqnum;

	time_t av_time;
};

struct apm_battery {
	int life;

#define	BSTATE_VALID	0
#define	BSTATE_NONE	1
#define	BSTATE_CHARGE	2
#define	BSTATE_ABNORMAL	3
#define	BSTATE_UNKNOWN	4
#define	BSTATE_MAX	5
	int state;
};

struct apm_power_info {
	struct apm_battery pack0;
	struct apm_battery pack1;

	int ac_state;

	int cpu_state;
};

/* apm ioctl */
#define	APM_IOC_APM	_IOW('A', 1, int)
#define	APM_CTRL_ARG_MASK	0x0ffff
#define	APM_CTRL_REQ_SUSPEND	0x10000
#define	APM_CTRL_ACK_SUSPEND	0x20000
#define	APM_CTRL_REQ_STANDBY	0x30000
#define	APM_CTRL_SET_TOUTCNT	0x40000

#define	APM_IOG_POWER	_IOR('A', 2, struct apm_power_info)
#define	APM_IOG_EVENT	_IOR('A', 3, struct apm_event_info)

#define	APM_IOC_CPU	_IOWR('A', 4, int)
#define	CPU_CTRL_OFF	0
#define	CPU_CTRL_ON	1

#define	APM_IOC_CONNECT		_IO('A', 5)
#define	APM_IOC_CONNECT_BIOS	_IO('A', 6)

/* functions extern */
#ifdef _KERNEL
void apm_cpu_busy __P((void));
void apm_cpu_idle __P((void));
void apm_pow_off __P((void));
int apm_32b_call __P((int, struct apm_reg_args *));
#endif	/* _KERNEL */
#endif	/* !_LOCORE */

#endif /* __i386_apm_h__ */
