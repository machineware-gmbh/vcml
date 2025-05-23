/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/dma/pl330.h"

namespace vcml {
namespace dma {

enum pl330_configs : u32 {
    PL330_INSN_MAXSIZE = 6,
    PL330_MAX_BURST_LEN = 16 * 16,
    PL330_WD_TIMEOUT = 1024,
};

enum ch_state : u8 {
    CHS_STOPPED = 0x0,
    CHS_EXECUTING = 0x1,
    CHS_CACHE_MISS = 0x2,
    CHS_UPDATING_PC = 0x3,
    CHS_WAITING_FOR_EVENT = 0x4,
    CHS_AT_BARRIER = 0x5,
    CHS_WAITING_FOR_PERIPHERAL = 0x7,
    CHS_KILLING = 0x8,
    CHS_COMPLETING = 0x9,
    CHS_FAULTING_COMPLETING = 0xe,
    CHS_FAULTING = 0xf,
};

enum mn_state : u8 {
    MNS_STOPPED = 0x0,
    MNS_EXECUTING = 0x1,
    MNS_CACHE_MISS = 0x2,
    MNS_UPDATING_PC = 0x3,
    MNS_WAITING_FOR_EVENT = 0x4,
    MNS_FAULTING = 0xF,
};

enum pl_330_mfifo_width : u32 {
    MFIFO_32BIT = 0b010,
    MFIFO_64BIT = 0b011,
    MFIFO_128BIT = 0b100,
};

enum dsr_bits : u32 {
    DSR_DNS = bit(9), // Manager security status
};

typedef field<0, 4, u32> DSR_DMA_STATUS;
typedef field<4, 5, u32> DSR_WAKEUP_EVENT;

enum fsrd_bits {
    FSRD_FS_MGR = bit(0),
};

typedef field<0, 8, u32> FSRC_FAULT_STATUS;

enum ftrd_bits : u32 {
    FTRD_UNDEF_INSTR = 0x0,
    FTRD_OPERAND_INVALID = 0x1,
    FTRD_DMAGO_ERR = 0x4,
    FTRD_EVNT_ERR = 0x5,
    FTRD_INSTR_FETCH_ERR = 0x10,
    FTRD_DBG_INSTR = 0x1e,
    FTRD_MASK = FTRD_UNDEF_INSTR | FTRD_OPERAND_INVALID | FTRD_DMAGO_ERR |
                FTRD_EVNT_ERR | FTRD_INSTR_FETCH_ERR | FTRD_DBG_INSTR,
};

enum ftr_bits : u32 {
    FTR_UNDEF_INSTR = 0x0,
    FTR_OPERAND_INVALID = 0x1,
    FTR_CH_EVNT_ERR = 0x5,
    FTR_CH_PERIPH_ERR = 0x6,
    FTR_CH_RDWR_ERR = 0x7,
    FTR_MFIFO_ERR = 0xc,
    FTR_ST_DATA_UNAVAILABLE = 0xd,
    FTR_INSTR_FETCH_ERR = 0x10,
    FTR_DATA_WRITE_ERR = 0x11,
    FTR_DATA_READ_ERR = 0x12,
    FTR_DBG_INSTR = 0x1e,
    FTR_LOCKUP_ERR = 0x1f,
    FTR_MASK = FTR_UNDEF_INSTR | FTR_OPERAND_INVALID | FTR_CH_EVNT_ERR |
               FTR_CH_PERIPH_ERR | FTR_CH_RDWR_ERR | FTR_MFIFO_ERR |
               FTR_ST_DATA_UNAVAILABLE | FTR_INSTR_FETCH_ERR |
               FTR_DATA_WRITE_ERR | FTR_DATA_READ_ERR | FTR_DBG_INSTR |
               FTR_LOCKUP_ERR,
};

enum csr_bits : u32 {
    CSR_DMAWFP_B_NS = bit(14),
    CSR_DMAWFP_PERIPH = bit(15),
    CSR_CNS = bit(21),
};

typedef field<0, 4, u32> CSR_CHANNEL_STATUS;
typedef field<4, 5, u32> CSR_WAKEUP_NUMBER;

enum ccr_bits : u32 {
    CCR_SRC_INC = bit(0),
    CCR_DST_INC = bit(14),
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

enum dbgstatus_bits : u32 {
    DBGSTATUS = bit(0),
};

enum dbgcmd_bits : u32 {
    DBGCMD = bit(0),
};

enum dbginst0_bits : u32 {
    DBGINST0_DEBUG_THREAD = bit(0),
};

typedef field<8, 3, u32> DBGINST0_CHANNEL_NUMBER;
typedef field<16, 8, u32> DBGINST0_INSTRUCTION_BYTE0;
typedef field<24, 8, u32> DBGINST0_INSTRUCTION_BYTE1;

typedef field<0, 8, u32> DBGINST1_INSTRUCTION_BYTE2;
typedef field<8, 8, u32> DBGINST1_INSTRUCTION_BYTE3;
typedef field<16, 8, u32> DBGINST1_INSTRUCTION_BYTE4;
typedef field<24, 8, u32> DBGINST1_INSTRUCTION_BYTE5;

enum cr0_bits : u32 {
    CR0_PERIPH_REQ = bit(0),
    CR0_BOOT_EN = bit(1),
    CR0_MGR_NS_AT_RST = bit(2),
    CR0_RESET_VALUE = 0x3ff002,
};

typedef field<4, 3, u32> CR0_NUM_CHNLS;
typedef field<12, 5, u32> CR0_NUM_PERIPH_REQ;
typedef field<17, 5, u32> CR0_NUM_EVENTS;

typedef field<0, 3, u32> CR1_I_CACHE_LEN;
typedef field<4, 4, u32> CR1_NUM_I_CACHE_LINES;

typedef field<0, 3, u32> CRD_DATA_WIDTH;
typedef field<4, 3, u32> CRD_WR_CAP;
typedef field<8, 4, u32> CRD_WR_Q_DEP;
typedef field<12, 3, u32> CRD_RD_CAP;
typedef field<16, 4, u32> CRD_RD_Q_DEP;
typedef field<20, 10, u32> CRD_DATA_BUFFER_DEP;

enum crd_bits : u32 {
    CRD_RESET_VALUE = (0x7 << 4) | (0x7 << 12),
};

enum wd_bits : u32 {
    WD_IRQ_ONLY = bit(0),
};

enum request_type : u32 {
    REQ_SINGLE = 0b00,
    REQ_BURST = 0b01,
};

static void pl330_handle_ch_fault(pl330& dma, pl330::channel& ch,
                                  ftr_bits fault) {
    ch.ftr |= bit(fault);
    ch.set_state(CHS_FAULTING);
    bool is_first_fault = !(get_field<FSRC_FAULT_STATUS>(dma.fsrc) &&
                            (dma.manager.fsrd & FSRD_FS_MGR));
    dma.fsrc |= bit(ch.chid);
    ch.log.warn("DMA channel faulting with fault: %d", fault);
    if (is_first_fault)
        dma.irq_abort = true;
}

static void pl330_handle_mn_fault(pl330& dma, ftrd_bits fault) {
    dma.manager.ftrd |= bit(fault);
    dma.manager.set_state(MNS_FAULTING);
    bool first_fault = !(get_field<FSRC_FAULT_STATUS>(dma.fsrc) &&
                         (dma.manager.fsrd & FSRD_FS_MGR));
    dma.manager.fsrd |= bit(0);
    dma.manager.log.warn("DMA manager faulting with fault: %d", fault);
    if (first_fault)
        dma.irq_abort = true;
}

static void pl330_insn_dmaadxh(pl330::channel* ch, u8* args, bool ra,
                               bool neg) {
    u32 im = (args[1] << 8) | args[0];
    if (neg)
        im |= 0xffffu << 16;

    if (ra)
        ch->dar += im;
    else
        ch->sar += im;
}

static void pl330_insn_dmaaddh(pl330* dma, pl330::channel* ch, u8 opcode,
                               u8* args, size_t len) {
    pl330_insn_dmaadxh(ch, args, !!(opcode & 0b10), false);
}

static void pl330_insn_dmaadnh(pl330* dma, pl330::channel* ch, u8 opcode,
                               u8* args, size_t len) {
    pl330_insn_dmaadxh(ch, args, !!(opcode & 0b10), true);
}

static void pl330_insn_dmaend(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if (ch->get_state() == CHS_EXECUTING) {
        // wait for all transfers to complete
        if (!dma->mfifo.empty(ch->chid) || !dma->read_queue.empty(ch->chid) ||
            !dma->write_queue.empty(ch->chid)) {
            ch->stall = true;
            return;
        }
    }

    // flush fifo, cache and queues
    dma->mfifo.remove_tagged(ch->chid);
    dma->read_queue.remove_tagged(ch->chid);
    dma->write_queue.remove_tagged(ch->chid);
    ch->set_state(CHS_STOPPED);
}

static void pl330_mn_insn_dmaend(pl330* dma, pl330::channel* ch, u8 opcode,
                                 u8* args, size_t len) {
    dma->manager.set_state(MNS_STOPPED);
}

// flush peripheral
static void pl330_insn_dmaflushp(pl330* dma, pl330::channel* ch, u8 opcode,
                                 u8* args, size_t len) {
    if (args[0] & 7) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    u8 periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if ((ch->csr & CSR_DMAWFP_B_NS) && !(dma->cr4 & bit(periph_id))) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    // do nothing
}

static void pl330_insn_dmago(pl330* dma, pl330::channel* ch, u8 opcode,
                             u8* args, size_t len) {
    u32 ns = !!(opcode & 0b10);
    u8 chan_id = args[0] & 7;
    if ((args[0] >> 3)) {
        pl330_handle_mn_fault(*dma, FTRD_OPERAND_INVALID);
        return;
    }

    if (chan_id >= dma->cr0.get_field<CR0_NUM_CHNLS>()) {
        pl330_handle_mn_fault(*dma, FTRD_OPERAND_INVALID);
        return;
    }

    auto& channel = dma->channels[chan_id];
    u32 pc = (((u32)args[4]) << 24) | (((u32)args[3]) << 16) |
             (((u32)args[2]) << 8) | (((u32)args[1]));

    if (!channel.is_state(CHS_STOPPED))
        return; // perform NOP

    if ((dma->manager.dsr & DSR_DNS) && !ns) {
        pl330_handle_mn_fault(*dma, FTRD_DMAGO_ERR);
        return;
    }

    set_bit<CSR_CNS>(channel.csr, ns);
    channel.cpc = pc;
    channel.set_state(CHS_EXECUTING);
}

static void pl330_insn_dmakill(pl330* dma, pl330::channel* ch, u8 opcode,
                               u8* args, size_t len) {
    if (ch->is_state(CHS_FAULTING) || ch->is_state(CHS_FAULTING_COMPLETING)) {
        // this is the only way for a channel to leave the faulting state
        ch->ftr &= ~FTR_MASK;
        dma->fsrc &= ~bit(ch->chid);
        bool faulting = (dma->fsrc & 0x7) && (dma->manager.fsrd & 0x1);
        if (!faulting)
            dma->irq_abort = false;
    }

    ch->set_state(CHS_KILLING);
    dma->mfifo.remove_tagged(ch->chid);
    dma->read_queue.remove_tagged(ch->chid);
    dma->write_queue.remove_tagged(ch->chid);
    ch->set_state(CHS_STOPPED);
}

static void pl330_mn_insn_dmakill(pl330* dma, pl330::channel* ch, u8 opcode,
                                  u8* args, size_t len) {
    dma->manager.set_state(MNS_STOPPED);
}

static void pl330_insn_dmald(pl330* dma, pl330::channel* ch, u8 opcode,
                             u8* args, size_t len) {
    u8 bs = opcode & 0b11;
    if (bs == 0b10) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    // DMALD[S|B] missmatch -> nop case
    if ((bs == 0b01 && ch->request_flag == REQ_BURST) ||
        (bs == 0b11 && ch->request_flag == REQ_SINGLE)) {
        // Perform NOP
        return;
    }

    u32 num; // DMALD case
    if (bs == 1 && ch->request_flag == REQ_SINGLE)
        num = 1;
    else
        num = ch->ccr.get_field<CCR_SRC_BURST_LEN>() + 1;

    u32 size = bit(ch->ccr.get_field<CCR_SRC_BURST_SIZE>());
    bool inc = ch->ccr & CCR_SRC_INC;

    pl330::queue_entry entry{ ch->sar, size, num, inc, 0, ch->chid };
    ch->stall = !dma->read_queue.push(entry);

    if (!ch->stall)
        ch->sar += inc ? size * num - (ch->sar & (size - 1)) : 0;
}

static void pl330_insn_dmaldp(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if (args[0] & 7) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    u8 periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if (ch->csr & CSR_CNS && !(dma->cr4 & bit(periph_id))) {
        pl330_handle_ch_fault(*dma, *ch, FTR_CH_PERIPH_ERR);
        return;
    }

    pl330_insn_dmald(dma, ch, opcode, args, len);
}

static void pl330_insn_dmalp(pl330* dma, pl330::channel* ch, u8 opcode,
                             u8* args, size_t len) {
    bool lc = opcode & 0b10;
    if (lc)
        ch->lc1.set_field<LC1_LOOP_COUNTER_ITERATIONS>(args[0]);
    else
        ch->lc0.set_field<LCO_LOOP_COUNTER_ITERATIONS>(args[0]);
}

static void pl330_insn_dmalpend(pl330* dma, pl330::channel* ch, u8 opcode,
                                u8* args, size_t len) {
    bool nf = opcode & 0b1'0000;
    u8 bs = opcode & 0b11;
    bool lc = opcode & 0b100;

    if (bs == 0b10) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if ((bs == 0b01 && ch->request_flag == REQ_BURST) ||
        (bs == 0b11 && ch->request_flag == REQ_SINGLE)) {
        // perform NOP
        return;
    }

    if (!nf || (lc ? ch->lc1 : ch->lc0)) {
        if (nf)
            lc ? ch->lc1-- : ch->lc0--;

        ch->cpc -= args[0];
        ch->cpc -= len + 1;
    }
}

static void pl330_insn_dmamov(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if ((args[0] >> 3)) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    u8 rd = args[0] & 0b0111;
    u32 im = ((u32)args[4] << 24) | ((u32)args[3] << 16) |
             ((u32)args[2] << 8) | ((u32)args[1]);

    bool ch_secure = !(ch->csr & CSR_CNS);
    bool src_secure = !(im & (0b010 << 8));
    bool dst_secure = !(im & (0b010 << 22));

    switch (rd) {
    case 0b000:
        ch->sar = im;
        break;

    case 0b001:
        if ((src_secure || dst_secure) && !ch_secure) {
            pl330_handle_ch_fault(*dma, *ch, FTR_CH_RDWR_ERR);
            break;
        }

        ch->ccr = im;
        break;

    case 0b010:
        ch->dar = im;
        break;

    default:
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }
}

static void pl330_insn_dmanop(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    // Perform NOP
}

static void pl330_insn_dmarmb(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if (!dma->read_queue.empty(ch->chid)) {
        ch->set_state(CHS_AT_BARRIER);
        ch->stall = 1;
    } else {
        ch->set_state(CHS_EXECUTING);
    }
}

static void pl330_insn_dmasev(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if (args[0] & 0b0111) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    u8 ev_id = (args[0] >> 3) & 0b0001'1111;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if (ch->csr & CSR_CNS && !(dma->cr3 & bit(ev_id))) {
        pl330_handle_ch_fault(*dma, *ch, FTR_CH_EVNT_ERR);
        return;
    }

