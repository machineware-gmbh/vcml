
#include "vcml/models/dma/pl330.h"
namespace vcml::dma{
using namespace mwr;


//enums + typedef field<offset, size, type>'s

enum dsr_bits {
    DNS = bit(9), // Manager security status
};
typedef field<0,4,u32> DSR_DMA_STATUS;
typedef field<4,5,u32> DSR_WAKEUP_EVENT;

enum fsrd_bits {
    FS_MGR = bit(0), // Manager fault state
};

typedef field<0,8,u32> FSRC_FAULT_STATUS;

enum ftrd_bits {
    FTRD_MASK = pl330::manager::undef_instr |
                pl330::manager::operand_invalid |
                pl330::manager::dmago_err |
                pl330::manager::evnt_err |
                pl330::manager::instr_fetch_err |
                pl330::manager::dbg_instr,
};

enum ftr_bits{
    FTR_MASK = pl330::channel::undef_instr |
               pl330::channel::operand_invalid |
               pl330::channel::ch_evnt_err |
               pl330::channel::ch_periph_err |
               pl330::channel::ch_rdwr_err |
               pl330::channel::mfifo_err |
               pl330::channel::st_data_unavailable |
               pl330::channel::instr_fetch_err |
               pl330::channel::data_write_err |
               pl330::channel::data_read_err |
               pl330::channel::dbg_instr |
               pl330::channel::lockup_err,
};

enum csr_bits {
    DMAWFP_B_NS = bit(14),
    DMAWFP_PERIPH = bit(15),
    CNS = bit(21),
};
typedef field<0,4,u32> CSR_CHANNEL_STATUS;
typedef field<4,5,u32> CSR_WAKEUP_NUMBER;

enum ccr_bits {
    SRC_INC = bit(0),
    DST_INC = bit(14),
};
typedef field<1,3,u32> CCR_SRC_BURST_SIZE;
typedef field<4,4,u32> CCR_SRC_BURST_LEN;
typedef field<8,3,u32> CCR_SRC_PROT_CTRL;
typedef field<11,3,u32> CCR_SRC_CACHE_CTRL;
typedef field<15,3,u32> CCR_DST_BURST_SIZE;
typedef field<18,4,u32> CCR_DST_BURST_LEN;
typedef field<22,3,u32> CCR_DST_PROT_CTRL;
typedef field<25,3,u32> CCR_DST_CACHE_CTRL;
typedef field<28,3,u32> CCR_ENDIAN_SWAP_SIZE;

typedef field<0,8,u32> LCO_LOOP_COUNTER_ITERATIONS;
typedef field<0,8,u32> LC1_LOOP_COUNTER_ITERATIONS;

enum dbgstatus_bits {
    DBGSTATUS = bit(0),
};

enum dbgcmd_bits {
    DBGCMD = bit(0),
};

enum dbginst0_bits {
    DEBUG_THREAD = bit(0),
};
typedef field<8,3,u32> DBGINST0_CHANNEL_NUMBER;
typedef field<16,8,u32> DBGINST0_INSTRUCTION_BYTE0;
typedef field<24,8,u32> DBGINST0_INSTRUCTION_BYTE1;

typedef field<0,8,u32> DBGINST1_INSTRUCTION_BYTE2;
typedef field<8,8,u32> DBGINST1_INSTRUCTION_BYTE3;
typedef field<16,8,u32> DBGINST1_INSTRUCTION_BYTE4;
typedef field<24,8,u32> DBGINST1_INSTRUCTION_BYTE5;

enum cr0_bits {
    PERIPH_REQ = bit(0),
    BOOT_EN = bit(1),
    MGR_NS_AT_RST = bit(2),
};
typedef field<4,3,u32> CR0_NUM_CHNLS;
typedef field<12,5,u32> CR0_NUM_PERIPH_REQ;
typedef field<17,5,u32> CR0_NUM_EVENTS;

typedef field<0,3,u32> CR1_I_CACHE_LEN;
typedef field<4,4,u32> CR1_NUM_I_CACHE_LINES;

typedef field<0,3,u32> CRD_DATA_WIDTH;
typedef field<4,3,u32> CRD_WR_CAP;
typedef field<8,4,u32> CRD_WR_Q_DEP;
typedef field<12,3,u32> CRD_RD_CAP;
typedef field<16,4,u32> CRD_RD_Q_DEP;
typedef field<20,10,u32> CRD_DATA_BUFFER_DEP;

enum wd_bits {
    WD_IRQ_ONLY = bit(0),
};

enum request_type {
    SINGLE = 0b00,
    BURST = 0b01,
};

/*
 * For information about instructions see PL330 Technical Reference Manual.
 *
 * Arguments:
 *   CH - channel executing the instruction
 *   OPCODE - opcode
 *   ARGS - array of 8-bit arguments
 *   LEN - number of elements in ARGS array
 */

inline static void pl330_dmaadxh(pl330::channel* ch, uint8_t* args, bool ra,
                                 bool neg) {
    uint32_t im = (args[1] << 8) | args[0];
    if (neg) {
        im |= 0xffffu << 16;
    }

    if (ra) {
        ch->dar += im;
    } else {
        ch->sar += im;
    }
}

static void pl330_dmaaddh(pl330* dma, pl330::channel* ch, uint8_t opcode,
                          uint8_t* args, int len) {
    pl330_dmaadxh(ch, args, !!(opcode & 0b10), false);
}

static void pl330_dmaadnh(pl330* dma, pl330::channel* ch, uint8_t opcode,
                          uint8_t* args, int len) {
    pl330_dmaadxh(ch, args, !!(opcode & 0b10), true);
}

static void pl330_dmaend(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    if (ch->get_state() == pl330::channel::Executing) {
        /* Wait for all transfers to complete */
        if (! dma->mfifo.emptyTag(ch->tag) || ! dma->read_queue.emptyTag(ch->tag) ||
            ! dma->write_queue.emptyTag(ch->tag) ) {
            ch->stall = 1;
            return;
        }
    }
    // flush fifo, cache and queues
    dma->mfifo.remove_tagged(ch->tag);
    dma->read_queue.remove_tagged(ch->tag);
    dma->write_queue.remove_tagged(ch->tag);
    ch->set_state(pl330::channel::Stopped);
}

static void pl330_mn_dmaend(pl330* dma, pl330::channel* ch, uint8_t opcode,
                            uint8_t* args, int len) {
    dma->manager.set_state(pl330::manager::Stopped);
}

// flush peripheral
static void pl330_dmaflushp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                            uint8_t* args, int len) {
    uint8_t periph_id;

    if (args[0] &
        7) { // bit 8,9,and 10 are also 0 for this instruction opp code
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if ((ch->csr & DMAWFP_B_NS) && !(dma->cr4 & (1 << periph_id))) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    /* Do nothing */ // TODO: it really seems dma peripherals are not
                     // implemented in this model
}

static void pl330_dmago(pl330* dma, pl330::channel* ch, uint8_t opcode,
                        uint8_t* args, int len) {
    uint8_t chan_id;
    uint32_t ns;
    uint32_t pc;

    //    trace_pl330_dmago();
    ns = !!(opcode & 0b10);
    chan_id = args[0] & 7;
    if ((args[0] >> 3)) { // top 5 bit of first byte should be 0 as per the
                          // spec
        //pl330_fault(ch, pl330::manager::operand_invalid);
        return;
    }
    if (chan_id >= dma->cr0.get_field<CR0_NUM_CHNLS>()) {
        //pl330_fault(ch, pl330::manager::operand_invalid); // TODO: should be
                                                      // manager thread abort
        return;
    }
    auto& channel = dma->channels[chan_id];
    pc = (((uint32_t)args[4]) << 24) | (((uint32_t)args[3]) << 16) |
         (((uint32_t)args[2]) << 8) | (((uint32_t)args[1]));
    if (!channel.is_state(pl330::channel::Stopped)) {
        //pl330_fault(dma->manager,pl330::manager::operand_invalid); // TODO: as per the spec if the
                                              // channel is not Stopped DMANOP
                                              // should be performed instead
                                              // (not a fault)
        return;
    }
    if ((dma->manager.dsr & DNS) && !ns) {
        //pl330_fault(dma->manager,pl330::manager::dmago_err); // TODO: should be manager thread abort
        return;
    }
    set_bit<CNS>(channel.csr,ns);
    channel.cpc = pc;
    channel.set_state(pl330::channel::Executing);
}

static void pl330_dmakill(pl330* dma, pl330::channel* ch, uint8_t opcode,
                          uint8_t* args, int len) {
    if (ch->is_state(pl330::channel::Faulting) ||
        ch->is_state(pl330::channel::Faulting_completing)) {
        /* This is the only way for a channel to leave the faulting state */
        ch->ftr &= ~FTR_MASK;
//        dma->num_faulting--;
//        if (dma->num_faulting == 0) {
//            //            trace_pl330_dmakill();
//            qemu_irq_lower(dma->irq_abort); // todo not qemu
//        }
    }
    ch->set_state(pl330::channel::Killing);
    // TODO: according to spec a channel thread should wait for AXI
    // transactions with its TAG as ID to complete here, but it isnt instead it
    // just removes them
    dma->mfifo.remove_tagged(ch->tag);
    dma->read_queue.remove_tagged(ch->tag);
    dma->write_queue.remove_tagged(ch->tag);
    ch->set_state(pl330::channel::Stopped);
}

static void pl330_mn_dmakill(pl330* dma, pl330::channel* ch, uint8_t opcode,
                             uint8_t* args, int len) {
    dma->manager.set_state(pl330::manager::Stopped);
}

static void pl330_dmald(pl330* dma, pl330::channel* ch, uint8_t opcode,
                        uint8_t* args, int len) {
    uint8_t bs = opcode & 0b11;
    uint32_t size, num;
    bool inc;

    if (bs == 0b10) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    // DMALD[S|B] missmatch -> nop case
    if ((bs == 0b01 && ch->request_flag == request_type::BURST) ||
        (bs == 0b11 && ch->request_flag == request_type::SINGLE)) {
        /* Perform NOP */
        return;
    }
    // DMALD case
    if (bs == 1 && ch->request_flag == request_type::SINGLE) {
        num = 1;
    } else {
        num = ch->ccr.get_field<CCR_SRC_BURST_LEN>() + 1;
    }
    size = (uint32_t)1 << ch->ccr.get_field<CCR_SRC_BURST_SIZE>();
    inc = mwr::get_bit(&(ch->ccr), ccr_bits::SRC_INC);
    // TODO comment put into read queue unless stall
    dma->read_queue.push({ch->sar, size, !!num, inc, 0, ch->tag});
    if (!ch->stall) {
        //        trace_pl330_dmald(ch->tag, ch->src, size, num, inc ? 'Y' :
        //        'N');
        ch->sar += inc ? size * num - (ch->sar & (size - 1))
                       : 0; // TODO spec does not mention src_inc for DMALD
                            // only for DMALDP... strange
    }
}

static void pl330_dmaldp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    uint8_t periph_id;

    if (args[0] & 7) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if (mwr::get_bit(&(ch->csr), csr_bits::CNS) &&
        !(dma->cr4 & (1 << periph_id))) {
        //pl330_fault(ch, pl330::channel::ch_periph_err);
        return;
    }
    pl330_dmald(dma, ch, opcode, args, len);
}

