/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_DMA_PL330_H
#define VCML_DMA_PL330_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace dma {

class pl330 : public peripheral
{
private:
    template <typename T>
    class tagged_queue
    {
    public:
        tagged_queue(size_t max_total_items):
            m_queues(),
            m_tags(),
            m_max_sum(max_total_items),
            m_current_sum(0) {
            // nothing to do
        }

        bool push(const T& item) {
            if (m_current_sum < m_max_sum) {
                m_queues[item.tag].push_back(item);
                m_tags.push_back(item.tag);
                m_current_sum += 1;
                return true;
            }
            return false;
        }

        std::optional<T> pop() {
            if (m_tags.empty())
                return std::nullopt;

            int front_tag = m_tags.front();
            T front_item = m_queues[front_tag].front();
            m_queues[front_tag].pop_front();
            m_tags.pop_front();
            m_current_sum--;
            return std::make_optional(front_item);
        }

        const T& front() const { return m_queues.at(m_tags.front()).front(); }
        T& front() { return m_queues.at(m_tags.front()).front(); }

        std::optional<T> pop(int tag) {
            if (m_queues[tag].empty())
                return std::nullopt;

            T item = m_queues[tag].front();
            m_queues[tag].pop_front();
            m_tags.erase(std::find(m_tags.begin(), m_tags.end(), tag));
            m_current_sum--;
            return std::make_optional(item);
        }

        void clear(int tag) {
            m_queues[tag].clear();
            m_tags.erase(std::remove(m_tags.begin(), m_tags.end(), tag),
                         m_tags.end());
        }

        void remove_tagged(int tag) { clear(tag); }

        void clear() {
            m_queues.clear();
            m_tags.clear();
        }

        void reset() { clear(); }

        bool empty() const { return m_tags.empty(); }
        bool empty(int tag) const { return m_queues.at(tag).empty(); }

        size_t size() const { return m_tags.size(); }
        size_t num_free() const { return m_max_sum - m_current_sum; }
        size_t size_tag(int tag) const { return m_queues[tag].size(); }

    private:
        std::unordered_map<int, std::deque<T>> m_queues;
        std::deque<int> m_tags;

        size_t m_max_sum;
        size_t m_current_sum;
    };

public:
    enum amba_ids : u32 {
        AMBA_PID = 0x00241330, // Peripheral ID
        AMBA_CID = 0xb105f00d, // PrimeCell ID
    };

    struct queue_entry {
        u32 data_addr;
        u32 data_len;
        u32 burst_len_counter;
        bool inc;
        bool zero_flag;
        u32 tag;
    };

    struct mfifo_entry {
        u8 buf;
        u8 tag;
    };

    class channel : public module
    {
    public:
        reg<u32> ftr; // channel fault type register
        reg<u32> csr; // channel status register
        reg<u32> cpc; // channel pc register
        reg<u32> sar; // source address register
        reg<u32> dar; // destination address register
        reg<u32> ccr; // channel control register
        reg<u32> lc0; // loop counter 0 register
        reg<u32> lc1; // loop counter 1 register

        u32 chid;
        bool stall;
        u32 request_flag;
        u32 watchdog_timer;

        bool is_state(u8 state) const { return get_state() == state; }
        u32 get_state() const { return csr & 0x7; }
        void set_state(u32 new_state) { csr = (csr & ~0x7) | new_state; }

        channel(const sc_module_name& nm, u32 tag);
        virtual ~channel() = default;
        VCML_KIND(dma::pl330::channel);
    };

    class manager : public module
    {
    public:
        reg<u32> dsr;  // DMA Manager Status Register
        reg<u32> dpc;  // DMA Program Counter Register
        reg<u32> fsrd; // Fault Status DMA Manager Register
        reg<u32> ftrd; // Fault Type DMA Manager Register

        bool stall;
        u32 watchdog_timer;

        bool is_state(u8 state) const { return (get_state() == state); }
        u32 get_state() const { return dsr & 0x7; }
        void set_state(u32 new_state) { dsr = (dsr & ~0x7) | new_state; }

        virtual ~manager() = default;
        VCML_KIND(dma::pl330::manager);
        manager(const sc_module_name& nm);
    };

    property<bool> enable_periph;
    property<u32> num_channels;
    property<u32> queue_size;
    property<u32> mfifo_width;
    property<u32> mfifo_lines;

    tagged_queue<queue_entry> read_queue;
    tagged_queue<queue_entry> write_queue;
    tagged_queue<mfifo_entry> mfifo;

    sc_vector<channel> channels;
    manager manager;

    reg<u32> fsrc; // Fault Status DMA Channel Register

    reg<u32> inten;         // Interrupt Enable Register
    reg<u32> int_event_ris; // Event-Interrupt Raw Status Register
    reg<u32> intmis;        // Interrupt Status Register
    reg<u32> intclr;        // Interrupt Clear Register

    reg<u32> dbgstatus; // Debug Status Register
    reg<u32> dbgcmd;    // Debug Command Register
    reg<u32> dbginst0;  // Debug Instructions-0 Register
    reg<u32> dbginst1;  // Debug Instructions-1 Register

    reg<u32> cr0; // Configuration Register 0
    reg<u32> cr1; // Configuration Register 1
    reg<u32> cr2; // Configuration Register 2
    reg<u32> cr3; // Configuration Register 3
    reg<u32> cr4; // Configuration Register 4
    reg<u32> crd; // DMA Configuration Register
    reg<u32> wd;  // Watchdog Register

    reg<u32, 4> periph_id; // Peripheral Identification Registers
    reg<u32, 4> pcell_id;  // Component Identification Registers

    bool periph_busy[32];
    gpio_target_array<32> periph_irq;

    tlm_target_socket in;
    tlm_initiator_socket dma;
    gpio_initiator_array<32> irq;
    gpio_initiator_socket irq_abort;

    pl330(const sc_module_name& nm);
    virtual ~pl330();
    VCML_KIND(dma::pl330);
    virtual void reset() override;

private:
    [[noreturn]] void pl330_thread();
    void run_manager();
    void run_channels();
    void handle_debug_instruction();

    sc_event m_dma;
    bool m_execute_debug;
};

} // namespace dma
} // namespace vcml

#endif // VCML_DMA_PL330_H
