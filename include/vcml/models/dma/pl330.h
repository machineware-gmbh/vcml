//
// Created by rubenware on 7/13/23.
//

#ifndef VCML_PL330_H
#define VCML_PL330_H

#include "vcml/core/peripheral.h"

namespace vcml {
namespace arm {

class pl330 : public peripheral
{
    enum pl330_configs : u32 {
    }; // TODO: probably should be modelled as properties or sth
public:
    struct queue_entry {
        u32 insn_addr;
        u32 insn_len;
        u8 ch_tag;
        u8 seqn;
    } qeueu_entry;
    class multi_queue
    {
    private:
        // TODO: maybe a pointer to the pl330
        queue_entry* entries;
        u32 queue_size;

    public:
        // todo multi_queue public api
    } read_queue, write_queue; // TODO: how to implement a queue in vcml (in
                               // qemu these are custom arrays of sorts)

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

    struct channel : module { //TODO should be sc_module for name hirarchy of regs but peripheral/component/module/sc_module which one?
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
        } state; // TODO: these are is channel status register [3:0] bit patterns, it should probably only exist once
        enum fault : u8 {
            undef_instr = 0x0,
            operand_invalid = 0x1,
            dmago_err = 0x4,
            evnt_err = 0x5,
            ch_periph_err = 0x6,       // channel_thread only
            ch_rdwr_err = 0x7,         // channel_thread only
            mfifo_err = 0xC,           // channel_thread only
            st_data_unavailable = 0xD, // channel_thread only
            instr_fetch_err = 0x10,
            data_write_err = 0x11, // channel_thread only
            data_read_err = 0x12,  // channel_thread only
            dbg_instr = 0x1E,
            lockup_err = 0x1F, // channel_thread only
        };                     // TODO: these are flags in ftrX
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
        channel(const sc_module_name& nm, u32 tag); //todo from the tag we can derive the base adress for all the registers
    };

    struct manager : module { //TODO should MAYBE be sc_module for name hirarchy of regs
        enum state : u8 {
            Stopped = 0x0,
            Executing = 0x1,
            Cache_miss = 0x2,
            Updating_pc = 0x3,
            Waiting_for_event = 0x4,
            Faulting = 0xF,
        } state; // TODO: these are is channel status register [3:0] bit patterns, it should probably only exist once
        enum fault : u8 {
            undef_instr = 0x0,
            operand_invalid = 0x1,
            dmago_err = 0x4,
            evnt_err = 0x5,
            instr_fetch_err = 0x10,
            dbg_instr = 0x1E,
        };                     // TODO: these are flags in ftrd
        reg<u32> dsr;  //@0x000 RO DMA Manager Status Register          // S/NS [9] | Wakeup_event for dmawfe [8:4] | DMA status [3:0]
        reg<u32> dpc;  //@0x004 RO DMA Program Counter Register
        reg<u32> fsrd; //@0x030 RO Fault Status DMA Manager Register    // fsrd[1] HIGH = manager is faulting
        reg<u32> ftrd; //@0x038 RO Fault Type DMA Manager Register      // see faults
        bool stall;
        manager(const sc_module_name& nm);
    };

    sc_vector<channel> channels;
    manager manager;

    reg<u32> fsrc; //@0x034 RO Fault Status DMA Channel Register // for [7:0] fsrc[N] HIGH = Channel N is faulting

    reg<u32> inten;         //@0x020 RW Interrupt Enable Register           // inten[N] HIGH to also send interrupt when event N is happening
    reg<u32> int_event_ris; //@0x024 RO Event-Interrupt Raw Status Register // int_event_ris[N] HIGH = event N active
    reg<u32> intmis;        //@0x028 RO Interrupt Status Register           // intmis[N] HIGH = interrupt N active
    reg<u32> intclr;        //@0x02C WO Interrupt Clear Register            // intclr[N] LOW = nothing, HIGH = clear inq[N] after event, if inten[N] is HIGH

    reg<u32> dbgstatus; //@0xD00 RO Debug Status Register           // dbgstatus [0], 1=busy, 0=idle
    reg<u32> dbgcmd;    //@0xD04 WO Debug Command Register          // dbgcmd [1:0] //TODO: all patterns except 0b00 are reserved, do we need this register?!
    reg<u32> dbginst0;  //@0xD08 WO Debug Instructions-0 Register   // insn byte 2 [31:24] | insn byte 1 [23:16] | ch_nr [10:8] | manager(0)/channel(1) thread [0]
    reg<u32> dbginst1;  //@0xD0C WO Debug Instructions-1 Register   // upper instruction bytes insn byte 6 | insn byte 5 | insn byte 4 | insn byte 3

    reg<u32> cr0; //@0xE00 RO Configuration Register 0 //TODO: info abt nr channels, nr peripheral request if, nr irqs etc
    reg<u32> cr1; //@0xE04 RO Configuration Register 1 //TODO: info about instruction cache probably not needed
    reg<u32> cr2; //@0xE08 RO Configuration Register 2 //TODO: provides boot_addr
    reg<u32> cr3; //@0xE0C RO Configuration Register 3 //TODO: security state of event-iterrupt resouxces after reset
    reg<u32> cr4; //@0xE10 RO Configuration Register 4 //TODO: security state of peripheral request ifs after reset
    reg<u32> crd; //@0xE14 RO DMA Configuration Register //TODO: lines in data buffer (mfifo?), depth of read queue, issuing cap of read transactions, depth of write queue, issuing cap of write transactions, data bus width of AXI (amba)
    reg<u32> wd;  //@0xE80 RW Watchdog Register

    reg<u8, 4> periph_id; // 0xFE0-0xFEC RO Peripheral Identification Registers // reserved[7:1] | integration_cfg[0] || revision [7:4] | designer_1 [3:0] || designer_0 [7:4]  part_number_0 [3:0] || part_number_0 [7:0]
    reg<u8, 4> pcell_id; // 0xFF0-0xFFC RO Component Identification Registers // return 0xB105F00D

    tlm_target_socket in;
    tlm_initiator_socket out;

    void execute_cycle(); //todo: sc_thread
    void channel_execute_cycle();
    void manager_execute_cycle();
    void reset();
    pl330(const sc_module_name& nm);
};

} // namespace arm
} // namespace vcml

#endif // VCML_PL330_H
