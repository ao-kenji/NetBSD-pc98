/*	$NetBSD: if_fxp.c,v 1.5.2.3 1998/02/07 10:26:42 mellon Exp $	*/

/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Modifications to support NetBSD and media selection:
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Id: if_fxp.c,v 1.44 1997/10/17 06:27:44 davidg Exp
 */

/*
 * Intel EtherExpress Pro/100B PCI Fast Ethernet driver
 */

#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"
#include "rnd.h"

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if defined(__NetBSD__)

#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if_ether.h>

#include <netinet/if_inarp.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <i386/pci/if_fxpreg.h>
#include <i386/pci/if_fxpvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#ifdef __alpha__		/* XXX */
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vm_offset_t)(va))
#endif /* __alpha__ */

#else /* __FreeBSD__ */

#include <sys/sockio.h>

#include <netinet/if_ether.h>

#include <vm/vm.h>		/* for vtophys */
#include <vm/pmap.h>		/* for vtophys */
#include <machine/clock.h>	/* for DELAY */

#include <pci/pcivar.h>
#include <pci/if_fxpreg.h>
#include <pci/if_fxpvar.h>

#endif /* __NetBSD__ */

/*
 * NOTE!  On the Alpha, we have an alignment constraint.  The
 * card DMAs the packet immediately following the RFA.  However,
 * the first thing in the packet is a 14-byte Ethernet header.
 * This means that the packet is misaligned.  To compensate,
 * we actually offset the RFA 2 bytes into the cluster.  This
 * alignes the packet after the Ethernet header at a 32-bit
 * boundary.  HOWEVER!  This means that the RFA is misaligned!
 */
#define	RFA_ALIGNMENT_FUDGE	2

/*
 * Inline function to copy a 16-bit aligned 32-bit quantity.
 */
static __inline void ofxp_lwcopy __P((volatile u_int32_t *,
	volatile u_int32_t *));
static __inline void
ofxp_lwcopy(src, dst)
	volatile u_int32_t *src, *dst;
{
	volatile u_int16_t *a = (u_int16_t *)src;
	volatile u_int16_t *b = (u_int16_t *)dst;

	b[0] = a[0];
	b[1] = a[1];
}

/*
 * Template for default configuration parameters.
 * See struct ofxp_cb_config for the bit definitions.
 */
static u_char ofxp_cb_config_template[] = {
	0x0, 0x0,		/* cb_status */
	0x80, 0x2,		/* cb_command */
	0xff, 0xff, 0xff, 0xff,	/* link_addr */
	0x16,	/*  0 */
	0x8,	/*  1 */
	0x0,	/*  2 */
	0x0,	/*  3 */
	0x0,	/*  4 */
	0x80,	/*  5 */
	0xb2,	/*  6 */
	0x3,	/*  7 */
	0x1,	/*  8 */
	0x0,	/*  9 */
	0x26,	/* 10 */
	0x0,	/* 11 */
	0x60,	/* 12 */
	0x0,	/* 13 */
	0xf2,	/* 14 */
	0x48,	/* 15 */
	0x0,	/* 16 */
	0x40,	/* 17 */
	0xf3,	/* 18 */
	0x0,	/* 19 */
	0x3f,	/* 20 */
	0x5	/* 21 */
};

/* Supported media types. */
struct ofxp_supported_media {
	const int	fsm_phy;	/* PHY type */
	const int	*fsm_media;	/* the media array */
	const int	fsm_nmedia;	/* the number of supported media */
	const int	fsm_defmedia;	/* default media for this PHY */
};

const int ofxp_media_standard[] = {
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_T|IFM_FDX,
	IFM_ETHER|IFM_100_TX,
	IFM_ETHER|IFM_100_TX|IFM_FDX,
	IFM_ETHER|IFM_AUTO,
};
#define	FXP_MEDIA_STANDARD_DEFMEDIA	(IFM_ETHER|IFM_AUTO)

const int ofxp_media_default[] = {
	IFM_ETHER|IFM_MANUAL,		/* XXX IFM_AUTO ? */
};
#define	FXP_MEDIA_DEFAULT_DEFMEDIA	(IFM_ETHER|IFM_MANUAL)

