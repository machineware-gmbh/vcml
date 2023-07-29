/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_ETHERNET_LAN9118_H
#define VCML_ETHERNET_LAN9118_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/model.h"
#include "vcml/core/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/eth.h"

namespace vcml {
namespace ethernet {

class lan9118;

class lan9118_phy : public peripheral
{
private:
    lan9118& m_parent;

    void negotiate_link();

    void write_control(u16 val);
    void write_advertise(u16 val);
    u16 read_int_source();
    void write_int_mask(u16 val);

public:
    reg<u16> control;
    reg<u16> status;
    reg<u16> ident1;
    reg<u16> ident2;
    reg<u16> advertise;
    reg<u16> link_partner;
    reg<u16> negotiate_ex;
    reg<u16> mode_ctrl;
    reg<u16> special_modes;
    reg<u16> special_ctrl;
    reg<u16> int_source;
    reg<u16> int_mask;
    reg<u16> special_status;

    lan9118_phy(const sc_module_name& name, lan9118& lan);
    virtual ~lan9118_phy();
    VCML_KIND(ethernet::lan9118_phy);
    virtual void reset() override;

    sc_time rxtx_delay(size_t bytes) const;

    bool link_status() const;
    void set_link_status(bool up);
};

class lan9118_mac : public peripheral
{
private:
    lan9118& m_parent;
    mac_addr m_addr;

    void write_cr(u32 val);
    void write_mii_acc(u32 val);
    void write_mii_data(u32 val);

public:
    mac_addr address() const { return m_addr; }
    void set_address(const mac_addr& addr);

    reg<u32> cr;
    reg<u32> addrh;
    reg<u32> addrl;
    reg<u32> hashh;
    reg<u32> hashl;
    reg<u32> mii_acc;
    reg<u32> mii_data;
    reg<u32> flow;
    reg<u32> vlan1;
    reg<u32> vlan2;
    reg<u32> wuff;
    reg<u32> wucsr;

    lan9118_mac(const sc_module_name& name, lan9118& lan);
    virtual ~lan9118_mac();
    VCML_KIND(ethernet::lan9118_mac);
    virtual void reset() override;

    bool filter(const mac_addr& dest) const;
};

class lan9118 : public peripheral, public eth_host
{
private:
    struct packet {
        enum { CMDA, CMDB, DATA } state;

        u32 cmda;
        u32 cmdb;

        vector<u8> data;

        size_t used_dw;
        size_t length;
        size_t offset;
        size_t remain;
        size_t padding;

        void reset() {
            used_dw = length = offset = remain = padding = 0;
            state = CMDA;
            data.clear();
        }
    };

    tlm_memory m_eeprom;

    sc_time m_last_reset;

    sc_time m_deas_cycle;
    sc_time m_deas_delta;
    sc_time m_deas_limit;
    sc_event m_deas_ev;

    sc_time m_frt_cycle;
    sc_time m_gpt_cycle;

    sc_time m_gpt_start;
    sc_event m_gpt_ev;

    sc_event m_rxev;
    sc_event m_txev;

    size_t m_rx_data_fifo_size;
    size_t m_rx_status_fifo_size;

    size_t m_tx_data_fifo_size;
    size_t m_tx_status_fifo_size;

    packet m_tx_pkt;
    deque<packet> m_tx_packets;
    deque<u32> m_tx_status_fifo;

    deque<u32> m_rx_data_fifo;
    deque<u32> m_rx_status_fifo;

    void reset_fifo_size(size_t txff_size);

    size_t tx_data_used() const {
        size_t ndw = m_tx_pkt.used_dw;
        for (const auto& pkt : m_tx_packets)
            ndw += pkt.used_dw;
        return ndw;
    }

    size_t tx_data_free() const {
        return m_tx_data_fifo_size - tx_data_used();
    }

    size_t tx_data_level() const { return ((fifo_int >> 24) & 0xff) * 64; }

