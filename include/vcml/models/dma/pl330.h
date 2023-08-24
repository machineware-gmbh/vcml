//
// Created by rubenware on 7/13/23.
//

// TODO:
// irq stuff
// faulting logic
// check state transitions
// test

#ifndef VCML_PL330_H
#define VCML_PL330_H

#include <vcml.h>

#include <deque>
#include <unordered_map>
#include <vector>

namespace vcml {
namespace dma {

template <typename QUEUE_ITEM>
class tagged_multi_queue
{
public:
    tagged_multi_queue(int max_total_items):
        m_max_sum(max_total_items), m_current_sum(0) {}

    bool push(const QUEUE_ITEM& item) {
        if (m_current_sum + 1 <= m_max_sum) {
            m_queues[item.tag].push_back(item);
            m_tags.push_back(item.tag);
            m_current_sum += 1;
            return true;
        }
        return false;
    }

    std::optional<QUEUE_ITEM> pop() {
        if (m_tags.empty()) {
            return std::nullopt;
        }
        int front_tag = m_tags.front();
        QUEUE_ITEM front_item = m_queues[front_tag].front();
        m_queues[front_tag].pop_front();
        m_tags.pop_front();
        m_current_sum -= 1;
        return std::make_optional(front_item);
    }

    const QUEUE_ITEM& front() const {
        return m_queues.at(m_tags.front()).front();
    }
    QUEUE_ITEM& front_mut() { return m_queues.at(m_tags.front()).front(); }

    std::optional<QUEUE_ITEM> pop(int tag) {
        if (m_queues[tag].empty()) {
            return std::nullopt;
        }
        QUEUE_ITEM item = m_queues[tag].front();
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
    inline void remove_tagged(int tag) { clear(tag); }

    void clear() {
        m_queues
            .clear(); // assumes QUEUE_ITEM does not contain owning pointers
        m_tags.clear();
    }
    inline void reset() { clear(); }

    inline bool empty() const { return m_tags.empty(); }

    inline bool empty(int tag) const { return m_queues.at(tag).empty(); }

    inline size_t size() const { return m_tags.size(); }

    inline size_t num_free() const { return m_max_sum - m_current_sum; }

    inline size_t size_tag(int tag) const { return m_queues[tag].size(); }

private:
    std::unordered_map<int, std::deque<QUEUE_ITEM>> m_queues;
    std::deque<int> m_tags;
    int m_max_sum;
    int m_current_sum;
};

class pl330 : public peripheral
{
public:
    enum pl330_configs : u32 {
        NIRQ = 6, // max = 32 todo tbd
        NPER = 6, // max = 32
        QUEUE_SIZE = 16,
        MFIFO_LINES = 256, // max = 1024
        INSN_MAXSIZE = 6,
        WD_TIMEOUT = 1024,
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
        u32 seqn;
    };
    tagged_multi_queue<queue_entry> read_queue, write_queue;

    struct mfifo_entry { // todo this is wip i am not sure how to model this
                         // well in c++
        u8 buf;
        u8 tag;
    };
    tagged_multi_queue<mfifo_entry> mfifo;

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
            MFIFO_ERR = 0xC,
            ST_DATA_UNAVAILABLE = 0xD,
            INSTR_FETCH_ERR = 0x10,
            DATA_WRITE_ERR = 0x11,
            DATA_READ_ERR = 0x12,
            DBG_INSTR = 0x1E,
            LOCKUP_ERR = 0x1F,
        };
        // pl330* parent; //todo see if i need this
        reg<u32> ftr; // channel fault type register
        reg<u32> csr; // channel status register
        reg<u32> cpc; // channel pc register
        reg<u32> sar; // source address register
        reg<u32> dar; // destination address register
        reg<u32> ccr; // channel control register
        reg<u32> lc0; // loop counter 0 register
        reg<u32> lc1; // loop counter 1 register
        u32 tag;      // aka channel id
        bool stall;
        u32 request_flag;
        u32 watchdog_timer;

        inline bool is_state(u8 state) const { return (get_state() == state); }
        inline u32 get_state() const { return csr & 0x7; }
        inline void set_state(u32 new_state) {
            csr = (csr & ~0x7) | new_state;
        }

        virtual ~channel() = default;
        VCML_KIND(dma::pl330::channel);
        channel(const sc_module_name& nm, u32 tag);
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
            DBG_INSTR = 0x1E,
        };
        reg<u32> dsr;  // DMA Manager Status Register
        reg<u32> dpc;  // DMA Program Counter Register
        reg<u32> fsrd; // Fault Status DMA Manager Register
        reg<u32> ftrd; // Fault Type DMA Manager Register
        bool stall;
        u32 watchdog_timer;

        inline bool is_state(u8 state) const { return (get_state() == state); }
        inline u32 get_state() const { return dsr & 0x7; }
        inline void set_state(u32 new_state) {
            dsr = (dsr & ~0x7) | new_state;
        }

        virtual ~manager() = default;
        VCML_KIND(dma::pl330::manager);
        manager(const sc_module_name& nm);
    };

    sc_event m_dma;

    property<u32> num_channels;

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
    reg<u32> cr4; // Configuration Register 4
    reg<u32> crd; // DMA Configuration Register
    reg<u32> wd;  // Watchdog Register

    reg<u32, 4> periph_id; // Peripheral Identification Registers
    reg<u32, 4> pcell_id;  // Component Identification Registers

    u8 periph_busy[NPER];

    tlm_target_socket in;
    tlm_initiator_socket dma;
    gpio_initiator_array irq;
    gpio_initiator_socket irq_abort;

    void pl330_thread();

    pl330(const sc_module_name& nm);
    virtual ~pl330();
    VCML_KIND(dma::pl330);
    virtual void reset() override;

private:
    bool m_execute_debug = false;
};

} // namespace dma
} // namespace vcml

#endif // VCML_PL330_H