const struct ofxp_supported_media ofxp_media[] = {
	{ FXP_PHY_DP83840, ofxp_media_standard,
	  sizeof(ofxp_media_standard) / sizeof(ofxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_DP83840A, ofxp_media_standard,
	  sizeof(ofxp_media_standard) / sizeof(ofxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_82553A, ofxp_media_standard,
	  sizeof(ofxp_media_standard) / sizeof(ofxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_82553C, ofxp_media_standard,
	  sizeof(ofxp_media_standard) / sizeof(ofxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_82555, ofxp_media_standard,
	  sizeof(ofxp_media_standard) / sizeof(ofxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_80C24, ofxp_media_default,
	  sizeof(ofxp_media_default) / sizeof(ofxp_media_default[0]),
	  FXP_MEDIA_DEFAULT_DEFMEDIA },
};
#define	NFXPMEDIA (sizeof(ofxp_media) / sizeof(ofxp_media[0]))

static int ofxp_mediachange	__P((struct ifnet *));
static void ofxp_mediastatus	__P((struct ifnet *, struct ifmediareq *));
void ofxp_set_media		__P((struct ofxp_softc *, int));
static inline void ofxp_scb_wait	__P((struct ofxp_softc *));
static FXP_INTR_TYPE ofxp_intr	__P((void *));
static void ofxp_start		__P((struct ifnet *));
static int ofxp_ioctl		__P((struct ifnet *,
				    FXP_IOCTLCMD_TYPE, caddr_t));
static void ofxp_init		__P((void *));
static void ofxp_stop		__P((struct ofxp_softc *));
static void ofxp_watchdog	__P((struct ifnet *));
static int ofxp_add_rfabuf	__P((struct ofxp_softc *, struct mbuf *));
static int ofxp_mdi_read		__P((struct ofxp_softc *, int, int));
static void ofxp_mdi_write	__P((struct ofxp_softc *, int, int, int));
static void ofxp_read_eeprom	__P((struct ofxp_softc *, u_int16_t *,
				    int, int));
static int ofxp_attach_common	__P((struct ofxp_softc *, u_int8_t *));
void ofxp_stats_update		__P((void *));
static void ofxp_mc_setup	__P((struct ofxp_softc *));

/*
 * Set initial transmit threshold at 64 (512 bytes). This is
 * increased by 64 (512 bytes) at a time, to maximum of 192
 * (1536 bytes), if an underrun occurs.
 */
static int tx_threshold = 64;

/*
 * Number of transmit control blocks. This determines the number
 * of transmit buffers that can be chained in the CB list.
 * This must be a power of two.
 */
#define FXP_NTXCB	128

/*
 * TxCB list index mask. This is used to do list wrap-around.
 */
#define FXP_TXCB_MASK	(FXP_NTXCB - 1)

/*
 * Number of receive frame area buffers. These are large so chose
 * wisely.
 */
#define FXP_NRFABUFS	64

/*
 * Maximum number of seconds that the receiver can be idle before we
 * assume it's dead and attempt to reset it by reprogramming the
 * multicast filter. This is part of a work-around for a bug in the
 * NIC. See ofxp_stats_update().
 */
#define FXP_MAX_RX_IDLE	15

/*
 * Wait for the previous command to be accepted (but not necessarily
 * completed).
 */
static inline void
ofxp_scb_wait(sc)
	struct ofxp_softc *sc;
{
	int i = 10000;

	while (CSR_READ_1(sc, FXP_CSR_SCB_COMMAND) && --i);
}

/*************************************************************
 * Operating system-specific autoconfiguration glue
 *************************************************************/

#if defined(__NetBSD__)

#ifdef __BROKEN_INDIRECT_CONFIG
static int ofxp_match __P((struct device *, void *, void *));
#else
static int ofxp_match __P((struct device *, struct cfdata *, void *));
#endif
static void ofxp_attach __P((struct device *, struct device *, void *));

static void	ofxp_shutdown __P((void *));

/* Compensate for lack of a generic ether_ioctl() */
static int	ofxp_ether_ioctl __P((struct ifnet *,
				    FXP_IOCTLCMD_TYPE, caddr_t));
#define	ether_ioctl	ofxp_ether_ioctl

struct cfattach ofxp_ca = {
	sizeof(struct ofxp_softc), ofxp_match, ofxp_attach
};

struct cfdriver ofxp_cd;

/*
 * Check if a device is an 82557.
 */
static int
ofxp_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82557:
		return (1);
	}

	return (0);
}

static void
ofxp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ofxp_softc *sc = (struct ofxp_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	u_int8_t enaddr[6];
	struct ifnet *ifp;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	int ioh_valid, memh_valid;

#if 1 /* XXX: see below */
	bus_addr_t addr;
	bus_size_t size;
	int flags;
#endif

	/*
	 * Map control/status registers.
	 */
	ioh_valid = (pci_mapreg_map(pa, FXP_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);
#if 0
	memh_valid = (pci_mapreg_map(pa, FXP_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, &addr, &size) == 0);
#else
	/*
	 * XXX Card reports that it is prefetchable, which causes
	 * XXX problems on the Alpha.
	 */
	memt = pa->pa_memt;
	memh_valid = (pci_mapreg_info(pa->pa_pc, pa->pa_tag,
	    FXP_PCI_MMBA, PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT,
	      &addr, &size, &flags) == 0 &&
	    bus_space_map(pa->pa_memt, addr, size,
	      flags & ~BUS_SPACE_MAP_CACHEABLE, &memh) == 0);
#endif

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
	} else if (ioh_valid) {
		sc->sc_st = iot;
		sc->sc_sh = ioh;
	} else {
		printf(": unable to map device registers\n");
		return;
	}

	printf(": Intel EtherExpress Pro 10+/100B Ethernet\n");

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, ofxp_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	/* Do generic parts of attach. */
	if (ofxp_attach_common(sc, enaddr)) {
		/* Failed! */
		return;
	}

	printf("%s: Ethernet address %s%s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(enaddr), sc->phy_10Mbps_only ? ", 10Mbps" : "");

	ifp = &sc->sc_ethercom.ec_if;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ofxp_ioctl;
	ifp->if_start = ofxp_start;
	ifp->if_watchdog = ofxp_watchdog;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
#if NBPFILTER > 0
	bpfattach(&sc->sc_ethercom.ec_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot. Not
	 * doing do could allow DMA to corrupt kernel memory during the
	 * reboot before the driver initializes.
	 */
	shutdownhook_establish(ofxp_shutdown, sc);
}

/*
 * Device shutdown routine. Called at system shutdown after sync. The
 * main purpose of this routine is to shut off receiver DMA so that
 * kernel memory doesn't get clobbered during warmboot.
 */
static void
ofxp_shutdown(sc)
	void *sc;
{
	ofxp_stop((struct ofxp_softc *) sc);
}

static int
ofxp_ether_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	FXP_IOCTLCMD_TYPE cmd;
	caddr_t data;
{
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ofxp_softc *sc = ifp->if_softc;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ofxp_init(sc);
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			 register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			 if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)
				    LLADDR(ifp->if_sadl);
			 else
				bcopy(ina->x_host.c_host, LLADDR(ifp->if_sadl),
				    ifp->if_addrlen);
			 /* Set new address. */
			 ofxp_init(sc);
			 break;
		    }
#endif
		default:
			ofxp_init(sc);
			break;
		}
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

#else /* __FreeBSD__ */

static u_long ofxp_count;
static char *ofxp_probe		__P((pcici_t, pcidi_t));
static void ofxp_attach		__P((pcici_t, int));

static void ofxp_shutdown	__P((int, void *));

static struct pci_device ofxp_device = {
	"ofxp",
	ofxp_probe,
	ofxp_attach,
	&ofxp_count,
	NULL
};
DATA_SET(pcidevice_set, ofxp_device);

/*
 * Return identification string if this is device is ours.
 */
static char *
ofxp_probe(config_id, device_id)
	pcici_t config_id;
	pcidi_t device_id;
{
	if (((device_id & 0xffff) == FXP_VENDORID_INTEL) &&
	    ((device_id >> 16) & 0xffff) == FXP_DEVICEID_i82557)
		return ("Intel EtherExpress Pro 10/100B Ethernet");

	return NULL;
}

static void
ofxp_attach(config_id, unit)
	pcici_t config_id;
	int unit;
{
	struct ofxp_softc *sc;
	paddr_t pbase;
	struct ifnet *ifp;
	int s;

	sc = malloc(sizeof(struct ofxp_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL)
		return;
	bzero(sc, sizeof(struct ofxp_softc));
	callout_handle_init(&sc->stat_ch);

	s = splimp();

	/*
	 * Map control/status registers.
	 */
	if (!pci_map_mem(config_id, FXP_PCI_MMBA,
	    (vm_offset_t *)&sc->csr, &pbase)) {
		printf("ofxp%d: couldn't map memory\n", unit);
		goto fail;
	}

	/*
	 * Allocate our interrupt.
	 */
	if (!pci_map_int(config_id, ofxp_intr, sc, &net_imask)) {
		printf("ofxp%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Do generic parts of attach. */
	if (ofxp_attach_common(sc, sc->arpcom.ac_enaddr)) {
		/* Failed! */
		(void) pci_unmap_int(config_id);
		goto fail;
	}

	printf("ofxp%d: Ethernet address %6D%s\n", unit,
	    sc->arpcom.ac_enaddr, ":", sc->phy_10Mbps_only ? ", 10Mbps" : "");

	ifp = &sc->arpcom.ac_if;
	ifp->if_unit = unit;
	ifp->if_name = "ofxp";
	ifp->if_output = ether_output;
	ifp->if_baudrate = 100000000;
	ifp->if_init = ofxp_init;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ofxp_ioctl;
	ifp->if_start = ofxp_start;
	ifp->if_watchdog = ofxp_watchdog;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);
#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot. Not
	 * doing do could allow DMA to corrupt kernel memory during the
	 * reboot before the driver initializes.
	 */
	at_shutdown(ofxp_shutdown, sc, SHUTDOWN_POST_SYNC);

	splx(s);
	return;

 fail:
	free(sc, M_DEVBUF);
	splx(s);
}

/*
 * Device shutdown routine. Called at system shutdown after sync. The
 * main purpose of this routine is to shut off receiver DMA so that
 * kernel memory doesn't get clobbered during warmboot.
 */
static void
ofxp_shutdown(howto, sc)
	int howto;
	void *sc;
{
	ofxp_stop((struct ofxp_softc *) sc);
}

#endif /* __NetBSD__ */

/*************************************************************
 * End of operating system-specific autoconfiguration glue
 *************************************************************/

/*
 * Do generic parts of attach.
 */
static int
ofxp_attach_common(sc, enaddr)
	struct ofxp_softc *sc;
	u_int8_t *enaddr;
{
	u_int16_t data, myea[3];
	int i, nmedia, defmedia;
	const int *media;

	/*
	 * Reset to a stable state.
	 */
	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
	DELAY(10);

	sc->cbl_base = malloc(sizeof(struct ofxp_cb_tx) * FXP_NTXCB,
	    M_DEVBUF, M_NOWAIT);
	if (sc->cbl_base == NULL)
		goto fail;

	sc->ofxp_stats = malloc(sizeof(struct ofxp_stats), M_DEVBUF, M_NOWAIT);
	if (sc->ofxp_stats == NULL)
		goto fail;
	bzero(sc->ofxp_stats, sizeof(struct ofxp_stats));

	sc->mcsp = malloc(sizeof(struct ofxp_cb_mcs), M_DEVBUF, M_NOWAIT);
	if (sc->mcsp == NULL)
		goto fail;

	/*
	 * Pre-allocate our receive buffers.
	 */
	for (i = 0; i < FXP_NRFABUFS; i++) {
		if (ofxp_add_rfabuf(sc, NULL) != 0) {
			goto fail;
		}
	}

	/*
	 * Get info about the primary PHY
	 */
	ofxp_read_eeprom(sc, &data, 6, 1);
	sc->phy_primary_addr = data & 0xff;
	sc->phy_primary_device = (data >> 8) & 0x3f;
	sc->phy_10Mbps_only = data >> 15;

	/*
	 * Read MAC address.
	 */
	ofxp_read_eeprom(sc, myea, 0, 3);
	bcopy(myea, enaddr, ETHER_ADDR_LEN);

	/*
	 * Initialize the media structures.
	 */

	media = ofxp_media_default;
	nmedia = sizeof(ofxp_media_default) / sizeof(ofxp_media_default[0]);
	defmedia = FXP_MEDIA_DEFAULT_DEFMEDIA;

	for (i = 0; i < NFXPMEDIA; i++) {
		if (sc->phy_primary_device == ofxp_media[i].fsm_phy) {
			media = ofxp_media[i].fsm_media;
			nmedia = ofxp_media[i].fsm_nmedia;
			defmedia = ofxp_media[i].fsm_defmedia;
		}
	}

	ifmedia_init(&sc->sc_media, 0, ofxp_mediachange, ofxp_mediastatus);
	for (i = 0; i < nmedia; i++) {
		if (IFM_SUBTYPE(media[i]) == IFM_100_TX && sc->phy_10Mbps_only)
			continue;
		ifmedia_add(&sc->sc_media, media[i], 0, NULL);
	}
	ifmedia_set(&sc->sc_media, defmedia);

	return (0);

 fail:
	printf(FXP_FORMAT ": Failed to malloc memory\n", FXP_ARGS(sc));
	if (sc->cbl_base)
		free(sc->cbl_base, M_DEVBUF);
	if (sc->ofxp_stats)
		free(sc->ofxp_stats, M_DEVBUF);
	if (sc->mcsp)
		free(sc->mcsp, M_DEVBUF);
	/* frees entire chain */
	if (sc->rfa_headm)
		m_freem(sc->rfa_headm);

	return (ENOMEM);
}

/*
 * Read from the serial EEPROM. Basically, you manually shift in
 * the read opcode (one bit at a time) and then shift in the address,
 * and then you shift out the data (all of this one bit at a time).
 * The word size is 16 bits, so you have to provide the address for
 * every 16 bits of data.
 */
static void
ofxp_read_eeprom(sc, data, offset, words)
	struct ofxp_softc *sc;
	u_int16_t *data;
	int offset;
	int words;
{
	u_int16_t reg;
	int i, x;

	for (i = 0; i < words; i++) {
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
		/*
		 * Shift in read opcode.
		 */
		for (x = 3; x > 0; x--) {
			if (FXP_EEPROM_OPC_READ & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		/*
		 * Shift in address.
		 */
		for (x = 6; x > 0; x--) {
			if ((i + offset) & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		reg = FXP_EEPROM_EECS;
		data[i] = 0;
		/*
		 * Shift out data.
		 */
		for (x = 16; x > 0; x--) {
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			if (CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) &
			    FXP_EEPROM_EEDO)
				data[i] |= (1 << (x - 1));
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
		DELAY(1);
	}
}

/*
 * Start packet transmission on the interface.
 */
static void
ofxp_start(ifp)
	struct ifnet *ifp;
{
	struct ofxp_softc *sc = ifp->if_softc;
	struct ofxp_cb_tx *txp;
	struct mbuf *m, *mb_head;
	int segment, first = 1;

txloop:
	/*
	 * See if we're all filled up with buffers to transmit, or
	 * if we need to suspend xmit until the multicast filter
	 * has been reprogrammed (which can only be done at the
	 * head of the command chain).
	 */
	if (sc->tx_queued >= FXP_NTXCB || sc->need_mcsetup)
		return;

	/*
	 * Grab a packet to transmit.
	 */
	IF_DEQUEUE(&ifp->if_snd, mb_head);
	if (mb_head == NULL) {
		/*
		 * No more packets to send.
		 */
		return;
	}

	/*
	 * Get pointer to next available (unused) descriptor.
	 */
	txp = sc->cbl_last->next;

	/*
	 * Go through each of the mbufs in the chain and initialize
	 * the transmit buffers descriptors with the physical address
	 * and size of the mbuf.
	 */
tbdinit:
	for (m = mb_head, segment = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (segment == FXP_NTXSEG)
				break;
			txp->tbd[segment].tb_addr =
			    vtophys(mtod(m, paddr_t));
			txp->tbd[segment].tb_size = m->m_len;
			segment++;
		}
	}
	if (m != NULL) {
		struct mbuf *mn;

		/*
		 * We ran out of segments. We have to recopy this mbuf
		 * chain first.
		 */
		MGETHDR(mn, M_DONTWAIT, MT_DATA);
		if (mn == NULL) {
			m_freem(mb_head);
			return;
		}
		if (mb_head->m_pkthdr.len > MHLEN) {
			MCLGET(mn, M_DONTWAIT);
			if ((mn->m_flags & M_EXT) == 0) {
				m_freem(mn);
				m_freem(mb_head);
				return;
			}
		}
		m_copydata(mb_head, 0, mb_head->m_pkthdr.len,
		    mtod(mn, caddr_t));
		mn->m_pkthdr.len = mn->m_len = mb_head->m_pkthdr.len;
		m_freem(mb_head);
		mb_head = mn;
		goto tbdinit;
	}

	txp->tbd_number = segment;
	txp->mb_head = mb_head;

	/*
	 * Finish the initialization of this TxCB.
	 */
	txp->cb_status = 0;
	txp->cb_command =
	    FXP_CB_COMMAND_XMIT | FXP_CB_COMMAND_SF | FXP_CB_COMMAND_S;
	txp->tx_threshold = tx_threshold;
	
	/*
	 * Advance the end-of-list forward.
	 */
	sc->cbl_last->cb_command &= ~FXP_CB_COMMAND_S;
	sc->cbl_last = txp;

	/*
	 * Advance the beginning of the list forward if there are
	 * no other packets queued (when nothing is queued, cbl_first
	 * sits on the last TxCB that was sent out)..
	 */
	if (sc->tx_queued == 0)
		sc->cbl_first = txp;

	sc->tx_queued++;

	/*
	 * Only need to wait prior to the first resume command.
	 */
	if (first) {
		first--;
		ofxp_scb_wait(sc);
	}

	/*
	 * Resume transmission if suspended.
	 */
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_RESUME);

#if NBPFILTER > 0
	/*
	 * Pass packet to bpf if there is a listener.
	 */
	if (ifp->if_bpf)
		bpf_mtap(FXP_BPFTAP_ARG(ifp), mb_head);
#endif
	/*
	 * Set a 5 second timer just in case we don't hear from the
	 * card again.
	 */
	ifp->if_timer = 5;

	goto txloop;
}

/*
 * Process interface interrupts.
 */
static FXP_INTR_TYPE
ofxp_intr(arg)
	void *arg;
{
	struct ofxp_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	u_int8_t statack;
#if defined(__NetBSD__)
	int claimed = 0;
#endif

	while ((statack = CSR_READ_1(sc, FXP_CSR_SCB_STATACK)) != 0) {
#if defined(__NetBSD__)
		claimed = 1;
#endif
		/*
		 * First ACK all the interrupts in this pass.
		 */
		CSR_WRITE_1(sc, FXP_CSR_SCB_STATACK, statack);

		/*
		 * Process receiver interrupts. If a no-resource (RNR)
		 * condition exists, get whatever packets we can and
		 * re-start the receiver.
		 */
		if (statack & (FXP_SCB_STATACK_FR | FXP_SCB_STATACK_RNR)) {
			struct mbuf *m;
			struct ofxp_rfa *rfa;
rcvloop:
			m = sc->rfa_headm;
			rfa = (struct ofxp_rfa *)(m->m_ext.ext_buf +
			    RFA_ALIGNMENT_FUDGE);

			if (rfa->rfa_status & FXP_RFA_STATUS_C) {
				/*
				 * Remove first packet from the chain.
				 */
				sc->rfa_headm = m->m_next;
				m->m_next = NULL;

				/*
				 * Add a new buffer to the receive chain.
				 * If this fails, the old buffer is recycled
				 * instead.
				 */
				if (ofxp_add_rfabuf(sc, m) == 0) {
					struct ether_header *eh;
					u_int16_t total_len;

					total_len = rfa->actual_size &
					    (MCLBYTES - 1);
					if (total_len <
					    sizeof(struct ether_header)) {
						m_freem(m);
						goto rcvloop;
					}
					m->m_pkthdr.rcvif = ifp;
					m->m_pkthdr.len = m->m_len =
					    total_len -
					    sizeof(struct ether_header);
					eh = mtod(m, struct ether_header *);
#if NBPFILTER > 0
					if (ifp->if_bpf) {
						bpf_tap(FXP_BPFTAP_ARG(ifp),
						    mtod(m, caddr_t),
						    total_len); 
						/*
						 * Only pass this packet up
						 * if it is for us.
						 */
						if ((ifp->if_flags &
						    IFF_PROMISC) &&
						    (rfa->rfa_status &
						    FXP_RFA_STATUS_IAMATCH) &&
						    (eh->ether_dhost[0] & 1)
						    == 0) {
							m_freem(m);
							goto rcvloop;
						}
					}
#endif /* NBPFILTER > 0 */
					m->m_data +=
					    sizeof(struct ether_header);
					ether_input(ifp, eh, m);
				}
				goto rcvloop;
			}
			if (statack & FXP_SCB_STATACK_RNR) {
				ofxp_scb_wait(sc);
				CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL,
				    vtophys(sc->rfa_headm->m_ext.ext_buf) +
					RFA_ALIGNMENT_FUDGE);
				CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND,
				    FXP_SCB_COMMAND_RU_START);
			}
		}
		/*
		 * Free any finished transmit mbuf chains.
		 */
		if (statack & FXP_SCB_STATACK_CNA) {
			struct ofxp_cb_tx *txp;

			for (txp = sc->cbl_first; sc->tx_queued &&
			    (txp->cb_status & FXP_CB_STATUS_C) != 0;
			    txp = txp->next) {
				if (txp->mb_head != NULL) {
					m_freem(txp->mb_head);
					txp->mb_head = NULL;
				}
				sc->tx_queued--;
			}
			sc->cbl_first = txp;
			if (sc->tx_queued == 0) {
				ifp->if_timer = 0;
				if (sc->need_mcsetup)
					ofxp_mc_setup(sc);
			}
			/*
			 * Try to start more packets transmitting.
			 */
			if (ifp->if_snd.ifq_head != NULL)
				ofxp_start(ifp);
		}
	}
#if defined(__NetBSD__)
	return (claimed);
#endif
}

/*
 * Update packet in/out/collision statistics. The i82557 doesn't
 * allow you to access these counters without doing a fairly
 * expensive DMA to get _all_ of the statistics it maintains, so
 * we do this operation here only once per second. The statistics
 * counters in the kernel are updated from the previous dump-stats
 * DMA and then a new dump-stats DMA is started. The on-chip
 * counters are zeroed when the DMA completes. If we can't start
 * the DMA immediately, we don't wait - we just prepare to read
 * them again next time.
 */
void
ofxp_stats_update(arg)
	void *arg;
{
	struct ofxp_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	struct ofxp_stats *sp = sc->ofxp_stats;
	int s;

	ifp->if_opackets += sp->tx_good;
	ifp->if_collisions += sp->tx_total_collisions;
	if (sp->rx_good) {
		ifp->if_ipackets += sp->rx_good;
		sc->rx_idle_secs = 0;
	} else {
		sc->rx_idle_secs++;
	}
	ifp->if_ierrors +=
	    sp->rx_crc_errors +
	    sp->rx_alignment_errors +
	    sp->rx_rnr_errors +
	    sp->rx_overrun_errors;
	/*
	 * If any transmit underruns occured, bump up the transmit
	 * threshold by another 512 bytes (64 * 8).
	 */
	if (sp->tx_underruns) {
		ifp->if_oerrors += sp->tx_underruns;
		if (tx_threshold < 192)
			tx_threshold += 64;
	}
	s = splimp();
	/*
	 * If we haven't received any packets in FXP_MAC_RX_IDLE seconds,
	 * then assume the receiver has locked up and attempt to clear
	 * the condition by reprogramming the multicast filter. This is
	 * a work-around for a bug in the 82557 where the receiver locks
	 * up if it gets certain types of garbage in the syncronization
	 * bits prior to the packet header. This bug is supposed to only
	 * occur in 10Mbps mode, but has been seen to occur in 100Mbps
	 * mode as well (perhaps due to a 10/100 speed transition).
	 */
	if (sc->rx_idle_secs > FXP_MAX_RX_IDLE) {
		sc->rx_idle_secs = 0;
		ofxp_mc_setup(sc);
	}
	/*
	 * If there is no pending command, start another stats
	 * dump. Otherwise punt for now.
	 */
	if (CSR_READ_1(sc, FXP_CSR_SCB_COMMAND) == 0) {
		/*
		 * Start another stats dump.
		 */
		CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND,
		    FXP_SCB_COMMAND_CU_DUMPRESET);
	} else {
		/*
		 * A previous command is still waiting to be accepted.
		 * Just zero our copy of the stats and wait for the
		 * next timer event to update them.
		 */
		sp->tx_good = 0;
		sp->tx_underruns = 0;
		sp->tx_total_collisions = 0;

		sp->rx_good = 0;
		sp->rx_crc_errors = 0;
		sp->rx_alignment_errors = 0;
		sp->rx_rnr_errors = 0;
		sp->rx_overrun_errors = 0;
	}
	splx(s);
	/*
	 * Schedule another timeout one second from now.
	 */
	FXP_TIMEOUT(sc, ofxp_stats_update, hz);
}

/*
 * Stop the interface. Cancels the statistics updater and resets
 * the interface.
 */
static void
ofxp_stop(sc)
	struct ofxp_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	struct ofxp_cb_tx *txp;
	int i;

	/*
	 * Cancel stats updater.
	 */
	FXP_UNTIMEOUT(sc, ofxp_stats_update);

	/*
	 * Issue software reset
	 */
	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
	DELAY(10);

	/*
	 * Release any xmit buffers.
	 */
	for (txp = sc->cbl_first; txp != NULL && txp->mb_head != NULL;
	    txp = txp->next) {
		m_freem(txp->mb_head);
		txp->mb_head = NULL;
	}
	sc->tx_queued = 0;

	/*
	 * Free all the receive buffers then reallocate/reinitialize
	 */
	if (sc->rfa_headm != NULL)
		m_freem(sc->rfa_headm);
	sc->rfa_headm = NULL;
	sc->rfa_tailm = NULL;
	for (i = 0; i < FXP_NRFABUFS; i++) {
		if (ofxp_add_rfabuf(sc, NULL) != 0) {
			/*
			 * This "can't happen" - we're at splimp()
			 * and we just freed all the buffers we need
			 * above.
			 */
			panic("ofxp_stop: no buffers!");
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

/*
 * Watchdog/transmission transmit timeout handler. Called when a
 * transmission is started on the interface, but no interrupt is
 * received before the timeout. This usually indicates that the
 * card has wedged for some reason.
 */
static void
ofxp_watchdog(ifp)
	struct ifnet *ifp;
{
	struct ofxp_softc *sc = ifp->if_softc;

	printf(FXP_FORMAT ": device timeout\n", FXP_ARGS(sc));
	ifp->if_oerrors++;

	ofxp_init(sc);
}

static void
ofxp_init(xsc)
	void *xsc;
{
	struct ofxp_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_if;
	struct ofxp_cb_config *cbp;
	struct ofxp_cb_ias *cb_ias;
	struct ofxp_cb_tx *txp;
	int i, s, prm;

	s = splimp();
	/*
	 * Cancel any pending I/O
	 */
	ofxp_stop(sc);

	prm = (ifp->if_flags & IFF_PROMISC) ? 1 : 0;
	sc->promisc_mode = prm;

	/*
	 * Initialize base of CBL and RFA memory. Loading with zero
	 * sets it up for regular linear addressing.
	 */
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, 0);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_BASE);

	ofxp_scb_wait(sc);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_RU_BASE);

	/*
	 * Initialize base of dump-stats buffer.
	 */
	ofxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, vtophys(sc->ofxp_stats));
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_DUMP_ADR);

	/*
	 * We temporarily use memory that contains the TxCB list to
	 * construct the config CB. The TxCB list memory is rebuilt
	 * later.
	 */
	cbp = (struct ofxp_cb_config *) sc->cbl_base;

	/*
	 * This bcopy is kind of disgusting, but there are a bunch of must be
	 * zero and must be one bits in this structure and this is the easiest
	 * way to initialize them all to proper values.
	 */
	bcopy(ofxp_cb_config_template, (void *)&cbp->cb_status,
		sizeof(ofxp_cb_config_template));

	cbp->cb_status =	0;
	cbp->cb_command =	FXP_CB_COMMAND_CONFIG | FXP_CB_COMMAND_EL;
	cbp->link_addr =	-1;	/* (no) next command */
	cbp->byte_count =	22;	/* (22) bytes to config */
	cbp->rx_fifo_limit =	8;	/* rx fifo threshold (32 bytes) */
	cbp->tx_fifo_limit =	0;	/* tx fifo threshold (0 bytes) */
	cbp->adaptive_ifs =	0;	/* (no) adaptive interframe spacing */
	cbp->rx_dma_bytecount =	0;	/* (no) rx DMA max */
	cbp->tx_dma_bytecount =	0;	/* (no) tx DMA max */
	cbp->dma_bce =		0;	/* (disable) dma max counters */
	cbp->late_scb =		0;	/* (don't) defer SCB update */
	cbp->tno_int =		0;	/* (disable) tx not okay interrupt */
	cbp->ci_int =		0;	/* interrupt on CU not active */
	cbp->save_bf =		prm;	/* save bad frames */
	cbp->disc_short_rx =	!prm;	/* discard short packets */
	cbp->underrun_retry =	1;	/* retry mode (1) on DMA underrun */
	cbp->mediatype =	!sc->phy_10Mbps_only; /* interface mode */
	cbp->nsai =		1;	/* (don't) disable source addr insert */
	cbp->preamble_length =	2;	/* (7 byte) preamble */
	cbp->loopback =		0;	/* (don't) loopback */
	cbp->linear_priority =	0;	/* (normal CSMA/CD operation) */
	cbp->linear_pri_mode =	0;	/* (wait after xmit only) */
	cbp->interfrm_spacing =	6;	/* (96 bits of) interframe spacing */
	cbp->promiscuous =	prm;	/* promiscuous mode */
	cbp->bcast_disable =	0;	/* (don't) disable broadcasts */
	cbp->crscdt =		0;	/* (CRS only) */
	cbp->stripping =	!prm;	/* truncate rx packet to byte count */
	cbp->padding =		1;	/* (do) pad short tx packets */
	cbp->rcv_crc_xfer =	0;	/* (don't) xfer CRC to host */
	cbp->force_fdx =	0;	/* (don't) force full duplex */
	cbp->fdx_pin_en =	1;	/* (enable) FDX# pin */
	cbp->multi_ia =		0;	/* (don't) accept multiple IAs */
	cbp->mc_all =		sc->all_mcasts;/* accept all multicasts */

	/*
	 * Start the config command/DMA.
	 */
	ofxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, vtophys(&cbp->cb_status));
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	while (!(cbp->cb_status & FXP_CB_STATUS_C));

	/*
	 * Now initialize the station address. Temporarily use the TxCB
	 * memory area like we did above for the config CB.
	 */
	cb_ias = (struct ofxp_cb_ias *) sc->cbl_base;
	cb_ias->cb_status = 0;
	cb_ias->cb_command = FXP_CB_COMMAND_IAS | FXP_CB_COMMAND_EL;
	cb_ias->link_addr = -1;
#if defined(__NetBSD__)
	bcopy(LLADDR(ifp->if_sadl), (void *)cb_ias->macaddr, 6);
#else
	bcopy(sc->arpcom.ac_enaddr, (void *)cb_ias->macaddr,
	    sizeof(sc->arpcom.ac_enaddr));
#endif /* __NetBSD__ */

	/*
	 * Start the IAS (Individual Address Setup) command/DMA.
	 */
	ofxp_scb_wait(sc);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	while (!(cb_ias->cb_status & FXP_CB_STATUS_C));

	/*
	 * Initialize transmit control block (TxCB) list.
	 */

	txp = sc->cbl_base;
	bzero(txp, sizeof(struct ofxp_cb_tx) * FXP_NTXCB);
	for (i = 0; i < FXP_NTXCB; i++) {
		txp[i].cb_status = FXP_CB_STATUS_C | FXP_CB_STATUS_OK;
		txp[i].cb_command = FXP_CB_COMMAND_NOP;
		txp[i].link_addr = vtophys(&txp[(i + 1) & FXP_TXCB_MASK].cb_status);
		txp[i].tbd_array_addr = vtophys(&txp[i].tbd[0]);
		txp[i].next = &txp[(i + 1) & FXP_TXCB_MASK];
	}
	/*
	 * Set the suspend flag on the first TxCB and start the control
	 * unit. It will execute the NOP and then suspend.
	 */
	txp->cb_command = FXP_CB_COMMAND_NOP | FXP_CB_COMMAND_S;
	sc->cbl_first = sc->cbl_last = txp;
	sc->tx_queued = 1;

	ofxp_scb_wait(sc);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);

	/*
	 * Initialize receiver buffer area - RFA.
	 */
	ofxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL,
	    vtophys(sc->rfa_headm->m_ext.ext_buf) + RFA_ALIGNMENT_FUDGE);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_RU_START);

	/*
	 * Set current media.
	 */
	ofxp_set_media(sc, sc->sc_media.ifm_cur->ifm_media);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);

	/*
	 * Start stats updater.
	 */
	FXP_TIMEOUT(sc, ofxp_stats_update, hz);
}

