
#include "vcml/models/dma/pl330.h"
namespace vcml::dma {
using namespace mwr;

enum dsr_bits {
    DNS = bit(9), // Manager security status
};
typedef field<0, 4, u32> DSR_DMA_STATUS;
typedef field<4, 5, u32> DSR_WAKEUP_EVENT;

enum fsrd_bits {
    FS_MGR = bit(0), // Manager fault state
};

typedef field<0, 8, u32> FSRC_FAULT_STATUS;

enum ftrd_bits {
    FTRD_MASK = pl330::manager::UNDEF_INSTR | pl330::manager::OPERAND_INVALID |
                pl330::manager::DMAGO_ERR | pl330::manager::EVNT_ERR |
                pl330::manager::INSTR_FETCH_ERR | pl330::manager::DBG_INSTR,
};

enum ftr_bits {
    FTR_MASK = pl330::channel::UNDEF_INSTR | pl330::channel::OPERAND_INVALID |
               pl330::channel::CH_EVNT_ERR | pl330::channel::CH_PERIPH_ERR |
               pl330::channel::CH_RDWR_ERR | pl330::channel::MFIFO_ERR |
               pl330::channel::ST_DATA_UNAVAILABLE |
               pl330::channel::INSTR_FETCH_ERR |
               pl330::channel::DATA_WRITE_ERR | pl330::channel::DATA_READ_ERR |
               pl330::channel::DBG_INSTR | pl330::channel::LOCKUP_ERR,
};

enum csr_bits {
    DMAWFP_B_NS = bit(14),
    DMAWFP_PERIPH = bit(15),
    CNS = bit(21),
};
typedef field<0, 4, u32> CSR_CHANNEL_STATUS;
typedef field<4, 5, u32> CSR_WAKEUP_NUMBER;

enum ccr_bits {
    SRC_INC = bit(0),
    DST_INC = bit(14),
};
typedef field<1, 3, u32> CCR_SRC_BURST_SIZE;
typedef field<4, 4, u32> CCR_SRC_BURST_LEN;
typedef field<8, 3, u32> CCR_SRC_PROT_CTRL;
typedef field<11, 3, u32> CCR_SRC_CACHE_CTRL;
typedef field<15, 3, u32> CCR_DST_BURST_SIZE;
typedef field<18, 4, u32> CCR_DST_BURST_LEN;
typedef field<22, 3, u32> CCR_DST_PROT_CTRL;
typedef field<25, 3, u32> CCR_DST_CACHE_CTRL;
typedef field<28, 3, u32> CCR_ENDIAN_SWAP_SIZE;

typedef field<0, 8, u32> LCO_LOOP_COUNTER_ITERATIONS;
typedef field<0, 8, u32> LC1_LOOP_COUNTER_ITERATIONS;

enum dbgstatus_bits {
    DBGSTATUS = bit(0),
};

enum dbgcmd_bits {
    DBGCMD = bit(0),
};

enum dbginst0_bits {
    DEBUG_THREAD = bit(0),
};
typedef field<8, 3, u32> DBGINST0_CHANNEL_NUMBER;
typedef field<16, 8, u32> DBGINST0_INSTRUCTION_BYTE0;
typedef field<24, 8, u32> DBGINST0_INSTRUCTION_BYTE1;

typedef field<0, 8, u32> DBGINST1_INSTRUCTION_BYTE2;
typedef field<8, 8, u32> DBGINST1_INSTRUCTION_BYTE3;
typedef field<16, 8, u32> DBGINST1_INSTRUCTION_BYTE4;
typedef field<24, 8, u32> DBGINST1_INSTRUCTION_BYTE5;

enum cr0_bits {
    PERIPH_REQ = bit(0),
    BOOT_EN = bit(1),
    MGR_NS_AT_RST = bit(2),
};
typedef field<4, 3, u32> CR0_NUM_CHNLS;
typedef field<12, 5, u32> CR0_NUM_PERIPH_REQ; // Number of peripheral requests
typedef field<17, 5, u32> CR0_NUM_EVENTS;     // Number of interrupt outputs

typedef field<0, 3, u32> CR1_I_CACHE_LEN;
typedef field<4, 4, u32> CR1_NUM_I_CACHE_LINES;

typedef field<0, 3, u32> CRD_DATA_WIDTH;        // data bus width of axi master
typedef field<4, 3, u32> CRD_WR_CAP;            // TODO what does this do?
typedef field<8, 4, u32> CRD_WR_Q_DEP;          // depth of the write queue
typedef field<12, 3, u32> CRD_RD_CAP;           // TODO what does this do?
typedef field<16, 4, u32> CRD_RD_Q_DEP;         // depth of the read queue
typedef field<20, 10, u32> CRD_DATA_BUFFER_DEP; // number of lines of mfifo

enum wd_bits {
    WD_IRQ_ONLY = bit(0),
};

enum request_type {
    SINGLE = 0b00,
    BURST = 0b01,
};

static void pl330_handle_ch_fault(pl330& dma, pl330::channel& ch,
                                  pl330::channel::fault fault) {
    ch.ftr |= 0b1 << fault;
    bool first_fault = !((dma.fsrc & 0x7) && (dma.manager.fsrd & 0x1));
    dma.fsrc |= 0b1 << ch.tag;
    ch.set_state(pl330::channel::FAULTING);
    if (first_fault)
        dma.irq_abort = true;
}

static void pl330_handle_mn_fault(pl330& dma, pl330::manager::fault fault) {
    dma.manager.ftrd |= 0b1 << fault;
    bool first_fault = !((dma.fsrc & 0x7) && (dma.manager.fsrd & 0x1));
    dma.manager.fsrd |= 0b1;
    dma.manager.set_state(pl330::manager::FAULTING);
    if (first_fault)
        dma.irq_abort = true;
}

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
    if (neg)
        im |= 0xffffu << 16;