static void pl330_dmalp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                        uint8_t* args, int len) {
    uint8_t lc = (opcode & 0b10) >> 1;
    switch (lc) {
    case 0:
        ch->lc0.set_field<LCO_LOOP_COUNTER_ITERATIONS>(args[0]);
        break;
    case 1:
        ch->lc1.set_field<LC1_LOOP_COUNTER_ITERATIONS>(args[0]);
        break;
    }
}

static void pl330_dmalpend(pl330* dma, pl330::channel* ch, uint8_t opcode,
                           uint8_t* args, int len) {
    uint8_t nf = (opcode & 0b1'0000) >> 4;
    uint8_t bs = opcode & 0b11;
    uint8_t lc = (opcode & 0b100) >> 2;

    //    trace_pl330_dmalpend(nf, bs, lc, ch->lc[lc], ch->request_flag);

    if (bs == 0b10) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if ((bs == 0b01 && ch->request_flag == request_type::BURST) ||
        (bs == 0b11 && ch->request_flag == request_type::SINGLE)) {
        /* Perform NOP */
        return;
    }

    if (!nf || lc ? ch->lc1 : ch->lc0) {
        if (nf) {
            lc ? ch->lc1-- : ch->lc0--;
        }
        //        trace_pl330_dmalpiter();
        ch->cpc -= args[0];
        ch->cpc -= len + 1;
        /* "ch->pc -= args[0] + len + 1" is incorrect when args[0] == 256 */
    } else {
        //        trace_pl330_dmalpfallthrough();
    }
}

// no pl330_dmalpfe needed because it does not result in a dma instruction

static void pl330_dmamov(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    uint8_t rd = args[0] & 0b0111;

    if ((args[0] >> 3)) {
        //pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    uint32_t im = (((uint32_t)args[4]) << 24) | (((uint32_t)args[3]) << 16) |
         (((uint32_t)args[2]) << 8)  | (((uint32_t)args[1]));

    switch (rd) {
    case 0b000:
        ch->sar = im;
        break;
    case 0b001:
        ch->ccr = im;
        break;
    case 0b010:
        ch->dar = im;
        break;
    default:
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
}

static void pl330_dmanop(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    /* NOP is NOP. */
}

static void pl330_dmarmb(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    if (! dma->read_queue.emptyTag(ch->tag)) {
        ch->set_state(pl330::channel::At_barrier);
        ch->stall = 1;
        return;
    } else {
        ch->set_state(pl330::channel::Executing);
    }
}

static void pl330_dmasev(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    uint8_t ev_id;

    if (args[0] & 0b0111) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    ev_id = (args[0] >> 3) & 0b0001'1111;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if (mwr::get_bit(&(ch->csr), CNS) && !(dma->cr3 & (1 << ev_id))) {
        //pl330_fault(ch, pl330::channel::ch_evnt_err);
        return;
    }
    if (dma->inten & (1 << ev_id)) {
        dma->intmis |= (1 << ev_id);
        //        trace_pl330_dmasev_evirq(ev_id);
//        qemu_irq_raise(dma->irq[ev_id]); //todo implement interrupts
    }
    //    trace_pl330_dmasev_event(ev_id);
    dma->int_event_ris |= (1 << ev_id);
}

static void pl330_mn_dmasev(pl330* dma, pl330::channel* ch, uint8_t opcode,
                            uint8_t* args, int len) {
    uint8_t ev_id;

    if (args[0] & 0b0111) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    ev_id = (args[0] >> 3) & 0b0001'1111;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if (mwr::get_bit(&(dma->manager.dsr), DNS) && !(dma->cr3 & (1 << ev_id))) {
        //pl330_fault(ch, pl330::channel::ch_evnt_err);
        return;
    }
    if (dma->inten & (1 << ev_id)) {
        dma->intmis |= (1 << ev_id);
        //        trace_pl330_dmasev_evirq(ev_id);
//        qemu_irq_raise(dma->irq[ev_id]); //todo implement interrupts
    }
    //    trace_pl330_dmasev_event(ev_id);
    dma->int_event_ris |= (1 << ev_id);
}

static void pl330_dmast(pl330* dma, pl330::channel* ch, uint8_t opcode,
                        uint8_t* args, int len) {
    uint8_t bs = opcode & 0b11;
    uint32_t size, num;
    bool inc;

    if (bs == 0b10) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if ((bs == 0b01 && ch->request_flag == request_type::BURST) ||
        (bs == 0b11 && ch->request_flag == request_type::SINGLE)) {
        /* Perform NOP */
        return;
    }
    num = ch->ccr.get_field<CCR_DST_BURST_LEN>() + 1;
    size = (uint32_t)1 << ch->ccr.get_field<CCR_DST_BURST_SIZE>();
    inc = mwr::get_bit(&(ch->ccr), ccr_bits::DST_INC);
    ch->stall = dma->write_queue.push({ch->dar, size, !!num,
                                     inc, 0, ch->tag});
    if (!ch->stall) {
        //        trace_pl330_dmast(ch->tag, ch->dar, size, num, inc ? 'Y' :
        //        'N');
        ch->dar += inc ? size * num - (ch->dar & (size - 1)) : 0;
    }
}

static void pl330_dmastp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    uint8_t periph_id;

    if (args[0] & 0b111) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    periph_id = (args[0] >> 3) & 0b0001'1111;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if (mwr::get_bit(&(ch->csr), CNS) && !(dma->cr4 & (1 << periph_id))) {
        //pl330_fault(ch, pl330::channel::ch_periph_err);
        return;
    }
    pl330_dmast(dma, ch, opcode, args, len);
}