void
ofxp_set_media(sc, media)
	struct ofxp_softc *sc;
	int media;
{

	switch (sc->phy_primary_device) {
	case FXP_PHY_DP83840:
	case FXP_PHY_DP83840A:
		ofxp_mdi_write(sc, sc->phy_primary_addr, FXP_DP83840_PCR,
		    ofxp_mdi_read(sc, sc->phy_primary_addr, FXP_DP83840_PCR) |
		    FXP_DP83840_PCR_LED4_MODE |	/* LED4 always indicates duplex */
		    FXP_DP83840_PCR_F_CONNECT |	/* force link disconnect bypass */
		    FXP_DP83840_PCR_BIT10);	/* XXX I have no idea */
		/* fall through */
	case FXP_PHY_82553A:
	case FXP_PHY_82553C:			/* XXX untested */
	case FXP_PHY_82555:
		if (IFM_SUBTYPE(media) != IFM_AUTO) {
			int flags;

			flags = (IFM_SUBTYPE(media) == IFM_100_TX) ?
			    FXP_PHY_BMCR_SPEED_100M : 0;
			flags |= (media & IFM_FDX) ?
			    FXP_PHY_BMCR_FULLDUPLEX : 0;
			ofxp_mdi_write(sc, sc->phy_primary_addr,
			    FXP_PHY_BMCR,
			    (ofxp_mdi_read(sc, sc->phy_primary_addr,
			    FXP_PHY_BMCR) &
			    ~(FXP_PHY_BMCR_AUTOEN | FXP_PHY_BMCR_SPEED_100M |
			     FXP_PHY_BMCR_FULLDUPLEX)) | flags);
		} else {
			ofxp_mdi_write(sc, sc->phy_primary_addr,
			    FXP_PHY_BMCR,
			    (ofxp_mdi_read(sc, sc->phy_primary_addr,
			    FXP_PHY_BMCR) | FXP_PHY_BMCR_AUTOEN));
		}
		break;
	/*
	 * The Seeq 80c24 doesn't have a PHY programming interface, so do
	 * nothing.
	 */
	case FXP_PHY_80C24:
		break;
	default:
		printf(FXP_FORMAT
		    ": warning: unsupported PHY, type = %d, addr = %d\n",
		     FXP_ARGS(sc), sc->phy_primary_device,
		     sc->phy_primary_addr);
	}
}