    if (ra)
        ch->dar += im;
    else
        ch->sar += im;
}

static void pl330_dmaaddh(pl330* dma, pl330::channel* ch, uint8_t opcode,
                          uint8_t* args, int len) {
    // std::cout << "pl330_dmaaddh" << std::endl;
    pl330_dmaadxh(ch, args, !!(opcode & 0b10), false);
}

static void pl330_dmaadnh(pl330* dma, pl330::channel* ch, uint8_t opcode,
                          uint8_t* args, int len) {
    // std::cout << "pl330_dmaadnh" << std::endl;
    pl330_dmaadxh(ch, args, !!(opcode & 0b10), true);
}

static void pl330_dmaend(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    //    std::cout<< "pl330_dmaend" <<std::endl;
    if (ch->get_state() == pl330::channel::EXECUTING) {
        /* Wait for all transfers to complete */
        if (!dma->mfifo.empty(ch->tag) || !dma->read_queue.empty(ch->tag) ||
            !dma->write_queue.empty(ch->tag)) {
            ch->stall = true;
            return;
        }
    }
    // flush fifo, cache and queues
    dma->mfifo.remove_tagged(ch->tag);
    dma->read_queue.remove_tagged(ch->tag);
    dma->write_queue.remove_tagged(ch->tag);
    ch->set_state(pl330::channel::STOPPED);
}

static void pl330_mn_dmaend(pl330* dma, pl330::channel* ch, uint8_t opcode,
                            uint8_t* args, int len) {
    //    std::cout<< "pl330_mn_dmaend" <<std::endl;
    dma->manager.set_state(pl330::manager::STOPPED);
}

// flush peripheral
static void pl330_dmaflushp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                            uint8_t* args, int len) {
    // std::cout << "pl330_dmaflushp" << std::endl;
    uint8_t periph_id;

    if (args[0] & 7) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    if ((ch->csr & DMAWFP_B_NS) && !(dma->cr4 & (1 << periph_id))) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    /* Do nothing */
}

static void pl330_dmago(pl330* dma, pl330::channel* ch, uint8_t opcode,
                        uint8_t* args, int len) {
    uint8_t chan_id;
    uint32_t ns;
    uint32_t pc;

    // std::cout<< "pl330_dmago" <<std::endl;
    // trace_pl330_dmago();
    ns = !!(opcode & 0b10);
    chan_id = args[0] & 7;
    if ((args[0] >> 3)) {
        pl330_handle_mn_fault(*dma, pl330::manager::OPERAND_INVALID);
        return;
    }
    if (chan_id >= dma->cr0.get_field<CR0_NUM_CHNLS>()) {
        pl330_handle_mn_fault(*dma, pl330::manager::OPERAND_INVALID);
        return;
    }
    auto& channel = dma->channels[chan_id];
    pc = (((uint32_t)args[4]) << 24) | (((uint32_t)args[3]) << 16) |
         (((uint32_t)args[2]) << 8) | (((uint32_t)args[1]));
    if (!channel.is_state(pl330::channel::STOPPED)) {
        /* Perform NOP */
        return;
    }
    if ((dma->manager.dsr & DNS) && !ns) {
        pl330_handle_mn_fault(*dma, pl330::manager::DMAGO_ERR);
        return;
    }
    set_bit<CNS>(channel.csr, ns);
    channel.cpc = pc;
    channel.set_state(pl330::channel::EXECUTING);
}

static void pl330_dmakill(pl330* dma, pl330::channel* ch, uint8_t opcode,
                          uint8_t* args, int len) {
    // std::cout << "pl330_dmakill" << std::endl;
    if (ch->is_state(pl330::channel::FAULTING) ||
        ch->is_state(pl330::channel::FAULTING_COMPLETING)) {
        /* This is the only way for a channel to leave the faulting state */
        ch->ftr &= ~FTR_MASK;
        dma->fsrc &= ~(1 << ch->tag);
        bool faulting = (dma->fsrc & 0x7) && (dma->manager.fsrd & 0x1);
        if (!faulting) {
            //  trace_pl330_dmakill();
            dma->irq_abort = false;
        }
    }
    ch->set_state(pl330::channel::KILLING);
    // TODO: according to spec a channel thread should wait for AXI
    // transactions with its TAG as ID to complete here, but it isn't instead
    // it just removes them in qemu what do we do?!
    dma->mfifo.remove_tagged(ch->tag);
    dma->read_queue.remove_tagged(ch->tag);
    dma->write_queue.remove_tagged(ch->tag);
    ch->set_state(pl330::channel::STOPPED);
}