static void pl330_dmastz(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    uint32_t num = ch->ccr.get_field<CCR_DST_BURST_LEN>() + 1;
    uint32_t size = (uint32_t)1 << ((ch->ccr >> 15) & 0x7);
    bool inc = !!((ch->ccr >> 14) & 0b1);
    ch->stall = dma->write_queue.push({ch->dar, size, !!num,
                                     inc, 1, ch->tag});
    if (inc) {
        ch->dar += size * num;
    }
}

static void pl330_dmawfe(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    uint8_t ev_id;

    if (args[0] & 0b101) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    ev_id = (args[0] >> 3) & 0x1f;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if (mwr::get_bit(&(ch->csr), CNS) && !(dma->cr3 & (1 << ev_id))) {
        //pl330_fault(ch, pl330::channel::ch_evnt_err);
        return;
    }
    ch->csr.set_field<CSR_WAKEUP_NUMBER>(ev_id);
    ch->set_state(pl330::channel::Waiting_for_event);
    if (~dma->inten & dma->int_event_ris & 1 << ev_id) {
        ch->set_state(pl330::channel::Executing);
        /* If anyone else is currently waiting on the same event, let them
         * clear the ev_status so they pick up event as well
         */
        for (u32 i = 0; i < dma->cr0.get_field<CR0_NUM_CHNLS>(); ++i) {
            pl330::channel* peer = &dma->channels[i];
            if (peer->is_state(pl330::channel::Waiting_for_event) &&
                ch->csr.get_field<CSR_WAKEUP_NUMBER>() == ev_id) {
                return;
            }
        }
        dma->int_event_ris &= ~(1 << ev_id);
        //        trace_pl330_dmawfe(ev_id);
    } else {
        ch->stall = 1;
    }
}