/*
 * Change media according to request.
 */
int
ofxp_mediachange(ifp)
	struct ifnet *ifp;
{
	struct ofxp_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_media;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	ofxp_set_media(sc, ifm->ifm_media);
	return (0);
}

/*
 * Notify the world which media we're using.
 */
void
ofxp_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct ofxp_softc *sc = ifp->if_softc;
	int flags;

	switch (sc->phy_primary_device) {
	case FXP_PHY_DP83840:
	case FXP_PHY_DP83840A:
	case FXP_PHY_82555:
		flags = ofxp_mdi_read(sc, sc->phy_primary_addr, FXP_PHY_BMCR);
		ifmr->ifm_active = IFM_ETHER;
		if (flags & FXP_PHY_BMCR_AUTOEN)
			ifmr->ifm_active |= IFM_AUTO;
		else {
			if (flags & FXP_PHY_BMCR_SPEED_100M)
				ifmr->ifm_active |= IFM_100_TX;
			else
				ifmr->ifm_active |= IFM_10_T;

			if (flags & FXP_PHY_BMCR_FULLDUPLEX)
				ifmr->ifm_active |= IFM_FDX;
		}
		break;

	case FXP_PHY_80C24:
	default:
		ifmr->ifm_active = IFM_ETHER|IFM_MANUAL; /* XXX IFM_AUTO ? */
	}
}