static void pl330_mn_dmakill(pl330* dma, pl330::channel* ch, uint8_t opcode,
                             uint8_t* args, int len) {
    // std::cout << "pl330_mn_dmakill" << std::endl;
    dma->manager.set_state(pl330::manager::STOPPED);
}

static void pl330_dmald(pl330* dma, pl330::channel* ch, uint8_t opcode,
                        uint8_t* args, int len) {
    uint8_t bs = opcode & 0b11;
    uint32_t size, num;
    bool inc;

    if (bs == 0b10) {
        ch->log.warn("Invalid operand during DMALD instruction");
        // pl330_fault(ch, pl330::channel::operand_invalid);
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
    inc = ch->ccr & ccr_bits::SRC_INC;

    ch->stall = !dma->read_queue.push(
        pl330::queue_entry{ ch->sar, size, num, inc, 0, ch->tag });
    if (!ch->stall) {
        // std::cout << "pc: " << ch->cpc << " pl330_dmald from: " << ch->sar
        //           << std::endl;
        //  trace_pl330_dmald(ch->tag, ch->src, size, num, inc ? 'Y' :
        //  'N');
        ch->sar += inc ? size * num - (ch->sar & (size - 1)) : 0;
    }
}

static void pl330_dmaldp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    // std::cout << "pl330_dmaldp" << std::endl;

    if (args[0] & 7) {
        // pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    uint8_t periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        // pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if (ch->csr & csr_bits::CNS && !(dma->cr4 & (1 << periph_id))) {
        // pl330_fault(ch, pl330::channel::ch_periph_err);
        return;
    }
    pl330_dmald(dma, ch, opcode, args, len);
}

static void pl330_dmalp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                        uint8_t* args, int len) {
    //    std::cout<<"pc: " <<ch->cpc<< " pl330_dmalp" <<std::endl;
    bool lc = opcode & 0b10;
    if (lc)
        ch->lc1.set_field<LC1_LOOP_COUNTER_ITERATIONS>(args[0]);
    else
        ch->lc0.set_field<LCO_LOOP_COUNTER_ITERATIONS>(args[0]);
}

static void pl330_dmalpend(pl330* dma, pl330::channel* ch, uint8_t opcode,
                           uint8_t* args, int len) {
    //    std::cout<< "pl330_dmalpend" <<std::endl;
    bool nf = opcode & 0b1'0000;
    uint8_t bs = opcode & 0b11;
    bool lc = opcode & 0b100;

    //    trace_pl330_dmalpend(nf, bs, lc, ch->lc[lc], ch->request_flag);

    if (bs == 0b10) {
        // pl330_fault(ch, pl330::channel::operand_invalid);
        return;
    }
    if ((bs == 0b01 && ch->request_flag == request_type::BURST) ||
        (bs == 0b11 && ch->request_flag == request_type::SINGLE))
        /* Perform NOP */
        return;

    if (!nf || (lc ? ch->lc1 : ch->lc0)) {
        if (nf)
            lc ? ch->lc1-- : ch->lc0--;
        //  trace_pl330_dmalpiter();
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
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    // todo abort if [9] is 0 and channel is ns==1
    uint32_t im = (((uint32_t)args[4]) << 24) | (((uint32_t)args[3]) << 16) |
                  (((uint32_t)args[2]) << 8) | (((uint32_t)args[1]));

    // std::cout<<"pc: "<<ch->cpc << " pl330_dmamov " << (u32)rd << " " << im
    // <<std::endl;

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
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
}

static void pl330_dmanop(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    std::cout << "pl330_dmanop" << std::endl;
    /* NOP is NOP. */
}

static void pl330_dmarmb(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    std::cout << "pl330_dmarmb" << std::endl;
    if (!dma->read_queue.empty(ch->tag)) {
        ch->set_state(pl330::channel::AT_BARRIER);
        ch->stall = 1;
        return;
    } else {
        ch->set_state(pl330::channel::EXECUTING);
    }
}

static void pl330_dmasev(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    uint8_t ev_id;

    if (args[0] & 0b0111) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    ev_id = (args[0] >> 3) & 0b0001'1111;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    if (ch->csr & CNS && !(dma->cr3 & (1 << ev_id))) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::CH_EVNT_ERR);
        return;
    }
    if (dma->inten & (1 << ev_id)) {
        dma->intmis |= (1 << ev_id);
        //  trace_pl330_dmasev_evirq(ev_id);
        dma->irq[ev_id] = true;
    }
    //    std::cout<< "pc: " << ch->cpc << " pl330_dmasev event id: " <<
    //    (u32)ev_id <<std::endl; trace_pl330_dmasev_event(ev_id);
    dma->int_event_ris |= (1 << ev_id);
}

