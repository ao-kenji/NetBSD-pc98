/*	$NecBSD: wdc_pc98_Cbus.c,v 1.3 1999/07/09 05:51:47 honda Exp $	*/
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

/*
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>

#include <vm/vm.h>

#include <i386/Cbus/dev/atapi/wdcreg.h>
#include <i386/Cbus/dev/atapi/wdcvar.h>
#include <i386/Cbus/dev/atapi/wdchw.h>
#include <i386/Cbus/dev/atapi/wdc_pc98_Cbus.h>
#include <dev/ata/atareg.h>

/*********************************************************
 * Proto
 *********************************************************/
static void wdc_data_write_ide98 __P((struct channel_softc *, u_int8_t *, u_int, int));
static void wdc_data_read_ide98 __P((struct channel_softc *, u_int8_t *, u_int, int));
static void wdc_data_write_pcide __P((struct channel_softc *, u_int8_t *, u_int, int));
static void wdc_data_read_pcide __P((struct channel_softc *, u_int8_t *, u_int, int));
static void wdc_data_write_ninja __P((struct channel_softc *, u_int8_t *, u_int, int));
static void wdc_data_read_ninja __P((struct channel_softc *, u_int8_t *, u_int, int));
static void wdc_open_ninja __P((struct channel_softc *));
static void wdc_close_ninja __P((struct channel_softc *));
static void wdc_data_write_kxl __P((struct channel_softc *, u_int8_t *, u_int, int));
static void wdc_data_read_kxl __P((struct channel_softc *, u_int8_t *, u_int, int));
static void wdc_open_kxl __P((struct channel_softc *));
static void wdc_close_kxl __P((struct channel_softc *));
static void wdc_select_bank_IDE98 __P((struct channel_softc *, u_int));

/*********************************************************
 * Bank select functions
 *********************************************************/
static void
wdc_select_bank_IDE98(chp, bank)
	struct channel_softc *chp;
	u_int bank;
{

	wdc_cr0_write(chp, ide98_mwcr, MWCR_READ | MWCR_WRITE);
	if (bank == WDC_BANK_UNKNOWN)
		bank = BSR_CH0_RACC;
	else
		bank = (BSR_CH0_RACC | BSR_CH0_WACC) << WDC_BANK_NO(chp, bank);
	wdc_cr0_write(chp, ide98_bsr, bank);
}

/*********************************************************
 * data access methods (multi)
 *********************************************************/
/* ide98 fast mmio */
static void
wdc_data_read_ide98(chp, bufp, cnt, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int cnt;
	int flags;
{
	u_int len;

	if (chp->ch_memh != NULL && (cnt % DEV_BSIZE) == 0)
	{
		len = cnt / DEV_BSIZE;
		wdc_cr0_write(chp, ide98_mbsr, MBSR_BM);
		wdc_cr0_write(chp, ide98_rblr, RBLR_RDIS);
		wdc_cr0_write(chp, ide98_iomr, IOM_ENA | IOM_MEM);	
		wdc_cr0_write(chp, ide98_rbpcr, 0x0);
		wdc_cr0_write(chp, ide98_rblr, len);
		for ( ;len > 0; len --, bufp += DEV_BSIZE)
			bus_space_read_region_4(chp->ch_memt, chp->ch_memh,
				IDE98_OFFSET, (u_int32_t *) bufp, DEV_BSIZE >> 2);	
		wdc_cr0_write(chp, ide98_rblr, 0x0);
		wdc_cr0_write(chp, ide98_iomr, 0x0);
		wdc_cr0_write(chp, ide98_mbsr, MBSR_ROM1);
		return;
	}

	wdc_cr0_write(chp, ide98_iomr, IOM_ENA);
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_cridx, wd_data);
	bus_space_read_multi_2(chp->data16iot, chp->data16ioh, wd_edata,
			       (u_int16_t *) bufp, cnt >> 1);
	wdc_cr0_write(chp, ide98_iomr, 0);
}

