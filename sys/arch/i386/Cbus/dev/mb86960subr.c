/*	$NecBSD: mb86960subr.c,v 1.6.6.4 1999/09/04 09:06:50 kmatsuda Exp $	*/
/*	$NetBSD: if_ate.c,v 1.21 1998/03/22 04:25:37 enami Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	Kouichi Matsuda. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Kouichi Matsuda.
 * 4. The names of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions copyright (C) 1993, David Greenman.  This software may be used,
 * modified, copied, distributed, and sold, in both source and binary form
 * provided that the above copyright and these terms are retained.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

/*
 * Probe routines of
 *	TDK LAC-98012/13/25 Ethernet interfaces
 *	CONTEC C-NET(9N)E, C-NET(9N)C, C-NET(98)P2 Ethernet interfaces
 *	Ungermann-Bass Access/PC N98C+ (PC85152) Ethernet interface
 *	Ungermann-Bass Access/NOET N98 (PC86132) Ethernet interface
 *	UB Networks Access/CARD (JC89532A) Ethernet interface
 *	Ratoc REX-9880/9881/9882/9883/9886/9887 Ethernet interfaces
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
 *
 * Probe and initialization routine for Fujitsu MBH10302 PCMCIA Ethernet
 * interface, check routine for specific bits in specific registers have
 * specific values, routines to read all bytes from the config EEPROM
 * through MB86965A are derived from netbsd:src/sys/dev/isa/if_fe.c.
 *
 * Probe and initialization routine for TDK/CONTEC PCMCIA Ethernet interface
 * are derived from FreeBSD.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <machine/bus.h>

#include <i386/Cbus/dev/mb86960reg.h>
#include <i386/Cbus/dev/mb86960var.h>
#include <i386/Cbus/dev/mb86960subr.h>
#include <i386/Cbus/dev/if_mbereg.h>
#include <i386/Cbus/dev/if_mbehw.h>
#include <i386/Cbus/dev/if_mbehwtab.h>

#ifdef DIAGNOSTIC
static int mbe_debug = 0;

void mb86960subr_dump __P((int, bus_space_tag_t, bus_space_handle_t));
#endif

static __inline__ int fe_simple_probe __P((bus_space_tag_t, 
    bus_space_handle_t, struct fe_simple_probe_struct const *));

/*
 * Check for specific bits in specific registers have specific values.
 */
static __inline__ int
fe_simple_probe (iot, ioh, sp)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct fe_simple_probe_struct const *sp;
{
	u_int8_t val;
	struct fe_simple_probe_struct const *p;

	for (p = sp; p->mask != 0; p++) {
		val = bus_space_read_1(iot, ioh, p->port);
		if ((val & p->mask) != p->bits) {
#ifdef DIAGNOSTIC
			if (mbe_debug) {
				printf("fe_simple_probe: %x & %x != %x\n",
				    val, p->mask, p->bits);
			}
#endif
			return (0);
		}
	}

	return (1);
}

void mbe_read_cnet98p2_eeprom __P((bus_space_tag_t, bus_space_handle_t,
    u_char *));
static __inline__ void mbe_strobe __P((bus_space_tag_t, bus_space_handle_t));
static void mbe_read_eeprom __P((bus_space_tag_t, bus_space_handle_t,
    u_char *));
void mbe_read_rex9883_config __P((bus_space_tag_t, bus_space_handle_t,
    u_char *));
void mbe_read_lac98_eeprom __P((bus_space_tag_t, bus_space_handle_t, u_char *));

/*
 * Probe routine of CONTEC C-NET(98)P2 Ethernet interface for NetBSD/pc98.
 * Ported by Kouichi Matsuda.
 *
 * CONTEC C-NET(98)P2 use TDK (formerly Silicon Systems) 78Q8377A as
 * Ethernet Controller and National Semiconductor NS46C66 as (256 * 16 bits)
 * Microwire Serial EEPROM.
 */
/* 
 * Routines to read bytes sequentially from EEPROM through 78Q8377A
 * for NetBSD/pc98.
 * 
 * This algorism is generic to read data sequentially from 4-Wire
 * Microwire Serial EEPROM.
 */
void
mbe_read_cnet98p2_eeprom(bst, bsh, data)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_char *data;
{
	u_char n, val, bit;

	/* Select register bank for BMPR */
	bus_space_write_1(bst, bsh, FE_DLCR7,
	    FE_D7_RDYPNS | FE_D7_POWER_UP | FE_D7_RBS_BMPR);

	/* Read bytes from EEPROM; two bytes per an iterration. */
	for (n = 0; n < (FE_EEPROM_SIZE * 2); n++) {
		/* Reset the EEPROM interface. */
		bus_space_write_1(bst, bsh, FE_BMPR12, FE_B12_EEPROM | 0x00);
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT);
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT);

		/* Start EEPROM access. */
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_CLOCK);
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_DATA);
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_CLOCK | FE_B12_DATA);
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_DATA);
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_CLOCK | FE_B12_DATA);
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT);
		bus_space_write_1(bst, bsh, FE_BMPR12,
		    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_CLOCK);

		/* Pass the iterration count to the chip. */
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(bst, bsh, FE_BMPR12,
			    FE_B12_EEPROM | FE_B12_SELECT |
			    ((n & bit) ? FE_B12_DATA : 0x00));
			bus_space_write_1(bst, bsh, FE_BMPR12,
			    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_CLOCK |
			    ((n & bit) ? FE_B12_DATA : 0x00));
		}

		/* Read a byte. */
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(bst, bsh, FE_BMPR12,
			    FE_B12_EEPROM | FE_B12_SELECT);
			bus_space_write_1(bst, bsh, FE_BMPR12,
			    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_CLOCK);
			if (bus_space_read_1(bst, bsh, FE_BMPR12) & FE_B12_DATA)
				val |= bit;
		}
		*data++ = val;

		/* Read one more byte. */
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(bst, bsh, FE_BMPR12,
			    FE_B12_EEPROM | FE_B12_SELECT);
			bus_space_write_1(bst, bsh, FE_BMPR12,
			    FE_B12_EEPROM | FE_B12_SELECT | FE_B12_CLOCK);
			if (bus_space_read_1(bst, bsh, FE_BMPR12) & FE_B12_DATA)
				val |= bit;
		}
		*data++ = val;

		bus_space_write_1(bst, bsh, FE_BMPR12, FE_B12_EEPROM | 0x00);
	}

	bus_space_write_1(bst, bsh, FE_BMPR12, 0x00);

#if FE_DEBUG >= 3
	/* Report what we got. */
	data -= (FE_EEPROM_SIZE * 4);
	log(LOG_INFO, "mbe_read_cnet98p2_eeprom: EEPROM:"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x\n",
	    data[ 0], data[ 1], data[ 2], data[ 3],
	    data[ 4], data[ 5], data[ 6], data[ 7],
	    data[ 8], data[ 9], data[10], data[11],
	    data[12], data[13], data[14], data[15],
	    data[16], data[17], data[18], data[19],
	    data[20], data[21], data[22], data[23],
	    data[24], data[25], data[26], data[27],
	    data[28], data[29], data[30], data[31],
	    data[32], data[33], data[34], data[35],
	    data[36], data[37], data[38], data[39],
	    data[40], data[41], data[42], data[43],
	    data[44], data[45], data[46], data[47],
	    data[48], data[49], data[50], data[51],
	    data[52], data[53], data[54], data[55],
	    data[56], data[57], data[58], data[59],
	    data[60], data[61], data[62], data[63],
	    data[64], data[65], data[66], data[67],
	    data[68], data[69], data[70], data[71],
	    data[72], data[73], data[74], data[75],
	    data[76], data[77], data[78], data[79],
	    data[80], data[81], data[82], data[83],
	    data[84], data[85], data[86], data[87],
	    data[88], data[89], data[90], data[91],
	    data[92], data[93], data[94], data[95]);
