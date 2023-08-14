//
// Created by rubenware on 7/13/23.
//

//TODO:
//synch policies for registers
//irq stuff
//mfifo structure
//faulting logic
//check state transitions
//test
//debug instructions

#ifndef VCML_PL330_H
#define VCML_PL330_H

#include "vcml/core/peripheral.h"

#include <deque>
#include <unordered_map>
#include <vector>

namespace vcml {
namespace dma {

template <typename QUEUE_ITEM>
class tagged_multi_queue
{
public:
    tagged_multi_queue(int max_total_items) : max_sum(max_total_items), current_sum(0) {}

    bool push(const QUEUE_ITEM& item) {
        if (current_sum + 1 <= max_sum) {
            queues[item.tag].push_back(item);
            tags.push_back(item.tag);
            current_sum += 1;
            return true;
        }
        return false;
    }

    std::optional<QUEUE_ITEM> pop() {
        if (tags.empty()) {
            return std::nullopt;
        }
        int frontTag = tags.front();
        QUEUE_ITEM frontItem = queues[frontTag].front();
        queues[frontTag].pop_front();
        tags.pop_front();
        current_sum -= 1;
        return std::make_optional(frontItem);
    }

    const QUEUE_ITEM& front() const {
        return queues.at(tags.front()).front();
    }

    std::optional<QUEUE_ITEM> pop(int tag) {
        if (queues[tag].empty()) {
            return std::nullopt;
        }
        QUEUE_ITEM Item = queues[tag].front();
        queues[tag].pop_front();
        tags.erase(std::find(tags.begin(), tags.end(), tag));
        current_sum -= 1;
        return std::make_optional(Item);
    }

    void remove_tagged(int tag) {
        queues[tag].clear();
        tags.erase(std::remove(tags.begin(), tags.end(), tag), tags.end());
    }

    bool empty() const {
        return tags.empty();
    }

    inline bool emptyTag(int tag) const {
        return queues.at(tag).empty();
    }

    inline size_t size() const {
        return tags.size();
    }

    inline size_t num_free() const {
        return max_sum - current_sum;
    }

    inline size_t sizeTag(int tag) const {
        return queues[tag].size();
    }

private:
    std::unordered_map<int, std::deque<QUEUE_ITEM>> queues;
    std::deque<int> tags;
    int max_sum;
    int current_sum;
};

class pl330 : public peripheral
{
public:
    enum pl330_configs : u32 {
        NIRQ = 6, //max = 32 todo tbd
        NPER = 6, //max = 32
        QUEUE_SIZE = 16,
        MFIFO_LINES = 256,//max = 1024
        INSN_MAXSIZE = 6,
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

    struct mfifo_entry {//todo this is wip i am not sure how to model this well in c++
        u8 buf;
        u8 tag;
    };
    tagged_multi_queue<mfifo_entry> mfifo;

    class channel : public module {
    public:
        enum state : u8 {
            Stopped = 0x0,
            Executing = 0x1,
            Cache_miss = 0x2,
            Updating_pc = 0x3,
            Waiting_for_event = 0x4,
            At_barrier = 0x5,             // channel_thread only
            Waiting_for_peripheral = 0x7, // channel_thread only
            Killing = 0x8,                // channel_thread only
            Completing = 0x9,             // channel_thread only
            Faulting_completing = 0xE,    // channel_thread only
            Faulting = 0xF,
        };
        enum fault : u8 {
            undef_instr = 0x0,
            operand_invalid = 0x1,
            ch_evnt_err = 0x5,
            ch_periph_err = 0x6,       // channel_thread only
            ch_rdwr_err = 0x7,         // channel_thread only
            mfifo_err = 0xC,           // channel_thread only
            st_data_unavailable = 0xD, // channel_thread only
            instr_fetch_err = 0x10,
            data_write_err = 0x11, // channel_thread only
            data_read_err = 0x12,  // channel_thread only
            dbg_instr = 0x1E,
            lockup_err = 0x1F, // channel_thread only
        };
        //pl330* parent; //todo see if i need this
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

        inline bool is_state(u8 state) const {return (get_state() == state); }
        inline u32 get_state() const { return csr & 0x7;}
        inline void set_state(u32 new_state) { csr = (csr & ~0x7) | new_state;}

        virtual ~channel() = default;
        VCML_KIND(dma::pl330::channel);
        channel(const sc_module_name& nm, u32 tag);
    };