/*
 * Add a buffer to the end of the RFA buffer list.
 * Return 0 if successful, 1 for failure. A failure results in
 * adding the 'oldm' (if non-NULL) on to the end of the list -
 * tossing out it's old contents and recycling it.
 * The RFA struct is stuck at the beginning of mbuf cluster and the
 * data pointer is fixed up to point just past it.
 */
static int
ofxp_add_rfabuf(sc, oldm)
	struct ofxp_softc *sc;
	struct mbuf *oldm;
{
	u_int32_t v;
	struct mbuf *m;
	struct ofxp_rfa *rfa, *p_rfa;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m != NULL) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			if (oldm == NULL)
				return 1;
			m = oldm;
			m->m_data = m->m_ext.ext_buf;
		}
	} else {
		if (oldm == NULL)
			return 1;
		m = oldm;
		m->m_data = m->m_ext.ext_buf;
	}

	/*
	 * Move the data pointer up so that the incoming data packet
	 * will be 32-bit aligned.
	 */
	m->m_data += RFA_ALIGNMENT_FUDGE;

	/*
	 * Get a pointer to the base of the mbuf cluster and move
	 * data start past it.
	 */
	rfa = mtod(m, struct ofxp_rfa *);
	m->m_data += sizeof(struct ofxp_rfa);
	rfa->size = MCLBYTES - sizeof(struct ofxp_rfa) - RFA_ALIGNMENT_FUDGE;

	/*
	 * Initialize the rest of the RFA.  Note that since the RFA
	 * is misaligned, we cannot store values directly.  Instead,
	 * we use an optimized, inline copy.
	 */
	rfa->rfa_status = 0;
	rfa->rfa_control = FXP_RFA_CONTROL_EL;
	rfa->actual_size = 0;

	v = -1;
	ofxp_lwcopy(&v, &rfa->link_addr);
	ofxp_lwcopy(&v, &rfa->rbd_addr);

	/*
	 * If there are other buffers already on the list, attach this
	 * one to the end by fixing up the tail to point to this one.
	 */
	if (sc->rfa_headm != NULL) {
		p_rfa = (struct ofxp_rfa *) (sc->rfa_tailm->m_ext.ext_buf +
		    RFA_ALIGNMENT_FUDGE);
		sc->rfa_tailm->m_next = m;
		v = vtophys(rfa);
		ofxp_lwcopy(&v, &p_rfa->link_addr);
		p_rfa->rfa_control &= ~FXP_RFA_CONTROL_EL;
	} else {
		sc->rfa_headm = m;
	}
	sc->rfa_tailm = m;

	return (m == oldm);
}

