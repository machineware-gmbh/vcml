/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ETHERNET_ETHOC_H
#define VCML_ETHERNET_ETHOC_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/eth.h"

namespace vcml {
namespace ethernet {

class ethoc : public peripheral, public eth_host
{
private:
    mac_addr m_mac;
    size_t m_tx_idx;
    size_t m_rx_idx;

    struct descriptor {
        u32 info;
        u32 addr;
    };

    enum : size_t {
        ETHOC_NUMBD = 128,
    };

    descriptor m_desc[ETHOC_NUMBD];

    size_t num_txbd() const;
    size_t num_rxbd() const;

    descriptor current_txbd() const;
    descriptor current_rxbd() const;

    void update_txbd(const descriptor& desc);
    void update_rxbd(const descriptor& desc);

    bool m_tx_enabled;
    bool m_rx_enabled;

    sc_event m_tx_event;
    sc_event m_rx_event;

    void tx_process();
    void rx_process();

    void tx_poll();
    void rx_poll();

    bool tx_packet(u32 addr, u32 size);
    bool rx_packet(u32 addr, u32& size);

    void interrupt(int source);

    void write_moder(u32 val);
    void write_int_source(u32 val);
    void write_int_mask(u32 val);
    void write_tx_bd_num(u32 val);
    void write_miicommand(u32 val);
    void write_mac_addr0(u32 val);
    void write_mac_addr1(u32 val);

    u32 read_mac_addr0();
    u32 read_mac_addr1();

    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info) override;
    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info) override;

    // Disabled
    ethoc();
    ethoc(const ethoc&);

public:
    enum ram_address {
        RAM_START = 0x400, // internal RAM start address
        RAM_END = 0x7ff,   // internal RAM end address
    };

    enum txbd_status {
        TXBD_CS = 1 << 0,   // carrier sense lost
        TXBD_DF = 1 << 1,   // defer indication
        TXBD_LC = 1 << 2,   // late collision
        TXBD_RL = 1 << 3,   // retransmission limit
        TXBD_RTRY_O = 4,    // retry count offset
        TXBD_RTRY_M = 0xf,  // retry count mask
        TXBD_UR = 1 << 8,   // underrun
        TXBD_CRC = 1 << 11, // CRC enabled
        TXBD_PAD = 1 << 12, // pad enabled
        TXBD_WR = 1 << 13,  // wrap
        TXBD_IRQ = 1 << 14, // IRQ enabled
        TXBD_RD = 1 << 15,  // ready
        TXBD_LEN_O = 16,    // length offset
        TXBD_LEN_M = 0xffff // length mask
    };

    enum rxbd_status {
        RXBD_LC = 1 << 0,    // late collision
        RXBD_CRC = 1 << 1,   // CRC error
        RXBD_SF = 1 << 2,    // short frame received
        RXBD_TL = 1 << 3,    // too long
        RXBD_DN = 1 << 4,    // dribble nibble
        RXBD_IS = 1 << 5,    // invalid symbol
        RXBD_OR = 1 << 6,    // overrun
        RXBD_M = 1 << 7,     // miss
        RXBD_CF = 1 << 8,    // control frame
        RXBD_WRAP = 1 << 13, // wrap
        RXBD_IRQ = 1 << 14,  // IRQ enabled
        RXBD_E = 1 << 15,    // empty
        RXBD_LEN_O = 16,     // length offset
        RXBD_LEN_M = 0xffff  // length mask
    };

    enum moder_status {
        MODER_RXEN = 1 << 0,      // receive enabled
        MODER_TXEN = 1 << 1,      // transmit enabled
        MODER_NOPRE = 1 << 2,     // no preamble
        MODER_BRO = 1 << 3,       // receive broadcast address frames
        MODER_IAM = 1 << 4,       // individual address mode enabled
        MODER_PRO = 1 << 5,       // promiscuous mode enabled
        MODER_IFG = 1 << 6,       // interframe gap
        MODER_LOOPBCK = 1 << 7,   // loop back TX to RX
        MODER_NOBCKOF = 1 << 8,   // no backoff
        MODER_EXDFREN = 1 << 9,   // excess defer enabled
        MODER_FULLD = 1 << 10,    // full duplex Mode
        MODER_RST = 1 << 11,      // reserved
        MODER_DLYCRCEN = 1 << 12, // delayed CRC enabled
        MODER_CRCEN = 1 << 13,    // CRC enabled
        MODER_HUGEN = 1 << 14,    // huge packets enabled
        MODER_PAD = 1 << 15,      // padding enabled
        MODER_RECSMALL = 1 << 16, // receive small packets
    };

    enum int_source_status {
        INT_SOURCE_TXB = 1 << 0,  // transmit buffer
        INT_SOURCE_TXE = 1 << 1,  // transmit error
        INT_SOURCE_RXB = 1 << 2,  // receive frame
        INT_SOURCE_RXE = 1 << 3,  // receive error
        INT_SOURCE_BUSY = 1 << 4, // busy
        INT_SOURCE_TXC = 1 << 5,  // transmit control frame
        INT_SOURCE_RXC = 1 << 6,  // receive control frame
    };

    enum int_mask_status {
        INT_MASK_TXB = 1 << 0,  // transmit buffer
        INT_MASK_TXE = 1 << 1,  // transmit error
        INT_MASK_RXB = 1 << 2,  // receive frame
        INT_MASK_RXE = 1 << 3,  // receive error
        INT_MASK_BUSY = 1 << 4, // busy
        INT_MASK_TXC = 1 << 5,  // transmit control frame
        INT_MASK_RXC = 1 << 6,  // receive control frame
    };