static void pl330_mn_dmawfe(pl330* dma, pl330::channel* ch, uint8_t opcode,
                            uint8_t* args, int len) {
    if (args[0] & 0b101) {
        //pl330_fault(ch, pl330::manager::operand_invalid);
        return;
    }
    u8 ev_id = (args[0] >> 3) & 0x1f;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        //pl330_fault(ch, pl330::manager::operand_invalid);
        return;
    }
    if (mwr::get_bit(&(dma->manager.dsr), DNS) && !(dma->cr3 & (1 << ev_id))) {
        //pl330_fault(ch, pl330::manager::evnt_err);
        return;
    }
    dma->manager.dsr.set_field<DSR_WAKEUP_EVENT>(ev_id);
    dma->manager.set_state(pl330::manager::Waiting_for_event);
    if (~dma->inten & dma->int_event_ris & 1 << ev_id) {
        dma->manager.set_state(pl330::manager::Executing);
        /* If anyone else is currently waiting on the same event, let them
         * clear the ev_status so they pick up event as well
         */
        for (u32 i = 0; i < dma->cr0.get_field<CR0_NUM_CHNLS>(); ++i) {
            pl330::channel* peer = &dma->channels[i];
            if (peer->is_state(pl330::manager::Waiting_for_event) &&
                dma->manager.dsr.get_field<DSR_WAKEUP_EVENT>() == ev_id) {
                return;
            }
        }
        dma->int_event_ris &= ~(1 << ev_id);
        //        trace_pl330_dmawfe(ev_id);
    } else {
        ch->stall = 1;
    }
}