    size_t tx_status_used() const { return m_tx_status_fifo.size() * 4; }

    size_t tx_status_free() const {
        return m_tx_status_fifo_size - tx_status_used();
    }

    size_t tx_status_level() const { return ((fifo_int >> 16) & 0xff) * 4; }

    size_t rx_status_used() const { return m_rx_status_fifo.size() * 4; }

    size_t rx_status_free() const {
        return m_rx_status_fifo_size - rx_status_used();
    }

    size_t rx_status_level() const { return (fifo_int & 0xff) * 4; }

    size_t rx_data_used() const { return m_rx_data_fifo.size() * 4; }

    size_t rx_data_free() const {
        return m_rx_data_fifo_size - rx_data_used();
    }

    bool tx_data_full() const { return tx_data_free() == 0; }
    bool tx_status_full() const { return tx_status_free() == 0; }
    bool rx_data_full() const { return rx_data_free() == 0; }
    bool rx_status_full() const { return rx_status_free() == 0; }

    void eeprom_reload();

    void deas_update();

    void gpt_restart();
    void gpt_update();

    bool rx_enqueue(const vector<u8>& data);

    void rx_thread();
    void tx_thread();

    u32 read_rx_data_fifo();
    void write_tx_data_fifo(u32 val);

    u32 read_rx_status_fifo();
    u32 read_rx_status_peek();
    u32 read_tx_status_fifo();
    u32 read_tx_status_peek();

    void write_irq_cfg(u32 val);
    void write_irq_sts(u32 val);
    void write_irq_en(u32 val);

    void write_fifo_int(u32 val);

    void write_rx_cfg(u32 val);
    void write_tx_cfg(u32 val);
    void write_hw_cfg(u32 val);

    void write_rx_dp_ctrl(u32 val);

    u32 read_rx_fifo_inf();
    u32 read_tx_fifo_inf();

    void write_pmt_ctrl(u32 val);

    void write_gpt_cfg(u32 val);
    u32 read_gpt_cnt();

    u32 read_free_run();

    u32 read_rx_drop();

    void write_mac_cmd(u32 val);
    void write_e2_p_cmd(u32 val);

public:
    property<string> eeprom_mac;

    reg<u32, 8> rx_data_fifo;
    reg<u32, 8> tx_data_fifo;
    reg<u32> rx_status_fifo;
    reg<u32> rx_status_peek;
    reg<u32> tx_status_fifo;
    reg<u32> tx_status_peek;

    reg<u32> id_rev;
    reg<u32> irq_cfg;
    reg<u32> irq_sts;
    reg<u32> irq_en;
    reg<u32> byte_test;
    reg<u32> fifo_int;
    reg<u32> rx_cfg;
    reg<u32> tx_cfg;
    reg<u32> hw_cfg;
    reg<u32> rx_dp_ctrl;
    reg<u32> rx_fifo_inf;
    reg<u32> tx_fifo_inf;
    reg<u32> pmt_ctrl;
    reg<u32> gpio_cfg;
    reg<u32> gpt_cfg;
    reg<u32> gpt_cnt;
    reg<u32> word_swap;
    reg<u32> free_run;
    reg<u32> rx_drop;
    reg<u32> mac_csr_cmd;
    reg<u32> mac_csr_data;
    reg<u32> afc_cfg;
    reg<u32> e2p_cmd;
    reg<u32> e2p_data;

    tlm_target_socket in;
    gpio_initiator_socket irq;

    eth_initiator_socket eth_tx;
    eth_target_socket eth_rx;

    lan9118_phy phy;
    lan9118_mac mac;

    mac_addr mac_address() const { return mac.address(); }

    lan9118(const sc_module_name& name);
    virtual ~lan9118();
    VCML_KIND(ethernet::lan9118);
    virtual void reset() override;

    void update_irq();

protected:
    virtual void eth_link_up() override;
    virtual void eth_link_down() override;
};

} // namespace ethernet
} // namespace vcml

#endif