static void pl330_mn_dmasev(pl330* dma, pl330::channel* ch, uint8_t opcode,
                            uint8_t* args, int len) {
    uint8_t ev_id;

    if (args[0] & 0b0111) {
        pl330_handle_mn_fault(*dma, pl330::manager::OPERAND_INVALID);
        return;
    }
    ev_id = (args[0] >> 3) & 0b0001'1111;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        pl330_handle_mn_fault(*dma, pl330::manager::OPERAND_INVALID);
        return;
    }
    if (dma->manager.dsr & DNS && !(dma->cr3 & (1 << ev_id))) {
        pl330_handle_mn_fault(*dma, pl330::manager::EVNT_ERR);
        return;
    }
    if (dma->inten & (1 << ev_id)) {
        dma->intmis |= (1 << ev_id);
        //  trace_pl330_dmasev_evirq(ev_id);
        dma->irq[ev_id] = true;
    }
    std::cout << "pl330_mn_dmasev" << std::endl;
    //    trace_pl330_dmasev_event(ev_id);
    dma->int_event_ris |= (1 << ev_id);
}

static void pl330_dmast(pl330* dma, pl330::channel* ch, uint8_t opcode,
                        uint8_t* args, int len) {
    uint8_t bs = opcode & 0b11;

    if (bs == 0b10) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    if ((bs == 0b01 && ch->request_flag == request_type::BURST) ||
        (bs == 0b11 && ch->request_flag == request_type::SINGLE)) {
        /* Perform NOP */
        return;
    }

    uint32_t num = ch->ccr.get_field<CCR_DST_BURST_LEN>() + 1;
    uint32_t size = (uint32_t)1 << ch->ccr.get_field<CCR_DST_BURST_SIZE>();
    bool inc = ch->ccr & ccr_bits::DST_INC;
    ch->stall = !dma->write_queue.push(
        pl330::queue_entry{ ch->dar, size, num, inc, 0, ch->tag });
    if (!ch->stall) {
        // std::cout << "pc: " << ch->cpc << " pl330_dmast to: " << ch->dar
        //           << std::endl;
        //   trace_pl330_dmast(ch->tag, ch->dar, size, num, inc ?
        //   'Y' : 'N');
        ch->dar += inc ? size * num - (ch->dar & (size - 1)) : 0;
    }
}

static void pl330_dmastp(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    // std::cout << "pl330_dmastp" << std::endl;
    if (args[0] & 0b111) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    uint8_t periph_id = (args[0] >> 3) & 0b0001'1111;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    if (ch->csr & CNS && !(dma->cr4 & (1 << periph_id))) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::CH_PERIPH_ERR);
        return;
    }
    pl330_dmast(dma, ch, opcode, args, len);
}

static void pl330_dmastz(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    std::cout << "pl330_dmastz" << std::endl;
    uint32_t num = ch->ccr.get_field<CCR_DST_BURST_LEN>() + 1;
    uint32_t size = (uint32_t)1 << ((ch->ccr >> 15) & 0x7);
    bool inc = !!((ch->ccr >> 14) & 0b1);
    ch->stall = dma->write_queue.push(
        pl330::queue_entry{ ch->dar, size, num, inc, 1, ch->tag });
    if (inc)
        ch->dar += size * num;
}

static void pl330_dmawfe(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    if (args[0] & 0b101) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    uint8_t ev_id = (args[0] >> 3) & 0x1f;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    if (ch->csr & CNS && !(dma->cr3 & (1 << ev_id))) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::CH_EVNT_ERR);
        return;
    }
    ch->csr.set_field<CSR_WAKEUP_NUMBER>(ev_id);
    ch->set_state(pl330::channel::WAITING_FOR_EVENT);
    if (~dma->inten & dma->int_event_ris & 1 << ev_id) {
        ch->set_state(pl330::channel::EXECUTING);
        /* If anyone else is currently waiting on the same event, let them
         * clear the ev_status, so they pick up event as well
         */
        for (u32 i = 0; i < dma->cr0.get_field<CR0_NUM_CHNLS>(); ++i) {
            pl330::channel* peer = &dma->channels[i];
            if (peer->is_state(pl330::channel::WAITING_FOR_EVENT) &&
                ch->csr.get_field<CSR_WAKEUP_NUMBER>() == ev_id) {
                return;
            }
        }
        dma->int_event_ris &= ~(1 << ev_id);
        std::cout << "pl330_dmawfe" << std::endl;
        //        trace_pl330_dmawfe(ev_id);
    } else {
        ch->stall = 1;
    }
}

