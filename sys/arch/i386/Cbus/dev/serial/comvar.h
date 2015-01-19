/*	$NecBSD: comvar.h,v 1.3 1999/01/26 07:49:42 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998
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

#ifndef	_COMVAR_H_
#define	_COMVAR_H_

/***********************************************************
 * MACROS
 ***********************************************************/
#define	COM_MAXTTY	2
#define	COM_IBUFSIZE	(2048)
#define	COM_OBUFSIZE	(1024)

/***********************************************************
 * attach args
 ***********************************************************/
struct ser_softc;
struct com_softc;
struct comcons_switch;
struct sertty_softc;
struct comspeed;

struct com_space_handle {
	bus_space_tag_t cs_iot;			/* io tag */
	bus_space_handle_t cs_ioh;		/* io handle */
	bus_space_handle_t cs_spioh;		/* fifo io handle */

	bus_space_tag_t cs_icrt;		/* interupt control io tag */
	bus_space_handle_t cs_icrh;		/* interupt control io handle */
	void (*cs_control_intr) __P((struct com_softc *, u_int8_t));

	bus_space_tag_t cs_msrt;		/* modem status io tag */
	bus_space_handle_t cs_msrh;		/* modem status io handle */
	u_int8_t (*cs_read_msr) __P((struct com_softc *));
};

struct commulti_attach_args {
	int ca_slave;				/* slave number */

	struct com_space_handle ca_h;		/* space handle */

	u_int ca_type;				/* hardware major type */
	u_int ca_subtype;			/* hardware subtype */
	u_int ca_hwflags;			/* hardware control flags */
	u_int ca_cfgflags;			/* dvcfg */
	int ca_console;				/* I am console device */

	u_int ca_freq;				/* board speed controls */
	struct comspeed *ca_speedtab;
	void ((*ca_speedfunc) __P((struct com_softc *, int)));

	int (*ca_emulintr) __P((void *));	/* emulate a hardware intr */
	void *ca_arg;				/* args */

#define	SER_PRI_PROBEHW		0x0000		/* should probe core chips */
#define	SER_PRI_MANFID		0x0001		/* trust other informations */
	int  ca_probepri;			/* probe pri */

	void *ca_m;				/* OUT: com_softc */
};

struct sertty_attach_args {
	int ca_id;				/* tty line id */
	struct com_softc *ca_sc;		/* com_softc */
};

/***********************************************************
 * sertty structure
 ***********************************************************/
struct sertty_softc {
	struct device sc_dev;
	
	struct com_softc *sc_com;	/* my hardware */

	struct tty *sc_tty;		/* tty line for us */
	int sc_openf;			/* open */
	int sc_wopen;			/* waitors */

	struct termios sc_termios;	/* initial termios */
	u_char sc_swflags;		/* (backup) sw flags */
};

/**************************************************************
 * com_softc structure
 **************************************************************/
struct v_ring_buf {
	volatile int vr_cc;			/* character count */
	int vr_top;				/* write position */
	int vr_tail;				/* read position */
	int vr_size;				/* size of rbuf */
	int vr_highwater;			/* hiwater */
	int vr_lowater;				/* lowater */
	int vr_mask;				/* position mask */
};

struct com_softc {
	struct device sc_dev;
	struct ser_softc *sc_ser;		/* my common parent */

	int sc_enable;

	TAILQ_ENTRY(com_softc) com_chain; 	/* active chain */

	struct com_switch *sc_cswp;		/* com switch structure */
	struct comcons_switch *sc_ccswp;	/* comcons switch if exists */

	/******************************************************
	 * software timer
	 ******************************************************/
	int sc_swtref;				/* software timer refc */

	/******************************************************
	 * hardware defintions
	 ******************************************************/
	u_int sc_type;				/* major type */
	u_int sc_subtype;			/* minor type */

	/******************************************************
	 * board speed controls
	 ******************************************************/
	u_int sc_freq;
	struct comspeed *sc_speedtab;
	void ((*sc_speedfunc) __P((struct com_softc *, int)));

	/******************************************************
	 * tty activate and deactivate
	 ******************************************************/
	int (*sc_sertty_activate) __P((struct com_softc *));
	int (*sc_sertty_deactivate) __P((struct com_softc *));

	/******************************************************
	 * tty lines connected
	 ******************************************************/
	int sc_nts;				/* num tty lines */
	struct sertty_softc *sc_ts[COM_MAXTTY]; /* tty lines */
	struct tty *sc_ctty; 			/* current tty line */
	struct sertty_softc *sc_cts;		/* current sertty */

	/******************************************************
	 * closing (drain control)
	 ******************************************************/
	int sc_state;				/* closing state */
#define	COM_STATE_CLOSED		0
#define	COM_STATE_CLOSING		1
#define	COM_STATE_CLOSING_TIMEOUT	2
#define	COM_STATE_DRAINING		3
	int sc_counter;				/* closing timeout counter */

	/******************************************************
	 * error counter
	 ******************************************************/
	int sc_overflows;			/* error */	
	int sc_floods;					
	int sc_errors;				

	/******************************************************
	 * tty control data
	 ******************************************************/
	struct termios sc_termios;		/* real termios data */	
	int sc_ospeed;				/* current hardware speed */ 