static void pl330_dmawfp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    uint8_t bs = opcode & 0b11;
    uint8_t periph_id;

    if (args[0] & 0b111) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if (mwr::get_bit(&(ch->csr), CNS) && !(dma->cr4 & (1 << periph_id))) {
        //pl330_fault(ch, pl330::channel::ch_periph_err);
        return;
    }
    switch (bs) {
    case 0b00: /* S */
        ch->request_flag = request_type::SINGLE;
        ch->csr.set_bit<DMAWFP_B_NS>(0);
        ch->csr.set_bit<DMAWFP_PERIPH>(0);
        break;
    case 0b01: /* P */
        ch->request_flag = request_type::BURST;
        ch->csr.set_bit<DMAWFP_PERIPH>(1);
        break;
    case 0b10: /* B */
        ch->request_flag = request_type::BURST;
        ch->csr.set_bit<DMAWFP_B_NS>(1);
        ch->csr.set_bit<DMAWFP_PERIPH>(0);
        break;
    default:
        //pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }

    if (dma->periph_busy[periph_id]) { // TODO: how do we wait for a peripheral
                                       // in vcml?!
        ch->set_state(pl330::channel::Waiting_for_peripheral);
        ch->stall = 1;
    } else if (ch->is_state(pl330::channel::Waiting_for_peripheral)) {
        ch->set_state(pl330::channel::Executing);
    }
}

static void pl330_dmawmb(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    if (! dma->write_queue.emptyTag(ch->tag)) {
        ch->set_state(pl330::channel::At_barrier);
        ch->stall = 1;
        return;
    } else {
        ch->set_state(pl330::channel::Executing);
    }
}

struct insn_descr { // TODO: maybe this only lives in the implementation,
                    // comment: I don't like this exec, this is very "C" to me
                    // in a bad way
    u32 opcode;
    u32 opmask;
    u32 size;
    void (*exec)(pl330*, pl330::channel*, u8 opcode, u8* args,
                 int len); // todo: channel* = NULL => manager_thread
};