    enum packetlen_status {
        PACKETLEN_MAXFL_M = 0xffff, // maximum frame length mask
        PACKETLEN_MAXFL_O = 0,      // maximum frame length offset
        PACKETLEN_MINFL_M = 0xffff, // minimum frame length mask
        PACKETLEN_MINFL_O = 16,     // minimum frame length offset
    };

    enum collconf_status {
        COLLCONF_COLLVALID = 0x3f, // collision valid field
        COLLCONF_MAXRET_M = 0xf,   // maximum retry mask
        COLLCONF_MAXRET_O = 16,    // maximum retry offset
    };

    enum tx_bd_num_mask {
        TX_BD_NUM_M = 0xFF, // transmit buffer descriptor number mask
    };

    enum ctrlmoder_status {
        CTRLMODER_PASSALL = 1 << 0, // pass all received frames
        CTRLMODER_RXFLOW = 1 << 1,  // receive flow control
        CTRLMODER_TXFLOW = 1 << 2,  // transmit flow control
    };

    enum miimoder_status {
        MIIMODER_CLKDIV = 0xf,      // clock divider
        MIIMODER_MIINOPRE = 1 << 8, // no preamble
    };

    enum miicommand_status {
        MIICOMMAND_SCANSTAT = 1 << 0,  // scan status
        MIICOMMAND_RSTAT = 1 << 1,     // read status
        MIICOMMAND_WCTRLDATA = 1 << 2, // write control data
    };

    enum miiaddress_status {
        MIIADDRESS_FIAD_M = 0x1f, // PHY Address Mask
        MIIADDRESS_FIAD_O = 0,    // PHY Address Offset
        MIIADDRESS_RGAD_M = 0x1f, // Register Address Mask
        MIIADDRESS_RGAD_O = 8,    // Register Address Offset
    };

    enum mii_status {
        MIISTATUS_LINKFAIL = 1 << 0, // link failed
        MIISTATUS_BUSY = 1 << 1,     // MII busy
        MIISTATUS_NVALID = 1 << 2,   // data in MSTATUS is invalid
    };

    enum mac_addr0_offset {
        MAC_ADDR0_B5 = 0x0,  // byte 5 of the Ethernet MAC address
        MAC_ADDR0_B4 = 0x8,  // byte 4 of the Ethernet MAC address
        MAC_ADDR0_B3 = 0x10, // byte 3 of the Ethernet MAC address
        MAC_ADDR0_B2 = 0x18, // byte 2 of the Ethernet MAC address
    };

    enum mac_addr1_offset {
        MAC_ADDR1_B1 = 0x0, // byte 1 of the Ethernet MAC address
        MAC_ADDR1_B0 = 0x8, // byte 0 of the Ethernet MAC address
    };

    enum txctrl_status {
        TXCTRL_TXPAUSETV_M = 0xffff, // value send in pause control frame
        TXCTRL_TXPAUSERQ = 1 << 16,  // TX pause request
    };

    reg<u32> moder;         // mode register
    reg<u32> int_source;    // interrupt source register
    reg<u32> int_mask;      // interrupt mask register
    reg<u32> ipgt;          // back to back inter-packet gap
    reg<u32> ipgr1;         // non back to back inter-packet gap 1
    reg<u32> ipgr2;         // non back to back inter-packet gap 2
    reg<u32> packetlen;     // packet length
    reg<u32> collconf;      // collision/retry configuration
    reg<u32> tx_bd_num;     // transmit buffer descriptor number
    reg<u32> ctrlmoder;     // control module mode register
    reg<u32> miimoder;      // MII mode register
    reg<u32> miicommand;    // MII command register
    reg<u32> miiaddress;    // MII address register
    reg<u32> miitx_data;    // MII transmit data
    reg<u32> miirx_data;    // MII receive data
    reg<u32> miistatus;     // MII status register
    reg<u32> mac_addr0;     // MAC address 0
    reg<u32> mac_addr1;     // MAC address 1
    reg<u32> eth_hash0_adr; // HASH0 register
    reg<u32> eth_hash1_adr; // HASH1 register
    reg<u32> eth_txctrl;    // transmit control register

    property<hz_t> clock;
    property<string> mac;

    gpio_initiator_socket irq;

    tlm_target_socket in;
    tlm_initiator_socket out;

    eth_initiator_socket eth_tx;
    eth_target_socket eth_rx;

    ethoc(const sc_module_name& name);
    virtual ~ethoc();
    VCML_KIND(ethernet::ethoc);

    virtual void reset() override;

    void set_mac_addr(u8 addr[6]);
};

inline size_t ethoc::num_txbd() const {
    return tx_bd_num;
}

inline size_t ethoc::num_rxbd() const {
    return ETHOC_NUMBD - tx_bd_num;
}

inline ethoc::descriptor ethoc::current_txbd() const {
    return { to_host_endian(m_desc[m_tx_idx].info),
             to_host_endian(m_desc[m_tx_idx].addr) };
}

inline ethoc::descriptor ethoc::current_rxbd() const {
    return { to_host_endian(m_desc[m_rx_idx].info),
             to_host_endian(m_desc[m_rx_idx].addr) };
}

inline void ethoc::update_txbd(const ethoc::descriptor& desc) {
    m_desc[m_tx_idx].info = from_host_endian(desc.info);
    m_desc[m_tx_idx].addr = from_host_endian(desc.addr);
}

inline void ethoc::update_rxbd(const ethoc::descriptor& desc) {
    m_desc[m_rx_idx].info = from_host_endian(desc.info);
    m_desc[m_rx_idx].addr = from_host_endian(desc.addr);
}

inline void ethoc::set_mac_addr(u8 addr[6]) {
    for (int i = 0; i < 6; i++)
        m_mac[i] = addr[i];
}

} // namespace ethernet
} // namespace vcml

#endif
