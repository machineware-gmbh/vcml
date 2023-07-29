/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/ethernet/lan9118.h"

namespace vcml {
namespace ethernet {

enum phy_cr_bits : u16 {
    PHY_CONTROL_COL_TEST = 1u << 7,
    PHY_CONTROL_DUPLEX = 1u << 8,
    PHY_CONTROL_RESTART = 1u << 9,
    PHY_CONTROL_AUTO_NEG = 1u << 12,
    PHY_CONTROL_SPEED_SEL = 1u << 13,
    PHY_CONTROL_PWR_DOWN = 1u << 11,
    PHY_CONTROL_LOOPBACK = 1u << 14,
    PHY_CONTROL_SW_RESET = 1u << 15,
    PHY_CONTROL_MASK = PHY_CONTROL_COL_TEST | PHY_CONTROL_DUPLEX |
                       PHY_CONTROL_AUTO_NEG | PHY_CONTROL_SPEED_SEL |
                       PHY_CONTROL_PWR_DOWN | PHY_CONTROL_LOOPBACK,
    PHY_CONTROL_RESET = PHY_CONTROL_AUTO_NEG | PHY_CONTROL_SPEED_SEL,
};

enum phy_status_bits : u16 {
    STATUS_EXT_CAP = 1u << 0,
    STATUS_JABBER = 1u << 1,
    STATUS_LINK = 1u << 2,
    STATUS_AUTO_NEG_CAP = 1u << 3,
    STATUS_REMOTE_FAULT = 1u << 4,
    STATUS_AUTO_NEG_DONE = 1u << 5,
    STATUS_10M_HALF_DUPLEX = 1u << 11,
    STATUS_10M_FULL_DUPLEX = 1u << 12,
    STATUS_100M_HALF_DUPLEX = 1u << 13,
    STATUS_100M_FULL_DUPLEX = 1u << 14,
    STATUS_100M_T4 = 1u << 15,
    STATUS_MASK = STATUS_EXT_CAP | STATUS_JABBER | STATUS_LINK |
                  STATUS_AUTO_NEG_CAP | STATUS_REMOTE_FAULT |
                  STATUS_AUTO_NEG_DONE | STATUS_10M_HALF_DUPLEX |
                  STATUS_10M_FULL_DUPLEX | STATUS_100M_HALF_DUPLEX |
                  STATUS_100M_FULL_DUPLEX | STATUS_100M_T4,
    STATUS_RESET = STATUS_EXT_CAP | STATUS_AUTO_NEG_CAP | STATUS_LINK |
                   STATUS_10M_HALF_DUPLEX | STATUS_10M_FULL_DUPLEX |
                   STATUS_100M_HALF_DUPLEX | STATUS_100M_FULL_DUPLEX,
};

enum phy_advertise_bits : u16 {
    ADVERTISE_IEEE_802_3 = 0b00001,
    ADVERTISE_10M = 1u << 5,
    ADVERTISE_10M_DUPLEX = 1u << 6,
    ADVERTISE_100M = 1u << 7,
    ADVERTISE_100M_DUPLEX = 1u << 8,
    ADVERTISE_100M_T4 = 1u << 9,
    ADVERTISE_NO_PAUSE = 0b00 << 10,
    ADVERTISE_PAUSE_SYM = 0b01 << 10,
    ADVERTISE_PAUSE_ASYM = 0b10 << 10,
    ADVERTISE_PAUSE_BOTH = 0b11 << 10,
    ADVERTISE_REMOTE_FAULT = 1u << 13,
    ADVERTISE_NEXT_PAGE = 1u << 15,

    ADVERTISE_RESET = ADVERTISE_IEEE_802_3 | ADVERTISE_10M_DUPLEX |
                      ADVERTISE_100M_DUPLEX,

    ADVERTISE_MASK = ADVERTISE_10M | ADVERTISE_10M_DUPLEX | ADVERTISE_100M |
                     ADVERTISE_100M_DUPLEX | ADVERTISE_PAUSE_BOTH |
                     ADVERTISE_REMOTE_FAULT | ADVERTISE_IEEE_802_3,

    ADVERTISE_LINK_PARTNER = ADVERTISE_IEEE_802_3 | ADVERTISE_10M |
                             ADVERTISE_10M_DUPLEX | ADVERTISE_100M |
                             ADVERTISE_100M_DUPLEX | ADVERTISE_100M_T4,
};

enum phy_irq_bits : u16 {
    PHY_IRQ_NEG_P0_RECV = 1u << 1,
    PHY_IRQ_DETECT_FAULT = 1u << 2,
    PHY_IRQ_NEG_LP_ACK = 1u << 3,
    PHY_IRQ_LINK_DOWN = 1u << 4,
    PHY_IRQ_REMOTE_FAULT = 1u << 5,
    PHY_IRQ_NEG_COMPLETE = 1u << 6,
    PHY_IRQ_ENERGY_ON = 1u << 7,
    PHY_IRQ_MASK = 0xff,
};

enum phy_special_mode_bits : u16 {
    SPECIAL_MODE_PHYAD = 0x1f,
    SPECIAL_MODE_10M_HALF_NO_AUTO = 0u << 5,
    SPECIAL_MODE_10M_FULL_NO_AUTO = 1u << 5,
    SPECIAL_MODE_100M_HALF_NO_AUTO = 2u << 5,
    SPECIAL_MODE_100M_FULL_NO_AUTO = 3u << 5,
    SPECIAL_MODE_100M_HALF_AUTO = 4u << 5,
    SPECIAL_MODE_100M_FULL_AUTO = 5u << 5,
    SPECIAL_MODE_ALL_CAPS = 7u << 5,
    SPECIAL_MODE_MASK = SPECIAL_MODE_PHYAD | SPECIAL_MODE_ALL_CAPS,
};

enum phy_special_control_bits : u16 {
    SPECIAL_CTRL_XPOL = 1u << 4,
    SPECIAL_CTRL_VCOOF_LP = 1u << 10,
};

enum phy_special_status_bits : u16 {
    SPECIAL_STATUS_10M_HALF = 1u << 2,
    SPECIAL_STATUS_10M_FULL = 5u << 2,
    SPECIAL_STATUS_100M_HALF = 2u << 2,
    SPECIAL_STATUS_100M_FULL = 6u << 2,
    SPECIAL_STATUS_AUTO_DONE = 1u << 12,
};

enum mac_cr_bits : u32 {
    CR_RXEN = 1u << 2,
    CR_TXEN = 1u << 3,
    CR_DFCHK = 1u << 5,
    CR_BOLMT = 3u << 6,
    CR_PADSTR = 1u << 8,
    CR_DISRTY = 1u << 10,
    CR_BCAST = 1u << 11,
    CR_LCOLL = 1u << 12,
    CR_HPFILT = 1u << 13,
    CR_HO = 1u << 15,
    CR_PASSBAD = 1u << 16,
    CR_INVFILT = 1u << 17,
    CR_PRMS = 1u << 18,
    CR_MCPAS = 1u << 19,
    CR_FDPX = 1u << 20,
    CR_LOOPBK = 1u << 21,
    CR_RCVOWN = 1u << 23,
    CR_RXALL = 1u << 31,

