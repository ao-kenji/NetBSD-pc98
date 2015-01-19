/*	$NecBSD: slot_device.c,v 1.13 1999/07/26 06:36:53 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
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
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/slot_device.h>

struct slot_device_handle_tab slot_device_handle_head;
#define	splsdv	splimp

#define	DEREF(dh)	(dh->dh_ref ++)
#define	DEREL(dh)	(-- dh->dh_ref)
/********************************************************
 * slot device bus tag allocate
 ********************************************************/
slot_device_slot_tag_t
slot_device_slot_tag_allocate(sb, sc, aux, bus, type)
	slot_device_bus_tag_t sb;
	void *sc;
	void *aux;
	void *bus;
	slot_device_slot_type_t type;
{
	slot_device_slot_tag_t st;

	if (slot_device_handle_head.tqh_first == NULL)
		TAILQ_INIT(&slot_device_handle_head);

	st = malloc(sizeof(*st), M_DEVBUF, M_NOWAIT);
	if (st == NULL)
		return st;

	memset(st, 0, sizeof(*st));

	st->st_sbtag = sb;
	st->st_bus = bus;
	st->st_sc = sc;
	st->st_aux = aux;
	st->st_type = type;
	TAILQ_INIT(&st->st_dhtab);
	return st;
}
	
/********************************************************
 * slot device identifier (de)allocate
 ********************************************************/
slot_device_ident_t
slot_device_ident_allocate(st, xname, size)
	slot_device_slot_tag_t st;
	u_char *xname;
	u_int size;
{
	slot_device_ident_t di;
	
	di = malloc(sizeof(*di), M_DEVBUF, M_NOWAIT);
	if (di == NULL)
		return di;

	di->di_magic = NULL;
	if (size != 0)
	{
		di->di_magic = malloc(size, M_DEVBUF, M_NOWAIT);
		if (di->di_magic == NULL)
		{
			free(di, M_DEVBUF);
			return NULL;
		}
	}

	strncpy(di->di_xname, xname, 16);
	di->di_xname[15] = 0;
	di->di_sbtag = st->st_sbtag;
	di->di_length = size;
	di->di_idn = 0;
	di->di_ids = "not initialized";
	return di;
}	

void
slot_device_ident_deallocate(di)
	slot_device_ident_t di;
{

	if (di->di_length != 0)
		free(di->di_magic, M_DEVBUF);
	free(di, M_DEVBUF);
}

/********************************************************
 * slot device handle (de)allocate
 ********************************************************/
slot_device_handle_t
slot_device_handle_lookup(st, dip, flags)
	slot_device_slot_tag_t st;
	slot_device_ident_t dip;
	u_int flags;
{
	register slot_device_handle_t dh;
	register slot_device_ident_t ti;

	for (dh = slot_device_handle_head.tqh_first; dh != NULL; 
	     dh = dh->dh_chain.tqe_next)
	{
		ti = dh->dh_id;
		if (ti->di_sbtag != dip->di_sbtag)
			continue;
		if (ti->di_length != dip->di_length)
			continue;
		if (strncmp(ti->di_xname, dip->di_xname, 16) != 0)
			continue;
		if (ti->di_length == 0 ||
		    bcmp(ti->di_magic, dip->di_magic, ti->di_length) == 0)
		{
			if ((dh->dh_flags & flags) != 0)
				continue;
			break;
		}
	}
	return dh;
}

int
slot_device_handle_allocate(st, dip, flags, dhp)
	slot_device_slot_tag_t st;
	slot_device_ident_t dip;
	int flags;
	slot_device_handle_t *dhp;
{
	slot_device_handle_t dh;
	int error = 0, s = splsdv();

	dh = slot_device_handle_lookup(st, dip, 0);
	if (dh != NULL)
	{
		if (dh->dh_flags & (DH_ACTIVE | DH_CREATE | DH_BUSY))
			error = EBUSY;
		else
			dh->dh_flags |= DH_BUSY;
		goto out;
	}

	dh = malloc(sizeof(*dh), M_DEVBUF, M_NOWAIT);
	if (dh == NULL)
	{
		error = ENOMEM;
		goto out;
	}

	memset(dh, 0, sizeof(*dh));
	dh->dh_id = dip;
	dh->dh_flags |= (DH_CREATE | DH_BUSY);
	TAILQ_INSERT_HEAD(&slot_device_handle_head, dh, dh_chain);

out:
	if (error == 0)
		DEREF(dh);
	splx(s);
	*dhp = dh;
	return error;
}
		
