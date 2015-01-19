/*	$NecBSD: mb86950subr.c,v 1.2.10.4 1999/09/28 05:42:33 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 */
/*
 *  Copyright (c) 1998
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
 * Probe routines of
 *	Sun Electronics AngeLan AL-98 series Ethernet interfaces
 *	Ungermann-Bass Access/PC N98C+ (PC85151) Ethernet interface
 *	Ungermann-Bass Access/NOTE N98 (PC86131) Ethernet interface
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
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

#include <i386/Cbus/dev/mb86950reg.h>
#include <i386/Cbus/dev/mb86950var.h>
#include <i386/Cbus/dev/mb86950subr.h>
#include <i386/Cbus/dev/if_fesreg.h>
#include <i386/Cbus/dev/if_feshw.h>
#include <i386/Cbus/dev/if_feshwtab.h>

#ifdef DIAGNOSTIC
static int fes_debug = 0;
#endif

static __inline__ int fes_simple_probe __P((bus_space_tag_t, 
    bus_space_handle_t, struct fes_simple_probe_struct const *));

/*
 * Check for specific bits in specific registers have specific values.
 */
static __inline__ int
fes_simple_probe(iot, ioh, sp)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct fes_simple_probe_struct const *sp;
{
	u_int8_t val;
	struct fes_simple_probe_struct const *p;

	for (p = sp; p->mask != 0; p++) {
		val = bus_space_read_1(iot, ioh, p->port);
		if ((val & p->mask) != p->bits) {
#ifdef DIAGNOSTIC
			if (fes_debug) {
				printf("fes_simple_probe: %x & %x != %x\n",
				    val, p->mask, p->bits);
			}
#endif
			return (0);
		}
	}

	return (1);
}

/* static */ void fes_read_al98_eeprom __P((bus_space_tag_t, bus_space_handle_t,
    u_char *));
u_char fes_nic_get_al98 __P((bus_space_tag_t, bus_space_handle_t, u_int8_t));
void fes_nic_put_al98 __P((bus_space_tag_t, bus_space_handle_t, u_int8_t,
    u_int8_t));

u_char fes_nic_get_generic __P((bus_space_tag_t, bus_space_handle_t, u_int8_t));
void fes_nic_put_generic __P((bus_space_tag_t, bus_space_handle_t, u_int8_t,
    u_int8_t));

/*
 * Probe routine of Sun Electronics AngeLan AL-98 series Ethernet interface
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
 *
 * Sun Electronics AngeLan AL-98 series use Fujitsu MB86950 as Ethernet
 * Controller.
 */
/*
 * Routines to read all bytes from the config EEPROM through MB86950 and
 * AngeLan AL-98 ASIC for NetBSD/pc98.
 * 
 * This algorism is generic to read data sequentially from 4-Wire
 * Microwire Serial EEPROM.
 */
#if 1
#define	EEPROM_DELAY(x)	delay(x)
#else
#define	EEPROM_DELAY(x)
#endif

/* static */ void
fes_read_al98_eeprom(bst, bsh, data)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_char *data;
{
	u_int8_t val, bit;
	int n;

	/* Read bytes from EEPROM; two bytes per an iterration. */
	for (n = 0; n < (FES_AL98_EEPROM_SIZE / 2); n++) {
		/* Enable EEPROM access. */
		bus_space_write_1(bst, bsh, FES_AL98_ECFG, 0x88);
	
		bus_space_write_1(bst, bsh, FES_AL98_EEP, 0);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, 0);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, 0);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, 0);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, 0);
		EEPROM_DELAY(20);
	
		/* Reads data stored in memory, at specified address. */
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT);
		EEPROM_DELAY(20);
		(void) bus_space_read_1(bst, bsh, FES_AL98_EEP);
	
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_DATA);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_DATA | FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_DATA | FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_DATA);
		EEPROM_DELAY(20);
	
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_DATA);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_DATA | FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_DATA | FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_DATA);
		EEPROM_DELAY(20);
	
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP,
		    FES_AL98_EEP_SELECT);
		EEPROM_DELAY(20);
		(void) bus_space_read_1(bst, bsh, FES_AL98_EEP);
	
		for (bit = 0x20; bit != 0x00; bit >>= 1) {
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT |
			    ((n & bit) ? FES_AL98_EEP_DATA : 0));
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK |
			    ((n & bit) ? FES_AL98_EEP_DATA : 0));
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK |
			    ((n & bit) ? FES_AL98_EEP_DATA : 0));
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT |
			    ((n & bit) ? FES_AL98_EEP_DATA : 0));
			EEPROM_DELAY(20);
			(void) bus_space_read_1(bst, bsh, FES_AL98_EEP);
		}
	
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT);
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK);
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK);
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT);
			EEPROM_DELAY(20);
			if (bus_space_read_1(bst, bsh, FES_AL98_EEP)
			    & FES_AL98_EEP_DATABIT)
				val |= bit;
		}
		*data++ = val;

		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT);
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK);
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT | FES_AL98_EEP_CLOCK);
			EEPROM_DELAY(20);
			bus_space_write_1(bst, bsh, FES_AL98_EEP,
			    FES_AL98_EEP_SELECT);
			EEPROM_DELAY(20);
			if (bus_space_read_1(bst, bsh, FES_AL98_EEP)
			    & FES_AL98_EEP_DATABIT)
				val |= bit;
		}

		*data++ = val;
	
		/* terminale EEPROM access. */
		bus_space_write_1(bst, bsh, FES_AL98_EEP, 0);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, FES_AL98_EEP_CLOCK);
		EEPROM_DELAY(20);
		bus_space_write_1(bst, bsh, FES_AL98_EEP, 0);
		EEPROM_DELAY(20);
	}