    if (dma->inten & bit(ev_id)) {
        dma->intmis |= bit(ev_id);
        dma->irq[ev_id] = true;
    }

    dma->int_event_ris |= bit(ev_id);
}

static void pl330_mn_insn_dmasev(pl330* dma, pl330::channel* ch, u8 opcode,
                                 u8* args, size_t len) {
    if (args[0] & 0b0111) {
        pl330_handle_mn_fault(*dma, FTRD_OPERAND_INVALID);
        return;
    }

    u8 ev_id = (args[0] >> 3) & 0b0001'1111;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        pl330_handle_mn_fault(*dma, FTRD_OPERAND_INVALID);
        return;
    }

    if (dma->manager.dsr & DSR_DNS && !(dma->cr3 & bit(ev_id))) {
        pl330_handle_mn_fault(*dma, FTRD_EVNT_ERR);
        return;
    }

    if (dma->inten & bit(ev_id)) {
        dma->intmis |= bit(ev_id);
        dma->irq[ev_id] = true;
    }

    dma->int_event_ris |= bit(ev_id);
}

static void pl330_insn_dmast(pl330* dma, pl330::channel* ch, u8 opcode,
                             u8* args, size_t len) {
    u8 bs = opcode & 0b11;
    if (bs == 0b10) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if ((bs == 0b01 && ch->request_flag == REQ_BURST) ||
        (bs == 0b11 && ch->request_flag == REQ_SINGLE)) {
        return; // perform NOP
    }

    u32 num = ch->ccr.get_field<CCR_DST_BURST_LEN>() + 1;
    u32 size = bit(ch->ccr.get_field<CCR_DST_BURST_SIZE>());
    bool inc = ch->ccr & CCR_DST_INC;

    pl330::queue_entry entry{ ch->dar, size, num, inc, 0, ch->chid };
    ch->stall = !dma->write_queue.push(entry);

    if (!ch->stall)
        ch->dar += inc ? size * num - (ch->dar & (size - 1)) : 0;
}

static void pl330_insn_dmastp(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if (args[0] & 0b111) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    u8 periph_id = (args[0] >> 3) & 0b0001'1111;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if (ch->csr & CSR_CNS && !(dma->cr4 & bit(periph_id))) {
        pl330_handle_ch_fault(*dma, *ch, FTR_CH_PERIPH_ERR);
        return;
    }