static volatile int
ofxp_mdi_read(sc, phy, reg)
	struct ofxp_softc *sc;
	int phy;
	int reg;
{
	int count = 10000;
	int value;

	CSR_WRITE_4(sc, FXP_CSR_MDICONTROL,
	    (FXP_MDI_READ << 26) | (reg << 16) | (phy << 21));

	while (((value = CSR_READ_4(sc, FXP_CSR_MDICONTROL)) & 0x10000000) == 0
	    && count--)
		DELAY(10);

	if (count <= 0)
		printf(FXP_FORMAT ": ofxp_mdi_read: timed out\n",
		    FXP_ARGS(sc));

	return (value & 0xffff);
}

static void
ofxp_mdi_write(sc, phy, reg, value)
	struct ofxp_softc *sc;
	int phy;
	int reg;
	int value;
{
	int count = 10000;

	CSR_WRITE_4(sc, FXP_CSR_MDICONTROL,
	    (FXP_MDI_WRITE << 26) | (reg << 16) | (phy << 21) |
	    (value & 0xffff));

	while((CSR_READ_4(sc, FXP_CSR_MDICONTROL) & 0x10000000) == 0 &&
	    count--)
		DELAY(10);

	if (count <= 0)
		printf(FXP_FORMAT ": ofxp_mdi_write: timed out\n",
		    FXP_ARGS(sc));
}