    class manager : public module {
    public:
        enum state : u8 {
            Stopped = 0x0,
            Executing = 0x1,
            Cache_miss = 0x2,
            Updating_pc = 0x3,
            Waiting_for_event = 0x4,
            Faulting = 0xF,
        };
        enum fault : u8 {
            undef_instr = 0x0,
            operand_invalid = 0x1,
            dmago_err = 0x4,// manager_thread only
            evnt_err = 0x5,
            instr_fetch_err = 0x10,
            dbg_instr = 0x1E,
        };
        reg<u32> dsr;  // DMA Manager Status Register
        reg<u32> dpc;  // DMA Program Counter Register
        reg<u32> fsrd; // Fault Status DMA Manager Register
        reg<u32> ftrd; // Fault Type DMA Manager Register
        bool stall;
        u32 watchdog_timer;

        inline bool is_state(u8 state) const {return (get_state() == state); }
        inline u32 get_state() const { return dsr & 0x7;}
        inline void set_state(u32 new_state) { dsr = (dsr & ~0x7) | new_state;}

        virtual ~manager() = default;
        VCML_KIND(dma::pl330::manager);
        manager(const sc_module_name& nm);
    };

    sc_event m_dma;

    property<u32> num_channels;

    sc_vector<channel> channels;
    manager manager;

    reg<u32> fsrc; //@0x034 RO Fault Status DMA Channel Register // for [7:0] fsrc[N] HIGH = Channel N is faulting

    reg<u32> inten;         //Interrupt Enable Register           // inten[N] HIGH to also send interrupt when event N is happening
    reg<u32> int_event_ris; //Event-Interrupt Raw Status Register // int_event_ris[N] HIGH = event N active
    reg<u32> intmis;        //Interrupt Status Register           // intmis[N] HIGH = interrupt N active
    reg<u32> intclr;        //Interrupt Clear Register            // intclr[N] LOW = nothing, HIGH = clear inq[N] after event, if inten[N] is HIGH

    reg<u32> dbgstatus; // Debug Status Register           // dbgstatus [0], 1=busy, 0=idle
    reg<u32> dbgcmd;    // Debug Command Register          // dbgcmd [1:0] //TODO: all patterns except 0b00 are reserved, do we need this register?!
    reg<u32> dbginst0;  // Debug Instructions-0 Register   // insn byte 2 [31:24] | insn byte 1 [23:16] | ch_nr [10:8] | manager(0)/channel(1) thread [0]
    reg<u32> dbginst1;  // Debug Instructions-1 Register   // upper instruction bytes insn byte 6 | insn byte 5 | insn byte 4 | insn byte 3

    reg<u32> cr0; // Configuration Register 0
    reg<u32> cr1; // Configuration Register 1 //TODO: probably not needed?!
    reg<u32> cr2; // Configuration Register 2 //provides boot_addr
    reg<u32> cr3; // Configuration Register 3 //security state of event-interrupt resources after reset
    reg<u32> cr4; // Configuration Register 4 //security state of peripheral request ifs after reset
    reg<u32> crd; // DMA Configuration Register //TODO: lines in data buffer (mfifo?), depth of read queue, issuing cap of read transactions, depth of write queue, issuing cap of write transactions, data bus width of AXI (amba)
    reg<u32> wd;  // Watchdog Register

    reg<u32, 4> periph_id; // 0xFE0-0xFEC RO Peripheral Identification Registers // reserved[7:1] | integration_cfg[0] || revision [7:4] | designer_1 [3:0] || designer_0 [7:4]  part_number_0 [3:0] || part_number_0 [7:0]
    reg<u32, 4> pcell_id; // 0xFF0-0xFFC RO Component Identification Registers // return 0xB105F00D


    u8 periph_busy[NPER];

    tlm_target_socket in;
    tlm_initiator_socket dma;
    // tlm_initiator_socket irq; // todo irqs need to be modelled (and undertood)


    void pl330_thread();
    int channel_execute_cycle(channel& channel);

    //todo debug functionality

    pl330(const sc_module_name& nm);
    virtual ~pl330();
    VCML_KIND(dma::pl330);
    virtual void reset() override;

private:
    bool execute_debug = false;
};

} // namespace arm
} // namespace vcml

#endif // VCML_PL330_H