    pl330_insn_dmast(dma, ch, opcode, args, len);
}

static void pl330_insn_dmastz(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    u32 num = ch->ccr.get_field<CCR_DST_BURST_LEN>() + 1;
    u32 size = bit((ch->ccr >> 15) & 0x7);
    bool inc = !!((ch->ccr >> 14) & 0b1);

    pl330::queue_entry entry{ ch->dar, size, num, inc, 1, ch->chid };
    ch->stall = dma->write_queue.push(entry);

    if (inc)
        ch->dar += size * num;
}

static void pl330_insn_dmawfe(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if (args[0] & 0b101) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    u8 ev_id = (args[0] >> 3) & 0x1f;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if (ch->csr & CSR_CNS && !(dma->cr3 & bit(ev_id))) {
        pl330_handle_ch_fault(*dma, *ch, FTR_CH_EVNT_ERR);
        return;
    }

    ch->csr.set_field<CSR_WAKEUP_NUMBER>(ev_id);
    ch->set_state(CHS_WAITING_FOR_EVENT);

    if (~dma->inten & dma->int_event_ris & bit(ev_id)) {
        ch->set_state(CHS_EXECUTING);
        // If anyone else is currently waiting on the same event, let them
        // clear the ev_status, so they pick up event as well
        for (u32 i = 0; i < dma->cr0.get_field<CR0_NUM_CHNLS>(); ++i) {
            pl330::channel* peer = &dma->channels[i];
            if (peer->is_state(CHS_WAITING_FOR_EVENT) &&
                ch->csr.get_field<CSR_WAKEUP_NUMBER>() == ev_id) {
                return;
            }
        }

        dma->int_event_ris &= ~bit(ev_id);
    } else {
        ch->stall = 1;
    }
}

