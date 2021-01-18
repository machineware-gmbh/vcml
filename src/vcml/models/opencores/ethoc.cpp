/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/opencores/ethoc.h"

#define ETH_ALEN                6
#define ETH_FRAME_LEN           1514
#define ETH_FCS_LEN             4
#define ETH_MAX_PACKET_LEN      (ETH_FRAME_LEN + ETH_FCS_LEN)

/* The following stuff is taken from the Linux kernel. It is used during
 * access of the MIICOMMAND register. */

/* Generic MII registers */
#define MII_BMCR                0x00    /* Basic mode control register */
#define MII_BMSR                0x01    /* Basic mode status register  */
#define MII_PHYSID1             0x02    /* PHYS ID 1                   */
#define MII_PHYSID2             0x03    /* PHYS ID 2                   */
#define MII_ADVERTISE           0x04    /* Advertisement control reg   */
#define MII_LPA                 0x05    /* Link partner ability reg    */
#define MII_EXPANSION           0x06    /* Expansion register          */
#define MII_CTRL1000            0x09    /* 1000BASE-T control          */
#define MII_STAT1000            0x0a    /* 1000BASE-T status           */
#define MII_ESTATUS             0x0f    /* Extended Status             */
#define MII_DCOUNTER            0x12    /* Disconnect counter          */
#define MII_FCSCOUNTER          0x13    /* False carrier counter       */
#define MII_NWAYTEST            0x14    /* N-way auto-neg test reg     */
#define MII_RERRCOUNTER         0x15    /* Receive error counter       */
#define MII_SREVISION           0x16    /* Silicon revision            */
#define MII_RESV1               0x17    /* Reserved...                 */
#define MII_LBRERROR            0x18    /* Lpback, rx, bypass error    */
#define MII_PHYADDR             0x19    /* PHY address                 */
#define MII_RESV2               0x1a    /* Reserved...                 */
#define MII_TPISTATUS           0x1b    /* TPI status for 10mbps       */
#define MII_NCONFIG             0x1c    /* Network interface config    */

/* Basic mode control register */
#define BMCR_RESV               0x003f  /* Unused...                   */
#define BMCR_SPEED1000          0x0040  /* MSB of Speed (1000)         */
#define BMCR_CTST               0x0080  /* Collision test              */
#define BMCR_FULLDPLX           0x0100  /* Full duplex                 */
#define BMCR_ANRESTART          0x0200  /* Auto negotiation restart    */
#define BMCR_ISOLATE            0x0400  /* Isolate data paths from MII */
#define BMCR_PDOWN              0x0800  /* Enable low power state      */
#define BMCR_ANENABLE           0x1000  /* Enable auto negotiation     */
#define BMCR_SPEED100           0x2000  /* Select 100Mbps              */
#define BMCR_LOOPBACK           0x4000  /* TXD loopback bits           */
#define BMCR_RESET              0x8000  /* Reset to default state      */

/* Basic mode status register */
#define BMSR_ERCAP              0x0001    /* Ext-reg capability          */
#define BMSR_JCD                0x0002    /* Jabber detected             */
#define BMSR_LSTATUS            0x0004    /* Link status                 */
#define BMSR_ANEGCAPABLE        0x0008    /* Able to do auto-negotiation */
#define BMSR_RFAULT             0x0010    /* Remote fault detected       */
#define BMSR_ANEGCOMPLETE       0x0020    /* Auto-negotiation complete   */
#define BMSR_RESV               0x00c0    /* Unused...                   */
#define BMSR_ESTATEN            0x0100    /* Extended Status in R15      */
#define BMSR_100HALF2           0x0200    /* Can do 100BASE-T2 HDX       */
#define BMSR_100FULL2           0x0400    /* Can do 100BASE-T2 FDX       */
#define BMSR_10HALF             0x0800    /* Can do 10mbps, half-duplex  */
#define BMSR_10FULL             0x1000    /* Can do 10mbps, full-duplex  */
#define BMSR_100HALF            0x2000    /* Can do 100mbps, half-duplex */
#define BMSR_100FULL            0x4000    /* Can do 100mbps, full-duplex */
#define BMSR_100BASE4           0x8000    /* Can do 100mbps, 4k packets  */