#endif
}

/*
 * Probe and initialization for CONTEC C-NET(98)P2 Ethernet interface
 * for NetBSD/pc98.
 */
int
mbe_find_cnet98p2(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	u_char eeprom[FE_EEPROM_SIZE * 4];
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO,
		    "mbe_find_cnet98p2: probe (0x%x) for CONTEC C-NET(98)P2\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif

	/*
	 * See if C-NET(98)P2 is on its address.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/*
	 * We are now almost sure we have an C-NET(98)P2 at the given
	 * address.  So, read EEPROM through 78Q8377A.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presense of the chip
	 * any further without reading EEPROM.
	 */
	mbe_read_cnet98p2_eeprom(iot, ioh, eeprom);
#ifdef DIAGNOSTIC
	if (mbe_debug)
		mb86960subr_dump(LOG_INFO, iot, ioh);
#endif

#ifdef	notyet
	/* Check if parent is isa. */
	if (sc->sc_dh == NULL) {
		/* Check if a board is not configured as PnP mode. */
		if (eeprom[FE_CNET98P2_EEP_PNPACT] & 0x01) {
			/* Stop probe if parent is isa and PnP Active. */
			printf("mbe_detect_cnet98p2: board is configured "
			    "as PnP mode\n");
			return (0);
		}
#endif

	/* XXX: only used if parent is isa. */
	/* Check iobase. */
	*iobase = (eeprom[FE_CNET98P2_EEP_PNP60] << 8)
	    | eeprom[FE_CNET98P2_EEP_PNP61];

	/* Check irq. */
	*irq = eeprom[FE_CNET98P2_EEP_PNP70] & 0x0F;

	return (1);
}

int
mbe_detect_cnet98p2(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	u_char eeprom[FE_EEPROM_SIZE * 4];
	int type, i;
	u_short rom_sum, sum = 0;

	/* Get our station address from EEPROM. */
	mbe_read_cnet98p2_eeprom(iot, ioh, eeprom);
	bcopy(eeprom + FE_CNET98P2_EEP_ADDR, enaddr, ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_cnet98p2: invalid ethernet address\n");
#endif
		return (0);
	}

	/* Check checksum. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sum += enaddr[i];
	sum = (sum & 0x00ff) + ((sum & 0xff00) >> 8);
	rom_sum = eeprom[FE_CNET98P2_EEP_CKSUM];
	if (sum != rom_sum) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_cnet98p2: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		return (0);
	}

	/*
	 * Determine the card type.
	 */
	type = FE_TYPE_CNET98P2;

	return (type);
}
	
void
mbe_attach_cnet98p2(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{
	u_char eeprom[FE_EEPROM_SIZE * 4];

	mbe_read_cnet98p2_eeprom(sc->sc_bst, sc->sc_bsh, eeprom);

	/*
	 * Initialize constants in the per-line structure.
	 */
	sc->type = MB86960_TYPE_8377;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL
	    | ((eeprom[FE_CNET98P2_EEP_DUPLEX] & 0x01) ? FE_D4_DSC : 0);
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 78Q8377A as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board. Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/*
	 * Minimum initialization.
	 */

	/* Minimul initialization of 78Q8377A. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts.  */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/* Derived from ate_strobe, netbsd:src/sys/dev/isa/if_ate.c. */
/*
 * Routines to read all bytes from the config EEPROM through MB86965A.
 * I'm not sure what exactly I'm doing here...  I was told just to follow
 * the steps, and it worked.  Could someone tell me why the following
 * code works?  (Or, why all similar codes I tried previously doesn't
 * work.)  FIXME.
 */

static __inline__ void
mbe_strobe (iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{

	/*
	 * Output same value twice.  To speed-down execution?
	 */
	bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT);
	bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT);
	bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT | FE_B16_CLOCK);
	bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT | FE_B16_CLOCK);
	bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT);
	bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT);
}

/* Derived from ate_read_eeprom, netbsd:src/sys/dev/isa/if_ate.c. */
static void
mbe_read_eeprom(iot, ioh, data)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_char *data;
{
	int n, count;
	u_char val, bit;

	/* Read bytes from EEPROM; two bytes per an iterration. */
	for (n = 0; n < FE_EEPROM_SIZE / 2; n++) {
		/* Reset the EEPROM interface. */
		bus_space_write_1(iot, ioh, FE_BMPR16, 0x00);
		bus_space_write_1(iot, ioh, FE_BMPR17, 0x00);
		bus_space_write_1(iot, ioh, FE_BMPR16, FE_B16_SELECT);

		/* Start EEPROM access. */
		bus_space_write_1(iot, ioh, FE_BMPR17, FE_B17_DATA);
		mbe_strobe(iot, ioh);

		/* Pass the iterration count to the chip. */
		count = 0x80 | n;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(iot, ioh, FE_BMPR17,
			    (count & bit) ? FE_B17_DATA : 0);
			mbe_strobe(iot, ioh);
		}
		bus_space_write_1(iot, ioh, FE_BMPR17, 0x00);

		/* Read a byte. */
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			mbe_strobe(iot, ioh);
			if (bus_space_read_1(iot, ioh, FE_BMPR17) &
			    FE_B17_DATA)
				val |= bit;
		}
		*data++ = val;

		/* Read one more byte. */
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			mbe_strobe(iot, ioh);
			if (bus_space_read_1(iot, ioh, FE_BMPR17) &
			    FE_B17_DATA)
				val |= bit;
		}
		*data++ = val;
	}

	/* Make sure the EEPROM is turned off. */
	bus_space_write_1(iot, ioh, FE_BMPR16, 0);
	bus_space_write_1(iot, ioh, FE_BMPR17, 0);

#if FE_DEBUG >= 3
	/* Report what we got. */
	data -= FE_EEPROM_SIZE;
	log(LOG_INFO, "mbe_read_eeprom: EEPROM at %04x:"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x\n",
	    (int) ioh,		/* XXX */
	    data[ 0], data[ 1], data[ 2], data[ 3],
	    data[ 4], data[ 5], data[ 6], data[ 7],
	    data[ 8], data[ 9], data[10], data[11],
	    data[12], data[13], data[14], data[15],
	    data[16], data[17], data[18], data[19],
	    data[20], data[21], data[22], data[23],
	    data[24], data[25], data[26], data[27],
	    data[28], data[29], data[30], data[31]);
#endif
}

/* Derived from freebsd:src/sys/pc98/pc98/if_fe.c */
/*
 * Probe and initialization for Allied-Telesis RE1000 series.
 */
