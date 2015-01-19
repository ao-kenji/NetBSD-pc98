/*	$NecBSD: sersubr.h,v 1.3 1999/07/23 20:57:31 honda Exp $	*/
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
#ifndef	_SERSUBR_H_
#define	_SERSUBR_H_

/* error operations */
#define	COM_SOVF	0	/* software buffer overflow */
#define	COM_HOVF	1	/* hardware buffer overflow */
void comoerr __P((struct com_softc *, int));
void comsetfake __P((struct com_softc *, struct termios *, int *));
struct comspeed *comfindspeed __P((struct comspeed *, u_long));
void com_setup_softc __P((struct commulti_attach_args *, struct com_softc *));

/* modem signal */
void commsc __P((struct com_softc *, struct tty *));

/* ring buffer operations */
#define	RBOSTAT	0xff
#define	RBFLUSH	((u_int) -1)

static __inline void rb_init __P((struct v_ring_buf *, u_int));
static __inline void rb_flush __P((struct v_ring_buf *));
static __inline u_int rb_wop __P((struct v_ring_buf *));
static __inline u_int rb_wer __P((struct v_ring_buf *));
static __inline u_int rb_rop __P((struct v_ring_buf *));
static __inline void rb_adj __P((struct v_ring_buf *, int));
static __inline u_int rb_rest __P((struct v_ring_buf *));
static __inline u_int rb_highwater __P((struct v_ring_buf *info));
static __inline u_int rb_lowater __P((struct v_ring_buf *info));
static __inline void rb_input __P((struct v_ring_buf *, u_int8_t *, u_int8_t *, u_int8_t, u_int8_t));

static __inline void
rb_init(info, size)
	struct v_ring_buf *info;
	u_int size;
{

	info->vr_cc = info->vr_top = info->vr_tail = 0;
	info->vr_size = size;
	info->vr_highwater = (size * 2) / 3;
	info->vr_lowater = size / 3;
	info->vr_mask = size - 1;
}

static __inline void
rb_flush(info)
	struct v_ring_buf *info;
{

	info->vr_cc = info->vr_top = info->vr_tail = 0;
}

static __inline u_int
rb_wop(info)
	struct v_ring_buf *info;
{

	info->vr_cc ++;
	return (info->vr_top++ & info->vr_mask);
}

static __inline u_int
rb_wer(info)
	struct v_ring_buf *info;
{

	return ((info->vr_top - 1) & info->vr_mask);
}

static __inline u_int
rb_rop(info)
	struct v_ring_buf *info;
{

	return (info->vr_tail & info->vr_mask);
}

static __inline void
rb_adj(info, adj)
	struct v_ring_buf *info;
	int adj;
{

	if (info->vr_cc < adj)
		return;
	info->vr_cc -= adj;
	info->vr_tail += adj;
}

static __inline u_int
rb_rest(info)
	struct v_ring_buf *info;
{

	return info->vr_size - info->vr_cc;
}

static __inline u_int
rb_lowater(info)
	struct v_ring_buf *info;
{

	return (info->vr_cc <= info->vr_lowater);
}

static __inline u_int
rb_highwater(info)
	struct v_ring_buf *info;
{

	return (info->vr_cc >= info->vr_highwater);
}

static __inline void
rb_input(rb, databuf, statbuf, c, lsr)
	struct v_ring_buf *rb;
	u_int8_t *databuf;
	u_int8_t *statbuf;
	u_int8_t c;
	u_int8_t lsr;
{
	register u_int idx;

	if (rb_rest(rb))
	{
		idx = rb_wop(rb);
		databuf[idx] = c;
		statbuf[idx] = lsr;
	}
	else
	{
		idx = rb_wer(rb);
		statbuf[rb_wer(rb)] = RBOSTAT;
	}
}
#endif	/* !_SERSUBR_H_ */
