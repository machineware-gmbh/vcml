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

#include <vcml.h>

namespace vcml {
namespace dma {

class pl330 : public peripheral
{
    template <typename T>
    class tagged_queue
    {
    public:
        tagged_queue(int max_total_items):
            m_queues(),
            m_tags(),
            m_max_sum(max_total_items),
            m_current_sum(0) {}

        bool push(const T& item) {
            if (m_current_sum + 1 <= m_max_sum) {
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
            m_current_sum -= 1;
            return std::make_optional(front_item);
        }

        const T& front() const { return m_queues.at(m_tags.front()).front(); }
        T& front_mut() { return m_queues.at(m_tags.front()).front(); }

        std::optional<T> pop(int tag) {
            if (m_queues[tag].empty())
                return std::nullopt;
            T item = m_queues[tag].front();
            m_queues[tag].pop_front();
            m_tags.erase(std::find(m_tags.begin(), m_tags.end(), tag));
            m_current_sum -= 1;
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
        int m_max_sum;
        int m_current_sum;
    };

public:
    enum pl330_configs : u32 {
        INSN_MAXSIZE = 6,
        WD_TIMEOUT = 1024,
    };
    enum mfifo_width : u32 {
        MFIFO_32BIT = 0b010,
        MFIFO_64BIT = 0b011,
        MFIFO_128BIT = 0b100,
    };
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
        enum state : u8 {
            STOPPED = 0x0,
            EXECUTING = 0x1,
            CACHE_MISS = 0x2,
            UPDATING_PC = 0x3,
            WAITING_FOR_EVENT = 0x4,
            AT_BARRIER = 0x5,
            WAITING_FOR_PERIPHERAL = 0x7,
            KILLING = 0x8,
            COMPLETING = 0x9,
            FAULTING_COMPLETING = 0xE,
            FAULTING = 0xF,
        };

        enum fault : u8 {
            UNDEF_INSTR = 0x0,
            OPERAND_INVALID = 0x1,
            CH_EVNT_ERR = 0x5,
            CH_PERIPH_ERR = 0x6,
            CH_RDWR_ERR = 0x7,
            MFIFO_ERR = 0xc,
            ST_DATA_UNAVAILABLE = 0xd,
            INSTR_FETCH_ERR = 0x10,
            DATA_WRITE_ERR = 0x11,
            DATA_READ_ERR = 0x12,
            DBG_INSTR = 0x1e,
            LOCKUP_ERR = 0x1f,
        };

        reg<u32> ftr; // channel fault type register
        reg<u32> csr; // channel status register
        reg<u32> cpc; // channel pc register
        reg<u32> sar; // source address register
        reg<u32> dar; // destination address register
        reg<u32> ccr; // channel control register
        reg<u32> lc0; // loop counter 0 register
        reg<u32> lc1; // loop counter 1 register

        u32 tag; // aka channel number
        bool stall;
        u32 request_flag;
        u32 watchdog_timer;

        bool is_state(u8 state) const { return (get_state() == state); }
        u32 get_state() const { return csr & 0x7; }
        void set_state(u32 new_state) { csr = (csr & ~0x7) | new_state; }

        channel(const sc_module_name& nm, u32 tag);
        virtual ~channel() = default;
        VCML_KIND(dma::pl330::channel);
    };

    class manager : public module
    {
    public:
        enum state : u8 {
            STOPPED = 0x0,
            EXECUTING = 0x1,
            CACHE_MISS = 0x2,
            UPDATING_PC = 0x3,
            WAITING_FOR_EVENT = 0x4,
            FAULTING = 0xF,
        };

        enum fault : u8 {
            UNDEF_INSTR = 0x0,
            OPERAND_INVALID = 0x1,
            DMAGO_ERR = 0x4,
            EVNT_ERR = 0x5,
            INSTR_FETCH_ERR = 0x10,
            DBG_INSTR = 0x1e,
        };

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
    property<u32> num_irq;
    property<u32> num_periph;
    property<u32> queue_size;
    property<u32> mfifo_width;
    property<u32> mfifo_lines;

    tagged_queue<queue_entry> read_queue, write_queue;
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
    reg<u32> cr1; // Configuration Register 1 //TODO: probably not needed?!
    reg<u32> cr2; // Configuration Register 2
    reg<u32> cr3; // Configuration Register 3
    reg<u32> cr4; // Configuration Register 4 // security state op peripheral
                  // requests
    reg<u32> crd; // DMA Configuration Register
    reg<u32> wd;  // Watchdog Register

    reg<u32, 4> periph_id; // Peripheral Identification Registers
    reg<u32, 4> pcell_id;  // Component Identification Registers

    u8 periph_busy[6]; // todo do we need this in addition to periph_irq?! its
                       // not parameter configurable
    gpio_target_array periph_irq;

    tlm_target_socket in;
    tlm_initiator_socket dma;
    gpio_initiator_array irq;
    gpio_initiator_socket irq_abort;

    pl330(const sc_module_name& nm);
    virtual ~pl330();
    VCML_KIND(dma::pl330);
    virtual void reset() override;

private:
    void pl330_thread();

    sc_event m_dma;
    bool m_execute_debug;
};

} // namespace dma
} // namespace vcml

#endif // VCML_DMA_PL330_H