static void pl330_mn_dmawfe(pl330* dma, pl330::channel* ch, uint8_t opcode,
                            uint8_t* args, int len) {
    std::cout << "pl330_mn_dmawfe" << std::endl;
    if (args[0] & 0b101) {
        pl330_handle_mn_fault(*dma, pl330::manager::OPERAND_INVALID);
        return;
    }
    u8 ev_id = (args[0] >> 3) & 0x1f;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        pl330_handle_mn_fault(*dma, pl330::manager::OPERAND_INVALID);
        return;
    }
    if (dma->manager.dsr & DNS && !(dma->cr3 & (1 << ev_id))) {
        pl330_handle_mn_fault(*dma, pl330::manager::EVNT_ERR);
        return;
    }
    dma->manager.dsr.set_field<DSR_WAKEUP_EVENT>(ev_id);
    dma->manager.set_state(pl330::manager::WAITING_FOR_EVENT);
    if (~dma->inten & dma->int_event_ris & (1 << ev_id)) {
        dma->manager.set_state(pl330::manager::EXECUTING);
        /* If anyone else is currently waiting on the same event, let them
         * clear the ev_status, so they pick up event as well
         */
        for (u32 i = 0; i < dma->cr0.get_field<CR0_NUM_CHNLS>(); ++i) {
            pl330::channel* peer = &dma->channels[i];
            if (peer->is_state(pl330::manager::WAITING_FOR_EVENT) &&
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
    std::cout << "pl330_dmawfp" << std::endl;
    uint8_t bs = opcode & 0b11;

    if (args[0] & 0b111) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    uint8_t periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }
    if (ch->csr & CNS && !(dma->cr4 & (1 << periph_id))) {
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::CH_PERIPH_ERR);
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
        pl330_handle_ch_fault(*dma, *ch, pl330::channel::OPERAND_INVALID);
        return;
    }

    if (dma->periph_busy[periph_id]) { // TODO: how do we wait for a peripheral
                                       // in vcml?!
        ch->set_state(pl330::channel::WAITING_FOR_PERIPHERAL);
        ch->stall = 1;
    } else if (ch->is_state(pl330::channel::WAITING_FOR_PERIPHERAL)) {
        ch->set_state(pl330::channel::EXECUTING);
    }
}

static void pl330_dmawmb(pl330* dma, pl330::channel* ch, uint8_t opcode,
                         uint8_t* args, int len) {
    std::cout << "pl330_dmawmb" << std::endl;
    if (!dma->write_queue.empty(ch->tag)) {
        ch->set_state(pl330::channel::AT_BARRIER);
        ch->stall = 1;
        return;
    } else {
        ch->set_state(pl330::channel::EXECUTING);
    }
}

struct insn_descr {
    u8 opcode;
    u8 opmask;
    u32 size;
    void (*exec)(pl330*, pl330::channel*, u8 opcode, u8* args, int len);
};

// clang-format off
/* Instructions which can be issued via channel threads. */
static const insn_descr CH_INSN_DESCR[] = {
    {.opcode = 0x54, .opmask = 0xFD, .size = 3, .exec = pl330_dmaaddh,},
    {.opcode = 0x5c, .opmask = 0xFD, .size = 3, .exec = pl330_dmaadnh,},
    {.opcode = 0x00, .opmask = 0xFF, .size = 1, .exec = pl330_dmaend,},
    {.opcode = 0x35, .opmask = 0xFF, .size = 2, .exec = pl330_dmaflushp,},
    {.opcode = 0x04, .opmask = 0xFC, .size = 1, .exec = pl330_dmald,},
    {.opcode = 0x25, .opmask = 0xFD, .size = 2, .exec = pl330_dmaldp,},
    {.opcode = 0x20, .opmask = 0xFD, .size = 2, .exec = pl330_dmalp,},
    {.opcode = 0x29, .opmask = 0xFD, .size = 2, .exec = pl330_dmastp,},
    {.opcode = 0x28, .opmask = 0xE8, .size = 2, .exec = pl330_dmalpend,},
    {.opcode = 0xBC, .opmask = 0xFF, .size = 6, .exec = pl330_dmamov,},
    {.opcode = 0x18, .opmask = 0xFF, .size = 1, .exec = pl330_dmanop,},
    {.opcode = 0x12, .opmask = 0xFF, .size = 1, .exec = pl330_dmarmb,},
    {.opcode = 0x34, .opmask = 0xFF, .size = 2, .exec = pl330_dmasev,},
    {.opcode = 0x08, .opmask = 0xFC, .size = 1, .exec = pl330_dmast,},
    {.opcode = 0x0C, .opmask = 0xFF, .size = 1, .exec = pl330_dmastz,},
    {.opcode = 0x36, .opmask = 0xFF, .size = 2, .exec = pl330_dmawfe,},
    {.opcode = 0x30, .opmask = 0xFC, .size = 2, .exec = pl330_dmawfp,},
    {.opcode = 0x13, .opmask = 0xFF, .size = 1, .exec = pl330_dmawmb,},
}; //todo consider reordering for speed