/* NULL terminated array of the instruction descriptions. */
static const insn_descr ch_insn_descr[] = {
    { .opcode = 0x54, .opmask = 0xFD, .size = 3, .exec = pl330_dmaaddh, }, // Add Halfword
    { .opcode = 0x5c, .opmask = 0xFD, .size = 3, .exec = pl330_dmaadnh, }, // Add Negative Halfword
    { .opcode = 0x00, .opmask = 0xFF, .size = 1, .exec = pl330_dmaend, }, // End
    { .opcode = 0x35, .opmask = 0xFF, .size = 2, .exec = pl330_dmaflushp, }, // Flush and Notify Peripheral
    { .opcode = 0x04, .opmask = 0xFC, .size = 1, .exec = pl330_dmald, }, // Load
    { .opcode = 0x25, .opmask = 0xFD, .size = 2, .exec = pl330_dmaldp, }, // Load and Notify Peripheral
    { .opcode = 0x20, .opmask = 0xFD, .size = 2, .exec = pl330_dmalp, }, // Loop
    /* dmastp  must be before dmalpend in this list, because their maps
     * are overlapping
     */
    { .opcode = 0x29, .opmask = 0xFD, .size = 2, .exec = pl330_dmastp, }, // Store and Notify peripheral
    { .opcode = 0x28, .opmask = 0xE8, .size = 2, .exec = pl330_dmalpend, }, // Loop End
    { .opcode = 0xBC, .opmask = 0xFF, .size = 6, .exec = pl330_dmamov, }, // Move
    { .opcode = 0x18, .opmask = 0xFF, .size = 1, .exec = pl330_dmanop, }, // NOP
    { .opcode = 0x12, .opmask = 0xFF, .size = 1, .exec = pl330_dmarmb, }, // Read Memory Barrier
    { .opcode = 0x34, .opmask = 0xFF, .size = 2, .exec = pl330_dmasev, }, // Send Event
    { .opcode = 0x08, .opmask = 0xFC, .size = 1, .exec = pl330_dmast, }, // Store
    { .opcode = 0x0C, .opmask = 0xFF, .size = 1, .exec = pl330_dmastz, }, // Store Zero
    { .opcode = 0x36, .opmask = 0xFF, .size = 2, .exec = pl330_dmawfe, }, // Wait For Event
    { .opcode = 0x30, .opmask = 0xFC, .size = 2, .exec = pl330_dmawfp, }, // Wait For Peripheral
    { .opcode = 0x13, .opmask = 0xFF, .size = 1, .exec = pl330_dmawmb, }, // Write Memory Barrier
    { .opcode = 0x00, .opmask = 0x00, .size = 0, .exec = NULL, }
};

/* NULL terminated array of the instruction descriptions. */
static const insn_descr mn_insn_descr[] = {
    { .opcode = 0x00, .opmask = 0xFF, .size = 1, .exec = pl330_mn_dmaend, }, // End
    { .opcode = 0xA0, .opmask = 0xFD, .size = 6, .exec = pl330_dmago, }, // Go
    { .opcode = 0x01, .opmask = 0xFF, .size = 1, .exec = pl330_mn_dmakill, }, // Kill
    { .opcode = 0x18, .opmask = 0xFF, .size = 1, .exec = pl330_dmanop, }, // NOP
    { .opcode = 0x34, .opmask = 0xFF, .size = 2, .exec = pl330_mn_dmasev, }, // Send Event
    { .opcode = 0x36, .opmask = 0xFF, .size = 2, .exec = pl330_mn_dmawfe, }, // Wait For Event
    { .opcode = 0x00, .opmask = 0x00, .size = 0, .exec = NULL, }
};

/* Instructions which can be issued via debug registers. */
//todo find out if these are manager or channel instructions (or both?!)
static const insn_descr debug_insn_desc[] = {
    { .opcode = 0xA0, .opmask = 0xFD, .size = 6, .exec = pl330_dmago, },
    { .opcode = 0x01, .opmask = 0xFF, .size = 1, .exec = pl330_dmakill, },
    { .opcode = 0x34, .opmask = 0xFF, .size = 2, .exec = pl330_dmasev, },
    { .opcode = 0x00, .opmask = 0x00, .size = 0, .exec = NULL, }
};

static inline const insn_descr* fetch_ch_insn(pl330* dma, pl330::channel& channel) {
    u8 opcode;
    dma->dma.read(channel.cpc, &opcode, 1);
    for (u32 i = 0; i < ch_insn_descr[i].size; i++) {
        if (ch_insn_descr[i].opcode == (opcode & ch_insn_descr[i].opmask))
            return &(ch_insn_descr[i]);
    }
    return NULL;
}