int
mbe_find_re1000(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	/* XXX: minimal */
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO,
		    "mbe_find_re1000: probe (0x%x) for Allied Telesis RE1000\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif
	/*
	 * See if RE1000 is on its address.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_re1000(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type, i;
	u_short rom_sum, sum = 0;

	/*
	 * RE1000 does not use 86965 EEPROM interface.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		enaddr[i] = bus_space_read_1(iot, ioh, FE_RE1000_SA0 + i * 2);
	}

	/* calculate checksum (xor'ed) from obtained ether addresses. */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sum ^= enaddr[i];
	}
	/* read checksum value from board. */
	rom_sum = bus_space_read_1(iot, ioh, FE_RE1000_CKSUM);
	if (rom_sum != sum) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_re1000: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		return (0);
	}

	if (enaddr[0] != 0x00 || enaddr[1] != 0x00 || enaddr[2] != 0xF4)
		return (0);

	type = FE_TYPE_RE1000;
	return (type);
}

void
mbe_attach_re1000(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{
	u_char irqconf;
	static int const irq_conf[16] = {
		-1, -1, -1, 0x10, -1, 0x20, 0x40, -1,
		-1, -1, -1, -1, 0x80, -1, -1, -1
	};

	/*
	 * set irq
	 */
	irqconf = bus_space_read_1(sc->sc_bst, sc->sc_bsh, FE_RE1000_ICR);
	irqconf &= (~ FE_RE1000_ICR_IRQ);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_RE1000_ICR,
	    irqconf | irq_conf[irq]);

	sc->type = MB86960_TYPE_86965;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;  /* FIXME */
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86965 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Initialize 86965. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/* Derived from freebsd:src/sys/pc98/pc98/if_fe.c */
/*
 * Probe and initialization for Allied-Telesis RE1000Plus/ME1500 series.
 */
int
mbe_find_re1000p(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	u_char eeprom[FE_EEPROM_SIZE];
	u_char save16, save17;
	int dlcr6, dlcr7, signature;
	int n, rv = 0;
	static int const irqmap[4] =
		{ 3,  5,  6,  12 };
	static struct fe_simple_probe_struct const probe_signature1[] = {
		{ FE_DLCR0,  0xBF, 0x00 },
		{ FE_DLCR2,  0xFF, 0x00 },
		{ FE_DLCR4,  0xFF, 0xF6 },
		{ FE_DLCR6,  0xFF, 0xB6 },
		{ 0 }
	};
	static struct fe_simple_probe_struct const probe_signature2[] = {
		{ FE_DLCR1,  0xBF, 0x00 },	/* exclude 0x40 */
		{ FE_DLCR3,  0xFF, 0x00 },
		{ FE_DLCR5,  0xFF, 0x41 },
		{ 0 }
	};
	static struct fe_simple_probe_struct const probe_table[] = {
		{ FE_DLCR2,  0x71, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
		{ FE_DLCR5,  0x80, 0x00 },
		{ 0 }
	};
	static struct fe_simple_probe_struct const vendor_code[] = {
		{ FE_DLCR8,  0xFF, 0x00 },
		{ FE_DLCR9,  0xFF, 0x00 },
		{ FE_DLCR10,  0xFF, 0xF4 },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO,
		    "mbe_find_re1000p: probe (0x%x) "
		    "for Allied Telesis RE1000Plus/ME1500\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif

	/* First, check the "signature" */
	signature = 0;
	if (fe_simple_probe(iot, ioh, probe_signature1)) {

		bus_space_write_1(iot, ioh, FE_DLCR6,
		    (bus_space_read_1(iot, ioh, FE_DLCR6) & 0xCF) | 0x16);
#ifdef DIAGNOSTIC
		if (mbe_debug)
			mb86960subr_dump(LOG_INFO, iot, ioh);
#endif
		if (fe_simple_probe(iot, ioh, probe_signature2))
			signature = 1;
	}

	/*
	 * If the "signature" not detected, 86965 *might* be previously
	 * initialized. So, check the Ethernet address here.
	 *
	 * Allied-Telesis uses 00 00 F4 ?? ?? ??.
	 */
	if (signature == 0) {
		/* Simple check */
		if (!fe_simple_probe(iot, ioh, probe_table))
			return (0);

		/* Disable DLC */
		dlcr6 = bus_space_read_1(iot, ioh, FE_DLCR6);
		delay(400);
		bus_space_write_1(iot, ioh, FE_DLCR6, dlcr6 | FE_D6_DLC_DISABLE);
		delay(400);
		/* Select register bank for DLCR */
		dlcr7 = bus_space_read_1(iot, ioh, FE_DLCR7);
		bus_space_write_1(iot, ioh, FE_DLCR7,
		    dlcr7 & (0xF3 | FE_D7_RBS_DLCR));

		/* Check the Ethernet address */
		if (!fe_simple_probe(iot, ioh, vendor_code))
			return (0);

		/* Restore configuration registers */
		delay(200);
		bus_space_write_1(iot, ioh, FE_DLCR6, dlcr6);
		bus_space_write_1(iot, ioh, FE_DLCR7, dlcr7);
	}

	/* Save old values of the registers. */
	save16 = bus_space_read_1(iot, ioh, FE_BMPR16);
	save17 = bus_space_read_1(iot, ioh, FE_BMPR17);

	/*
	 * We are now almost sure we have an RE1000plus at the given
	 * address.  So, read EEPROM through 86965.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presense of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	mbe_read_eeprom(iot, ioh, eeprom);

	/* Make sure that config info in EEPROM and 86965 agree. */
	if (eeprom[FE_EEPROM_CONF] != bus_space_read_1(iot, ioh, FE_BMPR19)) {
#ifdef DIAGNOSTIC
		printf("mbe_find_re1000p: "
		    "incorrect configuration in eeprom and chip\n");
#endif
		goto out;
	}

	/*
	 * Try to determine IRQ settings.
	 */
	n = (bus_space_read_1(iot, ioh, FE_BMPR19) & FE_B19_IRQ)
	    >> FE_B19_IRQ_SHIFT;
	*irq = irqmap[n];

	rv = 1;
out:
	/* Restore register values, in the case we had no 86965. */
	bus_space_write_1(iot, ioh, FE_BMPR16, save16);
	bus_space_write_1(iot, ioh, FE_BMPR17, save17);
	
	return (rv);
}

int
mbe_detect_re1000p(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	u_char eeprom[FE_EEPROM_SIZE];
	int type;

	/* Get our station address from EEPROM. */
	mbe_read_eeprom(iot, ioh, eeprom);
	bcopy(eeprom + FE_ATI_EEP_ADDR, enaddr, ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_re1000p: invalid ethernet address\n");
#endif
		return (0);
	}

	/*
	 * Determine the card type.
	 */
	type = FE_TYPE_RE1000P;

	return (type);
}

void
mbe_attach_re1000p(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{

	sc->type = MB86960_TYPE_86965;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;  /* FIXME */
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86965 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Initialize 86965. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/*
 * Probe routine of RATOC REX-9886/9887 Ethernet interface for NetBSD/pc98.
 * Ported by Kouichi Matsuda.
 *
 * RATOC REX-9886/9887 use Fujitsu MB86965 as Ethernet Controller and
 * National Semiconductor NS46C66 as (256 * 16 bits) Microwire Serial EEPROM.
 */
int
mbe_find_rex9886(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_rex9886/9887: probe (0x%x) for "
		    "Ratoc REX-9886/9887\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif

	/*
	 * See if Ratoc REX-9886/9887 is on its address.
	 * I'm not sure the following probe code works. FIXME.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_rex9886(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type, i;
	u_char eeprom[FE_EEPROM_SIZE];

	/*
	 * We are now almost sure we have an REX-9886/9887 at the given
	 * address.  So, read EEPROM through 86965.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presense of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	mbe_read_eeprom(iot, ioh, eeprom);

	/* Get our station address from EEPROM. */
	for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
		enaddr[i] = eeprom[i + 3];
		enaddr[i + 1] = eeprom[i + 2];
	}

	type = FE_TYPE_REX9886;

	return (type);
}

void
mbe_attach_rex9886(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{

	sc->type = MB86960_TYPE_86965;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE;  /* FIXME */
	sc->proto_dlcr5 = 0;
	/* XXX ? */
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_EOPPOL;	/* XXX */
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86965 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Initialize 86965. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/*
 * Probe routine of RATOC REX-9880/9881/9882/9883 Ethernet interface
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
 *
 * RATOC REX-9880/9881/9882/9883 use Fujitsu MB86960 as Ethernet Controller.
 */
void
mbe_read_rex9883_config(iot, ioh, data)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_char *data;
{
	u_char n, val;
	u_short bit;

	val = bus_space_read_1(iot, ioh, FE_REX80_IECFR);
	val &= FE_REX80_IECFR_IRQ;
	bus_space_write_1(iot, ioh, FE_REX80_IECFR, val);

	for (n = 0; n < 4; n++) {
		(void) bus_space_read_1(iot, ioh, FE_REX80_IECFR);
	}

	for (n = 0; n < 0x20; n++) {
		val = 0;
		for (bit = 1; bit != 0x100; bit <<= 1) {
			if (bus_space_read_1(iot, ioh, FE_REX80_IECFR) & FE_REX80_IECFR_DATABIT) {
				val |= bit;
			}
		}
		*data++ = val;
	}

#if FE_DEBUG >= 3
	/* Report what we got. */
	data -= 0x20;
	log(LOG_INFO, "mbe_read_rex9886_config: EEPROM:"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x\n",
	    data[ 0], data[ 1], data[ 2], data[ 3],
	    data[ 4], data[ 5], data[ 6], data[ 7],
	    data[ 8], data[ 9], data[10], data[11],
	    data[12], data[13], data[14], data[15],
	    data[16], data[17], data[18], data[19],
	    data[20], data[21], data[22], data[23],
	    data[24], data[25], data[26], data[27],
	    data[28], data[29], data[30], data[31]);
#endif
}

int
mbe_find_rex9883(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_rex9883: probe (0x%x) for "
		    "Ratoc REX-9880/9881/9882/9883\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif

	/*
	 * See if Ratoc REX-9880/9881/9882/9883 is on its address.
	 * I'm not sure the following probe code works. FIXME.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_rex9883(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type, i;
	u_char config[0x20];

	/*
	 * We are now almost sure we have an REX-9880/1/2/3 at the given
	 * address.  So, read EEPROM through 86960.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presense of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	mbe_read_rex9883_config(iot, ioh, config);

	/* Get our station address from EEPROM. */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		enaddr[i] = config[7 - i];
	}

	type = FE_TYPE_REX9880;

	return (type);
}

void
mbe_attach_rex9883(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{
	static u_char const irq_conf[16] = {
		-1, -1, -1, 0x10, -1, 0x20, 0x40, -1,
		-1, -1, -1, -1, 0x80, -1, -1, -1
	};

	sc->type = MB86960_TYPE_86960;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE;  /* FIXME */
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_EOPPOL;	/* XXX */
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 64KB, 100ns, word-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_64KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_WORD | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* assign irq to board. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_REX80_IECFR,
	    irq_conf[irq]);
	/* Initialize 86965. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/*
 * Probe routine of Ungermann-Bass Access/PC N98C+ (PC85152) Ehternet
 * interface for NetBSD/pc98. Ported by Kouichi Matsuda.
 *
 * Ungermann-Bass Access/PC N98C+ (PC85152) use Fujitsu MB86960 as
 * Ethernet Controller.
 */
int
mbe_find_pc85152(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct const probe_table[] = {
		{ FE_DLCR2,  0x70, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
		{ FE_DLCR6,  0x12, 0x12 },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_pc85152: probe (0x%x) for "
		    "Ungermann-Bass Access/PC N98C+ (PC85152)\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif

	/*
	 * See if Access/PC N98C+ is on its address.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_pc85152(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type, i;
	u_short rom_sum, sum = 0;

	/* Get our station address. */
	bus_space_read_region_1(iot, ioh, FE_PC85152_ADDR, enaddr,
	    ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	  || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_pc85152: invalid ethernet address\n");
#endif
		return (0);
	}

	/*
	 * Check checksum.
	 */
	/* calculate checksum (xor'ed) from obtained ether addresses. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sum ^= enaddr[i];
	/* read checksum value from board. */
	rom_sum = bus_space_read_1(iot, ioh, FE_PC85152_CKSUM);
	if (sum != rom_sum) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_pc85152: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		return (0);
	}

	/*
	 * Determine the card type.
	 */
	type = FE_TYPE_PC85152;

	return (type);
}

void
mbe_attach_pc85152(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{
	u_char save_dlcr7;
	static u_char const irq_conf[16] = {
		-1, -1, -1, 1, -1, 2, 4, -1, -1, -1, -1, -1, 8, -1, -1, -1
	};

	/*
	 * Initialize constants in the per-line structure.
	 */
	sc->type = MB86960_TYPE_86960;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO; /* FIXME */

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2. (XXX: 2KB x 2)
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/*
	 * Assign irq to chip/board.
	 */
	/* save current DCLR7 value for safe. */
	save_dlcr7 = bus_space_read_1(sc->sc_bst, sc->sc_bsh, FE_DLCR7);
	/* Select the BMPR bank for runtime register access. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP);
	/* assign irq to board. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_PC85152_IRQ,
	    irq_conf[irq]);
	/* back saveed DCLR7 value. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR7, save_dlcr7);

	/* Initialize 86965. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/*
 * Probe routine of Ungermann-Bass Access/NOTE N98 (PC86132) Ehternet
 * interface for NetBSD/pc98. Ported by Kouichi Matsuda.
 *
 * Ungermann-Bass Access/NOTE N98 (PC86132) use Fujitsu MB86960 as
 * Ethernet Controller.
 */
int
mbe_find_pc86132(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct const probe_table[] = {
		/* XXX: more check */
		{ FE_DLCR2,  0x70, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
		{ FE_DLCR6,  0x12, 0x12 },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_pc86132: probe (0x%x) for "
		    "Ungermann-Bass Access/NOTE N98 (PC86132)\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif

	/*
	 * See if Access/NOTE N98 is on its address.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_pc86132(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type, i;
	u_short rom_sum, sum = 0;
	u_int8_t dlcr7;

	dlcr7 = bus_space_read_1(iot, ioh, FE_DLCR7);
	bus_space_write_1(iot, ioh, FE_DLCR7, 0xe0 | dlcr7);
	delay(400);

	/* Get our station address. */
	bus_space_read_region_1(iot, ioh, FE_PC85152_ADDR, enaddr,
	    ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	  || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_pc86132: invalid ethernet address\n");
#endif
		goto out;
	}

	/*
	 * Check checksum.
	 */
	/* calculate checksum (xor'ed) from obtained ether addresses. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sum ^= enaddr[i];
	/* read checksum value from board. */
	rom_sum = bus_space_read_1(iot, ioh, FE_PC85152_CKSUM);
	if (sum != rom_sum) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_pc86132: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		goto out;
	}

	/*
	 * Determine the card type.
	 */
	type = FE_TYPE_PC86132;

	return (type);

out:
	bus_space_write_1(iot, ioh, FE_DLCR7, dlcr7);
	return (0);
}

void
mbe_attach_pc86132(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{
	u_char save_dlcr7;
	static u_char const irq_conf[16] = {
		-1, -1, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	};

	/*
	 * Initialize constants in the per-line structure.
	 */
	sc->type = MB86960_TYPE_86960;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO; /* FIXME */

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2. (XXX: 2KB x 2)
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/*
	 * Assign irq to chip/board.
	 */
	/* save current DCLR7 value for safe. */
	save_dlcr7 = bus_space_read_1(sc->sc_bst, sc->sc_bsh, FE_DLCR7);
	/* Select the BMPR bank for runtime register access. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP);
	/* assign irq to board. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_PC85152_IRQ,
	    irq_conf[irq]);
	/* back saveed DCLR7 value. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR7, save_dlcr7);

	/* Initialize 86965. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/*
 * Probe routine of TDK LAC-98012/13/25 Ethernet interface for NetBSD/pc98.
 * Ported by Kouichi Matsuda.
 *
 * TDK LAC-98012/13/25 use TDK (formerly Silicon Systems) 78Q8373-CGT as
 * Ethernet Controller and Xicor X24C01 as (128 * 8 bits) I2C Serial EEPROM.
 */
/* 
 * Routines to read bytes sequentially from EEPROM through 78Q8373
 * for NetBSD/pc98.
 * 
 * This algorism is generic to read data sequentially from I2C Serial
 * EEPROM.
 */
void
mbe_read_lac98_eeprom(bst, bsh, data)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_char *data;
{
	u_char n, val, bit;

	/*
	 * For the Start condition, X24C01's SDA (Serial Data) and 
	 * SCL (Serial Clock) pins are HIGH.
	 */
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_CLOCK | FE_LAC4_EEP_DATA);

	/*
	 * Start condition: SDA is a HIGH to LOW when SCL is a HIGH.
	 */
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_CLOCK);
	bus_space_write_1(bst, bsh, FE_LAC4, 0x00);

	/*
	 * Now, send 7-bit slave address. Here, we send 0000000b from MSB.
	 * (i.e., we request data from slave address 0.)
	 */
	for (n = 0; n < 7; n++) {
		bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_CLOCK);
		/*
		 * Send bit (7 - n) of 0000000b.
		 * Currently bit (7 - 0) is 0, so nothing to do.
		 *
		 * NB. If current bit is 1, SDA is a LOW to HIGH here.
		 */

		bus_space_write_1(bst, bsh, FE_LAC4, 0x00);
	}

	/*
	 * Specify our operation is READ.
	 * For this, SDA is a LOW to HIGH.
	 */
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_DATA);
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_CLOCK | FE_LAC4_EEP_DATA);
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_DATA);
	bus_space_write_1(bst, bsh, FE_LAC4, 0x00);
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_DATA);

	/* Wait Acknowledge from EEPROM. */
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_CLOCK | FE_LAC4_EEP_DATA);
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_DATA);
	bus_space_write_1(bst, bsh, FE_LAC4, 0x00);
	/* Keep SDA a HIGH while we read data bytes. */
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_DATA);

	for (n = 0; n < FE_EEPROM_SIZE; n++) {
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(bst, bsh, FE_LAC4,
			    FE_LAC4_EEP_CLOCK | FE_LAC4_EEP_DATA);
			if (bus_space_read_1(bst, bsh, FE_LAC4)
			    & FE_LAC_EEP_DATABIT)
				val |= bit;
			bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_DATA);
		}

		*data++ = val;

		bus_space_write_1(bst, bsh, FE_LAC4, 0x00);

		/*
		 * If we continue to read bytes, send Acknowlege.
		 */
		if (n != FE_EEPROM_SIZE - 1) {
			bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_CLOCK);
			bus_space_write_1(bst, bsh, FE_LAC4, 0x00);
			bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_DATA);
		}
	}

	/*
	 * Send the Stop condition.
	 */
	bus_space_write_1(bst, bsh, FE_LAC4, 0x00);
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_CLOCK);
	bus_space_write_1(bst, bsh, FE_LAC4, FE_LAC4_EEP_CLOCK | FE_LAC4_EEP_DATA);