static void pl330_mn_insn_dmawfe(pl330* dma, pl330::channel* ch, u8 opcode,
                                 u8* args, size_t len) {
    if (args[0] & 0b101) {
        pl330_handle_mn_fault(*dma, FTRD_OPERAND_INVALID);
        return;
    }

    u8 ev_id = (args[0] >> 3) & 0x1f;
    if (ev_id >= dma->cr0.get_field<CR0_NUM_EVENTS>()) {
        pl330_handle_mn_fault(*dma, FTRD_OPERAND_INVALID);
        return;
    }

    if (dma->manager.dsr & DSR_DNS && !(dma->cr3 & bit(ev_id))) {
        pl330_handle_mn_fault(*dma, FTRD_EVNT_ERR);
        return;
    }

    dma->manager.dsr.set_field<DSR_WAKEUP_EVENT>(ev_id);
    dma->manager.set_state(MNS_WAITING_FOR_EVENT);

    if (~dma->inten & dma->int_event_ris & bit(ev_id)) {
        dma->manager.set_state(MNS_EXECUTING);
        // If anyone else is currently waiting on the same event, let them
        // clear the ev_status, so they pick up event as well
        for (u32 i = 0; i < dma->cr0.get_field<CR0_NUM_CHNLS>(); ++i) {
            pl330::channel* peer = &dma->channels[i];
            if (peer->is_state(MNS_WAITING_FOR_EVENT) &&
                dma->manager.dsr.get_field<DSR_WAKEUP_EVENT>() == ev_id) {
                return;
            }
        }
        dma->int_event_ris &= ~bit(ev_id);
    } else {
        ch->stall = 1;
    }
}