static void
wdc_data_write_ide98(chp, bufp, cnt, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int cnt;
	int flags;
{
	u_int len;

	if (chp->ch_memh != NULL && (cnt % DEV_BSIZE) == 0)
	{
		len = cnt / DEV_BSIZE;
		wdc_cr0_write(chp, ide98_mbsr, MBSR_BM);
		wdc_cr0_write(chp, ide98_iomr, IOM_ENA | IOM_MEM);	
		bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_cridx,
				  wd_data);
		for ( ;len > 0; len --, bufp += DEV_BSIZE)
			bus_space_write_region_4(chp->ch_memt, chp->ch_memh, 
				IDE98_OFFSET, (u_int32_t *) bufp,
				DEV_BSIZE >> 2);
		wdc_cr0_write(chp, ide98_iomr, 0x0);
		wdc_cr0_write(chp, ide98_mbsr, MBSR_ROM1);
		return;
	}

	wdc_cr0_write(chp, ide98_iomr, IOM_ENA);
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_cridx, wd_data);
	bus_space_write_multi_2(chp->data16iot, chp->data16ioh, wd_edata,
				(u_int16_t *) bufp, cnt >> 1);
	wdc_cr0_write(chp, ide98_iomr, 0);
}

/* PCIDE fast mmio read/write */
#define	WDC_MAP_WAIT	1000
static int wdc_pcide_load_mem __P((struct channel_softc *));
static int wdc_pcide_unload_mem __P((struct channel_softc *));

static int
wdc_pcide_load_mem(chp)
	struct channel_softc *chp;
{

	/* map */
	wdc_cr1_write(chp, wd_mctlr, WDMC_MEMMAP | WDMC_MEMRW);
	delay(1);

	/* check busy state */
	if ((wdc_cr1_read(chp, wd_mctlr) & WDMC_BUSY) == 0)
		return 0;

	/* give up! */
	printf("%s: mem map failure\n", chp->wdc->sc_dev.dv_xname);
	(void) wdc_pcide_unload_mem(chp);
	return EIO;
}

static int
wdc_pcide_unload_mem(chp)
	struct channel_softc *chp;
{
	int error = 0, cnt;

	/* wait for unbusy state */
	for (cnt = WDC_MAP_WAIT; cnt > 0; cnt --)
		if ((wdc_cr1_read(chp, wd_mctlr) & WDMC_BUSY) == 0)
			break;

	/* check timeout */
	if (cnt <= 0)
	{
		/* timeout! */
		error = EIO;
		printf("%s: mem unmap timeout\n", chp->wdc->sc_dev.dv_xname);

		/* unmap */
		wdc_cr1_write(chp, wd_mctlr, 0);
		delay(1);

		/* wait for unbusy state */
		for (cnt = WDC_MAP_WAIT; cnt > 0; cnt --)
			if (!(wdc_cr1_read(chp, wd_mctlr) & WDMC_BUSY))
				break;
	}

	/* unmap */
	wdc_cr1_write(chp, wd_mctlr, 0);
	delay(1);
	return error;
}