#if FE_DEBUG >= 3
	/* Report what we got. */
	data -= FE_EEPROM_SIZE;
	log(LOG_INFO, "mbe_read_lac98_eeprom: EEPROM:"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x\n",
	    data[ 0], data[ 1], data[ 2], data[ 3],
	    data[ 4], data[ 5], data[ 6], data[ 7],
	    data[ 8], data[ 9], data[10], data[11],
	    data[12], data[13], data[14], data[15],
	    data[16], data[17], data[18], data[19],
	    data[20], data[21], data[22], data[23],
	    data[24], data[25], data[26], data[27],
	    data[28], data[29], data[30], data[31]);
#endif
}

/*
 * Probe and initialization for TDK LAC-98012/13/25 for NetBSD/pc98.
 */
int
mbe_find_lac98(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_lac98: probe (0x%x) "
		    "for TDK LAC-98012/13/25\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif
	/*
	 * See if TDK LAC-98012/13/25 is on its address.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_lac98(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	u_char eeprom[FE_EEPROM_SIZE];
	int type;

	/* Get our station address from EEPROM. */
	mbe_read_lac98_eeprom(iot, ioh, eeprom);
	bcopy(eeprom + FE_LAC_EEP_ADDR, enaddr, ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_lac98: invalid ethernet address\n");
#endif
		return (0);
	}

	/*
	 * Determine the card type.
	 */
	type = FE_TYPE_TDKLAC;

	return (type);
}