/* Instructions which can be issued via the manager thread. */
static const insn_descr MN_INSN_DESCR[] = {
    {.opcode = 0x00,.opmask = 0xFF,.size = 1,.exec = pl330_mn_dmaend,},
    {.opcode = 0xA0,.opmask = 0xFD,.size = 6,.exec = pl330_dmago,},
    {.opcode = 0x01,.opmask = 0xFF,.size = 1,.exec = pl330_mn_dmakill,},
    {.opcode = 0x18,.opmask = 0xFF,.size = 1,.exec = pl330_dmanop,},
    {.opcode = 0x34,.opmask = 0xFF,.size = 2,.exec = pl330_mn_dmasev,},
    {.opcode = 0x36,.opmask = 0xFF,.size = 2,.exec = pl330_mn_dmawfe,},
};

/* Instructions which can be issued via debug registers. */
static const insn_descr DEBUG_INSN_DESC[] = {
    {.opcode = 0xA0,.opmask = 0xFD,.size = 6,.exec = pl330_dmago,},
    {.opcode = 0x01,.opmask = 0xFF,.size = 1,.exec = pl330_dmakill,},
    {.opcode = 0x34,.opmask = 0xFF,.size = 2,.exec = pl330_dmasev,},
};
// clang-format on

static inline const insn_descr* fetch_ch_insn(pl330* dma,
                                              pl330::channel& channel) {
    u8 opcode;
    dma->dma.read(channel.cpc, &opcode, 1);
    for (auto& insn_candidate : CH_INSN_DESCR) {
        if (insn_candidate.opcode == (opcode & insn_candidate.opmask))
            return &insn_candidate;
    }
    return nullptr;
}

static inline const insn_descr* fetch_mn_insn(pl330* dma) {
    u8 opcode;
    dma->dma.read(dma->manager.dpc, &opcode, 1);
    for (auto& insn_candidate : MN_INSN_DESCR) {
        if (insn_candidate.opcode == (opcode & insn_candidate.opmask))
            return &insn_candidate;
    }
    return nullptr;
}

static inline void execute_insn(pl330& dma, pl330::channel& channel,
                                const insn_descr* insn) {
    uint8_t buffer[pl330::INSN_MAXSIZE];
    dma.dma.read(channel.cpc, (void*)buffer, insn->size);
    insn->exec(&dma, &channel, buffer[0], &buffer[1], insn->size - 1);
}

static int channel_execute_one_insn(pl330& dma, pl330::channel& channel) {
    // check state
    if (!channel.is_state(pl330::channel::EXECUTING) &&
        !channel.is_state(pl330::channel::WAITING_FOR_PERIPHERAL) &&
        !channel.is_state(pl330::channel::AT_BARRIER) &&
        !channel.is_state(pl330::channel::WAITING_FOR_EVENT)) {
        return 0;
    }
    channel.stall = false;
    const insn_descr* insn = fetch_ch_insn(&dma, channel);
    if (!insn) {
        pl330_handle_ch_fault(dma, channel, pl330::channel::INSTR_FETCH_ERR);
        return 0;
    }

    execute_insn(dma, channel, insn);
    if (!channel.stall && !channel.is_state(pl330::channel::STOPPED)) {
        channel.cpc += insn->size;
        channel.watchdog_timer = 0;
        return 1;
    } else if (channel.is_state(pl330::channel::EXECUTING)) {
        channel.watchdog_timer += 1;
        if (channel.watchdog_timer > pl330::WD_TIMEOUT)
            VCML_ERROR("pl330 channel %d watchdog timeout", channel.tag);
    }
    return 0;
}

static int channel_execute_cycle(pl330& dma, pl330::channel& channel) {
    u32 num_exec = 0;
    num_exec += channel_execute_one_insn(dma, channel);

    // one insn form read queue
    if (!dma.read_queue.empty() &&
        dma.read_queue.front().data_len <= dma.mfifo.num_free()) {
        auto& insn = dma.read_queue.front_mut();
        // crop length otherwise in case of an unaligned address the first read
        // would be too long
        u32 len = insn.data_len - (insn.data_addr & (insn.data_len - 1));
        u8 buffer[len];
        if (failed(dma.dma.read(insn.data_addr, (void*)buffer, len)))
            std::cout << "dma read failed" << std::endl;

        if (dma.mfifo.num_free() >= len) {
            for (u32 i = 0; i < len; i++) {
                dma.mfifo.push(
                    pl330::mfifo_entry{ buffer[i], (u8)channel.tag });
            }
            if (insn.inc)
                insn.data_addr += len;
            insn.burst_len_counter--;
            if (!insn.burst_len_counter)
                dma.read_queue.pop();
            num_exec++;
        }
    }
    // one insn from write queue
    if (!dma.write_queue.empty() && dma.mfifo.front().tag == channel.tag) {
        auto& insn = dma.write_queue.front_mut();
        // crop length otherwise in case of an unaligned address the first read
        // would be too long
        int len = insn.data_len - (insn.data_addr & (insn.data_len - 1));
        u8 buffer[len];
        if (insn.zero_flag) {
            for (int i = 0; i < len; i++) {
                buffer[i] = 0;
            }
        } else {
            for (int i = 0; i < len; i++) {
                assert(!dma.mfifo.empty());
                buffer[i] = dma.mfifo.pop().value().buf;
            }
        }
        if (failed(dma.dma.write(insn.data_addr, (void*)buffer, len)))
            std::cout << "dma write failed" << std::endl;
        if (insn.inc)
            insn.data_addr += len;
        insn.burst_len_counter--;
        if (!insn.burst_len_counter)
            dma.write_queue.pop();
        num_exec++;
    }
    return num_exec;
}