    CR_RESET = CR_PRMS,
    CR_MASK = CR_RXEN | CR_TXEN | CR_DFCHK | CR_BOLMT | CR_PADSTR | CR_DISRTY |
              CR_BCAST | CR_HPFILT | CR_HO | CR_PASSBAD | CR_INVFILT |
              CR_PRMS | CR_MCPAS | CR_FDPX | CR_LOOPBK | CR_RCVOWN |
              CR_RCVOWN | CR_RXALL,
};

enum lan_mac_mii_bits : u32 {
    MII_ACC_BUSY = 1u << 0,
    MII_ACC_WRITE = 1u << 1,
    MII_ACC_MASK = 0x0000ffc2,
    MII_DATA_MASK = bitmask(16),
};

enum lan_irq_cfg_bits : u32 {
    IRQ_CFG_TYPE = 1u << 0,
    IRQ_CFG_POL = 1u << 4,
    IRQ_CFG_EN = 1u << 8,
    IRQ_CFG_INT = 1u << 12,
    IRQ_CFG_DEAS_STS = 1u << 13,
    IRQ_CFG_DEAS_CLR = 1u << 14,
    IRQ_CFG_DEAS = bitmask(8, 24),

    IRQ_CFG_MASK = IRQ_CFG_TYPE | IRQ_CFG_POL | IRQ_CFG_EN | IRQ_CFG_DEAS,
};

enum lan_irq_bits : u32 {
    IRQ_GPIO0 = 1u << 0,
    IRQ_GPIO1 = 1u << 1,
    IRQ_GPIO2 = 1u << 2,
    IRQ_RSFL = 1u << 3,    // RX FIFO level
    IRQ_RSFF = 1u << 4,    // RX FIFO full
    IRQ_RXDF = 1u << 6,    // RX dropped frame
    IRQ_TSFL = 1u << 7,    // TX status FIFO level
    IRQ_TSFF = 1u << 8,    // TX status FIFO full
    IRQ_TDFA = 1u << 9,    // TX data FIFO available
    IRQ_TDFO = 1u << 10,   // TX FIFO overrun
    IRQ_TXE = 1u << 13,    // transmitter error
    IRQ_RXE = 1u << 14,    // receiver error
    IRQ_RWT = 1u << 15,    // receiver watch-dog timeout
    IRQ_TXSO = 1u << 16,   // TX status FIFO overflow
    IRQ_PME = 1u << 17,    // power management interrupt
    IRQ_PHY = 1u << 18,    // PHY interrupt
    IRQ_GPT = 1u << 19,    // general timer interrupt
    IRQ_RXD = 1u << 20,    // RX DMA interrupt
    IRQ_TXIOC = 1u << 21,  // TX FIFO IOC event
    IRQ_RXDFH = 1u << 23,  // RX dropped frame half-way
    IRQ_RXSTOP = 1u << 24, // RX stopped
    IRQ_TXSTOP = 1u << 25, // TX stopped
    IRQ_SW = 1u << 31,     // software interrupt
    IRQ_MASK = IRQ_GPIO0 | IRQ_GPIO1 | IRQ_GPIO2 | IRQ_RSFL | IRQ_RSFF |
               IRQ_RXDF | IRQ_TSFL | IRQ_TSFF | IRQ_TDFA | IRQ_TDFO | IRQ_TXE |
               IRQ_RXE | IRQ_RWT | IRQ_TXSO | IRQ_PME | IRQ_PHY | IRQ_GPT |
               IRQ_RXD | IRQ_TXIOC | IRQ_RXDFH | IRQ_RXSTOP | IRQ_TXSTOP |
               IRQ_SW,
};

typedef field<12, 6, u32> RX_CFG_DMA_COUNT;

enum lan_rx_cfg : u32 {
    RX_CFG_RXDOFF = bitmask(5, 8),
    RX_CFG_RX_DUMP = 1u << 15,
    RX_CFG_RX_DMA_MASK = RX_CFG_DMA_COUNT::MASK,
    RX_CFG_END_ALIGN_4 = 0u << 30,
    RX_CFG_END_ALIGN_16 = 1u << 30,
    RX_CFG_END_ALIGN_32 = 2u << 30,
    RX_CFG_END_ALIGN_MASK = bitmask(2, 30),
    RX_CFG_MASK = RX_CFG_END_ALIGN_MASK | RX_CFG_RX_DMA_MASK | RX_CFG_RXDOFF,
};

enum lan_tx_cfg : u32 {
    TX_CFG_STOP_TX = 1u << 0,
    TX_CFG_TX_ON = 1u << 1,
    TX_CFG_TXSAO = 1u << 2,
    TX_CFG_TXD_DUMP = 1u << 14,
    TX_CFG_TXS_DUMP = 1u << 15,
    TX_CFG_MASK = TX_CFG_TX_ON | TX_CFG_TXSAO,
};

enum lan_hwcfg_bits : u32 {
    HW_CFG_SRST = 1u << 0,
    HW_CFG_SRST_TO = 1u << 1,
    HW_CFG_32BIT = 1u << 2,
    HW_CFG_TX_FF_SZ = bitmask(4, 16),
    HW_CFG_MBO = 1u << 20,

    HW_CFG_MASK = HW_CFG_TX_FF_SZ | HW_CFG_MBO,
    HW_CFG_RESET = 5 << 16 | HW_CFG_32BIT,
};

enum lan_rx_dp_ctrl_bits : u32 {
    RX_DP_CTRL_FF = 1u << 31,
};

enum lan_pmt_ctrl_bits : u32 {
    PMT_CTRL_READY = 1u << 0,
    PMT_CTRL_PME_EN = 1u << 1,
    PMT_CTRL_PME_POL = 1u << 2,
    PMT_CTRL_PME_IND = 1u << 3,
    PMT_CTRL_WUPS_NONE = 0u << 4,
    PMT_CTRL_WUPS_ENERGY = 1u << 4,
    PMT_CTRL_WUPS_WAKEUP = 2u << 4,
    PMT_CTRL_WUPS_MULTIPLE = PMT_CTRL_WUPS_ENERGY | PMT_CTRL_WUPS_WAKEUP,
    PMT_CTRL_PME_TYPE = 1u << 6,
    PMT_CTRL_ED_EN = 1u << 8,
    PMT_CTRL_WOL_EN = 1u << 9,
    PMT_CTRL_PHY_RST = 1u << 10,
    PMT_CTRL_PM_D0 = 0u << 12,
    PMT_CTRL_PM_D1 = 1u << 12,
    PMT_CTRL_PM_D2 = 2u << 12,
    PMT_CTRL_PM_MASK = 3u << 12,