void
mbe_attach_lac98(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{
	static u_char const irq_conf[16] = {
		-1, -1, -1, 0x10, -1, 0x20, 0x40, -1,
		-1, -1, -1, -1, 0x80, -1, -1, -1
	};

	/* Assign irq to chip/board. */
	if ((1 << irq) & FE_LAC_IRQMASK) {
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_LAC4,
		    irq_conf[irq]);
	} else {
#ifdef DIAGNOSTIC
		printf("%s: configured irq %d invalid\n", sc->sc_dev.dv_xname,
		    irq);
#endif
		return;
	}

	sc->type = MB86960_TYPE_8373;

	/* Should find all register prototypes here. FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 78Q8373 as follows:
	 *	SRAM: 32KB, 150ns, byte-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 8 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board. Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_BYTE | FE_D6_SRAM_150ns;

	/*
	 * Minimum initialization.
	 */

	/* Minimul initialization of 78Q8373. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/*
 * Probe and initialization for CONTEC C-NET(9N)E Ethernet interface
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
 */
/*
 * Probe routine of CONTEC C-NET(9N)E Ethernet interface for NetBSD/pc98.
 * Ported by Kouichi Matsuda.
 *
 * CONTEC C-NET(9N)E use Fujitsu MB86960 as Ethernet Controller.
 */
int
mbe_find_cnet9ne(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	/* check MB86960 reserved bit, which is should be 0/1. */
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR0, 0x05, 0x00 },
		{ FE_DLCR2, 0x79, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_cnet9ne: probe (0x%x) "
		    "for Contec C-NET(9N)E\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif
	/*
	 * See if C-NET(9N)E is on its address.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_cnet9ne(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type, i;
	u_short rom_sum, sum = 0;

	/*
	 * C-NET(9N)E does not use 86965 EEPROM interface.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		enaddr[i] = bus_space_read_1(iot, ioh, FE_CNET9NE_SA0 + i * 2);
	}

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_cnet9ne: invalid ethernet address\n");
#endif
		return (0);
	}

	/* calculate checksum from obtained ether addresses. */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sum += enaddr[i];
	}
	sum = (sum & 0x00ff) + ((sum & 0xff00) >> 8);
	/* read checksum value from board. */
	rom_sum = bus_space_read_1(iot, ioh, FE_CNET9NE_CKSUM);
	if (rom_sum != sum) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_cnet9ne: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		return (0);
	}

	type = FE_TYPE_CNET9NE;
	return (type);
}

