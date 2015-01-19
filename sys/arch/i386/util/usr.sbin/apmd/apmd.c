/*	$NecBSD: apmd.c,v 1.8 1998/03/14 07:11:44 kmatsuda Exp $	*/
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
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <machine/apmvar.h>

#include "apmprint.h"

#define	_PATH_LOGPID	"/var/run/apmd.pid"
char *PidFile = _PATH_LOGPID;

int ApmFd;
struct timeval tout;
u_char *suspend_exec;
u_char *resume_exec;
u_char *crit_exec;
u_char *dev_file;

static int power_status __P((int, int));
void apmd_alarm __P((int));
void apmd_exit __P((int));
void apmd_main __P((int));

int main __P((int, char **));

static int
power_status(fd, force)
	int fd;
	int force;
{
	u_char buf[1024];
	struct apm_power_info bstate;
	static struct apm_power_info last;

	if (apm_get_power(fd, &bstate) == 0)
	{
		if (force || bcmp(&bstate, &last, sizeof(bstate)))
		{
			apm_sprint_power(&bstate, buf);
			syslog(LOG_NOTICE, "%s\n", buf);
			if (apm_crit_exec(&bstate))
				system(crit_exec);

			last = bstate;
		}
	}
	else
		syslog(LOG_ERR, "cannot fetch power status: %m");

	return bstate.ac_state;
}

void
apmd_alarm(signo)
	int signo;
{

	power_status(ApmFd, 0);
	alarm(tout.tv_sec);
}

void
apmd_exit(signo)
	int signo;
{

	exit(0);
}

void
apmd_main(signo)
	int signo;
{
	int mode, error;
	struct apm_event_info ainfo;

	while (1)
	{
		error = ioctl(ApmFd, APM_IOG_EVENT, &ainfo);
		if (error < 0)
		{
			if (errno == ENOENT)
				return;

			syslog(LOG_WARNING, "strange event handling\n");
			exit(1);
		}

		switch (ainfo.av_type)
		{
		case APM_SUSPEND_REQ:
			syslog(LOG_NOTICE, "suspend requested (power off)\n");
			goto susp;

		case APM_USER_SUSPEND_REQ:
			syslog(LOG_NOTICE, "suspend requested (user)\n");

susp:
			system(suspend_exec);

			mode = APM_CTRL_ACK_SUSPEND;
			ioctl(ApmFd, APM_IOC_APM, &mode);
			break;

		case APM_NORMAL_RESUME:
			system(resume_exec);

			syslog(LOG_NOTICE, "system resumed");
			break;

		case APM_POWER_CHANGE:
			power_status(ApmFd, 1);
			break;

		default:
			break;
		}
	}
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int i, ch;
	extern int opterr, optind;
	extern char *optarg;
	FILE *fp;

	bzero(&tout, sizeof(tout));
	suspend_exec = "/etc/exec.apm_suspend";
	resume_exec = "/etc/exec.apm_resume";
	crit_exec = "/etc/exec.apm_crit";
	dev_file = "/dev/apm";

	opterr = 0;
	while ((ch = getopt(argc, argv, "t:s:r:c:f:")) != EOF)
	{

		switch (ch)
		{
		case 't':
			tout.tv_sec = atoi(optarg);
			break;

		case 's':
			suspend_exec = optarg;
			break;

		case 'r':
			resume_exec = optarg;
			break;

		case 'c':
			crit_exec = optarg;
			break;

		case 'f':
			dev_file = optarg;
			break;
		}

		opterr = 0;
	}

	argc -= optind;
	argv += optind;

	if ((ApmFd = open(dev_file, O_RDWR)) < 0)
	{
		perror(dev_file);
		exit(1);
	}
	close(ApmFd);

	if (fork() == 0)
	{

		chdir("/");

		for(i = 0; i < FOPEN_MAX; i ++)
			close(i);

		setsid();

		if ((ApmFd = open(dev_file, O_RDWR, 0)) < 0)
			exit(1);

		signal(SIGUSR1, apmd_main);
		signal(SIGTERM, apmd_exit);
		signal(SIGINT, apmd_exit);

		if (ioctl(ApmFd, APM_IOC_CONNECT))
		{
			syslog(LOG_WARNING, "connect failed\n");
			exit(1);
		}

		fp = fopen(PidFile, "w");
		if (fp != NULL)
		{
			fprintf(fp, "%d\n", getpid());
			(void) fclose(fp);
		}

		power_status(ApmFd, 1);

		if (tout.tv_sec)
		{
			signal(SIGALRM, apmd_alarm);
			alarm(tout.tv_sec);
		}

		while(1)
			pause();
	}

	exit(0);
}