static int
ofxp_ioctl(ifp, command, data)
	struct ifnet *ifp;
	FXP_IOCTLCMD_TYPE command;
	caddr_t data;
{
	struct ofxp_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splimp();

	switch (command) {

	case SIOCSIFADDR:
#if !defined(__NetBSD__)
	case SIOCGIFADDR:
	case SIOCSIFMTU:
#endif
		error = ether_ioctl(ifp, command, data);
		break;

	case SIOCSIFFLAGS:
		sc->all_mcasts = (ifp->if_flags & IFF_ALLMULTI) ? 1 : 0;

		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 * XXX If it's up then re-initialize it. This is so flags
		 * such as IFF_PROMISC are handled.
		 */
		if (ifp->if_flags & IFF_UP) {
			ofxp_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ofxp_stop(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sc->all_mcasts = (ifp->if_flags & IFF_ALLMULTI) ? 1 : 0;
#if defined(__NetBSD__)
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ethercom) :
		    ether_delmulti(ifr, &sc->sc_ethercom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (!sc->all_mcasts)
				ofxp_mc_setup(sc);
			/*
			 * ofxp_mc_setup() can turn on all_mcasts if we run
			 * out of space, so check it again rather than else {}.
			 */
			if (sc->all_mcasts)
				ofxp_init(sc);
			error = 0;
		}
#else /* __FreeBSD__ */
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		if (!sc->all_mcasts)
			ofxp_mc_setup(sc);
		/*
		 * ofxp_mc_setup() can turn on sc->all_mcasts, so check it
		 * again rather than else {}.
		 */
		if (sc->all_mcasts)
			ofxp_init(sc);
		error = 0;
#endif /* __NetBSD__ */
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, command);
		break;

	default:
		error = EINVAL;
	}
	(void) splx(s);
	return (error);
}