void
mbe_attach_cnet9ne(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{

	sc->type = MB86960_TYPE_86960;

	/* Should find all register prototypes here. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = FE_D5_BUFEMP;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO |
	    FE_B13_BSTCTL_1;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 64KB, 100ns, word-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_64KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_WORD | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Initialize 86960. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/*
 * Probe and initialization for CONTEC C-NET(9N)C Ethernet interface
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
 */
/*
 * Probe routine of CONTEC C-NET(9N)C Ethernet interface for NetBSD/pc98.
 * Ported by Kouichi Matsuda.
 *
 * CONTEC C-NET(9N)C use Fujitsu MB86960 as Ethernet Controller.
 */
int
mbe_find_cnet9nc(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	/* check MB86960 reserved bit, which is should be 0/1. */
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR0, 0x05, 0x00 },
		{ FE_DLCR2, 0x79, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_cnet9nc: probe (0x%x) "
		    "for Contec C-NET(9N)C\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif
	/*
	 * See if C-NET(9N)C is on its address.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_cnet9nc(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type, i;
	u_short rom_sum, sum = 0;

	/*
	 * C-NET(9N)C does not use 86965 EEPROM interface.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		enaddr[i] = bus_space_read_1(iot, ioh, FE_CNET9NC_SA0 + i * 2);
	}

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_cnet9nc: invalid ethernet address\n");
#endif
		return (0);
	}

	/* calculate checksum from obtained ether addresses. */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sum += enaddr[i];
	}
	sum = (sum & 0x00ff) + ((sum & 0xff00) >> 8);
	/* read checksum value from board. */
	rom_sum = bus_space_read_1(iot, ioh, FE_CNET9NC_CKSUM);
	if (rom_sum != sum) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_cnet9nc: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		return (0);
	}

	type = FE_TYPE_CNET9NC;
	return (type);
}

void
mbe_attach_cnet9nc(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{

	sc->type = MB86960_TYPE_86960;

	/* Should find all register prototypes here. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = FE_D5_BUFEMP;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_NICE;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO |
	    FE_B13_BSTCTL_1;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 64KB, 100ns, word-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_64KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_WORD | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Initialize 86960. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/* Derived from netbsd:src/sys/dev/isa/if_fe.c. */
/*
 * Probe and initialization for Fujitsu MBH10302 PCMCIA Ethernet interface.
 */
int
mbe_find_mbh1040x(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
#if 0
	/*
	 * Test *vendor* part of the address for Fujitsu.
	 * The test will gain reliability of probe process, but
	 * it rejects clones by other vendors, or OEM product
	 * supplied by resalers other than Fujitsu.
	 */
		{ FE_MBH10, 0xFF, 0x00 },
		{ FE_MBH11, 0xFF, 0x00 },
		{ FE_MBH12, 0xFF, 0x0E },
#else
	/*
	 * We can always verify the *first* 2 bits (in Ehternet
	 * bit order) are "global" and "unicast" even for
	 * unknown vendors.
	 */
		{ FE_MBH10, 0x03, 0x00 },
#endif
	/* Just a gap? Seems reliable, anyway. */
		{ 0x12, 0xFF, 0x00 },
		{ 0x13, 0xFF, 0x00 },
		{ 0x14, 0xFF, 0x00 },
		{ 0x15, 0xFF, 0x00 },
		{ 0x16, 0xFF, 0x00 },
		{ 0x17, 0xFF, 0x00 },
		{ 0x18, 0xFF, 0xFF },
		{ 0x19, 0xFF, 0xFF },

		{ 0 }
	};

	static struct fe_simple_probe_struct probe_table2[] = {
		{ FE_DLCR0, 0x05, 0x00 },
		{ FE_DLCR2, 0x71, 0x00 },
		{ FE_DLCR4, 0xFF, 0xF6 },
		{ FE_DLCR6, 0xFF, 0xB6 },
		{ 0 }
	};

	/*
	 * See if MBH10302 is on its address.
	 * I'm not sure the following probe code works. FIXME.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table) &&
	    !fe_simple_probe(iot, ioh, probe_table2))
			return (0);

	return (1);
}

int
mbe_detect_mbh1040x(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type;
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
#if 0
	/*
	 * Test *vendor* part of the address for Fujitsu.
	 * The test will gain reliability of probe process, but
	 * it rejects clones by other vendors, or OEM product
	 * supplied by resalers other than Fujitsu.
	 */
		{ FE_MBH10, 0xFF, 0x00 },
		{ FE_MBH11, 0xFF, 0x00 },
		{ FE_MBH12, 0xFF, 0x0E },
#else
	/*
	 * We can always verify the *first* 2 bits (in Ehternet
	 * bit order) are "global" and "unicast" even for
	 * unknown vendors.
	 */
		{ FE_MBH10, 0x03, 0x00 },
#endif
	/* Just a gap? Seems reliable, anyway. */
		{ 0x12, 0xFF, 0x00 },
		{ 0x13, 0xFF, 0x00 },
		{ 0x14, 0xFF, 0x00 },
		{ 0x15, 0xFF, 0x00 },
		{ 0x16, 0xFF, 0x00 },
		{ 0x17, 0xFF, 0x00 },
		{ 0x18, 0xFF, 0xFF },
		{ 0x19, 0xFF, 0xFF },

		{ 0 }
	};

	static struct fe_simple_probe_struct probe_table2[] = {
		{ FE_DLCR0, 0x05, 0x00 },
		{ FE_DLCR2, 0x71, 0x00 },
		{ FE_DLCR4, 0xFF, 0xF6 },
		{ FE_DLCR6, 0xFF, 0xB6 },
		{ 0 }
	};

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
		return (0);
	}

	/*
	 * See if MBH10302 is on its address.
	 * I'm not sure the following probe code works. FIXME.
	 */
	if (fe_simple_probe(iot, ioh, probe_table)) {
		type = FE_TYPE_MBH10302;
	} else {
		if (fe_simple_probe(iot, ioh, probe_table2)) {
			type = FE_TYPE_MBH10304;
		} else {
			return (0);
		}
	}

	return (type);
}

