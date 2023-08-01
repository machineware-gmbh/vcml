//
// Created by rubenware on 7/13/23.
//

#ifndef VCML_PL330_H
#define VCML_PL330_H

#include "vcml/core/peripheral.h"

#include <deque>
#include <unordered_map>
#include <vector>

namespace vcml {
namespace dma {

template <typename QueueItem>
class TaggedMultiDequeue {
public:
    TaggedMultiDequeue(int maxSum) : max_sum(maxSum), current_sum(0) {}

    void enqueue(const QueueItem& item) {
        if (current_sum + itemSize <= max_sum) {
            queues[item.tag].push_back(item);
            tags.push_back(item.tag);
            current_sum += itemSize;
        } else {
            // Handle exceeding sum (e.g., reject or dequeue items with the lowest priority)
            while (!tags.empty() && current_sum + itemSize > max_sum) {
                int frontTag = tags.front();
                QueueItem frontItem = queues[frontTag].front();
                queues[frontTag].pop_front();
                tags.pop_front();
                current_sum -= itemSize;
            }
            if (current_sum + itemSize <= max_sum) {
                queues[item.tag].push_back(item);
                tags.push_back(item.tag);
                current_sum += itemSize;
            } else {
                // Handle further cases of exceeding sum (e.g., throw an exception)
            }
        }
    }

    void push(const QueueItem& item) {
        if (current_sum + itemSize <= max_sum) {
            queues[item.tag].push_back(item);
            tags.push_front(item.tag);
            current_sum += itemSize;
        } else {
            // Handle exceeding sum (e.g., reject or pop items with the lowest priority)
            while (!tags.empty() && current_sum + itemSize > max_sum) {
                int backTag = tags.back();
                QueueItem backItem = queues[backTag].back();
                queues[backTag].pop_back();
                tags.pop_back();
                current_sum -= itemSize;
            }
            if (current_sum + itemSize <= max_sum) {
                queues[item.tag].push_back(item);
                tags.push_front(item.tag);
                current_sum += itemSize;
            } else {
                // Handle further cases of exceeding sum (e.g., throw an exception)
            }
        }
    }

    QueueItem dequeue() {
        if (tags.empty()) {
            // Handle empty dequeue (e.g., throw an exception)
        }
        int frontTag = tags.front();
        QueueItem frontItem = queues[frontTag].front();
        queues[frontTag].pop_front();
        tags.pop_front();
        current_sum -= itemSize;
        return frontItem;
    }

    QueueItem pop() {
        if (tags.empty()) {
            // Handle empty pop (e.g., throw an exception)
        }
        int backTag = tags.back();
        QueueItem backItem = queues[backTag].back();
        queues[backTag].pop_back();
        tags.pop_back();
        current_sum -= itemSize;
        return backItem;
    }

    bool empty() const {
        return tags.empty();
    }

    bool emptyTag(int tag) const {
        return queues[tag].empty();
    }

    size_t size() const {
        return tags.size();
    }

    size_t sizeTag(int tag) const {
        return queues[tag].size();
    }

private:
    std::unordered_map<int, std::deque<QueueItem>> queues;
    std::deque<int> tags;
    int max_sum;
    int current_sum;
    static constexpr int itemSize = sizeof(QueueItem); // You may adjust this based on your actual data size
};

class pl330 : public peripheral
{
    enum pl330_configs : u32 {
        NIRQ = 999, //todo tbd
        NPER = 999,
        QUEUE_SIZE = 16,

    };
public:
    struct queue_entry {
        u32 insn_addr;
        u32 insn_len;
        u8 tag;
        u8 seqn;
    };
    TaggedMultiDequeue<queue_entry> read_queue, write_queue;

    class MFIFO
    {
    private:
        u8* buf;
        u8* tag;
        u32 head;
        u32 num;
        u32 size;

    public:
        // todo mfifo public api
    } MFIO; // TODO: what data structure (in qemu this is a ringbuffer)



public:

    class channel : public module { //TODO should be sc_module for name hirarchy of regs but peripheral/component/module/sc_module which one?
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
        reg<u32> ftr; // channel fault type register  0x040 + tag * 0x04 // see fault
        reg<u32> csr; // channel status register      0x100 + tag * 0x08 // S/NS [21] | dmawfp_periph [15] | dmawfp_b_ns [14] | wfp wakeup number [8:4] | status [3:0]
        reg<u32> cpc; // channel pc register          0x104 + tag * 0x08
        reg<u32> sar; // source address register      0x400 + tag * 0x20
        reg<u32> dar; // destination address register 0x404 + tag * 0x20
        // ccr:  endian_swap_siqe [27:25] | dst_cache_ctrl [27:15] | dst_prot_ctr [14:22] | dst_burst_len [21:18] | dst_burst_size [17:15] | dst_inc [14] | src_cache_ctrl [13:11] | src_prot_ctrl [10:8] | src_burst_len [7:4] | src_burst_size [3:1] | src_inc [0]
        reg<u32> ccr; // channel control register     0x408 + tag * 0x20
        reg<u32> lc0; // loop counter 0 register      0x40c + tag * 0x20 //iterations [7:0]
        reg<u32> lc1; // loop counter 1 register      0x410 + tag * 0x20 //iterations [7:0]
        u32 tag;      // aka channel id
        bool stall;
        u32 request_flag;

        inline bool is_state(u8 state) const {return (get_state() == state); }
        inline u32 get_state() const { return csr & 0x7;}
        inline void set_state(u32 new_state) { csr = (csr & ~0x7) | new_state;}
        VCML_KIND(dma::pl330::channel);

        channel(const sc_module_name& nm, u32 tag); //todo from the tag we can derive the base adress for all the registers
    };

    class manager : public module { //TODO should MAYBE be sc_module for name hirarchy of regs
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
        reg<u32> dsr;  // DMA Manager Status Register          // S/NS [9] | Wakeup_event for dmawfe [8:4] | DMA status [3:0]
        reg<u32> dpc;  // DMA Program Counter Register
        reg<u32> fsrd; // Fault Status DMA Manager Register    // fsrd[1] HIGH = manager is faulting
        reg<u32> ftrd; // Fault Type DMA Manager Register      // see faults
        bool stall;

        inline bool is_state(u8 state) const {return (get_state() == state); }
        inline u32 get_state() const { return dsr & 0x7;}
        inline void set_state(u32 new_state) { dsr = (dsr & ~0x7) | new_state;}

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

    reg<u8, 4> periph_id; // 0xFE0-0xFEC RO Peripheral Identification Registers // reserved[7:1] | integration_cfg[0] || revision [7:4] | designer_1 [3:0] || designer_0 [7:4]  part_number_0 [3:0] || part_number_0 [7:0]
    reg<u8, 4> pcell_id; // 0xFF0-0xFFC RO Component Identification Registers // return 0xB105F00D

    tlm_target_socket in;
    tlm_initiator_socket dma;
    tlm_initiator_socket_array<NIRQ> irq;

    void pl330_thread();
    void execute_cycle();
    void channel_execute_cycle(channel& channel);
    void manager_execute_cycle();

    //todo debug functionality

    void reset();
    pl330(const sc_module_name& nm);

private:
    u32 last_rr_channel = 0;
};

} // namespace arm
} // namespace vcml

#endif // VCML_PL330_H
