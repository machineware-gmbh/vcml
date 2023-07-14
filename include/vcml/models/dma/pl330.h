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
    enum pl330_configs :u32 {};
public:

    struct queue_entry {} qeueu_entry;
    struct queue {} read_queue, write_queue;//TODO: how to implement a queue in vcml (in qemu these are custom arrays of sorts)
    struct fifo_entry{} fifo_entry;//TODO: what data structure (in qemu this is a ringbuffer)
    struct MFIFO{} MFIO;

    enum pl330_thread_state{
        Executing,
        Cache_miss,
        Updating_pc,
        Waiting_for_event,
        At_barrier, //channel_thread only
        Waiting_for_peripheral, //channel_thread only
        Completing, //channel_thread only
        Killing, //channel_thread only
        Stopped,
        Faulting,
        Faulting_completing, //channel_thread only
    };
    struct pl330_channel{
        u32 src_addr; //maybe this should be a reg (see regs below)
        u32 dest_addr; //maybe this should be a reg (see regs below)
        u32 pc_legacy; //maybe this should be a reg (see regs below)
        reg<u32> ftr; //channel fault type register
        reg<u32> csr; //channel status register
        reg<u32> cpc; //channel pc register
        reg<u32> sar; //source address register
        reg<u32> dar; //destination address register
        reg<u32> ccr; //channel control register
        reg<u32> lc0; //loop counter 0 register
        reg<u32> lc1; //loop counter 1 register
        u32 tag; // aka channel id
        pl330_thread_state state;
        bool is_manager;
    } channels[8], manager;

    struct insn_descr{
        u32 opcode;
        u32 opmask;
        u32 size;
        void (*exec)(pl330_channel *, uint8_t opcode, uint8_t *args, int len);;
    };