static inline const insn_descr* fetch_mn_insn(pl330* dma) {
    // get pc & load insn opcode byte

    u8 opcode;
    dma->dma.read(dma->manager.dpc, &opcode, 1);
    // iterate over insn_desc to find correct one
    // return insn
    for (u32 i = 0; i < mn_insn_descr[i].size; i++) {
        if (ch_insn_descr[i].opcode == (opcode & ch_insn_descr[i].opmask))
            return &(ch_insn_descr[i]);
    }
    return NULL;
}


static inline void execute_insn(pl330& dma, pl330::channel& channel, const insn_descr *insn) {
    uint8_t buffer[pl330::INSN_MAXSIZE];
    dma.dma.read(channel.cpc, (void*)buffer, insn->size);
    insn->exec(&dma,&channel,buffer[0],&buffer[1],insn->size-1);
}

void pl330::execute_cycle() {
    manager_execute_cycle(); // todo this is already not cycle accurate because
                             // we do both manager and channel thread in a
                             // single cycle but they actually alternate via
                             // the spec, however qumu just does everything in
                             // one cycle and in the wrong order.

    for (u32 i = 0; i < num_channels; i++) {
        u32 rr_tag = (last_rr_channel + i + 1) % num_channels;
        if (!(channels[rr_tag].is_state(channel::state::Stopped))) {
            channel_execute_cycle(channels[rr_tag]);
            last_rr_channel = channels[rr_tag].tag;
            break;
        }
    }
}

void pl330::manager_execute_cycle() {// check state
    if (!manager.is_state(pl330::manager::Executing) &&
        !manager.is_state(pl330::manager::Waiting_for_event)) {
        return;
    }
    manager.stall = false;
    // do an insn fetch with correct insns array
    const insn_descr* insn = fetch_mn_insn(this);
    if(! insn)
        return;

    uint8_t buffer[pl330::INSN_MAXSIZE];
    dma.read(manager.dpc, (void*)buffer, insn->size);
    insn->exec(this,NULL,buffer[0],&buffer[1],insn->size-1);
    if (! manager.stall) {
            manager.dpc += insn->size;
            manager.watchdog_timer = 0;
            return;
    } else if (manager.is_state(pl330::channel::Executing)) {
            manager.watchdog_timer += 1;
            //todo if wdt is too high fault/abort here
    }
    return;
}

static void channel_execute_one_insn(pl330& dma, pl330::channel& channel) {
    // check state
    if (!channel.is_state(pl330::channel::Executing) &&
        !channel.is_state(pl330::channel::Waiting_for_peripheral) &&
        !channel.is_state(pl330::channel::At_barrier) &&
        !channel.is_state(pl330::channel::Waiting_for_event)) {
        return;
    }
    channel.stall = false;
    const insn_descr* insn = fetch_ch_insn(&dma, channel);
    if(! insn)
        return;

    execute_insn(dma, channel,insn);
    if (! channel.stall) {
            channel.cpc += insn->size;
            channel.watchdog_timer = 0;
            return;
    } else if (channel.is_state(pl330::channel::Executing)) {
            channel.watchdog_timer += 1;
            //todo if wdt is too high fault/abort here
    }
    return;

    //  fetch one instruction and see if it can be placed in queue, stall if
    //  queue was not available if it was placed/executed advance pc?! or
    //  rather do that in the functions?!

    // do actual work (the above just transfer commands)
    // check if axi read/write is available and clear one command form the
    // MFIFO if MFIFO has space move read command (or if not available write
    // command) to it
}

void pl330::channel_execute_cycle(channel& channel){

    //u8 buf[PL330_MAX_BURST_LEN]; maybe this to save on memory allocations

    channel_execute_one_insn(*this, channel);

    //one insn form read queue
    if(! read_queue.empty() && read_queue.front().data_len <= mfifo.num_free()) {//todo i couldnt find a reason for why we only execute the top of the queues during the cycle of the same channel but this is what qemu does
        auto insn = read_queue.front();
        int len = insn.data_len - (insn.data_addr & (insn.data_len -1));//todo not sure what this does but they do this in qemu
        u8 buffer[len];//todo make static maybe
        dma.read(insn.data_addr,(void*)buffer,len);
        if ( mfifo.push({buffer[1],(u8)channel.tag,0,0,0})) {//todo mfifo this is not correct
            if (insn.inc)
              insn.data_addr += len;
            insn.burst_len_counter--;
            if(!insn.burst_len_counter)
              read_queue.pop();
        }

    }
    //one insn from write queue
    if (!write_queue.empty() && mfifo.front().tag == channel.tag) {
        auto insn = write_queue.front();
        int len = insn.data_len - (insn.data_addr & (insn.data_len - 1)); // todo not sure what this does but they do this in qemu
        u8 buffer[insn.data_len]; // todo make static maybe
        if (insn.zero_flag) {
            for (int i = 0; i < len; i++) {
              buffer[i] = 0;
            }
        } else {
            auto tmp_buf = mfifo.pop().value().buf; //todo mfifo this is not correct
            for (int i = 0; i < len; i++) {
              buffer[i] = tmp_buf;
            }
        }
        dma.write(insn.data_addr,(void*)buffer,len);
        if (insn.inc)
            insn.data_len += len;
        insn.burst_len_counter--;
        if(!insn.burst_len_counter) {
            write_queue.pop();
        }
    }
    //big TODO what to do about unaligned reads and writes, it looks to me like qemu is ignoring them, or is that what the weird "len" does? they make the mfifo way more complicated
}