static void pl330_insn_dmawfp(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if (args[0] & 0b111) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    u8 periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= dma->cr0.get_field<CR0_NUM_PERIPH_REQ>()) {
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if (ch->csr & CSR_CNS && !(dma->cr4 & bit(periph_id))) {
        pl330_handle_ch_fault(*dma, *ch, FTR_CH_PERIPH_ERR);
        return;
    }

    u8 bs = opcode & 0b11;
    switch (bs) {
    case 0b00: // S
        ch->request_flag = REQ_SINGLE;
        ch->csr.set_bit<CSR_DMAWFP_B_NS>(0);
        ch->csr.set_bit<CSR_DMAWFP_PERIPH>(0);
        break;

    case 0b01: // P
        ch->request_flag = REQ_BURST;
        ch->csr.set_bit<CSR_DMAWFP_PERIPH>(1);
        break;

    case 0b10: // B
        ch->request_flag = REQ_BURST;
        ch->csr.set_bit<CSR_DMAWFP_B_NS>(1);
        ch->csr.set_bit<CSR_DMAWFP_PERIPH>(0);
        break;

    default:
        pl330_handle_ch_fault(*dma, *ch, FTR_OPERAND_INVALID);
        return;
    }

    if (dma->periph_busy[periph_id]) {
        ch->set_state(CHS_WAITING_FOR_PERIPHERAL);
        ch->stall = 1;
    } else if (ch->is_state(CHS_WAITING_FOR_PERIPHERAL)) {
        ch->set_state(CHS_EXECUTING);
    }
}

static void pl330_insn_dmawmb(pl330* dma, pl330::channel* ch, u8 opcode,
                              u8* args, size_t len) {
    if (!dma->write_queue.empty(ch->chid)) {
        ch->set_state(CHS_AT_BARRIER);
        ch->stall = 1;
    } else {
        ch->set_state(CHS_EXECUTING);
    }
}

struct insn_descr {
    u8 opcode;
    u8 opmask;
    u32 size;

    void (*exec)(pl330*, pl330::channel*, u8 opcode, u8* args, size_t len);
};