/*
 * Program the multicast filter.
 *
 * We have an artificial restriction that the multicast setup command
 * must be the first command in the chain, so we take steps to ensure
 * that. By requiring this, it allows us to keep the performance of
 * the pre-initialized command ring (esp. link pointers) by not actually
 * inserting the mcsetup command in the ring - i.e. it's link pointer
 * points to the TxCB ring, but the mcsetup descriptor itself is not part
 * of it. We then can do 'CU_START' on the mcsetup descriptor and have it
 * lead into the regular TxCB ring when it completes.
 *
 * This function must be called at splimp.
 */
static void
ofxp_mc_setup(sc)
	struct ofxp_softc *sc;
{
	struct ofxp_cb_mcs *mcsp = sc->mcsp;
	struct ifnet *ifp = &sc->sc_if;
#if defined(__NetBSD__)
	struct ethercom *ec = &sc->sc_ethercom;
	struct ether_multi *enm;
	struct ether_multistep step;
#else
	struct ifmultiaddr *ifma;
#endif /* __NetBSD__ */
	int nmcasts;

	if (sc->tx_queued) {
		sc->need_mcsetup = 1;
		return;
	}
	sc->need_mcsetup = 0;

	/*
	 * Initialize multicast setup descriptor.
	 */
	mcsp->next = sc->cbl_base;
	mcsp->mb_head = NULL;
	mcsp->cb_status = 0;
	mcsp->cb_command = FXP_CB_COMMAND_MCAS | FXP_CB_COMMAND_S;
	mcsp->link_addr = vtophys(&sc->cbl_base->cb_status);

	nmcasts = 0;
	if (!sc->all_mcasts) {
#if defined(__NetBSD__)
		ETHER_FIRST_MULTI(step, ec, enm);
		while (enm != NULL) {
			/*
			 * Check for too many multicast addresses or if we're
			 * listening to a range.  Either way, we simply have
			 * to accept all multicasts.
			 */
			if (nmcasts >= MAXMCADDR ||
			    bcmp(enm->enm_addrlo, enm->enm_addrhi,
			    ETHER_ADDR_LEN) != 0) {
				sc->all_mcasts = 1;
				nmcasts = 0;
				break;
			}
			bcopy(enm->enm_addrlo,
			    (void *) &sc->mcsp->mc_addr[nmcasts][0],
			    ETHER_ADDR_LEN);
			nmcasts++;
			ETHER_NEXT_MULTI(step, enm);
		}
#else /* __FreeBSD__ */
		for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
		    ifma = ifma->ifma_link.le_next) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			if (nmcasts >= MAXMCADDR) {
				sc->all_mcasts = 1;
				nmcasts = 0;
				break;
			}
			bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
			    (void *) &sc->mcsp->mc_addr[nmcasts][0], 6);
			nmcasts++;
		}
#endif /* __NetBSD__ */
	}
	mcsp->mc_cnt = nmcasts * 6;
	sc->cbl_first = sc->cbl_last = (struct ofxp_cb_tx *) mcsp;
	sc->tx_queued = 1;

	/*
	 * Wait until command unit is not active. This should never
	 * be the case when nothing is queued, but make sure anyway.
	 */
	while ((CSR_READ_1(sc, FXP_CSR_SCB_RUSCUS) >> 6) ==
	    FXP_SCB_CUS_ACTIVE) ;

	/*
	 * Start the multicast setup command.
	 */
	ofxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, vtophys(&mcsp->cb_status));
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);

	ifp->if_timer = 5;
	return;
}