static inline void run_channel(pl330& dma, pl330::channel& channel) {
    while (channel_execute_cycle(dma, channel) > 0) {
    }
}

static void run_channels(pl330& dma) {
    for (auto& channel : dma.channels) {
        if (!(channel.is_state(pl330::channel::STOPPED))) {
            run_channel(dma, channel);
        }
    }
}

static void run_manager(pl330& dma) {
    if (!dma.manager.is_state(pl330::manager::EXECUTING) &&
        !dma.manager.is_state(pl330::manager::WAITING_FOR_EVENT)) {
        return;
    }
    uint8_t buffer[pl330::INSN_MAXSIZE];
    dma.manager.stall = false;
    while (((!dma.manager.stall) &&
            dma.manager.is_state(pl330::manager::WAITING_FOR_EVENT)) ||
           dma.manager.is_state(pl330::manager::EXECUTING)) {
        const insn_descr* insn = fetch_mn_insn(&dma);
        if (!insn) {
            pl330_handle_mn_fault(dma, pl330::manager::INSTR_FETCH_ERR);
            break;
        }

        dma.dma.read(dma.manager.dpc, (void*)buffer, insn->size);
        insn->exec(&dma, nullptr, buffer[0], &buffer[1], insn->size - 1);
        if (!dma.manager.stall) {
            dma.manager.dpc += insn->size;
            dma.manager.watchdog_timer = 0;
        } else if (dma.manager.is_state(pl330::channel::EXECUTING)) {
            dma.manager.watchdog_timer += 1;
            if (dma.manager.watchdog_timer > pl330::WD_TIMEOUT)
                VCML_ERROR("pl330 manager watchdog timeout");
        }
    }
}

static void handle_debug_insn(pl330& dma) {
    uint8_t args[5];
    uint8_t opcode;
    uint8_t chan_id;

    // check dbg status idle
    if (dma.dbgstatus & dbgstatus_bits::DBGSTATUS) {
        return; // dbg busy case
    }
    dma.dbgstatus.set_bit<DBGSTATUS>(true); // set dbg busy

    chan_id = (dma.dbginst0 >> 8) & 0x07;
    opcode = (dma.dbginst0 >> 16) & 0xff;
    args[0] = (dma.dbginst0 >> 24) & 0xff;
    args[1] = (dma.dbginst1 >> 0) & 0xff;
    args[2] = (dma.dbginst1 >> 8) & 0xff;
    args[3] = (dma.dbginst1 >> 16) & 0xff;
    args[4] = (dma.dbginst1 >> 24) & 0xff;

    //    check if insn exists
    insn_descr insn = { 0, 0, 0, nullptr };
    for (auto& insn_candidate : DEBUG_INSN_DESC) {
        if ((opcode & insn_candidate.opmask) == insn_candidate.opcode)
            insn = insn_candidate;
    }
    if (!insn.exec) {
        std::cout << "Pl330 invalid debug instruction opcode" << std::endl;
        return;
    }

    // check target
    if (dma.dbginst0 & dbginst0_bits::DEBUG_THREAD) {
        // target is channel
        dma.channels[chan_id].stall = false;
        insn.exec(&dma, &dma.channels[chan_id], opcode, args, insn.size);
    } else {
        insn.exec(&dma, nullptr, opcode, args, insn.size);
    }
    dma.dbgstatus.set_bit<DBGSTATUS>(false); // set dbg idle
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
    auto reg_setter = [&](reg<mwr::u32>& reg) {
        reg.tag = tag;
        reg.allow_read_only();
        reg.sync_never();
    };
    reg_setter(csr);
    reg_setter(cpc);
    reg_setter(sar);
    reg_setter(dar);
    reg_setter(ccr);
    reg_setter(lc0);
    reg_setter(lc1);
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
    dsr.sync_never();
    dpc.allow_read_only();
    dpc.sync_never();
    fsrd.allow_read_only();
    fsrd.sync_never();
    ftrd.allow_read_only();
    ftrd.sync_never();
}

void pl330::pl330_thread() {
    while (true) {
        wait(m_dma);
        if (m_execute_debug) {
            handle_debug_insn(*this);
            m_execute_debug = false;
        }
        run_manager(*this);
        run_channels(*this);
        // todo re-trigger if any channels are in states where they might have
        // just been waiting for other channels, or sth ?!
    }
}