// Instructions which can be issued via channel threads.
static const insn_descr CH_INSN_DESCR[] = {
    { 0x54, 0xfd, 3, pl330_insn_dmaaddh },
    { 0x5c, 0xfd, 3, pl330_insn_dmaadnh },
    { 0x00, 0xff, 1, pl330_insn_dmaend },
    { 0x35, 0xff, 2, pl330_insn_dmaflushp },
    { 0x04, 0xfc, 1, pl330_insn_dmald },
    { 0x25, 0xfd, 2, pl330_insn_dmaldp },
    { 0x20, 0xfd, 2, pl330_insn_dmalp },
    { 0x29, 0xfd, 2, pl330_insn_dmastp },
    { 0x28, 0xe8, 2, pl330_insn_dmalpend },
    { 0xbc, 0xff, 6, pl330_insn_dmamov },
    { 0x18, 0xff, 1, pl330_insn_dmanop },
    { 0x12, 0xff, 1, pl330_insn_dmarmb },
    { 0x34, 0xff, 2, pl330_insn_dmasev },
    { 0x08, 0xfc, 1, pl330_insn_dmast },
    { 0x0c, 0xff, 1, pl330_insn_dmastz },
    { 0x36, 0xff, 2, pl330_insn_dmawfe },
    { 0x30, 0xfc, 2, pl330_insn_dmawfp },
    { 0x13, 0xff, 1, pl330_insn_dmawmb },
};

// Instructions which can be issued via the manager thread.
static const insn_descr MN_INSN_DESCR[] = {
    { 0x00, 0xff, 1, pl330_mn_insn_dmaend },
    { 0xa0, 0xfd, 6, pl330_insn_dmago },
    { 0x01, 0xff, 1, pl330_mn_insn_dmakill },
    { 0x18, 0xff, 1, pl330_insn_dmanop },
    { 0x34, 0xff, 2, pl330_mn_insn_dmasev },
    { 0x36, 0xff, 2, pl330_mn_insn_dmawfe },
};

// Instructions which can be issued via debug registers.
static const insn_descr DEBUG_INSN_DESC[] = {
    { 0xa0, 0xfd, 6, pl330_insn_dmago },
    { 0x01, 0xff, 1, pl330_insn_dmakill },
    { 0x34, 0xff, 2, pl330_insn_dmasev },
};

template <size_t N>
static const insn_descr* find_insn(u8 opcode, const insn_descr (&insns)[N]) {
    for (auto& insn : insns)
        if (insn.opcode == (opcode & insn.opmask))
            return &insn; // Found the matching instruction

    return nullptr;
}

template <size_t N>
static const insn_descr* fetch_and_exec(pl330& dma, pl330::channel* channel,
                                        u32 pc,
                                        const insn_descr (&insn_descrs)[N]) {
    u8 insn_buf[PL330_INSN_MAXSIZE];
    dma.dma.read(pc, insn_buf, 1); // fetch the first byte of the instruction
    const insn_descr* insn = find_insn(insn_buf[0], insn_descrs);
    if (!insn)
        return nullptr;

    // fetch remaining bytes if the instruction size is greater than 1
    if (insn->size > 1)
        dma.dma.read(pc + 1, insn_buf + 1, insn->size - 1);

    insn->exec(&dma, channel, insn_buf[0], &insn_buf[1], insn->size - 1);
    return insn;
}

static int channel_execute_one_insn(pl330& dma, pl330::channel& channel) {
    // check if the channel is ready for instruction execution
    if (!channel.is_state(CHS_EXECUTING) &&
        !channel.is_state(CHS_WAITING_FOR_PERIPHERAL) &&
        !channel.is_state(CHS_AT_BARRIER) &&
        !channel.is_state(CHS_WAITING_FOR_EVENT)) {
        return 0; // channel not ready for instruction execution
    }

    channel.stall = false;

    auto insn = fetch_and_exec(dma, &channel, channel.cpc, CH_INSN_DESCR);
    if (!insn) {
        pl330_handle_ch_fault(dma, channel, FTR_INSTR_FETCH_ERR);
        return 0;
    }

    if (!channel.stall && !channel.is_state(CHS_STOPPED)) {
        channel.cpc += insn->size;
        channel.watchdog_timer = 0;
        return 1;
    }

    if (channel.is_state(CHS_EXECUTING)) {
        channel.watchdog_timer += 1;
        if (channel.watchdog_timer > PL330_WD_TIMEOUT)
            dma.log.error("pl330 channel %d watchdog timeout", channel.chid);
    }

    return 0; // instruction not executed
}