/* Advertisement control register */
#define ADVERTISE_SLCT          0x001f    /* Selector bits               */
#define ADVERTISE_CSMA          0x0001    /* Only selector supported     */
#define ADVERTISE_10HALF        0x0020    /* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL     0x0020    /* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL        0x0040    /* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF     0x0040    /* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF       0x0080    /* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE    0x0080    /* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL       0x0100    /* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM 0x0100    /* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4      0x0200    /* Try for 100mbps 4k packets  */
#define ADVERTISE_PAUSE_CAP     0x0400    /* Try for pause               */
#define ADVERTISE_PAUSE_ASYM    0x0800    /* Try for asymetric pause     */
#define ADVERTISE_RESV          0x1000    /* Unused...                   */
#define ADVERTISE_RFAULT        0x2000    /* Say we can detect faults    */
#define ADVERTISE_LPACK         0x4000    /* Ack link partners response  */
#define ADVERTISE_NPAGE         0x8000    /* Next page bit               */

#define ADVERTISE_FULL          (ADVERTISE_100FULL | ADVERTISE_10FULL   | \
                                 ADVERTISE_CSMA)
#define ADVERTISE_ALL           (ADVERTISE_10HALF | ADVERTISE_10FULL    | \
                                 ADVERTISE_100HALF | ADVERTISE_100FULL)

/* Link partner ability register */
#define LPA_SLCT                0x001f    /* Same as advertise selector  */
#define LPA_10HALF              0x0020    /* Can do 10mbps half-duplex   */
#define LPA_10FULL              0x0040    /* Can do 10mbps full-duplex   */
#define LPA_100HALF             0x0080    /* Can do 100mbps half-duplex  */
#define LPA_100FULL             0x0100    /* Can do 100mbps full-duplex  */
#define LPA_100BASE4            0x0200    /* Can do 100mbps 4k packets   */
#define LPA_RESV                0x1c00    /* Unused...                   */
#define LPA_RFAULT              0x2000    /* Link partner faulted        */
#define LPA_LPACK               0x4000    /* Link partner acked us       */
#define LPA_NPAGE               0x8000    /* Next page bit               */

#define LPA_DUPLEX              (LPA_10FULL | LPA_100FULL)
#define LPA_100                 (LPA_100FULL | LPA_100HALF | LPA_100BASE4)

/* ID of DP83848C 10/100 PHY (National Semiconductor / Texas Instruments) */
#define ETHOC_PHYID1            0x2000
#define ETHOC_PHYID2            0x5c90

namespace vcml { namespace opencores {

    static const u8 bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    SC_HAS_PROCESS(ethoc);

    void ethoc::tx_process() {
        while (true) {
            wait(m_tx_event);
            while (m_tx_enabled) {
                tx_poll();
                sc_time cycle = sc_time(1.0 / clock, SC_SEC);
                sc_time quantum = tlm_global_quantum::instance().get();
                wait(max(cycle, quantum));
            }
        }
    }

    void ethoc::rx_process() {
        while (true) {
            wait(m_rx_event);
            while (m_rx_enabled) {
                rx_poll();
                sc_time cycle = sc_time(1.0 / clock, SC_SEC);
                sc_time quantum = tlm_global_quantum::instance().get();
                wait(max(cycle, quantum));
            }
        }
    }

    void ethoc::tx_poll() {
        if (IRQ.read())
            return;

        descriptor bd = current_txbd();
        if (!(bd.info & TXBD_RD))
            return;

        bd.info &= ~(TXBD_UR | TXBD_RL | TXBD_LC | TXBD_DF | TXBD_CS);
        u32 packet_length = bd.info >> TXBD_LEN_O;

        bool success = tx_packet(bd.addr, packet_length);
        if (success && (bd.info & TXBD_IRQ))
            interrupt(INT_SOURCE_TXB);
        if (!success)
            interrupt(INT_SOURCE_TXE);
        if (success)
            log_debug("packet transmitted, %d bytes", packet_length);

        bd.info &= ~TXBD_RD;
        update_txbd(bd);

        m_tx_idx++;
        if ((m_tx_idx >= num_txbd()) || (bd.info & TXBD_WR))
            m_tx_idx = 0;
    }

    void ethoc::rx_poll() {
        if (IRQ.read())
            return;

        descriptor bd = current_rxbd();
        if (!(bd.info & RXBD_E))
            return;

        bd.info &= ~(RXBD_M  | RXBD_OR | RXBD_IS | RXBD_DN);
        bd.info &= ~(RXBD_TL | RXBD_SF | RXBD_LC);

        u32 packet_length = 0;
        bool success = rx_packet(bd.addr, packet_length);
        if (success && (packet_length == 0))
            return; // nothing received
        if (success && (bd.info & RXBD_IRQ))
            interrupt(INT_SOURCE_RXB);
        if (!success)
            interrupt(INT_SOURCE_RXE);
        if (success)
            log_debug("packet received, %d bytes", packet_length);

        bd.info &= ~RXBD_E;
        bd.info &= RXBD_LEN_M;
        bd.info |= (packet_length + 4) << RXBD_LEN_O;
        update_rxbd(bd);

        m_rx_idx++;
        if ((m_rx_idx >= VCML_OPENCORES_ETHOC_NUMBD) || (bd.info & RXBD_WRAP))
            m_rx_idx = num_txbd();
    }