pl330::~pl330() {
    // for now do nothing
}

void pl330::reset() {
    peripheral::reset();

    // set control registers //todo many of these should probably be properties
    cr0.set_bit<PERIPH_REQ>(0b0);
    cr0.set_bit<BOOT_EN>(0b1);
    cr0.set_bit<MGR_NS_AT_RST>(0b0);
    cr0.set_field<CR0_NUM_CHNLS>(num_channels - 1);
    cr0.set_field<CR0_NUM_PERIPH_REQ>(NPER - 1);
    cr0.set_field<CR0_NUM_EVENTS>(NIRQ - 1);
    crd.set_field<CRD_DATA_BUFFER_DEP>(MFIFO_LINES - 1);
    crd.set_field<CRD_DATA_WIDTH>(0b010); // 0b011 = 64-bit
    crd.set_field<CRD_RD_CAP>(0);
    crd.set_field<CRD_RD_Q_DEP>(QUEUE_SIZE - 1);
    crd.set_field<CRD_WR_CAP>(0);
    crd.set_field<CRD_WR_Q_DEP>(QUEUE_SIZE - 1);

    // reset dmac
    inten = 0;
    int_event_ris = 0;
    intmis = 0;
    dbgstatus = 0;
    fsrc = 0;
    // reset manager
    manager.dsr.set_bit<dsr_bits::DNS>(!!(cr0 & cr0_bits::MGR_NS_AT_RST));
    // reset channels
    for (auto& channel : channels) {
        channel.sar = 0;
        channel.dar = 0;
        channel.cpc = 0;
        channel.ccr = 0;
        channel.ftr = 0;
        channel.set_state(channel::STOPPED);
        channel.watchdog_timer = 0;
        channel.stall = false;
    }

    read_queue.reset();
    write_queue.reset();
    mfifo.reset();
    // reset id registers
    for (size_t i = 0; i < periph_id.count(); i++)
        periph_id[i] = (AMBA_PID >> (i * 8)) & 0xff;

    for (size_t i = 0; i < pcell_id.count(); i++)
        pcell_id[i] = (AMBA_CID >> (i * 8)) & 0xff;
}

pl330::pl330(const sc_module_name& nm):
    peripheral(nm),
    read_queue(QUEUE_SIZE),
    write_queue(QUEUE_SIZE),
    mfifo(MFIFO_LINES * 8), // todo mfifo is not correct yet
    num_channels("num_channels", 8),
    channels("channel", num_channels,
             [](const char* nm, u32 tag) { return new channel(nm, tag); }),
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
    //{0x00241330} or 0x30, 0x13, 0x24, 0x00
    periph_id("periph_id", 0xfe0, 0x00000000),
    //{0xB105F00D}), or 0x0d, oxf0, 0x05, 0xb1
    pcell_id("pcell_id", 0xff0, 0x00000000),
    in("in"),
    dma("dma"),
    irq("irq", 32ul),
    irq_abort("irq_abort") {
    fsrc.allow_read_only();
    fsrc.sync_never();
    inten.allow_read_write();
    inten.sync_never();

    int_event_ris.allow_read_only();
    int_event_ris.sync_always();

    intmis.allow_read_only();
    intmis.sync_never();

    intclr.allow_write_only();
    intclr.sync_on_write();
    intclr.on_write([&](u32 v, size_t i) -> void {
        u32 irq_clear_mask = v & inten;
        for (u32 j = 0; j < 32; j++) {
            if (irq_clear_mask & (1 << j))
                irq[j] = false;
        }
        int_event_ris &= ~irq_clear_mask;
        intmis &= ~irq_clear_mask;
        m_dma.notify(SC_ZERO_TIME);
    });

    dbgstatus.allow_read_only();
    dbgstatus.sync_never();

    dbgcmd.allow_write_only();
    dbgcmd.sync_on_write();
    dbgcmd.on_write([&](u32 v, size_t i) -> void {
        if ((v & 0b11) == 0)
            m_execute_debug = true;
        dbgcmd = v;
        m_dma.notify(SC_ZERO_TIME);
    });

    dbginst0.allow_write_only();
    dbginst0.sync_never();
    dbginst1.allow_write_only();
    dbginst1.sync_never();

    cr0.allow_read_only();
    cr0.sync_never();
    cr1.allow_read_only();
    cr1.sync_never();
    cr2.allow_read_only();
    cr2.sync_never();
    cr3.allow_read_only();
    cr3.sync_never();
    cr4.allow_read_only();
    cr4.sync_never();
    crd.allow_read_only();
    crd.sync_never();

    wd.allow_read_write();
    wd.sync_never();

    periph_id.allow_read_only();
    periph_id.sync_never();
    pcell_id.allow_read_only();
    pcell_id.sync_never();

    SC_HAS_PROCESS(pl330);
    SC_THREAD(pl330_thread);
}

} // namespace vcml::dma