#if FES_DEBUG >= 3
	/* Report what we got. */
	data -= FES_AL98_EEPROM_SIZE;
	log(LOG_INFO, "fes_read_al98_eeprom: EEPROM:"
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

u_char
fes_nic_get_al98(bst, bsh, reg)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t reg;
{

	return (bus_space_read_1(bst, bsh, reg));
}

void
fes_nic_put_al98(bst, bsh, reg, val)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t reg, val;
{
	u_int16_t v16;

	if (reg & 1) {
		v16 = val << 8;
		bus_space_write_2(bst, bsh, reg, v16);
	} else {
		bus_space_write_1(bst, bsh, reg, val);
	}
}

int
fes_find_al98(iot, ioh, asich, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh, asich;
	int *iobase, *irq;
{
	int i;
	static struct fes_simple_probe_struct probe_table[] = {
		{ FES_DLCR1, 0x70, 0x00 },
		{ FES_DLCR5, 0x40, 0x40 },
		{ FES_DLCR6, 0x40, 0x40 },
		{ FES_DLCR14, 0xFF, 0xFF },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (fes_debug) {
		log(LOG_INFO, "fes_find_al98: probe (0x%x) for "
		    "Sun Electronics AngeLan AL-98 series\n",
		    (int) ioh);
		log(LOG_INFO, "\tDLCR = %04x %04x %04x %04x %04x %04x %04x %04x\n",
		    bus_space_read_2(iot, ioh, FES_DLCR0),
		    bus_space_read_2(iot, ioh, FES_DLCR1),
		    bus_space_read_2(iot, ioh, FES_DLCR2),
		    bus_space_read_2(iot, ioh, FES_DLCR3),
		    bus_space_read_2(iot, ioh, FES_DLCR4),
		    bus_space_read_2(iot, ioh, FES_DLCR5),
		    bus_space_read_2(iot, ioh, FES_DLCR6),
		    bus_space_read_2(iot, ioh, FES_DLCR7));
	
		log(LOG_INFO, "\t       %04x %04x %04x %04x %04x %04x %04x %04x\n",
		    bus_space_read_2(iot, ioh, FES_DLCR8),
		    bus_space_read_2(iot, ioh, FES_DLCR9),
		    bus_space_read_2(iot, ioh, FES_DLCR10),
		    bus_space_read_2(iot, ioh, FES_DLCR11),
		    bus_space_read_2(iot, ioh, FES_DLCR12),
		    bus_space_read_2(iot, ioh, FES_DLCR13),
		    bus_space_read_2(iot, ioh, FES_DLCR14),
		    bus_space_read_2(iot, ioh, FES_DLCR15));
	}
#endif

	/*
	 * See if AngeLan AL-98 is on its address.
	 */
	if (!fes_simple_probe(iot, ioh, probe_table))
		return (0);
	for (i = 18; i < 32; i++) {
		u_int8_t tmp;

		/*
		 * assumption: Any other probe routines have not
		 *	written to these ports.
		 */ 
		tmp = bus_space_read_1(iot, ioh, i);
#ifdef DIAGNOSTIC
		if (fes_debug)
			printf("%d: %02x ", i, tmp);
#endif
		if (tmp & 0xC8)
			return (0);
	}

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
fes_detect_al98(iot, ioh, asich, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh, asich;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	u_char eeprom[FES_AL98_EEPROM_SIZE];
	int type, i;

	/* Get our station address from EEPROM. */
	fes_read_al98_eeprom(iot, asich, eeprom);
	for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
		enaddr[i + 1] = eeprom[i * 2];
		enaddr[i] = eeprom[i * 2 + 1];
	}

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("fes_detect_al98: invalid ethernet address\n");
#endif
		return (0);
	}

	/*
	 * Determine the card type.
	 */
	type = FES_TYPE_AL98;

	return (type);
}

void
fes_attach_al98(sc, type, myea, iobase, irq)
	struct mb86950_softc *sc;
	enum mb86950_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{

	/*
	 * Initialize constants in the per-line structure.
	 */
	sc->type = MB86950_TYPE_86950;
	sc->sc_nic_get = fes_nic_get_al98;
	sc->sc_nic_put = fes_nic_put_al98;
	sc->txb_pending = 0;

	/*
	 * Minimum initialization.
	 */

	/* Wait for a while. I'm not sure this is necessary. FIXME. */
	delay(400);

	/* Minimul initialization of 86950. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FES_DLCR6, FES_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FES_DLCR1, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FES_DLCR3, 0);
}

/*
 * Probe routines of
 * Ungermann-Bass Access/PC N98C+ (PC85151) Ehternet interface and
 * Ungermann-Bass Access/NOTE N98 (PC86131) Ehternet interface
 * for NetBSD/pc98. Ported by Kouichi Matsuda.
 *
 * Ungermann-Bass Access/PC N98C+ (PC85151) and
 * Ungermann-Bass Access/NOTE N98 (PC86131) use Fujitsu MB86950 as
 * Ethernet Controller.
 */
u_char
fes_nic_get_generic(bst, bsh, reg)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t reg;
{

	return (bus_space_read_1(bst, bsh, reg));
}

void
fes_nic_put_generic(bst, bsh, reg, val)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t reg, val;
{

	bus_space_write_1(bst, bsh, reg, val);
}

int
fes_find_pc85151(iot, ioh, asich, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh, asich;
	int *iobase, *irq;
{
	static struct fes_simple_probe_struct probe_table[] = {
		{ FES_DLCR1, 0x70, 0x00 },
		{ FES_DLCR5, 0x40, 0x40 },
		{ FES_DLCR6, 0x40, 0x40 },
		{ FES_DLCR14, 0xFF, 0xFF },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (fes_debug) {
		log(LOG_INFO, "fes_find_pc85151: probe (0x%x) for "
		    "Ungermann-Bass Access/PC N98C+ (PC85151)\n",
		    (int) ioh);
		log(LOG_INFO, "\tDLCR = %02x %02x %02x %02x %02x %02x %02x %02x\n",
		    bus_space_read_1(iot, ioh, FES_DLCR0),
		    bus_space_read_1(iot, ioh, FES_DLCR1),
		    bus_space_read_1(iot, ioh, FES_DLCR2),
		    bus_space_read_1(iot, ioh, FES_DLCR3),
		    bus_space_read_1(iot, ioh, FES_DLCR4),
		    bus_space_read_1(iot, ioh, FES_DLCR5),
		    bus_space_read_1(iot, ioh, FES_DLCR6),
		    bus_space_read_1(iot, ioh, FES_DLCR7));
	
		log(LOG_INFO, "\t       %02x %02x %02x %02x %02x %02x %02x %02x\n",
		    bus_space_read_1(iot, ioh, FES_DLCR8),
		    bus_space_read_1(iot, ioh, FES_DLCR9),
		    bus_space_read_1(iot, ioh, FES_DLCR10),
		    bus_space_read_1(iot, ioh, FES_DLCR11),
		    bus_space_read_1(iot, ioh, FES_DLCR12),
		    bus_space_read_1(iot, ioh, FES_DLCR13),
		    bus_space_read_1(iot, ioh, FES_DLCR14),
		    bus_space_read_1(iot, ioh, FES_DLCR15));
	}
#endif

	/*
	 * See if Access/PC N98C+ is on its address.
	 */
	if (!fes_simple_probe(iot, ioh, probe_table))
		return (0);

	/* Check ASIC identificaion register. */
	if (bus_space_read_1(iot, asich, FES_PC85151_IDENT)
	    != FES_IDENT_PC85151)
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
fes_detect_pc85151(iot, ioh, asich, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh, asich;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type;
	int i;
	u_short rom_sum, sum = 0;

	/* Get our station address from EEPROM. */
	bus_space_read_region_1(iot, asich, FES_PC85151_SA0, enaddr,
	    ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("fes_detect_pc85151: invalid ethernet address\n");
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
	rom_sum = bus_space_read_1(iot, asich, FES_PC85151_CKSUM);
	if (sum != rom_sum) {
#ifdef DIAGNOSTIC
		printf("fes_detect_pc85151: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		return (0);
	}

	/*
	 * Determine the card type.
	 */
	type = FES_TYPE_PC85151;

	return (type);
}

int
fes_find_pc86131(iot, ioh, asich, iobase, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh, asich;
	int *iobase, *irq;
{
	static struct fes_simple_probe_struct probe_table[] = {
		{ FES_DLCR1, 0x70, 0x00 },
		{ FES_DLCR4, 0x94, 0x00 },
		{ FES_DLCR5, 0x40, 0x40 },
		{ FES_DLCR6, 0x40, 0x40 },
		{ FES_DLCR14, 0xFF, 0xFF },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	if (fes_debug) {
		log(LOG_INFO, "fes_find_pc86131: probe (0x%x) for "
		    "Ungermann-Bass Access/NOTE N98 (PC86131)\n",
		    (int) ioh);
		log(LOG_INFO, "\tDLCR = %02x %02x %02x %02x %02x %02x %02x %02x\n",
		    bus_space_read_1(iot, ioh, FES_DLCR0),
		    bus_space_read_1(iot, ioh, FES_DLCR1),
		    bus_space_read_1(iot, ioh, FES_DLCR2),
		    bus_space_read_1(iot, ioh, FES_DLCR3),
		    bus_space_read_1(iot, ioh, FES_DLCR4),
		    bus_space_read_1(iot, ioh, FES_DLCR5),
		    bus_space_read_1(iot, ioh, FES_DLCR6),
		    bus_space_read_1(iot, ioh, FES_DLCR7));
	
		log(LOG_INFO, "\t       %02x %02x %02x %02x %02x %02x %02x %02x\n",
		    bus_space_read_1(iot, ioh, FES_DLCR8),
		    bus_space_read_1(iot, ioh, FES_DLCR9),
		    bus_space_read_1(iot, ioh, FES_DLCR10),
		    bus_space_read_1(iot, ioh, FES_DLCR11),
		    bus_space_read_1(iot, ioh, FES_DLCR12),
		    bus_space_read_1(iot, ioh, FES_DLCR13),
		    bus_space_read_1(iot, ioh, FES_DLCR14),
		    bus_space_read_1(iot, ioh, FES_DLCR15));
	}
#endif

	/*
	 * See if Access/NOTE N98 is on its address.
	 */
	if (!fes_simple_probe(iot, ioh, probe_table))
		return (0);

	/* Check ASIC identificaion register. */
	if (bus_space_read_1(iot, asich, FES_PC85151_IDENT)
	    != FES_IDENT_PC85151)
		return (0);

	/* No iobase and irq configuration data are available from board. */
	return (1);
}

int
fes_detect_pc86131(iot, ioh, asich, enaddr)
	bus_space_tag_t iot;
	bus_space_handle_t ioh, asich;
	u_int8_t enaddr[ETHER_ADDR_LEN];
{
	int type;
	int i;
	u_short rom_sum, sum = 0;

	/* Get our station address from EEPROM. */
	bus_space_read_region_1(iot, asich, FES_PC85151_SA0, enaddr,
	    ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00
	    || (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
#ifdef DIAGNOSTIC
		printf("fes_detect_pc86131: invalid ethernet address\n");
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
	rom_sum = bus_space_read_1(iot, asich, FES_PC85151_CKSUM);
	if (sum != rom_sum) {
#ifdef DIAGNOSTIC
		printf("fes_detect_pc86131: checksum mismatch; "
		    "calculated %02x != read %02x\n",
		    sum, rom_sum);
#endif
		return (0);
	}

	/*
	 * Determine the card type.
	 */
	type = FES_TYPE_PC86131;

	return (type);
}

void
fes_attach_pc85151(sc, type, myea, iobase, irq)
	struct mb86950_softc *sc;
	enum mb86950_type type;
	u_int8_t myea[ETHER_ADDR_LEN];
	int iobase, irq;
{

	/*
	 * Initialize constants in the per-line structure.
	 */
	sc->type = MB86950_TYPE_86950;
	sc->sc_nic_get = fes_nic_get_generic;
	sc->sc_nic_put = fes_nic_put_generic;
	sc->txb_pending = 0;

	/*
	 * Minimum initialization.
	 */

	/* Wait for a while. I'm not sure this is necessary. FIXME. */
	delay(400);

	/* Minimul initialization of 86950. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FES_DLCR6, FES_D6_DLC_DISABLE);
	delay(400);

	/* Disable all interrupts. */
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FES_DLCR1, 0);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, FES_DLCR3, 0);
}