void
mbe_attach_mbh1040x(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{

	sc->type = MB86960_TYPE_86960;

	/* Should find all register prototypes here. FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_NICE;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board. Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Setup hooks. We need a special initialization procedure. */
	if (type == FE_TYPE_MBH10302)
		sc->init_card = mbe_init_mbh;

	/*
	 * Minimum initialization.
	 */

	/* Wait for a while. I'm not sure this is necessary. FIXME. */
	delay(400);

	/* Minimul initialization of 86960. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);

#if 1	/* FIXME. */
	/* Initialize system bus interface and encoder/decoder operation. */
	if (type == FE_TYPE_MBH10302)
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_MBH0,
		    FE_MBH0_MAGIC | FE_MBH0_INTR_DISABLE);
#endif

}

/* MBH10402 specific initialization routine. */
void
mbe_init_mbh(sc)
	struct mb86960_softc *sc;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;

	/* Probably required after hot-insertion... */

	/* Wait for a while. I'm not sure this is necessary. FIXME. */
	delay(400);

	/* Minimul initialization of 86960. */
	bus_space_write_1(bst, bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(bst, bsh, FE_DLCR2, 0);
	bus_space_write_1(bst, bsh, FE_DLCR3, 0);

	/* Enable master interrupt flag. */
	bus_space_write_1(bst, bsh, FE_MBH0,
	    FE_MBH0_MAGIC | FE_MBH0_INTR_ENABLE);
}

/* Derived from freebsd:src/sys/i386/isa/if_fe.c. */
/*
 * Probe and initialization for TDK/CONTEC PCMCIA Ethernet interface.
 */
int
mbe_find_tdk(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_tdk: probe (0x%x) "
		    "for TDK/CONTEC pcmcia\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif

	/*
	 * See if C-NET(PC)C is on its address.
	 * I'm not sure the following probe code works. FIXME.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_tdk(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type;

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_tdk: invalid ethernet address\n");
#endif
		return (0);
	}

	/*
	 * Determine the card type.
	 */
	type = FE_TYPE_TDK;

	return (type);
}

