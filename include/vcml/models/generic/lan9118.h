/******************************************************************************
 *                                                                            *
 * Copyright 2022 Jan Henrik Weinstock                                        *
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

#ifndef VCML_MODELS_LAN9118_H
#define VCML_MODELS_LAN9118_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/bitops.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/irq.h"

#include "vcml/net/adapter.h"

#include "vcml/peripheral.h"

namespace vcml { namespace generic {

    class lan9118;

    class lan9118_phy : public peripheral
    {
    private:
        lan9118& m_parent;

        void negotiate_link();

        void write_CONTROL(u16 val);
        void write_ADVERTISE(u16 val);
        u16  read_INT_SOURCE();
        void write_INT_MASK(u16 val);

    public:
        reg<u16> CONTROL;
        reg<u16> STATUS;
        reg<u16> IDENT1;
        reg<u16> IDENT2;
        reg<u16> ADVERTISE;
        reg<u16> LINK_PARTNER;
        reg<u16> NEGOTIATE_EX;
        reg<u16> MODE_CTRL;
        reg<u16> SPECIAL_MODES;
        reg<u16> SPECIAL_CTRL;
        reg<u16> INT_SOURCE;
        reg<u16> INT_MASK;
        reg<u16> SPECIAL_STATUS;

        lan9118_phy(const sc_module_name& name, lan9118& lan);
        virtual ~lan9118_phy();
        VCML_KIND(lan9118_phy);
        virtual void reset();

        sc_time rxtx_delay(size_t bytes) const;

        bool link_status() const;
        void set_link_status(bool up);
    };

    class lan9118_mac : public peripheral
    {
    private:
        lan9118& m_parent;
        net::mac_addr m_addr;

        void write_CR(u32 val);
        void write_MII_ACC(u32 val);
        void write_MII_DATA(u32 val);

    public:
        net::mac_addr address() const { return m_addr; }
        void set_address(const net::mac_addr& addr);

        reg<u32> CR;
        reg<u32> ADDRH;
        reg<u32> ADDRL;
        reg<u32> HASHH;
        reg<u32> HASHL;
        reg<u32> MII_ACC;
        reg<u32> MII_DATA;
        reg<u32> FLOW;
        reg<u32> VLAN1;
        reg<u32> VLAN2;
        reg<u32> WUFF;
        reg<u32> WUCSR;

        lan9118_mac(const sc_module_name& name, lan9118& lan);
        virtual ~lan9118_mac();
        VCML_KIND(lan9118_mac);
        virtual void reset();

        bool filter(const net::mac_addr& dest) const;
    };

    class lan9118 : public peripheral, public net::adapter
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

        sc_time  m_deas_cycle;
        sc_time  m_deas_delta;
        sc_time  m_deas_limit;
        sc_event m_deas_ev;

        sc_time  m_frt_cycle;
        sc_time  m_gpt_cycle;

        sc_time  m_gpt_start;
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

        size_t tx_data_level() const {
            return ((FIFO_INT >> 24) & 0xff) * 64;
        }

        size_t tx_status_used() const {
            return m_tx_status_fifo.size() * 4;
        }

        size_t tx_status_free() const {
            return m_tx_status_fifo_size - tx_status_used();
        }

        size_t tx_status_level() const {
            return ((FIFO_INT >> 16) & 0xff) * 4;
        }

        size_t rx_status_used() const {
            return m_rx_status_fifo.size() * 4;
        }

        size_t rx_status_free() const {
            return m_rx_status_fifo_size - rx_status_used();
        }

        size_t rx_status_level() const {
            return (FIFO_INT & 0xff) * 4;
        }

        size_t rx_data_used() const {
            return m_rx_data_fifo.size() * 4;
        }

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

        u32  read_RX_DATA_FIFO();
        void write_TX_DATA_FIFO(u32 val);

        u32  read_RX_STATUS_FIFO();
        u32  read_RX_STATUS_PEEK();
        u32  read_TX_STATUS_FIFO();
        u32  read_TX_STATUS_PEEK();

        void write_IRQ_CFG(u32 val);
        void write_IRQ_STS(u32 val);
        void write_IRQ_EN(u32 val);

        void write_FIFO_INT(u32 val);

        void write_RX_CFG(u32 val);
        void write_TX_CFG(u32 val);
        void write_HW_CFG(u32 val);

        void write_RX_DP_CTRL(u32 val);

        u32  read_RX_FIFO_INF();
        u32  read_TX_FIFO_INF();

        void write_PMT_CTRL(u32 val);

        void write_GPT_CFG(u32 val);
        u32  read_GPT_CNT();

        u32  read_FREE_RUN();

        u32  read_RX_DROP();

        void write_MAC_CMD(u32 val);
        void write_E2P_CMD(u32 val);

    public:
        property<string> eeprom_mac;

        reg<u32, 8> RX_DATA_FIFO;
        reg<u32, 8> TX_DATA_FIFO;
        reg<u32> RX_STATUS_FIFO;
        reg<u32> RX_STATUS_PEEK;
        reg<u32> TX_STATUS_FIFO;
        reg<u32> TX_STATUS_PEEK;

        reg<u32> ID_REV;
        reg<u32> IRQ_CFG;
        reg<u32> IRQ_STS;
        reg<u32> IRQ_EN;
        reg<u32> BYTE_TEST;
        reg<u32> FIFO_INT;
        reg<u32> RX_CFG;
        reg<u32> TX_CFG;
        reg<u32> HW_CFG;
        reg<u32> RX_DP_CTRL;
        reg<u32> RX_FIFO_INF;
        reg<u32> TX_FIFO_INF;
        reg<u32> PMT_CTRL;
        reg<u32> GPIO_CFG;
        reg<u32> GPT_CFG;
        reg<u32> GPT_CNT;
        reg<u32> WORD_SWAP;
        reg<u32> FREE_RUN;
        reg<u32> RX_DROP;
        reg<u32> MAC_CSR_CMD;
        reg<u32> MAC_CSR_DATA;
        reg<u32> AFC_CFG;
        reg<u32> E2P_CMD;
        reg<u32> E2P_DATA;

        tlm_target_socket IN;
        irq_initiator_socket IRQ;

        lan9118_phy phy;
        lan9118_mac mac;

        net::mac_addr mac_address() const { return mac.address(); }

        lan9118(const sc_module_name& name);
        virtual ~lan9118();
        VCML_KIND(lan9118);
        virtual void reset() override;

        void update_irq();

    protected:
        virtual void on_link_up();
        virtual void on_link_down();
    };

}}

#endif