    bool ethoc::tx_packet(u32 addr, u32 length) {
        if (length > ETH_MAX_PACKET_LEN) {
            log_warn("packet size %d exceeds max, ignored", length);
            return false;
        }

        unsigned char buffer[ETH_MAX_PACKET_LEN] = { 0 };
        tlm_response_status rs = OUT.read(addr, buffer, length);
        if (failed(rs)) {
            log_warn("tx error  %s while reading from 0x%08x",
                     tlm_response_to_str(rs), addr);
            return false;
        }

        stringstream ss;
        for (unsigned int i = 0; i < length; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << (int)buffer[i] << " ";
        }

        log_debug("sending packet:");
        log_debug(ss.str().c_str());

        bewrite(buffer, length);
        return true;
    }

    bool ethoc::rx_packet(u32 addr, u32& size) {
        unsigned char buffer[ETH_MAX_PACKET_LEN] = { 0 };
        size_t length = beread(buffer, sizeof(buffer));
        if (length == 0)
            return true;

        stringstream ss;
        for (unsigned int i = 0; i < length; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << (int)buffer[i] << " ";
        }

        log_debug("received packet:");
        log_debug(ss.str().c_str());

        // promiscuous mode disabled, check destination HW address
        if (!(MODER & MODER_PRO)) {
            if ((memcmp(buffer, m_mac, ETH_ALEN) != 0) &&
                (memcmp(buffer, bcast, ETH_ALEN) != 0)) {
                log_debug("ignoring broadcast packet");
                return true; // packet not for us
            }
        }

        tlm_response_status rs = OUT.write(addr, buffer, length);
        if (failed(rs)) {
            log_warn("rx error %s while writing to 0x%08x",
                     tlm_response_to_str(rs), addr);
            return false;
        }

        size = (u32)length;
        return true;
    }

    void ethoc::interrupt(int source) {
        INT_SOURCE |= source;
        if (INT_MASK & source)
            IRQ = true;
    }

    u32 ethoc::write_MODER(u32 val) {
        if ((val & MODER_TXEN) && !m_tx_enabled) {
            log_debug("ethoc transmitter enabled");
            m_tx_enabled = true;
            m_tx_idx = 0;
            m_tx_event.notify(SC_ZERO_TIME);
        }

        if ((val & MODER_RXEN) && !m_rx_enabled) {
            log_debug("ethoc receiver enabled");
            m_rx_enabled = true;
            m_rx_idx = num_txbd();
            m_rx_event.notify(SC_ZERO_TIME);
        }

        if (!(val & MODER_TXEN) && m_tx_enabled) {
            log_debug("ethoc transmitter disabled");
            m_tx_enabled = false;
        }

        if (!(val & MODER_RXEN) && m_rx_enabled) {
            log_debug("ethoc receiver disabled");
            m_rx_enabled = false;
        }

        if (val & MODER_RST) {
            log_debug("ethoc reset");
            val &= ~MODER_RST;
            reset();
        }

        return val;
    }

    u32 ethoc::write_INT_SOURCE(u32 source) {
        INT_SOURCE &= ~source; // clear IRQs with 1 in source
        IRQ = (INT_SOURCE & INT_MASK) != 0;
        return INT_SOURCE;
    }

    u32 ethoc::write_INT_MASK(u32 data) {
        INT_MASK = data;
        IRQ = (INT_SOURCE & INT_MASK) != 0;
        return INT_MASK;
    }

    u32 ethoc::write_TX_BD_NUM(u32 data) {
        TX_BD_NUM = data & TX_BD_NUM_M;
        log_debug("ethoc num bd tx = %d rx = %d", num_txbd(), num_rxbd());
        return TX_BD_NUM;
    }