static u32 process_read_queue(pl330& dma, pl330::channel& channel) {
    if (!dma.read_queue.empty() &&
        dma.read_queue.front().data_len <= dma.mfifo.num_free()) {
        auto& insn = dma.read_queue.front();
        u32 len = insn.data_len * insn.burst_len_counter;
        // crop length in case of an unaligned address
        len = len - (insn.data_addr & (insn.data_len - 1));

        u8 data[PL330_MAX_BURST_LEN];
        if (insn.inc || insn.burst_len_counter == 1) {
            if (failed(dma.dma.read(insn.data_addr, data, len)))
                dma.log.error("DMA channel read failed");
        } else {
            // stream I/O reads
            tlm_generic_payload tx;
            tx_setup(tx, TLM_READ_COMMAND, insn.data_addr, data, len);
            tx.set_streaming_width(insn.data_len);
            if (failed(dma.dma.send(tx)))
                dma.log.error("DMA channel read failed");
        }

        if (dma.mfifo.num_free() >= len) {
            for (u32 i = 0; i < len; i++) {
                pl330::mfifo_entry entry{ data[i], (u8)channel.chid };
                dma.mfifo.push(entry);
            }

            dma.read_queue.pop();
            return 1;
        }
    }

    return 0;
}

static u32 process_write_queue(pl330& dma, pl330::channel& channel) {
    if (!dma.write_queue.empty() && dma.mfifo.front().tag == channel.chid) {
        auto& insn = dma.write_queue.front();
        int len = insn.data_len * insn.burst_len_counter;
        // crop length in case of an unaligned address
        len = len - (insn.data_addr & (insn.data_len - 1));

        u8 data[PL330_MAX_BURST_LEN];
        if (insn.zero_flag)
            std::fill_n(data, len, 0);
        else {
            assert(len <= static_cast<int>(dma.mfifo.size()));
            std::generate_n(data, len,
                            [&]() { return dma.mfifo.pop().value().buf; });
        }

        if (insn.inc || insn.burst_len_counter == 1) {
            if (failed(dma.dma.write(insn.data_addr, data, len)))
                dma.log.error("DMA channel write failed");
        } else {
            // stream I/O writes
            tlm_generic_payload tx;
            tx_setup(tx, TLM_WRITE_COMMAND, insn.data_addr, data, len);
            tx.set_streaming_width(insn.data_len);
            if (failed(dma.dma.send(tx)))
                dma.log.error("DMA channel write failed");
        }

        dma.write_queue.pop();
        return 1;
    }

    return 0;
}

void pl330::run_channels() {
    for (auto& channel : channels) {
        if (!channel.is_state(CHS_STOPPED)) {
            u32 num_exec;
            do {
                num_exec = channel_execute_one_insn(*this, channel);
                num_exec += process_read_queue(*this, channel);
                num_exec += process_write_queue(*this, channel);
            } while (num_exec > 0);
        }
    }
}

void pl330::run_manager() {
    if (!manager.is_state(MNS_EXECUTING) &&
        !manager.is_state(MNS_WAITING_FOR_EVENT)) {
        return;
    }

    manager.stall = false;
    while (((!manager.stall) && manager.is_state(MNS_WAITING_FOR_EVENT)) ||
           manager.is_state(MNS_EXECUTING)) {
        auto insn = fetch_and_exec(*this, nullptr, manager.dpc, MN_INSN_DESCR);
        if (!insn) {
            pl330_handle_mn_fault(*this, FTRD_INSTR_FETCH_ERR);
            break;
        }

        if (!manager.stall) {
            manager.dpc += insn->size;
            manager.watchdog_timer = 0;
        } else if (manager.is_state(CHS_EXECUTING)) {
            manager.watchdog_timer += 1;
            if (manager.watchdog_timer > PL330_WD_TIMEOUT)
                log.error("pl330 manager watchdog timeout");
        }
    }
}

void pl330::handle_debug_instruction() {
    // check dbg status idle
    if (dbgstatus & dbgstatus_bits::DBGSTATUS)
        return; // dbg busy case

    dbgstatus.set_bit<DBGSTATUS>(true); // set dbg busy

    u8 chan_id = get_field<DBGINST0_CHANNEL_NUMBER>(dbginst0);
    u8 opcode = get_field<DBGINST0_INSTRUCTION_BYTE0>(dbginst0);

    u8 args[5];
    args[0] = get_field<DBGINST0_INSTRUCTION_BYTE1>(dbginst0);
    args[1] = get_field<DBGINST1_INSTRUCTION_BYTE2>(dbginst1);
    args[2] = get_field<DBGINST1_INSTRUCTION_BYTE3>(dbginst1);
    args[3] = get_field<DBGINST1_INSTRUCTION_BYTE4>(dbginst1);
    args[4] = get_field<DBGINST1_INSTRUCTION_BYTE5>(dbginst1);

    const insn_descr* insn = find_insn(opcode, DEBUG_INSN_DESC);
    if (!insn) {
        log.warn("Pl330 invalid debug instruction opcode");
        return;
    }

    // check target
    if (dbginst0 & DBGINST0_DEBUG_THREAD) { // target is channel
        channels[chan_id].stall = false;
        insn->exec(this, &channels[chan_id], opcode, args, insn->size);
    } else {
        insn->exec(this, nullptr, opcode, args, insn->size);
    }

    dbgstatus.set_bit<DBGSTATUS>(false); // set dbg idle
}