void
mbe_attach_tdk(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{
	/*
	 * Initialize constants in the per-line structure.
	 */
	sc->type = MB86960_TYPE_8373;

	/* Should find all register prototypes here. FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board. Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/*
	 * Minimum initialization.
	 */

	/* Wait for a while. I'm not sure this is necessary. FIXME. */
	delay(400);

	/* Minimul initialization of 86960. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

/*
 * check (minimal) hardware reset signature bits and MB86960 reserved bit,
 * which is should be 0/1.
 */
int
mbe_find_mb86960(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct const probe_signature1[] = {
		{ FE_DLCR0, 0x05, 0x00 },
		{ FE_DLCR2, 0x79, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	/*	{ FE_DLCR5, 0x84, 0x00 },	Does not work well */
		{ FE_DLCR6, 0xB6, 0xB6 },
		{ FE_DLCR7, 0x20, 0x20 },	/* we are NICE */
		{ 0 }
	};
	static struct fe_simple_probe_struct const probe_signature2[] = {
		{ FE_DLCR0, 0x05, 0x00 },
		{ FE_DLCR2, 0x79, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	/*	{ FE_DLCR5, 0x84, 0x00 },	Does not work well */
		{ FE_DLCR6, 0x40, 0x40 },
		{ FE_DLCR7, 0x20, 0x20 },	/* we are NICE */
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_mb86960: generic probe (0x%x) "
		    "for MB86960\n",
		    (int) ioh);
		mb86960subr_dump(LOG_INFO, iot, ioh);
	}
#endif
	/*
	 * See if MB86960 is on its address.
	 */
	if (fe_simple_probe(iot, ioh, probe_signature1)) {
		return (1);
	}
	if (fe_simple_probe(iot, ioh, probe_signature2)) {
		return (1);
	}

	/* No iobase and irq configuration data are available from board. */
	return (0);
}

/*
 * Probe routine of UB Networks Access/CARD (JC89532A) Ehternet
 * interface for NetBSD/pc98. Ported by Kouichi Matsuda.
 *
 * UB Networks Access/CARD (JC89532A) use Fujitsu MB86960 as
 * Ethernet Controller.
 */
int
mbe_find_jc89532a(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct const probe_table[] = {
		{ FE_DLCR2,  0x70, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
		{ FE_DLCR5,  0x80, 0x00 },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (mbe_debug) {
		log(LOG_INFO, "mbe_find_jc89532a: probe (0x%x) for "
		    "UB Networks Access/CARD (JC89532A)\n",
		    (int) ioh);
		log(LOG_INFO, "\tDLCR = %02x %02x %02x %02x %02x %02x %02x %02x\n",
		    bus_space_read_1(iot, ioh, FE_DLCR0),
		    bus_space_read_1(iot, ioh, FE_DLCR1),
		    bus_space_read_1(iot, ioh, FE_DLCR2),
		    bus_space_read_1(iot, ioh, FE_DLCR3),
		    bus_space_read_1(iot, ioh, FE_DLCR4),
		    bus_space_read_1(iot, ioh, FE_DLCR5),
		    bus_space_read_1(iot, ioh, FE_DLCR6),
		    bus_space_read_1(iot, ioh, FE_DLCR7));
	
		log(LOG_INFO, "\t       %02x %02x %02x %02x %02x %02x %02x %02x\n",
		    bus_space_read_1(iot, ioh, FE_DLCR8),
		    bus_space_read_1(iot, ioh, FE_DLCR9),
		    bus_space_read_1(iot, ioh, FE_DLCR10),
		    bus_space_read_1(iot, ioh, FE_DLCR11),
		    bus_space_read_1(iot, ioh, FE_DLCR12),
		    bus_space_read_1(iot, ioh, FE_DLCR13),
		    bus_space_read_1(iot, ioh, FE_DLCR14),
		    bus_space_read_1(iot, ioh, FE_DLCR15));
	}
#endif

	/*
	 * See if Access/CARD is on its address.
	 */
	if (!fe_simple_probe(iot, ioh, probe_table))
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
mbe_detect_jc89532a(iot, ioh, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type, i;
	u_short rom_sum, sum = 0;

	/* Get our station address. */
	bus_space_read_region_1(iot, ioh, FE_JC89532_ADDR, enaddr,
	    ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	  || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_jc89532a: invalid ethernet address\n");
#endif
		goto out;
	}

	/*
	 * Check checksum.
	 */
	/* calculate checksum (xor'ed) from obtained ether addresses. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sum ^= enaddr[i];
	/* read checksum value from board. */
	rom_sum = bus_space_read_1(iot, ioh, FE_JC89532_CKSUM);
	if (sum != rom_sum) {
#ifdef DIAGNOSTIC
		printf("mbe_detect_jc89532a: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		goto out;
	}

	/*
	 * Determine the card type.
	 */
	type = FE_TYPE_JC89532A;

	return (type);

out:
	return (0);
}

void
mbe_attach_jc89532a(sc, type, myea, iobase, irq)
	struct mb86960_softc *sc;
	enum mb86960_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{

	/*
	 * Initialize constants in the per-line structure.
	 */
	sc->type = MB86960_TYPE_86960;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2. (XXX: 2KB x 2)
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Initialize 86965. */
	delay(400);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR6,
	    sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR2, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_DLCR3, 0);
}

#ifdef	notyet
int
mbe_find_mb86964(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct const probe_signature1[] = {
		{ FE_DLCR0, 0xBF, 0x00 },
		{ FE_DLCR1, 0xFF, 0x00 },
		{ FE_DLCR2, 0xFF, 0x00 },
		{ FE_DLCR3, 0xFF, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ FE_DLCR5, 0xFF, 0x42 },
		{ FE_DLCR6, 0xFF, 0xB6 },
		{ FE_DLCR7, 0xEF, 0x60 },
	};
	static struct fe_simple_probe_struct const probe_signature2[] = {
		{ FE_DLCR0, 0x01, 0x00 },	/* reserved bit should be 0 */
		{ FE_DLCR2, 0x79, 0x00 },	/* reserved bit should be 0 */
		{ FE_DLCR4, 0x08, 0x00 },	/* reserved bit should be 0 */
		{ FE_DLCR5, 0x80, 0x00 },	/* reserved bit should be 0 */
		{ FE_DLCR6, 0x50, 0x10 },	/* reserved bit should be 0 */
	/*	{ FE_DLCR15, 0xC0, 0x00 }, */
	};

	if (fe_simple_probe(iot, ioh, probe_signature1)) {
#ifdef DIAGNOSTIC
		printf("mbe_find_mb86964: after hard reset (OK)\n");
#endif
		return (1);
	}
	if (fe_simple_probe(iot, ioh, probe_signature2)) {
#ifdef DIAGNOSTIC
		printf("mbe_find_mb86964: check reserved bits (OK)\n");
#endif
		return (1);
	}
	return (0);
}

int
mbe_find_mb86965(iot, ioh, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *iobase, *irq;
{
	static struct fe_simple_probe_struct const probe_signature1[] = {
		{ FE_DLCR0, 0xBF, 0x00 },
		{ FE_DLCR1, 0xFF, 0x00 },
		{ FE_DLCR2, 0xFF, 0x00 },
		{ FE_DLCR3, 0xFF, 0x00 },
		{ FE_DLCR4, 0x0B, 0x02 },
		{ FE_DLCR5, 0xFF, 0x42 },
		{ FE_DLCR6, 0xFF, 0xB6 },
		{ FE_DLCR7, 0xEF, 0xE0 },
	};
	static struct fe_simple_probe_struct const probe_signature2[] = {
		{ FE_DLCR0, 0x01, 0x00 },	/* reserved bit should be 0 */
		{ FE_DLCR2, 0x71, 0x00 },	/* reserved bit should be 0 */
		{ FE_DLCR4, 0x08, 0x00 },	/* reserved bit should be 0 */
		{ FE_DLCR5, 0x80, 0x00 },	/* reserved bit should be 0 */
	/*	{ FE_DLCR15, 0xC0, 0x00 }, */
	};

	if (fe_simple_probe(iot, ioh, probe_signature1)) {
#ifdef DIAGNOSTIC
		printf("mbe_find_mb86965: after hard reset (OK)\n");
#endif
		return (1);
	}
	if (fe_simple_probe(iot, ioh, probe_signature2)) {
#ifdef DIAGNOSTIC
		printf("mbe_find_mb86965: check reserved bits (OK)\n");
#endif
		return (1);
	}
	return (0);
}
#endif

#ifdef DIAGNOSTIC
void
mb86960subr_dump(level, bst, bsh)
	int level;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
{
	u_int8_t save_dlcr7;

	save_dlcr7 = bus_space_read_1(bst, bsh, FE_DLCR7);

	log(level, "\tDLCR = %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    bus_space_read_1(bst, bsh, FE_DLCR0),
	    bus_space_read_1(bst, bsh, FE_DLCR1),
	    bus_space_read_1(bst, bsh, FE_DLCR2),
	    bus_space_read_1(bst, bsh, FE_DLCR3),
	    bus_space_read_1(bst, bsh, FE_DLCR4),
	    bus_space_read_1(bst, bsh, FE_DLCR5),
	    bus_space_read_1(bst, bsh, FE_DLCR6),
	    bus_space_read_1(bst, bsh, FE_DLCR7));

	bus_space_write_1(bst, bsh, FE_DLCR7,
	    (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_DLCR);
	delay(400);
	log(level, "\t       %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    bus_space_read_1(bst, bsh, FE_DLCR8),
	    bus_space_read_1(bst, bsh, FE_DLCR9),
	    bus_space_read_1(bst, bsh, FE_DLCR10),
	    bus_space_read_1(bst, bsh, FE_DLCR11),
	    bus_space_read_1(bst, bsh, FE_DLCR12),
	    bus_space_read_1(bst, bsh, FE_DLCR13),
	    bus_space_read_1(bst, bsh, FE_DLCR14),
	    bus_space_read_1(bst, bsh, FE_DLCR15));

	bus_space_write_1(bst, bsh, FE_DLCR7,
	    (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_MAR);
	delay(400);
	log(level, "\tMAR  = %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    bus_space_read_1(bst, bsh, FE_MAR8),
	    bus_space_read_1(bst, bsh, FE_MAR9),
	    bus_space_read_1(bst, bsh, FE_MAR10),
	    bus_space_read_1(bst, bsh, FE_MAR11),
	    bus_space_read_1(bst, bsh, FE_MAR12),
	    bus_space_read_1(bst, bsh, FE_MAR13),
	    bus_space_read_1(bst, bsh, FE_MAR14),
	    bus_space_read_1(bst, bsh, FE_MAR15));

	bus_space_write_1(bst, bsh, FE_DLCR7,
	    (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_BMPR);
	delay(400);
	/*
	 * XXX: DON'T access ports, offset over 16!
	 * All mb86965 clone does not have these registers.
	 * (Moreover, mb86960 does not have.)
	 */
	log(level,
	    "\tBMPR = xx xx %02x %02x %02x %02x %02x %02x\n",
	    bus_space_read_1(bst, bsh, FE_BMPR10),
	    bus_space_read_1(bst, bsh, FE_BMPR11),
	    bus_space_read_1(bst, bsh, FE_BMPR12),
	    bus_space_read_1(bst, bsh, FE_BMPR13),
	    bus_space_read_1(bst, bsh, FE_BMPR14),
	    bus_space_read_1(bst, bsh, FE_BMPR15));

	bus_space_write_1(bst, bsh, FE_DLCR7, save_dlcr7);
	delay(400);
}
#endif