    u32 ethoc::write_MIICOMMAND(u32 data) {
        if (data != MIICOMMAND_RSTAT)
            return data;

        if (MIIADDRESS & MIIADDRESS_FIAD_M)
            return data;

        switch ((MIIADDRESS >> MIIADDRESS_RGAD_O) & MIIADDRESS_RGAD_M) {
        case MII_BMCR:        MIIRX_DATA = BMCR_FULLDPLX; break;
        case MII_BMSR:        MIIRX_DATA = BMSR_LSTATUS | BMSR_ANEGCOMPLETE |
                                           BMSR_10HALF  | BMSR_10FULL |
                                           BMSR_100HALF | BMSR_100FULL;
                              break;
        case MII_PHYSID1:     MIIRX_DATA = ETHOC_PHYID1; break;
        case MII_PHYSID2:     MIIRX_DATA = ETHOC_PHYID2; break;
        case MII_ADVERTISE:   MIIRX_DATA = ADVERTISE_FULL; break;
        case MII_LPA:         MIIRX_DATA = LPA_DUPLEX | LPA_100; break;
        case MII_EXPANSION:   MIIRX_DATA = 0; break;
        case MII_CTRL1000:    MIIRX_DATA = 0; break;
        case MII_STAT1000:    MIIRX_DATA = 0; break;
        case MII_ESTATUS:     MIIRX_DATA = 0; break;
        case MII_DCOUNTER:    MIIRX_DATA = 0; break;
        case MII_FCSCOUNTER:  MIIRX_DATA = 0; break;
        case MII_NWAYTEST:    MIIRX_DATA = 0; break;
        case MII_RERRCOUNTER: MIIRX_DATA = 0; break;
        case MII_SREVISION:   MIIRX_DATA = 0; break;
        case MII_RESV1:       MIIRX_DATA = 0; break;
        case MII_LBRERROR:    MIIRX_DATA = 0; break;
        case MII_PHYADDR:     MIIRX_DATA = 0; break; // TODO: get PHY address
        case MII_RESV2:       MIIRX_DATA = 0; break;
        case MII_TPISTATUS:   MIIRX_DATA = 0; break;
        case MII_NCONFIG:     MIIRX_DATA = 0; break;
        default: MIIRX_DATA = 0xffff; break;
        }

        return data;
    }

    u32 ethoc::write_MAC_ADDR0(u32 data) {
        m_mac[5] = (data >> MAC_ADDR0_B5) & 0xff;
        m_mac[4] = (data >> MAC_ADDR0_B4) & 0xff;
        m_mac[3] = (data >> MAC_ADDR0_B3) & 0xff;
        m_mac[2] = (data >> MAC_ADDR0_B2) & 0xff;
        return data;
    }

    u32 ethoc::write_MAC_ADDR1(u32 data) {
        m_mac[1] = (data >> MAC_ADDR1_B1) & 0xff;
        m_mac[0] = (data >> MAC_ADDR1_B0) & 0xff;
        return data;
    }

    u32 ethoc::read_MAC_ADDR0() {
        u32 mac = 0;
        mac |= m_mac[2] << MAC_ADDR0_B2;
        mac |= m_mac[3] << MAC_ADDR0_B3;
        mac |= m_mac[4] << MAC_ADDR0_B4;
        mac |= m_mac[5] << MAC_ADDR0_B5;
        return mac;
    }

    u32 ethoc::read_MAC_ADDR1() {
        u32 mac = 0;
        mac |= m_mac[0] << MAC_ADDR1_B0;
        mac |= m_mac[1] << MAC_ADDR1_B1;
        return mac;
    }

    tlm_response_status ethoc::read(const range& addr, void* data,
                                    const sideband& info) {
        if ((addr.start < RAM_START) || (addr.end > RAM_END))
            return TLM_ADDRESS_ERROR_RESPONSE;

        const u8* from = reinterpret_cast<const u8*>(m_desc);
        memcpy(data, from + addr.start - RAM_START, addr.length());

        return TLM_OK_RESPONSE;
    }

    tlm_response_status ethoc::write(const range& addr, const void* data,
                                     const sideband& info) {
        if ((addr.start < RAM_START) || (addr.end > RAM_END))
            return TLM_ADDRESS_ERROR_RESPONSE;

        u8* dest = reinterpret_cast<u8*>(m_desc);
        memcpy(dest + addr.start - RAM_START, data, addr.length());

        return TLM_OK_RESPONSE;
    }