[[noreturn]] void pl330::pl330_thread() {
    while (true) {
        wait(m_dma);

        if (m_execute_debug) {
            handle_debug_instruction();
            m_execute_debug = false;
        }

        run_manager();
        run_channels();
    }
}

pl330::channel::channel(const sc_module_name& nm, mwr::u32 chid):
    module(nm),
    ftr("ftr", 0x040 + chid * 0x04),
    csr("csr", 0x100 + chid * 0x08),
    cpc("cpc", 0x104 + chid * 0x08),
    sar("sar", 0x400 + chid * 0x20),
    dar("dar", 0x404 + chid * 0x20),
    ccr("ccr", 0x408 + chid * 0x20),
    lc0("lc0", 0x40c + chid * 0x20),
    lc1("lc1", 0x410 + chid * 0x20),
    chid(chid),
    stall(false),
    request_flag(),
    watchdog_timer() {
    auto reg_setter = [&](reg<mwr::u32>& reg) {
        reg.tag = chid;
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

    VCML_ERROR_ON(chid > 7, "Too many channels specified for pl330");
}

pl330::manager::manager(const sc_module_name& nm):
    module(nm),
    dsr("dsr", 0x000),
    dpc("dpc", 0x004),
    fsrd("fsrd", 0x030),
    ftrd("ftrd", 0x038),
    stall(),
    watchdog_timer() {
    dsr.allow_read_only();
    dsr.sync_never();

    dpc.allow_read_only();
    dpc.sync_never();

    fsrd.allow_read_only();
    fsrd.sync_never();

    ftrd.allow_read_only();
    ftrd.sync_never();
}

pl330::~pl330() {
    // for now do nothing
}

void pl330::reset() {
    peripheral::reset();

    // set control registers
    cr0.set_bit<CR0_PERIPH_REQ>(enable_periph);
    cr0.set_field<CR0_NUM_CHNLS>(num_channels - 1);
    crd.set_field<CRD_DATA_BUFFER_DEP>(mfifo_lines - 1);
    crd.set_field<CRD_DATA_WIDTH>(mfifo_width);
    crd.set_field<CRD_RD_Q_DEP>(queue_size - 1);
    crd.set_field<CRD_WR_Q_DEP>(queue_size - 1);

    // reset manager
    manager.dsr.set_bit<DSR_DNS>(!!(cr0 & CR0_MGR_NS_AT_RST));
    manager.watchdog_timer = 0;
    manager.stall = false;

    // reset channels
    for (auto& ch : channels) {
        ch.set_state(CHS_STOPPED);
        ch.watchdog_timer = 0;
        ch.stall = false;
    }

    // reset queues
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
    enable_periph("enable_periph", false),
    num_channels("num_channels", 8),
    queue_size("queue_size", 16),
    mfifo_width("mfifo_width", MFIFO_32BIT),
    mfifo_lines("mfifo_lines", 256),
    read_queue(queue_size),
    write_queue(queue_size),
    mfifo(mfifo_lines * 8 * (1 << (mfifo_width - 2))),
    channels("channel", num_channels,
             [](const char* nm, u32 chid) { return new channel(nm, chid); }),
    manager("manager"),
    fsrc("fsrc", 0x034),
    inten("inten", 0x020),
    int_event_ris("int_event_ris", 0x024),
    intmis("intmis", 0x028),
    intclr("intclr", 0x02c),
    dbgstatus("dbgstatus", 0xd00),
    dbgcmd("dbgcmd", 0xd04),
    dbginst0("dbginst0", 0xd08),
    dbginst1("dbginst1", 0xd0c),
    cr0("cr0", 0xe00, CR0_RESET_VALUE),
    cr1("cr1", 0xe04),
    cr2("cr2", 0xe08),
    cr3("cr3", 0xe0c),
    cr4("cr4", 0xe10),
    crd("crd", 0xe14, CRD_RESET_VALUE),
    wd("wd", 0xe80),
    periph_id("periph_id", 0xfe0, 0x00000000),
    pcell_id("pcell_id", 0xff0, 0x00000000),
    periph_busy(),
    periph_irq("periph_irq"),
    in("in"),
    dma("dma"),
    irq("irq"),
    irq_abort("irq_abort"),
    m_dma(),
    m_execute_debug(false) {
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
        for (u32 j = 0; j < 32; j++)
            if (irq_clear_mask & bit(j))
                irq[j] = false;
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

    for (size_t i = 0; i < periph_id.count(); i++)
        periph_id[i] = (AMBA_PID >> (i * 8)) & 0xff;

    for (size_t i = 0; i < pcell_id.count(); i++)
        pcell_id[i] = (AMBA_CID >> (i * 8)) & 0xff;

    SC_HAS_PROCESS(pl330);
    SC_THREAD(pl330_thread);
}

} // namespace dma
} // namespace vcml