	/******************************************************
	 * control flags
	 ******************************************************/
	u_int sc_hwflags;			/* hw flags */	
#define	COM_HW_EMM		0xffff
#define	COM_HW_NOIEN		0x0001
#define	COM_HW_FIFO		0x0002
#define	COM_HW_KGDB		0x0020
#define	COM_HW_CONSOLE		0x0040
#define	COM_HW_SHAREDINT 	0x0100
#define	COM_HW_POLL		0x0200
#define	COM_HW_ACCEL		0x0400
#define	COM_HW_INTERNAL		0x0800
#define	COM_HW_INDEXED		0x1000
#define	COM_HW_TIMER		0x2000
#define	COM_HW_AFE		0x8000
#define	COM_HW_USER		(COM_HW_NOIEN | COM_HW_AFE)

	u_int sc_swflags;			/* sw flags */
#define	COM_SW_SOFTCAR	0x01
#define	COM_SW_CLOCAL	0x02
#define	COM_SW_CRTSCTS	0x04
#define	COM_SW_MDMBUF	0x08

	u_int sc_cflags; 			/* cr flags */
#define	COM_POLL_QUEUED	0x0001
#define	COM_TTY_WANTED	0x0002
#define	COM_MODE_FIXED	0x1000
#define	COM_RTSOFF	0x2000

	/******************************************************
	 * modem signal and flow control
	 ******************************************************/
	u_int sc_rtscts;			/* rts/cts */
	u_int8_t sc_msr, sc_nmsr;		/* msr */
	u_int8_t sc_mdcd;			/* carrier */
	u_int8_t sc_noop;			/* pad */

	/******************************************************
	 * recieve data
	 ******************************************************/
	u_int8_t *sc_rdata; 			/* recieve data */	
	u_int8_t *sc_rstat; 			/* recieve status */
	struct v_ring_buf sc_invrb;				

	/******************************************************
	 * transmit data
	 ******************************************************/
	int sc_maxputs;				/* max burst outputs */	

	int sc_cc;				/* nd count */
	u_int8_t *ob_cp;			/* current pointer */
	int ob_cc;				/* num bytes */	

	/******************************************************
	 * interrupt emulate
	 ******************************************************/
	int sc_vicr;				/* virtual int control */
	int sc_txintr;				/* tx intr */
#define	COM_TXINTR_ASSERT  0x1
	int sc_check;				/* timeout watch dog request */

	int (*sc_emulintr) __P((void *));	/* emulate hardware intr */
	void *sc_arg;				/* args */		
};

/**************************************************************
 * function call branch table
 **************************************************************/
struct com_switch {
	void	(*cswt_hwattach) __P((struct com_softc *));
	int	(*cswt_hwactivate) __P((struct com_softc *));
	int	(*cswt_hwdeactivate) __P((struct com_softc *, int));
	int	(*cswt_hwdrain) __P((struct com_softc *, int));
	void	(*cswt_hwfifo) __P((struct com_softc *, int));

	void	((*cswt_start)) __P((struct com_softc *, struct tty *));
	int     ((*cswt_intr) __P((void *)));
	int	cswt_ipl;

	int	(*cswt_ioctl)
		__P((struct com_softc *, u_long, caddr_t, int, struct proc *));
	int	(*cswt_comparam)
		__P((struct com_softc *, struct tty *, struct termios *));
	void	(*cswt_modem) __P((struct com_softc *, struct tty *));
	u_int	(*cswt_errmap) __P((struct com_softc *, u_int, u_int8_t));

	u_long	cswt_speed;			/* default rate */
};

struct comspeed {
	u_long  cs_speed;
	int	cs_div;
};

/* major hardware */
#define	COM_HW_I8251	0x000000
#define	COM_HW_I8251_F	0x010000
#define	COM_HW_I8251_C	0x020000
#define	COM_HW_NS16550	0x100000
#define	COM_HW_HAYES	0x110000
#define	COM_HW_SERA	0x120000
#define	COM_HW_SERB	0x130000

/* minor hardware */
#define	COM_HW_DEFAULT		0
#define	COM_HW_I8251F_NDIV	(COM_HW_I8251_F | 0x0000)
#define	COM_HW_I8251F_EDIV	(COM_HW_I8251_F | 0x0001)
#define	COM_HW_RSA98		(COM_HW_SERA | 0x0000)
#define	COM_HW_RSA98III		(COM_HW_SERA | 0x0001)
#define	COM_HW_RSB2000		(COM_HW_SERB | 0x0000)
#define	COM_HW_MCRS98		(COM_HW_NS16550 | 0x0001)
#define	COM_HW_RSB384		(COM_HW_NS16550 | 0x0002)
#define	COM_HW_NSMODEM  	(COM_HW_NS16550 | 0x0003)

/***********************************************************
 * Bit operation macro
 ***********************************************************/
#define	SET(t, f)	((t) |= (f))
#define	CLR(t, f)	((t) &= ~(f))
#define	ISSET(t, f)	((t) & (f))

/***********************************************************
 * Special operation macro
 ***********************************************************/
#define	IPL_COM	IPL_SERIAL
#define	splcom	splserial
#define	setsofttty setsoftserial

#if defined(i386)
#define	EMUL_INTR_REQUIRED(sc)	{			\
	if ((sc)->sc_emulintr != NULL)			\
		((*(sc)->sc_emulintr) ((sc)->sc_arg));	\
}
#else	/* !i386 */
#define	EMUL_INTR_REQUIRED(sc)
#endif	/* !i386 */
#endif	/* !_COMVAR_H_ */