void
slot_device_handle_deallocate(st, dh)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
{
	int s = splsdv();

	DEREL(dh);
	if (dh->dh_ref != 0)
	{
		splx(s);
		return;
	}

	if (dh->dh_flags & DH_ACTIVE)
		slot_device_handle_deactivate(st, dh);
	TAILQ_REMOVE(&slot_device_handle_head, dh, dh_chain);
	splx(s);

	if (dh->dh_id != NULL)
		slot_device_ident_deallocate(dh->dh_id);
	free(dh, M_DEVBUF);	
}

/********************************************************
 * handle activate deactivate
 ********************************************************/
void
slot_device_handle_activate(st, dh)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
{
	int s = splsdv();

	if (dh->dh_flags & DH_ACTIVE)
		panic("slot_device_handle_activate: already active");

	TAILQ_INSERT_HEAD(&st->st_dhtab, dh, dh_dvgchain);
	dh->dh_flags |= DH_ACTIVE;
	dh->dh_flags &= ~(DH_INACTIVE | DH_CREATE);
	dh->dh_stag = st;
	DEREF(dh);
	splx(s);
}

void
slot_device_handle_deactivate(st, dh)
	slot_device_slot_tag_t st;
	slot_device_handle_t dh;
{
	int s = splsdv();

	if ((dh->dh_flags & DH_ACTIVE) == 0)
		panic("slot_device_handle_deactivate: not active");

	TAILQ_REMOVE(&st->st_dhtab, dh, dh_dvgchain);
	dh->dh_flags &= ~DH_ACTIVE;
	dh->dh_flags |= DH_INACTIVE;
	dh->dh_stag = NULL;
	DEREL(dh);
	splx(s);
}

/********************************************************
 * allocate interrupt args 
 ********************************************************/
slot_device_intr_arg_t
slot_device_allocate_intr_arg(dh, n, intrfunc, arg)
	slot_device_handle_t dh;
	int n;
	int (*intrfunc) __P((void *));
	void *arg;
{
	struct slot_device_intr_arg *sa;

	sa = malloc(sizeof(*sa), M_DEVBUF, M_NOWAIT);
	if (sa == NULL)
		return sa;

	memset(sa, 0, sizeof(*sa));
	sa->sa_handler = intrfunc;
	sa->sa_arg = arg;
	sa->sa_dh = dh;
	sa->sa_pin = n;

	return sa;
}

int
slot_device_deallocate_intr_arg(sa)
	slot_device_intr_arg_t sa;
{

	free(sa, M_DEVBUF);
	return 0;
}
/********************************************************
 * debug section
 ********************************************************/
void print_all_dh __P((void));

void
print_all_dh()
{
	slot_device_handle_t dh;
	u_char c = 0;
	int i;

	for (dh = slot_device_handle_head.tqh_first; dh != NULL;
	     dh = dh->dh_chain.tqe_next)
	{
		printf("-DEVICE HANDLE-\n");
		printf("tag 0x%lx handle 0x%lx flags 0x%x refc 0x%x\n",
			(u_long) dh->dh_stag, (u_long) dh,
			dh->dh_flags, dh->dh_ref);

		if (dh->dh_id)
		{
			printf("name %s\n", dh->dh_id->di_xname);
			for (i = 0; i < dh->dh_id->di_length; i ++)
			{
				c = ((u_char *)dh->dh_id->di_magic)[i];
				if (c >= 0x20 && c < 128)
					printf("%c", c);
			}
			printf("\n");
		}

		for (i = 0; i < SLOT_DEVICE_NIM; i ++)
			printf("iom[%d] t 0x%lx  h 0x%lx b 0x%lx \n",
			  i,
			 (u_long)dh->dh_space.sp_imh[i].im_tag,
			 (u_long)dh->dh_space.sp_imh[i].im_handle,
			 (u_long)dh->dh_space.sp_imh[i].im_bmap);
		for (i = 0; i < SLOT_DEVICE_NPIN; i ++)
			printf("ch[%d] tag 0x%lx  handle 0x%lx\n",
			 i,
			 (u_long)dh->dh_space.sp_pinh[i].ch_tag,
			 (u_long)dh->dh_space.sp_pinh[i].ch_handle);
	}
}