static void
wdc_data_read_pcide(chp, bufp, len, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int len;
	int flags;
{
	u_int nlen;

	if (chp->ch_memh != NULL && (len % DEV_BSIZE) == 0)
	{
		if (wdc_pcide_load_mem(chp) != 0)
			goto out;

		for ( ; len > 0; bufp += nlen, len -= nlen)
		{
			nlen = (len > WDMC_BUFSZ) ? WDMC_BUFSZ : len;
			bus_space_read_region_4(chp->ch_memt, chp->ch_memh, 0,
				(u_int32_t *) bufp, nlen >> 2);	
		}	

		(void) wdc_pcide_unload_mem(chp);
		return;
	}

out:
	bus_space_read_multi_2(chp->data16iot, chp->data16ioh, wd_data,
		(u_int16_t *) bufp, len >> 1);
}

static void
wdc_data_write_pcide(chp, bufp, len, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int len;
	int flags;
{
	u_int nlen;

	if (chp->ch_memh != NULL && (len % DEV_BSIZE) == 0)
	{
		if (wdc_pcide_load_mem(chp) != 0)
			goto out;

		for ( ; len > 0; bufp += nlen, len -= nlen)
		{
			nlen = (len > WDMC_BUFSZ) ? WDMC_BUFSZ : len;
			bus_space_write_region_4(chp->ch_memt, chp->ch_memh, 0,
				(u_int32_t *) bufp, nlen >> 2);	
		}	

		(void) wdc_pcide_unload_mem(chp);
		return;
	}

out:
	bus_space_write_multi_2(chp->data16iot, chp->data16ioh, wd_data,
		(u_int16_t *) bufp, len >> 1);
}

/* Ninja chip */
static void
wdc_open_ninja(chp)
	struct channel_softc *chp;
{

	wdc_cr1_write(chp, wd_mctlr, WDMC_WB_ENABLE | WDMC_WB_CORER);
}

static void
wdc_close_ninja(chp)
	struct channel_softc *chp;
{

	wdc_cr1_write(chp, wd_mctlr, WDMC_WB_ENABLE);
}

static void
wdc_data_read_ninja(chp, bufp, len, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int len;
	int flags;
{
	u_int nlen;

	if (chp->ch_memh != NULL && (len % sizeof(u_int32_t)) == 0)
	{
		wdc_cr1_write(chp, wd_mctlr, WDMC_WB_ENABLE | WDMC_WB_BUS32 | 
				  	     WDMC_WB_CORER | WDMC_WB_FIFOEN);

		for ( ; len > 0; bufp += nlen, len -= nlen)
		{
			nlen = (len > WDMC_WB_BUFSZ) ? WDMC_WB_BUFSZ : len;
			bus_space_read_region_4(chp->ch_memt, chp->ch_memh, 0,
			          (u_int32_t *) bufp, nlen >> 2);	
	 	}	

		wdc_cr1_write(chp, wd_mctlr, WDMC_WB_ENABLE | WDMC_WB_CORER);
		return;
	}

	bus_space_read_multi_2(chp->data16iot, chp->data16ioh, wd_data,
		(u_int16_t *) bufp, len >> 1);
}

static void
wdc_data_write_ninja(chp, bufp, len, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int len;
	int flags;
{
	u_int nlen;

	if (chp->ch_memh != NULL && (len % sizeof(u_int32_t)) == 0)
	{
		wdc_cr1_write(chp, wd_mctlr, WDMC_WB_ENABLE | WDMC_WB_BUS32 | 
				             WDMC_WB_CORER | WDMC_WB_FIFOEN);

		for ( ; len > 0; bufp += nlen, len -= nlen)
		{
			nlen = (len > WDMC_WB_BUFSZ) ? WDMC_WB_BUFSZ : len;
			bus_space_write_region_4(chp->ch_memt, chp->ch_memh, 0,
				(u_int32_t *) bufp, nlen >> 2);	
		}	

		wdc_cr1_write(chp, wd_mctlr, WDMC_WB_ENABLE | WDMC_WB_CORER);
		return;
	}

	bus_space_write_multi_2(chp->data16iot, chp->data16ioh, wd_data,
		(u_int16_t *) bufp, len >> 1);
}

/* KXL chip */
#define	WDC_KXL_DELAY	delay(1)

static void
wdc_open_kxl(chp)
	struct channel_softc *chp;
{

	/* reset smit */
	wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_RST | WDMC_KXL_SMITR);
	delay(100);

	/* reset core */
	wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_RST | WDMC_KXL_CORER);
	delay(100);

	/* enable core and smit */
	wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_ENABLE | WDMC_KXL_CORER);
	delay(10);
}

static void
wdc_close_kxl(chp)
	struct channel_softc *chp;
{

	/* shutdown smit first */
	wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_COREEN);
	delay(10);

	/* shutdown core */
	wdc_cr1_write(chp, wd_mctlr, 0);
	delay(10);
}

static void
wdc_data_read_kxl(chp, bufp, len, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int len;
	int flags;
{
	u_int nlen;

	if (chp->ch_memh != NULL && (len % sizeof(u_int32_t)) == 0)
	{
		for ( ; len > 0; bufp += nlen, len -= nlen)
		{
			wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_ENABLE | 
						     WDMC_KXL_CORER);
			WDC_KXL_DELAY;
			nlen = (len > WDMC_WB_BUFSZ) ? WDMC_WB_BUFSZ : len;
			bus_space_read_region_4(chp->ch_memt, chp->ch_memh, 0,
			          (u_int32_t *) bufp, nlen >> 2);	
	 	}	
	}
	else
	{
		wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_ENABLE);
		WDC_KXL_DELAY;
		bus_space_read_multi_2(chp->data16iot, chp->data16ioh, wd_data,
			(u_int16_t *) bufp, len >> 1);
	}

	wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_ENABLE | WDMC_KXL_CORER);
	WDC_KXL_DELAY;
}