pl330::channel::channel(const sc_module_name& nm, mwr::u32 tag):
    module(nm),
    ftr("ftr", 0x040 + tag * 0x04),
    csr("csr", 0x100 + tag * 0x08),
    cpc("cpc", 0x104 + tag * 0x08),
    sar("sar", 0x400 + tag * 0x20),
    dar("dar", 0x404 + tag * 0x20),
    ccr("ccr", 0x408 + tag * 0x20),
    lc0("lc0", 0x40c + tag * 0x20),
    lc1("lc1", 0x410 + tag * 0x20),
    tag(tag),
    stall(false) {
    ftr.tag = tag;
    ftr.allow_read_only();

    csr.tag = tag;
    csr.allow_read_only();

    cpc.tag = tag;
    cpc.allow_read_only();

    sar.tag = tag;
    sar.allow_read_only();

    dar.tag = tag;
    dar.allow_read_only();

    ccr.tag = tag;
    ccr.allow_read_only();

    lc0.tag = tag;
    lc0.allow_read_only();

    lc1.tag = tag;
    lc1.allow_read_only();
    if (tag > 7)
        VCML_ERROR("Too many channels specified for pl330");
}

pl330::manager::manager(const sc_module_name& nm):
    module(nm),
    dsr("dsr", 0x000),
    dpc("dpc", 0x004),
    fsrd("fsrd", 0x030),
    ftrd("ftrd", 0x038) {
    dsr.allow_read_only();
    dpc.allow_read_only();
    fsrd.allow_read_only();
    ftrd.allow_read_only();
}

void pl330::pl330_thread() {
    execute_cycle();
}

pl330::pl330(const sc_module_name& nm):
    peripheral(nm),
    read_queue(16),
    write_queue(16),
    mfifo(30), //todo mfifo is not correct yet
    num_channels("num_channels", 8),
    channels("channel", num_channels,
             [this](const char* nm, u32 tag) { return new channel(nm, tag); }),
    manager("manager"),
    fsrc("fsrc", 0x034),
    inten("inten", 0x020),
    int_event_ris("int_event_ris", 0x024),
    intmis("intmis", 0x028),
    intclr("intclr", 0x02C),
    dbgstatus("dbgstatus", 0xD00),
    dbgcmd("dbgcmd", 0xD04),
    dbginst0("dbginst0", 0xD08),
    dbginst1("dbginst1", 0xD0C),
    cr0("cr0", 0xE00),
    cr1("cr1", 0xE04),
    cr2("cr2", 0xE08),
    cr3("cr3", 0xE0C),
    cr4("cr4", 0xE10),
    crd("crd", 0xE14),
    wd("wd", 0xE80),
    periph_id("periph_id", 0xFE0),
    pcell_id("pcell_id", 0xFF0), //,{0xB105F00D}),
    in("in"), // todo do i need to configure dmi for this one and "out"?!
    dma("dma") {
    fsrc.allow_read_only();
    inten.allow_read_write();
    int_event_ris.allow_read_only();
    intmis.allow_read_only();
    intclr.allow_write_only();
    dbgstatus.allow_read_only();
    dbgcmd.allow_write_only();
    dbginst0.allow_write_only();
    dbginst1.allow_read_only();
    cr0.allow_read_only();
    cr1.allow_read_only();
    cr2.allow_read_only();
    cr3.allow_read_only();
    cr4.allow_read_only();
    crd.allow_read_only();
    wd.allow_read_write();
    periph_id.allow_read_only();
    pcell_id.allow_read_only();

    SC_HAS_PROCESS(pl330);
    SC_THREAD(pl330_thread);
}

} // namespace vcml::dma