public:
    reg<u32> dsr; //@0x000 RO DMA Manager Status Register
    reg<u32> dpc; //@0x004 RO DMA Program Counter Register
    reg<u32> inten; //@0x020 RW Interrupt Enable Register
    reg<u32> int_event_ris; //@0x024 RO Event-Interrupt Raw Status Register
    reg<u32> intmis; //@0x028 RO Interrupt Status Register
    reg<u32> intclr; //@0x02C WO Interrupt Clear Register
    reg<u32> fsrd; //@0x030 RO Fault Status DMA Manager Register
    reg<u32> fsrc; //@0x034 RO Fault Status DMA Channel Register
    reg<u32> ftrd; //@0x038 RO Fault Type DMA Manager Register

    struct {
        reg<u32> ftr0; //@0x040 RO Fault type for DMA channel 0
        reg<u32> ftr1; //@0x044 RO Fault type for DMA channel 1
        reg<u32> ftr2; //@0x048 RO Fault type for DMA channel 2
        reg<u32> ftr3; //@0x04C RO Fault type for DMA channel 3
        reg<u32> ftr4; //@0x050 RO Fault type for DMA channel 4
        reg<u32> ftr5; //@0x054 RO Fault type for DMA channel 5
        reg<u32> ftr6; //@0x058 RO Fault type for DMA channel 6
        reg<u32> ftr7; //@0x05C RO Fault type for DMA channel 7

        reg<u32> csr0; //@0x100 RO Channel status for DMA channel 0
        reg<u32> cpc0; //@0x104 RO Channel PC for DMA channel 0
        reg<u32> csr1; //@0x108 RO Channel status for DMA channel 1
        reg<u32> cpc1; //@0x10C RO Channel PC for DMA channel 1
        reg<u32> csr2; //@0x110 RO Channel status for DMA channel 2
        reg<u32> cpc2; //@0x114 RO Channel PC for DMA channel 2
        reg<u32> csr3; //@0x118 RO Channel status for DMA channel 3
        reg<u32> cpc3; //@0x11C RO Channel PC for DMA channel 3
        reg<u32> csr4; //@0x120 RO Channel status for DMA channel 4
        reg<u32> cpc4; //@0x124 RO Channel PC for DMA channel 4
        reg<u32> csr5; //@0x128 RO Channel status for DMA channel 5
        reg<u32> cpc5; //@0x12C RO Channel PC for DMA channel 5
        reg<u32> csr6; //@0x130 RO Channel status for DMA channel 6
        reg<u32> cpc6; //@0x134 RO Channel PC for DMA channel 6
        reg<u32> csr7; //@0x138 RO Channel status for DMA channel 7
        reg<u32> cpc7; //@0x13C RO Channel PC for DMA channel 7

        reg<u32> sar0;  //@0x400 RO Source address for DMA channel 0
        reg<u32> dar0;  //@0x404 RO Destination address for DMA channel 0
        reg<u32> ccr0;  //@0x408 RO Channel control for DMA channel 0
        reg<u32> lc0_0; //@0x40C RO Loop counter 0 for DMA channel 0
        reg<u32> lc1_0; //@0x410 RO Loop counter 1 for DMA channel 0

        reg<u32> sar1;  //@0x420 RO Source address for DMA channel 1
        reg<u32> dar1;  //@0x424 RO Destination address for DMA channel 1
        reg<u32> ccr1;  //@0x428 RO Channel control for DMA channel 1
        reg<u32> lc0_1; //@0x42C RO Loop counter 0 for DMA channel 1
        reg<u32> lc1_1; //@0x430 RO Loop counter 2 for DMA channel 1

        reg<u32> sar2;  //@0x440 RO Source address for DMA channel 2
        reg<u32> dar2;  //@0x444 RO Destination address for DMA channel 2
        reg<u32> ccr2;  //@0x448 RO Channel control for DMA channel 2
        reg<u32> lc0_2; //@0x44C RO Loop counter 0 for DMA channel 2
        reg<u32> lc1_2; //@0x450 RO Loop counter 3 for DMA channel 2

        reg<u32> sar3;  //@0x460 RO Source address for DMA channel 3
        reg<u32> dar3;  //@0x464 RO Destination address for DMA channel 3
        reg<u32> ccr3;  //@0x468 RO Channel control for DMA channel 3
        reg<u32> lc0_3; //@0x46C RO Loop counter 0 for DMA channel 3
        reg<u32> lc1_3; //@0x470 RO Loop counter 4 for DMA channel 3

        reg<u32> sar4;  //@0x480 RO Source address for DMA channel 4
        reg<u32> dar4;  //@0x484 RO Destination address for DMA channel 4
        reg<u32> ccr4;  //@0x488 RO Channel control for DMA channel 4
        reg<u32> lc0_4; //@0x48C RO Loop counter 0 for DMA channel 4
        reg<u32> lc1_4; //@0x490 RO Loop counter 5 for DMA channel 4

        reg<u32> sar5;  //@0x4A0 RO Source address for DMA channel 5
        reg<u32> dar5;  //@0x4A4 RO Destination address for DMA channel 5
        reg<u32> ccr5;  //@0x4A8 RO Channel control for DMA channel 5
        reg<u32> lc0_5; //@0x4AC RO Loop counter 0 for DMA channel 5
        reg<u32> lc1_5; //@0x4B0 RO Loop counter 6 for DMA channel 5

        reg<u32> sar6;  //@0x4C0 RO Source address for DMA channel 6
        reg<u32> dar6;  //@0x4C4 RO Destination address for DMA channel 6
        reg<u32> ccr6;  //@0x4C8 RO Channel control for DMA channel 6
        reg<u32> lc0_6; //@0x4CC RO Loop counter 0 for DMA channel 6
        reg<u32> lc1_6; //@0x4D0 RO Loop counter 7 for DMA channel 6

        reg<u32> sar7;  //@0x4E0 RO Source address for DMA channel 7
        reg<u32> dar7;  //@0x4E4 RO Destination address for DMA channel 7
        reg<u32> ccr7;  //@0x4E8 RO Channel control for DMA channel 7
        reg<u32> lc0_7; //@0x4EC RO Loop counter 0 for DMA channel 7
        reg<u32> lc1_7; //@0x4F0 RO Loop counter 8 for DMA channel 7
    };

    reg<u32> dbgstatus; //@0xD00 RO Debug Status Register
    reg<u32> dbgcmd; //@0xD04 WO Debug Command Register
    reg<u32> dbginst0; //@0xD08 WO Debug Instructions-0 Register
    reg<u32> dbginst1; //@0xD0C WO Debug Instructions-1 Register

    reg<u32> cr0; //@0xE00 RO Configuration Register 0
    reg<u32> cr1; //@0xE04 RO Configuration Register 1
    reg<u32> cr2; //@0xE08 RO Configuration Register 2
    reg<u32> cr3; //@0xE0C RO Configuration Register 3
    reg<u32> cr4; //@0xE10 RO Configuration Register 4
    reg<u32> crd; //@0xE14 RO DMA Configuration Register
    reg<u32> wd; //@0xE80 RW Watchdog Register

    reg<u32,4> periph_id_n; //0xFE0-0xFEC RO Peripheral Identification Registers
    reg<u32,4> pcell_id_n; //0xFF0-0xFFC RO Component Identification Registers // return 0xB105F00D

    void channel_execute_cycle();
    void pl330_execute_cycle();
    void reset();
    pl330();

};

} // namespace arm
} // namespace vcml

#endif // VCML_PL330_H