static void
wdc_data_write_kxl(chp, bufp, len, flags)
	struct channel_softc *chp;
	u_int8_t *bufp;
	u_int len;
	int flags;
{
	u_int nlen;

	if (chp->ch_memh != NULL && (len % sizeof(u_int32_t)) == 0)
	{
		for ( ; len > 0; bufp += nlen, len -= nlen)
		{
			wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_ENABLE | 
						     WDMC_KXL_CORER);
			WDC_KXL_DELAY;
			nlen = (len > WDMC_WB_BUFSZ) ? WDMC_WB_BUFSZ : len;
			bus_space_write_region_4(chp->ch_memt, chp->ch_memh, 0,
				(u_int32_t *) bufp, nlen >> 2);	
		}	
	}
	else
	{
		wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_ENABLE);
		WDC_KXL_DELAY;

		bus_space_write_multi_2(chp->data16iot, chp->data16ioh, wd_data,
			(u_int16_t *) bufp, len >> 1);
	}

	wdc_cr1_write(chp, wd_mctlr, WDMC_KXL_ENABLE | WDMC_KXL_CORER);
	WDC_KXL_DELAY;
}

/**************************************************************
 * Cbus hardware info
 **************************************************************/

struct wdc_pc98_hw wdc_pc98_hw_ibm = {
	{
		"ATbus",
		WDC_DTYPE_GENERIC,

		WDC_ACCESS_PLAIN,
		2,

		{ NULL,
		  NULL,
		  NULL,
		  wdc_cr0_read_plain,
		  wdc_cr0_write_plain,
		  wdc_cr1_read_plain,
		  wdc_cr1_write_plain,
		  wdc_data_read_generic,
		  wdc_data_write_generic
		},

		WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_MODE,
		3,
		0
	},

	8,
	2,

	BUS_SPACE_IAT_1,
	0,
	8,

	BUS_SPACE_IAT_1,
	0x206,
	2,
};

struct wdc_pc98_hw wdc_pc98_hw_ibmuni = {
	{
		"ATbus-uni",
		WDC_DTYPE_GENERIC,

		WDC_ACCESS_PLAIN,
		2,

		{ NULL,
		  NULL,
		  NULL,
		  wdc_cr0_read_plain,
		  wdc_cr0_write_plain,
		  wdc_cr1_read_plain,
		  wdc_cr1_write_plain,
		  wdc_data_read_generic,
		  wdc_data_write_generic
		},

		WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_MODE,
		3,
		0
	},

	8,
	1,

	BUS_SPACE_IAT_1,
	0,
	16,

	BUS_SPACE_IAT_1,
	8,
	8,
};

static bus_addr_t wdc_pc98_hw_iat_integral1[] = {
	0x0,
	0x0,			/* no alt status */	
};	

struct wdc_pc98_hw wdc_pc98_hw_integral = {
	{
		"ATbusI",
		WDC_DTYPE_GENERIC,

		WDC_ACCESS_PLAIN,
		2,

		{ NULL,
		  NULL,
		  NULL,
		  wdc_cr0_read_plain,
		  wdc_cr0_write_plain,
		  wdc_cr1_read_plain,
		  wdc_cr1_write_plain,
		  wdc_data_read_generic,
		  wdc_data_write_generic
		},

		WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_MODE,
		3,
		0,
	},

	8,
	2,

	BUS_SPACE_IAT_1,
	0,
	8,

	wdc_pc98_hw_iat_integral1,
	0x206,
	BUS_SPACE_IAT_SZ(wdc_pc98_hw_iat_integral1),
};

struct wdc_pc98_hw wdc_pc98_hw_nec = {
	{
		"Cbus",
		WDC_DTYPE_GENERIC,

		WDC_ACCESS_PLAIN | WDC_ACCESS_HASBANK,
		2,
		{ NULL,
		  NULL,
		  NULL,			/* allocated later */
		  wdc_cr0_read_plain,
		  wdc_cr0_write_plain,
		  wdc_cr1_read_plain,
		  wdc_cr1_write_plain,
		  wdc_data_read_generic,
		  wdc_data_write_generic
		},

		WDC_CAPABILITY_DATA16,		/* mo mode setup */
		3,				/* meaningless */
		0,
	},