    ethoc::ethoc(const sc_module_name &nm):
        peripheral(nm),
        m_mac(),
        m_tx_idx(0),
        m_rx_idx(VCML_OPENCORES_ETHOC_NUMBD / 2),
        m_desc(),
        m_tx_enabled(false),
        m_rx_enabled(false),
        m_tx_event("tx_ev"),
        m_rx_event("rx_ev"),
        MODER("MODER", 0x00, 0xA000),
        INT_SOURCE("INT_SOURCE", 0x04, 0),
        INT_MASK("INT_MASK", 0x08, 0),
        IPGT("IPGT", 0x0C, 0x12),
        IPGR1("IPGR1", 0x10, 0xC),
        IPGR2("IPGR2", 0x14, 0x12),
        PACKETLEN("PACKETLEN", 0x18, 0x400600),
        COLLCONF("COLLCONF", 0x1C, 0xF003F),
        TX_BD_NUM("TX_BD_NUM", 0x20, VCML_OPENCORES_ETHOC_NUMBD / 2),
        CTRLMODER("CTRLMODER", 0x24, 0),
        MIIMODER("MIIMODER", 0x28, 0x64),
        MIICOMMAND("MIICOMMAND", 0x2C, 0),
        MIIADDRESS("MIIADDRESS", 0x30, 0),
        MIITX_DATA("MIITX_DATA", 0x34, 0),
        MIIRX_DATA("MIIRX_DATA", 0x38, 0),
        MIISTATUS("MIISTATUS", 0x3C, 0),
        MAC_ADDR0("MAC_ADDR0", 0x40, 0),
        MAC_ADDR1("MAC_ADDR1", 0x44, 0),
        ETH_HASH0_ADR("ETH_HASH0_ADR", 0x48, 0),
        ETH_HASH1_ADR("ETH_HASH1_ADR", 0x4C, 0),
        ETH_TXCTRL("ETH_TXCTRL", 0x50, 0),
        clock("clock", VCML_OPENCORES_ETHOC_CLK), // input polling frequency
        mac("mac", "12:34:56:78:9A:BC"),
        IRQ("IRQ"),
        IN("IN"),
        OUT("OUT") {
        unsigned int addr[6] = { 0 };
        if (sscanf(mac.get().c_str(), "%x:%x:%x:%x:%x:%x",  addr + 0,
                   addr + 1, addr + 2, addr + 3, addr + 4, addr + 5) != 6) {
            VCML_ERROR("invalid MAC address specified: %s", mac.get().c_str());
        }

        for (int i = 0; i < 6; i++)
            m_mac[i] = addr[i];

        log_debug("using MAC addr: %02x:%02x:%02x:%02x:%02x:%02x",
                  addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

        SC_THREAD(tx_process);
        SC_THREAD(rx_process);

        MODER.allow_read_write();
        MODER.write = &ethoc::write_MODER;

        INT_SOURCE.allow_read_write();
        INT_SOURCE.write = &ethoc::write_INT_SOURCE;

        INT_MASK.allow_read_write();
        INT_MASK.write = &ethoc::write_INT_MASK;

        TX_BD_NUM.allow_read_write();
        TX_BD_NUM.write = &ethoc::write_TX_BD_NUM;

        IPGT.allow_read_write();
        IPGR1.allow_read_write();
        IPGR2.allow_read_write();
        PACKETLEN.allow_read_write();
        COLLCONF.allow_read_write();
        CTRLMODER.allow_read_write();
        MIIMODER.allow_read_write();

        MIICOMMAND.allow_read_write();
        MIICOMMAND.write = &ethoc::write_MIICOMMAND;

        MIIADDRESS.allow_read_write();
        MIITX_DATA.allow_read_write();
        MIIRX_DATA.allow_read_only();
        MIISTATUS.allow_read_only();

        MAC_ADDR0.allow_read_write();
        MAC_ADDR0.write = &ethoc::write_MAC_ADDR0;
        MAC_ADDR0.read = &ethoc::read_MAC_ADDR0;

        MAC_ADDR1.allow_read_write();
        MAC_ADDR1.write = &ethoc::write_MAC_ADDR1;
        MAC_ADDR1.read = &ethoc::read_MAC_ADDR1;

        ETH_HASH0_ADR.allow_read_write();
        ETH_HASH1_ADR.allow_read_write();
        ETH_TXCTRL.allow_read_write();
    }

    ethoc::~ethoc() {
        /* nothing to do */
    }

    void ethoc::reset() {
        MODER = 0xa000;
        INT_SOURCE = 0;
        INT_MASK = 0;
        IPGT = 0x12;
        IPGR1 = 0xc;
        IPGR2 = 0x12;
        PACKETLEN = 0x400600;
        COLLCONF = 0xf003f;
        TX_BD_NUM = VCML_OPENCORES_ETHOC_NUMBD / 2;
        CTRLMODER = 0;
        MIIMODER = 0x64;
        MIICOMMAND = 0;
        MIIADDRESS = 0;
        MIITX_DATA = 0;
        MIIRX_DATA = 0;
        MIISTATUS = 0;
        MAC_ADDR0 = 0;
        MAC_ADDR1 = 0;
        ETH_HASH0_ADR = 0;
        ETH_HASH1_ADR = 0;
        ETH_TXCTRL = 0;

        m_tx_idx = 0;
        m_rx_idx = num_txbd();

        IRQ = false;
    }

}}