    PMT_CTRL_RESET = PMT_CTRL_READY,
    PMT_CTRL_MASK = PMT_CTRL_PME_EN | PMT_CTRL_PME_POL | PMT_CTRL_PME_IND |
                    PMT_CTRL_WUPS_MULTIPLE | PMT_CTRL_PME_TYPE |
                    PMT_CTRL_ED_EN | PMT_CTRL_WOL_EN | PMT_CTRL_PM_MASK,
};

enum lan_gpt_cfg_bits : u32 {
    GPT_CFG_LOAD_MASK = bitmask(16),
    GPT_CFG_TIMER_EN = 1u << 29,
    GPT_CFG_MASK = GPT_CFG_LOAD_MASK | GPT_CFG_TIMER_EN,
    GPT_CFG_RESET = GPT_CFG_LOAD_MASK,
};

enum lan_mac_cmd_bits : u32 {
    MAC_CMD_ADDR = 0xff,
    MAC_CMD_R_NW = 1u << 30,
    MAC_CMD_BUSY = 1u << 31,
    MAC_CMD_MASK = MAC_CMD_ADDR | MAC_CMD_R_NW,
};

enum lan_e2p_cmd_bits : u32 {
    E2P_CMD_BUSY = 1u << 31,
    E2P_CMD_READ = 0u << 28,
    E2P_CMD_EWDS = 1u << 28,
    E2P_CMD_EWEN = 2u << 28,
    E2P_CMD_WRITE = 3u << 28,
    E2P_CMD_WRAL = 4u << 28,
    E2P_CMD_ERASE = 5u << 28,
    E2P_CMD_ERAL = 6u << 28,
    E2P_CMD_RELOAD = 7u << 28,
    E2P_CMD_TIMEOUT = 1u << 9,
    E2P_CMD_MAC_LOADED = 1u << 8,
    E2P_CMD_ADDR_MASK = 0xff,
    E2P_CMD_MASK = E2P_CMD_ADDR_MASK | E2P_CMD_MAC_LOADED | E2P_CMD_TIMEOUT |
                   E2P_CMD_RELOAD | E2P_CMD_BUSY,
};

enum pkt_cmda_bits : u32 {
    CMDA_BUFSZ_MASK = bitmask(11),
    CMDA_LAST = 1u << 12,
    CMDA_FIRST = 1u << 13,
    CMDA_OFFSET_MASK = bitmask(5, 16),
    CMDA_END_ALIGN_4 = 0u << 24,
    CMDA_END_ALIGN_16 = 1u << 24,
    CMDA_END_ALIGN_32 = 2u << 24,
    CMDA_END_ALIGN_MASK = 3u << 24,
    CMDA_TX_IOC = 1u << 31,
    CMDA_MASK = CMDA_BUFSZ_MASK | CMDA_LAST | CMDA_FIRST | CMDA_OFFSET_MASK |
                CMDA_END_ALIGN_MASK | CMDA_TX_IOC,
};

enum pkt_cmdb_bits : u32 {
    CMDB_PKT_LEN = bitmask(11),
    CMDB_PAD_DIS = 1u << 12,
    CMDB_CRC_DIS = 1u << 13,
    CMDB_PKT_TAG = bitmask(16, 16),
    CMDB_MASK = CMDB_PKT_LEN | CMDB_PAD_DIS | CMDB_CRC_DIS | CMDB_PKT_TAG,
};

enum pkt_txsts_bits : u32 {
    PKT_TXSTS_DEFERRED = 1u << 0,
    PKT_TXSTS_EX_DEF = 1u << 2,
    PKT_TXSTS_COL_CNT = bitmask(4, 3),
    PKT_TXSTS_EX_COL = 1u << 8,
    PKT_TXSTS_LATE_COL = 1u << 9,
    PKT_TXSTS_NO_CARRY = 1u << 10,
    PKT_TXSTS_LOST_CARRY = 1u << 11,
    PKT_TXSTS_ERROR = 1u << 15,
    PKT_TXSTS_TAG_MASK = bitmask(16, 16),
};

enum pkt_rxsts_bits : u32 {
    PKT_RXSTS_ERR_CRC = 1u << 1,
    PKT_RXSTS_DRIBBLE = 1u << 2,
    PKT_RXSTS_ERR_MII = 1u << 3,
    PKT_RXSTS_WATCHDOG = 1u << 4,
    PKT_RXSTS_FRAME_TYPE = 1u << 5,
    PKT_RXSTS_COLLISION = 1u << 6,
    PKT_RXSTS_TOO_LONG = 1u << 7,
    PKT_RXSTS_MULTICAST = 1u << 10,
    PKT_RXSTS_RUNT = 1u << 11,
    PKT_RXSTS_ERR_LENGTH = 1u << 12,
    PKT_RXSTS_BROADCAST = 1u << 13,
    PKT_RXSTS_ERROR = 1u << 15,
    PKT_RXSTS_LEN_MASK = bitmask(14, 16),
    PKT_RXSTS_FILTERED = 1u << 30,
};

static constexpr u32 chiprev(u16 id, u16 rev) {
    return (u32)id << 16 | rev;
}

void lan9118_phy::negotiate_link() {
    status |= STATUS_AUTO_NEG_DONE;
    int_source |= PHY_IRQ_NEG_COMPLETE;
}

void lan9118_phy::write_control(u16 val) {
    if (val & PHY_CONTROL_SW_RESET) {
        reset();
        return;
    }

    if (val & PHY_CONTROL_PWR_DOWN) {
        int_source &= ~PHY_IRQ_ENERGY_ON;
        int_source &= ~PHY_IRQ_NEG_COMPLETE;
        status &= ~STATUS_AUTO_NEG_DONE;
        control = val & PHY_CONTROL_MASK;
        return;
    }

    if (!(val & PHY_CONTROL_AUTO_NEG)) {
        status &= ~STATUS_AUTO_NEG_DONE;
        int_source &= ~PHY_IRQ_NEG_COMPLETE;
    }

    if (control & PHY_CONTROL_PWR_DOWN) {
        int_source |= PHY_IRQ_ENERGY_ON;
        if (val & PHY_CONTROL_AUTO_NEG)
            negotiate_link();
    }

    if (val & (PHY_CONTROL_AUTO_NEG | PHY_CONTROL_RESTART))
        negotiate_link();

    control = val & PHY_CONTROL_MASK;
}

void lan9118_phy::write_advertise(u16 val) {
    advertise = val & ADVERTISE_MASK;
}

u16 lan9118_phy::read_int_source() {
    u16 val = int_source;
    int_source = 0;
    m_parent.update_irq();
    return val;
}

void lan9118_phy::write_int_mask(u16 val) {
    int_mask = val & IRQ_MASK;
    m_parent.update_irq();
}

lan9118_phy::lan9118_phy(const sc_module_name& nm, lan9118& parent):
    peripheral(nm),
    m_parent(parent),
    control("control", 0 * 2, PHY_CONTROL_RESET),
    status("status", 1 * 2, STATUS_RESET),
    ident1("ident1", 2 * 2, 0x0007),
    ident2("ident2", 3 * 2, 0xc0d1),
    advertise("advertise", 4 * 2, 0x0000),
    link_partner("link_partner", 5 * 2, ADVERTISE_LINK_PARTNER),
    negotiate_ex("negotiate_ex", 6 * 2, 0x0000),
    mode_ctrl("mode_ctrl", 17 * 2, 0x0000),
    special_modes("special_modes", 18 * 2, 0x0000),
    special_ctrl("special_ctrl", 27 * 2, 0x0000),
    int_source("int_source", 29 * 2, 0x0000),
    int_mask("int_mask", 30 * 2, 0x0000),
    special_status("special_status", 31 * 2, 0x0000) {
    control.sync_always();
    control.allow_read_write();
    control.on_write(&lan9118_phy::write_control);

    status.sync_always();
    status.allow_read_only();

    ident1.sync_never();
    ident1.allow_read_only();

    ident2.sync_never();
    ident2.allow_read_only();

    advertise.sync_always();
    advertise.allow_read_write();
    advertise.on_write(&lan9118_phy::write_advertise);

    link_partner.sync_never();
    link_partner.allow_read_only();

    negotiate_ex.sync_never();
    negotiate_ex.allow_read_only();

    int_source.sync_always();
    int_source.no_writeback();
    int_source.allow_read_only();
    int_source.on_read(&lan9118_phy::read_int_source);

    int_mask.sync_always();
    int_mask.allow_read_write();
    int_mask.on_write(&lan9118_phy::write_int_mask);
}

lan9118_phy::~lan9118_phy() {
    // nothing to do
}

void lan9118_phy::reset() {
    peripheral::reset();

    int_source |= PHY_IRQ_ENERGY_ON;
    negotiate_link();
}

sc_time lan9118_phy::rxtx_delay(size_t bytes) const {
    if (control & PHY_CONTROL_SPEED_SEL)
        return (8 + bytes + 12) * 8 * sc_time(10, SC_NS); // 100M
    else
        return (8 + bytes + 12) * 8 * sc_time(100, SC_NS); // 10M
}

bool lan9118_phy::link_status() const {
    return status & STATUS_LINK;
}

void lan9118_phy::set_link_status(bool link_up) {
    if (link_up) {
        status |= STATUS_LINK;
        if (control & PHY_CONTROL_AUTO_NEG)
            negotiate_link();
    } else {
        status &= ~(STATUS_LINK | STATUS_AUTO_NEG_DONE);
        int_source |= PHY_IRQ_LINK_DOWN;
    }
}

void lan9118_mac::write_cr(u32 val) {
    if (!(val & CR_RXEN) && (cr & CR_RXEN))
        m_parent.irq_sts |= IRQ_RXSTOP;
    if (!(val & CR_TXEN) && (cr & CR_TXEN))
        m_parent.irq_sts |= IRQ_TXSTOP;

    cr = val & CR_MASK;
    m_parent.update_irq();
}

void lan9118_mac::write_mii_acc(u32 val) {
    mii_acc = val & MII_ACC_MASK;
    u32 phy_addr = extract(val, 11, 5);
    if (phy_addr != 1) {
        log_warn("MII_ACC ignoring invalid PHY_ADDR %u", phy_addr);
        return;
    }

    bool wnr = val & MII_ACC_WRITE;
    u16 data = wnr ? mii_data : 0;
    u64 size = sizeof(data);
    u64 addr = extract(val, 6, 5);
    auto cmd = wnr ? TLM_WRITE_COMMAND : TLM_READ_COMMAND;

    tlm_generic_payload tx;
    tlm_sbi sbi = in_debug_transaction() ? SBI_DEBUG : SBI_NONE;
    tx_setup(tx, cmd, addr * size, &data, size);
    u32 res = m_parent.phy.receive(tx, sbi, VCML_AS_DEFAULT);

    if (failed(tx) || res != size)
        log_warn("PHY CSR access failed %s", to_string(tx).c_str());
    else if (!wnr)
        mii_data = data;
}

void lan9118_mac::write_mii_data(u32 val) {
    mii_data = val & MII_DATA_MASK;
}

void lan9118_mac::set_address(const mac_addr& addr) {
    m_addr = addr;

    addrh = (u32)m_addr[4] | (u32)m_addr[5] << 8;
    addrl = (u32)m_addr[0] | (u32)m_addr[1] << 8 | (u32)m_addr[2] << 16 |
            (u32)m_addr[3] << 24;

    log_debug("using address %s", to_string(addr).c_str());
}

lan9118_mac::lan9118_mac(const sc_module_name& nm, lan9118& parent):
    peripheral(nm),
    m_parent(parent),
    cr("cr", 1 * 4, CR_RESET),
    addrh("addrh", 2 * 4, 0x0000ffff),
    addrl("addrl", 3 * 4, 0xffffffff),
    hashh("hashh", 4 * 4, 0x00000000),
    hashl("hashl", 5 * 4, 0x00000000),
    mii_acc("mii_acc", 6 * 4, 0x00000000),
    mii_data("mii_data", 7 * 4, 0x00000000),
    flow("flow", 8 * 4, 0x00000000),
    vlan1("vlan1", 9 * 4, 0x00000000),
    vlan2("vlan2", 10 * 4, 0x00000000),
    wuff("wuff", 11 * 4, 0x00000000),
    wucsr("wucsr", 12 * 4, 0x00000000) {
    cr.sync_always();
    cr.allow_read_write();
    cr.on_write(&lan9118_mac::write_cr);

    addrh.sync_always();
    addrh.allow_read_write();

    addrl.sync_always();
    addrl.allow_read_write();

    hashh.sync_always();
    hashh.allow_read_write();

    hashl.sync_always();
    hashl.allow_read_write();

    mii_acc.sync_always();
    mii_acc.allow_read_write();
    mii_acc.on_write(&lan9118_mac::write_mii_acc);

    mii_data.sync_always();
    mii_data.allow_read_write();
    mii_data.on_write(&lan9118_mac::write_mii_data);
}

lan9118_mac::~lan9118_mac() {
    // nothing to do
}

void lan9118_mac::reset() {
    peripheral::reset();
    set_address(m_addr);
}

bool lan9118_mac::filter(const mac_addr& dest) const {
    if (cr & CR_PRMS) // promiscuous
        return true;

    // broadcast packets disabled?
    if (dest.is_broadcast())
        return (cr & CR_BCAST) == 0;

    // multicast packets passed?
    if (dest.is_multicast() && (cr & CR_MCPAS))
        return true;

    // hash filtering requested?
    if (dest.is_multicast() ? cr & CR_HPFILT : cr & CR_HO) {
        u32 hash = dest.hash_crc32() >> 26;
        u32 mask = 1u << (hash & 0x1f);
        return (hash > 0x1f ? hashh : hashl) & mask;
    }

    if (cr & CR_INVFILT)
        return dest != m_addr;
    else
        return dest == m_addr;
}

void lan9118::reset_fifo_size(size_t txff_size) {
    size_t sram_size = 16 * KiB;
    size_t rxff_size = sram_size - txff_size;

    m_tx_status_fifo_size = 512;
    m_tx_data_fifo_size = txff_size - m_tx_status_fifo_size;

    m_rx_status_fifo_size = rxff_size / 16;
    m_rx_data_fifo_size = rxff_size - m_rx_status_fifo_size;

    log_debug("TX STATUS FIFO: %zu bytes", m_tx_status_fifo_size);
    log_debug("TX DATA FIFO:   %zu bytes", m_tx_data_fifo_size);
    log_debug("RX STATUS FIFO: %zu bytes", m_rx_status_fifo_size);
    log_debug("RX DATA FIFO:   %zu bytes", m_rx_data_fifo_size);
}

void lan9118::eeprom_reload() {
    if (m_eeprom[0] != 0xa5) {
        e2p_cmd &= ~E2P_CMD_MAC_LOADED;
        return;
    }

    mac_addr addr;
    for (int i = 0; i < 6; i++)
        addr[i] = m_eeprom[i + 1];

    mac.set_address(addr);
    e2p_cmd |= E2P_CMD_MAC_LOADED;
}

void lan9118::deas_update() {
    update_irq();
}

void lan9118::gpt_restart() {
    m_gpt_ev.cancel();

    if (!(gpt_cfg & GPT_CFG_TIMER_EN))
        return;

    u32 load = gpt_cfg & GPT_CFG_LOAD_MASK;
    m_gpt_ev.notify(m_gpt_cycle * load);
    m_gpt_start = sc_time_stamp();
}

void lan9118::gpt_update() {
    irq_sts |= IRQ_GPT;
    gpt_restart();
    update_irq();
}

static size_t calc_rx_padding(u32 rxcfg, u32 len, u32 off) {
    size_t ndw = (len + off) / 4;
    switch (rxcfg & RX_CFG_END_ALIGN_MASK) {
    case RX_CFG_END_ALIGN_32:
        return -ndw & 7;
    case RX_CFG_END_ALIGN_16:
        return -ndw & 3;
    case RX_CFG_END_ALIGN_4:
    default:
        return 0;
    }
}

bool lan9118::rx_enqueue(const vector<u8>& pkt) {
    if (!(mac.cr & CR_RXEN))
        return false;

    if (pkt.size() > 2048) {
        irq_sts |= IRQ_RWT;
        return false;
    }

    if (pkt.size() < 14) {
        log_warn("received short Ethernet frame: %zu bytes", pkt.size());
        return false;
    }

    mac_addr dest(pkt);
    bool filter = mac.filter(pkt);
    if (!filter && (mac.cr & CR_RXALL) == 0)
        return true;

    // not sure if this is the correct crc32 to use
    u32 crc = crc32(pkt.data(), pkt.size());

    size_t offset = extract(rx_cfg.get(), 8, 5);
    size_t padding = calc_rx_padding(rx_cfg, pkt.size(), offset);
    size_t length = pkt.size() + sizeof(crc) + offset + padding * 4;
    if (rx_data_free() < length)
        return false;

    while (offset > 4) {
        m_rx_data_fifo.push_back(0);
        offset -= 4;
    }

    u32 val = 0;
    for (u8 src : pkt) {
        val = (val >> 8) | (src << 24);
        if (++offset == 4) {
            m_rx_data_fifo.push_back(val);
            val = offset = 0;
        }
    }

    if (offset > 0) {
        val >>= (4 - offset) * 8;
        val |= crc << (offset * 8);
        m_rx_data_fifo.push_back(val);
        val = crc >> (4 - offset) * 8;
        m_rx_data_fifo.push_back(val);
    } else {
        m_rx_data_fifo.push_back(crc);
    }

    while (padding--)
        m_rx_data_fifo.push_back(0);

    u32 status = (length << 16) & PKT_RXSTS_LEN_MASK;
    if (!filter)
        status |= PKT_RXSTS_FILTERED;
    if (dest.is_broadcast())
        status |= PKT_RXSTS_BROADCAST;
    else if (dest.is_multicast())
        status |= PKT_RXSTS_MULTICAST;
    m_rx_status_fifo.push_back(status);

    return true;
}

void lan9118::rx_thread() {
    while (true) {
        while (!(mac.cr & CR_RXEN))
            wait(m_rxev);

        sc_time delay = tlm_global_quantum::instance().get();

        eth_frame frame;
        if (eth_rx_pop(frame)) {
            delay = phy.rxtx_delay(frame.size());
            if (!rx_enqueue(frame)) {
                irq_sts |= IRQ_RXDF;
                rx_drop++;
            }
        }

        update_irq();
        wait(delay);
    }
}

void lan9118::tx_thread() {
    while (true) {
        while (!(mac.cr & CR_TXEN) || !(tx_cfg & TX_CFG_TX_ON) ||
               m_tx_packets.empty() ||
               (!(tx_cfg & TX_CFG_TXSAO) && tx_status_full())) {
            wait(m_txev);
        }

        packet pkt = m_tx_packets.front();
        m_tx_packets.pop_front();

        if (pkt.length < 64 && !(pkt.cmdb & CMDB_PAD_DIS)) {
            for (size_t i = pkt.length; i < 64; i++)
                pkt.data.push_back(0);
        }

        if (phy.control & PHY_CONTROL_LOOPBACK)
            rx_enqueue(pkt.data);
        else
            eth_tx.send(pkt.data);

        u32 status = pkt.cmdb & CMDB_PKT_TAG;
        // error injection?
        // status |= PKT_STS_EX_COL;
        // status |= PKT_STS_ERROR;

        if (!tx_status_full())
            m_tx_status_fifo.push_back(status);

        if (pkt.cmda & CMDA_TX_IOC)
            irq_sts |= IRQ_TXIOC;

        update_irq();
    }
}

u32 lan9118::read_rx_data_fifo() {
    if (m_rx_data_fifo.empty()) {
        irq_sts |= IRQ_RXE;
        return 0;
    }

    u32 val = m_rx_data_fifo.front();
    m_rx_data_fifo.pop_front();

    u32 dma = rx_cfg.get_field<RX_CFG_DMA_COUNT>();
    if (dma > 0) {
        rx_cfg.set_field<RX_CFG_DMA_COUNT>(--dma);
        if (dma == 0) {
            irq_sts |= IRQ_RXD;
            update_irq();
        }
    }

    return val;
}

static size_t calc_tx_padding(u32 cmda, size_t off, size_t length) {
    size_t ndw = (off + length) / 4;
    switch (cmda & CMDA_END_ALIGN_MASK) {
    case CMDA_END_ALIGN_32:
        return -ndw & 7;
    case CMDA_END_ALIGN_16:
        return -ndw & 3;
    case CMDA_END_ALIGN_4:
    default:
        return 0;
    }
}

void lan9118::write_tx_data_fifo(u32 val) {
    if (tx_data_full()) {
        log_warn("TX error: FIFO overflow");
        irq_sts |= IRQ_TDFO;
        update_irq();
        return;
    }

    switch (m_tx_pkt.state) {
    case packet::CMDA: {
        m_tx_pkt.used_dw++;
        m_tx_pkt.cmda = val & CMDA_MASK;
        m_tx_pkt.offset = extract(val, 16, 5);
        m_tx_pkt.remain = extract(val, 0, 11);
        m_tx_pkt.padding = calc_tx_padding(m_tx_pkt.cmda, m_tx_pkt.offset,
                                           m_tx_pkt.remain);
        m_tx_pkt.state = packet::CMDB;
        break;
    }

    case packet::CMDB: {
        m_tx_pkt.used_dw++;
        if (m_tx_pkt.cmda & CMDA_FIRST) {
            m_tx_pkt.cmdb = val & CMDB_MASK;
            m_tx_pkt.length = extract(val, 0, 11);
        }

        if (val != m_tx_pkt.cmdb) {
            log_warn("TX error: CMDB mismatch");
            m_tx_pkt.reset();
            irq_sts |= IRQ_TXE;
            update_irq();
            break;
        }

        m_tx_pkt.state = packet::DATA;
        break;
    }

    case packet::DATA:
        if (m_tx_pkt.offset >= 4) {
            m_tx_pkt.offset -= 4;
        } else if (m_tx_pkt.remain > 0) {
            m_tx_pkt.used_dw++;
            for (int i = 0; i < 4; i++, val >>= 8) {
                if (m_tx_pkt.offset > 0)
                    m_tx_pkt.offset--;
                else if (m_tx_pkt.remain > 0) {
                    m_tx_pkt.data.push_back(val & 0xff);
                    m_tx_pkt.remain--;
                }
            }
        } else if (m_tx_pkt.padding > 0) {
            m_tx_pkt.padding--;
        }

        if (m_tx_pkt.offset || m_tx_pkt.remain || m_tx_pkt.padding)
            break; // more to read in current buffer

        if ((m_tx_pkt.cmda & CMDA_LAST) == 0) {
            m_tx_pkt.state = packet::CMDA;
            break; // need to read more buffers
        }

        if (m_tx_pkt.data.size() != m_tx_pkt.length) {
            log_warn("TX error: invalid packet length");
            m_tx_pkt.reset();
            irq_sts |= IRQ_TXE;
            update_irq();
        }

        m_tx_packets.push_back(m_tx_pkt);
        m_tx_pkt.reset();
        m_txev.notify();
        break;
    }
}

u32 lan9118::read_rx_status_fifo() {
    if (m_rx_status_fifo.empty()) {
        irq_sts |= IRQ_RXE;
        return 0;
    }

    u32 val = m_rx_status_fifo.front();
    m_rx_status_fifo.pop_front();
    return val;
}

u32 lan9118::read_rx_status_peek() {
    if (m_rx_status_fifo.empty())
        return 0;
    return m_rx_status_fifo.front();
}

u32 lan9118::read_tx_status_fifo() {
    if (m_tx_status_fifo.empty())
        return 0;

    if (tx_status_full())
        m_txev.notify();

    u32 val = m_tx_status_fifo.front();
    m_tx_status_fifo.pop_front();
    return val;
}

u32 lan9118::read_tx_status_peek() {
    if (m_tx_status_fifo.empty())
        return 0;
    return m_tx_status_fifo.front();
}

void lan9118::write_irq_cfg(u32 val) {
    irq_cfg = val & IRQ_CFG_MASK;
    m_deas_delta = (val >> 24) * m_deas_cycle;

    if (val & IRQ_CFG_DEAS_CLR)
        m_deas_limit = sc_time_stamp() + m_deas_delta;

    update_irq();
}

void lan9118::write_irq_sts(u32 val) {
    irq_sts &= ~(val & IRQ_MASK);
    update_irq();
}

void lan9118::write_irq_en(u32 val) {
    irq_en = val & IRQ_MASK;
    irq_sts |= val & IRQ_SW;
    update_irq();
}

void lan9118::write_fifo_int(u32 val) {
    fifo_int = val & 0xffff00ff;
    log_debug("new IRQ_RSFL level: %zu bytes", rx_status_level());
    log_debug("new IRQ_TSFL level: %zu bytes", tx_status_level());
    log_debug("new IRQ_TFDA level: %zu bytes", tx_data_level());
    update_irq();
}

void lan9118::write_rx_cfg(u32 val) {
    if (val & RX_CFG_RX_DUMP) {
        m_rx_data_fifo.clear();
        m_rx_status_fifo.clear();
    }

    rx_cfg = val & RX_CFG_MASK;

    static const size_t align[] = { 4, 16, 32, 0xff };
    log_debug("RX padding: %zu bytes", align[val >> 30]);
    log_debug("RX DMA count: %u bytes", extract(val, 16, 12) * 4);
    log_debug("RX offset: %u bytes", extract(val, 5, 8));
}

void lan9118::write_tx_cfg(u32 val) {
    if (val & TX_CFG_TXS_DUMP)
        m_tx_status_fifo.clear();

    if (val & TX_CFG_TXD_DUMP) {
        m_tx_packets.clear();
        m_tx_pkt.reset();
    }

    if (val & TX_CFG_STOP_TX) {
        irq_sts |= IRQ_TXSTOP;
        update_irq();
    }

    tx_cfg = val & TX_CFG_MASK;
    m_txev.notify();
}

void lan9118::write_hw_cfg(u32 val) {
    if (val & HW_CFG_SRST) {
        reset();
        return;
    }

    size_t txff_size = (val >> 16) & 0xf;
    if (txff_size < 2 || txff_size > 14) {
        log_warn("invalid TXFIFO size: %zu KiB", txff_size);
        return;
    }

    reset_fifo_size(txff_size * KiB);

    hw_cfg = val & HW_CFG_MASK;
    hw_cfg |= HW_CFG_32BIT;
}

void lan9118::write_rx_dp_ctrl(u32 val) {
    if (val & RX_DP_CTRL_FF) {
        size_t length = (read_rx_status_fifo() >> 16) & 0x3ff;
        size_t offset = extract(rx_cfg.get(), 8, 5);
        size_t padding = calc_rx_padding(rx_cfg, length, offset);
        size_t ndw = (length + offset) / 4 + padding;
        log_debug("triggering fast-forward for %zu dwords", ndw);

        while (ndw--)
            read_rx_data_fifo();
    }

    update_irq();
}

u32 lan9118::read_rx_fifo_inf() {
    return ((m_rx_status_fifo.size() & 0xff) << 16) |
           ((m_rx_data_fifo.size() * 4) & 0xffff);
}

u32 lan9118::read_tx_fifo_inf() {
    return ((m_tx_status_fifo.size() & 0xff) << 16) |
           ((m_tx_data_fifo_size - tx_data_used() * 4) & 0xffff);
}

void lan9118::write_pmt_ctrl(u32 val) {
    if (val & PMT_CTRL_PHY_RST)
        phy.reset();

    pmt_ctrl = val & PMT_CTRL_MASK;
    pmt_ctrl |= PMT_CTRL_READY;
}

void lan9118::write_gpt_cfg(u32 val) {
    gpt_cfg = val & GPT_CFG_MASK;
    gpt_restart();
}

u32 lan9118::read_gpt_cnt() {
    if (!(gpt_cfg & GPT_CFG_TIMER_EN))
        return 0xffff;

    u64 cnt = (sc_time_stamp() - m_gpt_start) / m_gpt_cycle;
    return ((gpt_cfg & GPT_CFG_LOAD_MASK) - cnt) & 0xffff;
}

u32 lan9118::read_free_run() {
    return (sc_time_stamp() - m_last_reset) / m_frt_cycle;
}

u32 lan9118::read_rx_drop() {
    u32 val = rx_drop;
    rx_drop = 0;
    return val;
}

void lan9118::write_mac_cmd(u32 val) {
    mac_csr_cmd = val & MAC_CMD_MASK;

    if (!(val & MAC_CMD_BUSY))
        return;

    u32 data = mac_csr_data;
    u32 size = sizeof(data);
    u32 addr = val & MAC_CMD_ADDR;
    auto cmd = val & MAC_CMD_R_NW ? TLM_READ_COMMAND : TLM_WRITE_COMMAND;

    tlm_generic_payload tx;
    tlm_sbi sbi = in_debug_transaction() ? SBI_DEBUG : SBI_NONE;
    tx_setup(tx, cmd, addr * size, &data, size);
    u32 res = mac.receive(tx, sbi, VCML_AS_DEFAULT);

    if (failed(tx) || res != size)
        log_warn("MAC CSR access failed %s", to_string(tx).c_str());
    else if (tx.is_read())
        mac_csr_data = data;

    m_rxev.notify();
    m_txev.notify();
}

void lan9118::write_e2_p_cmd(u32 val) {
    e2p_cmd = val & E2P_CMD_MASK;
    u32 idx = val & E2P_CMD_ADDR_MASK;

    switch (e2p_cmd & E2P_CMD_RELOAD) {
    case E2P_CMD_READ:
        e2p_data = m_eeprom[idx];
        break;

    case E2P_CMD_EWDS:
        log_debug("EEPROM write disabled");
        m_eeprom.allow_read_only();
        break;

    case E2P_CMD_EWEN:
        log_debug("EEPROM write enabled");
        m_eeprom.allow_read_write();
        break;

    case E2P_CMD_WRITE:
        if (failed(m_eeprom.write<u8>(idx, e2p_data)))
            log_debug("EEPROM write ignored");
        break;

    case E2P_CMD_WRAL:
        if (failed(m_eeprom.fill(e2p_data, false)))
            log_debug("EEPROM write_all ignored");
        break;

    case E2P_CMD_ERASE:
        if (failed(m_eeprom.write<u8>(idx, 0xff)))
            log_debug("EEPROM write ignored");
        break;

    case E2P_CMD_ERAL:
        if (failed(m_eeprom.fill(0xff, false)))
            log_debug("EEPROM write_all ignored");
        break;

    case E2P_CMD_RELOAD:
        eeprom_reload();
        break;
    }
}

lan9118::lan9118(const sc_module_name& nm):
    peripheral(nm),
    eth_host(),
    m_eeprom(128),
    m_last_reset(),
    m_deas_cycle(10, SC_US),
    m_deas_delta(),
    m_deas_limit(),
    m_deas_ev("deas_ev"),
    m_frt_cycle(40, SC_NS),  // 25MHz
    m_gpt_cycle(100, SC_US), // 10kHz
    m_gpt_start(),
    m_gpt_ev("gpt_ev"),
    m_rxev("rxev"),
    m_txev("txev"),
    m_rx_data_fifo_size(),
    m_rx_status_fifo_size(),
    m_tx_data_fifo_size(),
    m_tx_status_fifo_size(),
    m_tx_pkt(),
    m_tx_packets(),
    m_tx_status_fifo(),
    m_rx_data_fifo(),
    m_rx_status_fifo(),
    eeprom_mac("eeprom_mac", "12:34:56:78:9a:bc"),
    rx_data_fifo("rx_data_fifo", 0x00, 0x00000000),
    tx_data_fifo("tx_data_fifo", 0x20, 0x00000000),
    rx_status_fifo("rx_status_fifo", 0x40, 0x00000000),
    rx_status_peek("rx_status_peek", 0x44, 0x00000000),
    tx_status_fifo("tx_status_fifo", 0x48, 0x00000000),
    tx_status_peek("tx_status_peek", 0x4c, 0x00000000),
    id_rev("id_rev", 0x50, chiprev(0x118, 1)),
    irq_cfg("irq_cfg", 0x54, 0x00000000),
    irq_sts("irq_sts", 0x58, 0x00000000),
    irq_en("irq_en", 0x5c, 0x00000000),
    byte_test("byte_test", 0x64, 0x87654321),
    fifo_int("fifo_int", 0x68, 0x48000000),
    rx_cfg("rx_cfg", 0x6c, 0x00000000),
    tx_cfg("tx_cfg", 0x70, 0x00000000),
    hw_cfg("hw_cfg", 0x74, HW_CFG_RESET),
    rx_dp_ctrl("rx_dp_ctrl", 0x78, 0x00000000),
    rx_fifo_inf("rx_fifo_inf", 0x7c, 0x00000000),
    tx_fifo_inf("tx_fifo_inf", 0x80, 0x00001200),
    pmt_ctrl("pmt_ctrl", 0x84, PMT_CTRL_RESET),
    gpio_cfg("gpio_cfg", 0x88, 0x00000000),
    gpt_cfg("gpt_cfg", 0x8c, GPT_CFG_RESET),
    gpt_cnt("gpt_cnt", 0x90, 0x0000ffff),
    word_swap("word_swap", 0x98, 0x00000000),
    free_run("free_run", 0x9c, 0x00000000),
    rx_drop("rx_drop", 0xa0, 0x00000000),
    mac_csr_cmd("mac_csr_cmd", 0xa4, 0x00000000),
    mac_csr_data("mac_csr_data", 0xa8, 0x00000000),
    afc_cfg("afc_cfg", 0xac, 0x00000000),
    e2p_cmd("e2p_cmd", 0xb0, 0x00000000),
    e2p_data("e2p_data", 0xb4, 0x00000000),
    in("in"),
    irq("irq"),
    eth_tx("eth_tx"),
    eth_rx("eth_rx"),
    phy("phy", *this),
    mac("mac", *this) {
    rx_data_fifo.sync_always();
    rx_data_fifo.allow_read_only();
    rx_data_fifo.on_read(&lan9118::read_rx_data_fifo);

    tx_data_fifo.sync_always();
    tx_data_fifo.allow_write_only();
    tx_data_fifo.on_write(&lan9118::write_tx_data_fifo);

    rx_status_fifo.sync_always();
    rx_status_fifo.allow_read_only();
    rx_status_fifo.on_read(&lan9118::read_rx_status_fifo);

    rx_status_peek.sync_always();
    rx_status_peek.allow_read_only();
    rx_status_peek.on_read(&lan9118::read_rx_status_peek);

    tx_status_fifo.sync_always();
    tx_status_fifo.allow_read_only();
    tx_status_fifo.on_read(&lan9118::read_tx_status_fifo);

    tx_status_peek.sync_always();
    tx_status_peek.allow_read_only();
    tx_status_peek.on_read(&lan9118::read_tx_status_peek);

    id_rev.sync_never();
    id_rev.allow_read_only();

    irq_cfg.sync_always();
    irq_cfg.allow_read_write();
    irq_cfg.on_write(&lan9118::write_irq_cfg);

    irq_sts.sync_always();
    irq_sts.allow_read_write();
    irq_sts.on_write(&lan9118::write_irq_sts);

    irq_en.sync_always();
    irq_en.allow_read_write();
    irq_en.on_write(&lan9118::write_irq_en);

    byte_test.sync_never();
    byte_test.allow_read_only();

    fifo_int.sync_always();
    fifo_int.allow_read_write();
    fifo_int.on_write(&lan9118::write_fifo_int);

    rx_cfg.sync_always();
    rx_cfg.allow_read_write();
    rx_cfg.on_write(&lan9118::write_rx_cfg);

    tx_cfg.sync_always();
    tx_cfg.allow_read_write();
    tx_cfg.on_write(&lan9118::write_tx_cfg);

    hw_cfg.sync_always();
    hw_cfg.allow_read_write();
    hw_cfg.on_write(&lan9118::write_hw_cfg);

    rx_dp_ctrl.sync_always();
    rx_dp_ctrl.allow_read_write();
    rx_dp_ctrl.on_write(&lan9118::write_rx_dp_ctrl);

    rx_fifo_inf.sync_on_read();
    rx_fifo_inf.allow_read_only();
    rx_fifo_inf.on_read(&lan9118::read_rx_fifo_inf);

    tx_fifo_inf.sync_on_read();
    tx_fifo_inf.allow_read_only();
    tx_fifo_inf.on_read(&lan9118::read_tx_fifo_inf);

    pmt_ctrl.sync_always();
    pmt_ctrl.allow_read_write();
    pmt_ctrl.on_write(&lan9118::write_pmt_ctrl);

    gpt_cfg.sync_always();
    gpt_cfg.allow_read_write();
    gpt_cfg.on_write(&lan9118::write_gpt_cfg);

    gpt_cnt.sync_always();
    gpt_cnt.allow_read_only();
    gpt_cnt.on_read(&lan9118::read_gpt_cnt);

    free_run.sync_always();
    free_run.allow_read_only();
    free_run.on_read(&lan9118::read_free_run);

    rx_drop.sync_on_read();
    rx_drop.no_writeback();
    rx_drop.allow_read_only();
    rx_drop.on_read(&lan9118::read_rx_drop);

    mac_csr_cmd.sync_always();
    mac_csr_cmd.allow_read_write();
    mac_csr_cmd.on_write(&lan9118::write_mac_cmd);

    mac_csr_data.sync_always();
    mac_csr_data.allow_read_write();

    e2p_cmd.sync_always();
    e2p_cmd.allow_read_write();
    e2p_cmd.on_write(&lan9118::write_e2_p_cmd);

    e2p_data.sync_always();
    e2p_data.allow_read_write();

    SC_HAS_PROCESS(lan9118);
    SC_THREAD(rx_thread);
    SC_THREAD(tx_thread);

    SC_METHOD(gpt_update);
    sensitive << m_gpt_ev;
    dont_initialize();

    SC_METHOD(deas_update);
    sensitive << m_deas_ev;
    dont_initialize();

    rst.bind(mac.rst);
    rst.bind(phy.rst);
    clk.bind(mac.clk);
    clk.bind(phy.clk);

    m_eeprom.fill(0);
    m_eeprom[0] = 0xa5;

    const char* factory_mac = eeprom_mac.get().c_str();
    if (sscanf(factory_mac, mac_addr::format, m_eeprom.data() + 1,
               m_eeprom.data() + 2, m_eeprom.data() + 3, m_eeprom.data() + 4,
               m_eeprom.data() + 5, m_eeprom.data() + 6) != 6) {
        VCML_ERROR("invalid MAC address specified: %s", factory_mac);
    }
}

lan9118::~lan9118() {
    // nothing to do
}

void lan9118::reset() {
    m_eeprom.allow_read_only();
    m_last_reset = sc_time_stamp();

    m_tx_pkt.reset();
    m_tx_packets.clear();

    reset_fifo_size(5 * KiB);

    peripheral::reset();
    eeprom_reload();
    gpt_restart();
}

void lan9118::update_irq() {
    if (phy.int_source & phy.int_mask)
        irq_sts |= IRQ_PHY;
    else
        irq_sts &= ~IRQ_PHY;

    if (tx_status_full())
        irq_sts |= IRQ_TSFF;

    if (rx_status_full())
        irq_sts |= IRQ_RSFF;

    if (tx_data_free() > tx_data_level())
        irq_sts |= IRQ_TDFA;

    if (tx_status_used() > tx_status_level())
        irq_sts |= IRQ_TSFL;

    if (rx_status_used() > rx_status_level())
        irq_sts |= IRQ_RSFL;

    if (rx_drop > 0x7fffffffu)
        irq_sts |= IRQ_RXDF;

    u32 irqs = irq_sts & irq_en;
    if (irqs)
        irq_cfg |= IRQ_CFG_INT;
    else
        irq_cfg &= ~IRQ_CFG_INT;

    if (irqs == 0 || !(irq_cfg & IRQ_CFG_EN)) {
        irq_cfg &= ~IRQ_CFG_DEAS_STS;
        irq = false;
        return;
    }

    if (sc_time_stamp() >= m_deas_limit) {
        irq_cfg &= ~IRQ_CFG_DEAS_STS;
        m_deas_limit = sc_time_stamp() + m_deas_delta;
        irq = true;
        return;
    }

    sc_time throttle = m_deas_limit - sc_time_stamp();
    log_debug("interrupt throttled for %lluns", time_to_ns(throttle));
    m_deas_ev.notify(throttle);
    irq_cfg |= IRQ_CFG_DEAS_STS;
}

void lan9118::eth_link_up() {
    phy.set_link_status(true);
}

void lan9118::eth_link_down() {
    phy.set_link_status(false);
}

VCML_EXPORT_MODEL(vcml::ethernet::lan9118, name, args) {
    return new lan9118(name);
}

} // namespace ethernet
} // namespace vcml