	0x10,	
	2,

	BUS_SPACE_IAT_2,
	0,
	8,

	BUS_SPACE_IAT_2,
	0x10c,
	2,
};

struct wdc_pc98_hw wdc_pc98_hw_ide98 = {
	{
		"ide98",
		WDC_DTYPE_IDE98,

		WDC_ACCESS_INDEXED | WDC_ACCESS_MEM | WDC_ACCESS_HASBANK,
		4,

		{ NULL,
		  NULL,
		  wdc_select_bank_IDE98,
		  wdc_cr0_read_indexed,
		  wdc_cr0_write_indexed,
		  wdc_cr1_read_indexed,
		  wdc_cr1_write_indexed,
		  wdc_data_read_ide98,
		  wdc_data_write_ide98
		},

		WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE,
		4,
		0,
	},

	8,
	1,

	BUS_SPACE_IAT_1,
	0,
	8,

	BUS_SPACE_IAT_1,
	0,
	8,
};

struct wdc_pc98_hw wdc_pc98_hw_pcide = {
	{
		"pcide",
		WDC_DTYPE_GENERIC,

		WDC_ACCESS_PLAIN | WDC_ACCESS_MEM,
		2,

		{ NULL,
		  NULL,
		  NULL,
		  wdc_cr0_read_plain,
		  wdc_cr0_write_plain,
		  wdc_cr1_read_plain,
		  wdc_cr1_write_plain,
		  wdc_data_read_pcide,
		  wdc_data_write_pcide
		},

		WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE,
		4,
		0,
	},

	8,
	2,

	BUS_SPACE_IAT_1,
	0,
	8,

	BUS_SPACE_IAT_1,
	0x206,
	2,
};

struct wdc_pc98_hw wdc_pc98_hw_ninja = {
	{
		"Ninja",
		WDC_DTYPE_GENERIC,

		WDC_ACCESS_PLAIN | WDC_ACCESS_MEM  | WDC_ACCESS_BRESET,
		2,

		{ wdc_open_ninja,
		  wdc_close_ninja,
		  NULL,
		  wdc_cr0_read_plain,
		  wdc_cr0_write_plain,
		  wdc_cr1_read_plain,
		  wdc_cr1_write_plain,
		  wdc_data_read_ninja,
		  wdc_data_write_ninja
		},

		WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE,
		4,
		0,
	},

	8,
	2,

	BUS_SPACE_IAT_1,
	0,
	8,

	BUS_SPACE_IAT_1,
	0x206,
	2,
};

struct wdc_pc98_hw wdc_pc98_hw_kxl = {
	{
		"KXL",
		WDC_DTYPE_GENERIC,

		WDC_ACCESS_PLAIN | WDC_ACCESS_MEM  | WDC_ACCESS_BRESET,
		2,

		{ wdc_open_kxl,
		  wdc_close_kxl,
		  NULL,
		  wdc_cr0_read_plain,
		  wdc_cr0_write_plain,
		  wdc_cr1_read_plain,
		  wdc_cr1_write_plain,
		  wdc_data_read_kxl,
		  wdc_data_write_kxl
		},

		WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE,
		4,
		0,
	},

	8,
	2,

	BUS_SPACE_IAT_1,
	0,
	8,

	BUS_SPACE_IAT_1,
	0x206,
	2,
};

#define	WDC_DEFAULT_HWTAB	(&wdc_pc98_hw_nec)

static dvcfg_hw_t wdc_pc98_hwsel_array[] = {
/* 0x00 */	WDC_DEFAULT_HWTAB,
/* 0x01 */	&wdc_pc98_hw_ibm,
/* 0x02 */	&wdc_pc98_hw_nec,
/* 0x03 */	&wdc_pc98_hw_ide98,
/* 0x04 */	&wdc_pc98_hw_integral,
/* 0x05 */	&wdc_pc98_hw_pcide,
/* 0x06 */	&wdc_pc98_hw_ninja,
/* 0x07 */	&wdc_pc98_hw_kxl,
/* 0x08 */	&wdc_pc98_hw_ibmuni
};

struct dvcfg_hwsel wdc_pc98_hwsel = {
	DVCFG_HWSEL_SZ(wdc_pc98_hwsel_array),
	wdc_pc98_hwsel_array
};
