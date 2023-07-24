
#include "vcml/models/dma/pl330.h"
using namespace vcml::arm;
using namespace mwr;

/*
 * For information about instructions see PL330 Technical Reference Manual.
 *
 * Arguments:
 *   CH - channel executing the instruction
 *   OPCODE - opcode
 *   ARGS - array of 8-bit arguments
 *   LEN - number of elements in ARGS array
 */

inline static void pl330_dmaadxh(pl330::pl330_channel *ch, uint8_t *args, bool ra, bool neg)
{
    uint32_t im = (args[1] << 8) | args[0];
    if (neg) {
        im |= 0xffffu << 16;
    }

    if (ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }
    if (ra) {
        ch->dar += im;
    } else {
        ch->dar += im;
    }
}

static void pl330_dmaaddh(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    pl330_dmaadxh(ch, args, !!(opcode & 0b10), false);
}

static void pl330_dmaadnh(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    pl330_dmaadxh(ch, args, !!(opcode & 0b10), true);
    //TODO check if this does the right thing twos-complement can be tricky and it doesnt look right to me
}

static void pl330_dmaend(pl330::pl330_channel *ch, uint8_t opcode,
                         uint8_t *args, int len)
{
    PL330State *s = ch->parent;

    if (ch->state == vcml::arm::pl330::Executing && !ch->is_manager) {
        /* Wait for all transfers to complete */
        if (pl330_fifo_has_tag(&s->fifo, ch->tag) ||
            pl330_queue_find_insn(&s->read_queue, ch->tag, false) != NULL ||
            pl330_queue_find_insn(&s->write_queue, ch->tag, false) != NULL) {

            ch->stall = 1;//TODO: not sure what we need to stall here how would we get here if we are currently executing anyway
            return;
        }
    }
    //trace_pl330_dmaend();

    //flush fifo, cache and queues
    pl330_fifo_tagged_remove(&s->fifo, ch->tag);
    pl330_queue_remove_tagged(&s->read_queue, ch->tag);
    pl330_queue_remove_tagged(&s->write_queue, ch->tag);
    ch->state = vcml::arm::pl330::Stopped;
}

//flush peripheral
static void pl330_dmaflushp(pl330::pl330_channel *ch, uint8_t opcode,
                            uint8_t *args, int len)
{
    uint8_t periph_id;

    if (ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }

    if (args[0] & 7) { //bit 8,9,and 10 are also 0 for this instruction opp code
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= ch->parent->num_periph_req) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if (ch->ns && !(ch->parent->cfg[CFG_PNS] & (1 << periph_id))) {//TODO: using secure interface from non-secure (ns) channel error (probably)
        pl330_fault(ch, PL330_FAULT_CH_PERIPH_ERR);
        return;
    }
    /* Do nothing */ //TODO: it really seems dma peripherals are not implemented in this model
}

static void pl330_dmago(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    uint8_t chan_id;
    uint8_t ns;
    uint32_t pc;
    pl330::pl330_channel *s;

//    trace_pl330_dmago();

    if (!ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }
    ns = !!(opcode & 0b10);
    chan_id = args[0] & 7;
    if ((args[0] >> 3)) {//top 5 bit of first byte should be 0 as per the spec
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if (chan_id >= ch->parent->num_chnls) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);//TODO: should be manager thread abort
        return;
    }
    pc = (((uint32_t)args[4]) << 24) | (((uint32_t)args[3]) << 16) |
         (((uint32_t)args[2]) << 8)  | (((uint32_t)args[1]));
    if (ch->parent->chan[chan_id].state != pl330_chan_stopped) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID); //TODO: as per the spec if the channel is not Stopped DMANOP should be performed instead (not a fault)
        return;
    }
    if (ch->ns && !ns) {
        pl330_fault(ch, PL330_FAULT_DMAGO_ERR);//TODO: should be manager thread abort
        return;
    }
    s = &ch->parent->chan[chan_id];
    s->ns = ns;
    s->pc = pc;
    s->state = pl330_chan_executing;
}

static void pl330_dmakill(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    if (ch->state == pl330_chan_fault ||
        ch->state == pl330_chan_fault_completing) {
        /* This is the only way for a channel to leave the faulting state */
        ch->fault_type = 0;
        ch->parent->num_faulting--;
        if (ch->parent->num_faulting == 0) {
//            trace_pl330_dmakill();
            qemu_irq_lower(ch->parent->irq_abort);
        }
    }
    ch->state = pl330_chan_killing;
    //TODO: according to spec a channel thread should wait for AXI transactions with its TAG as ID to complete here, but it isnt
    pl330_fifo_tagged_remove(&ch->parent->fifo, ch->tag);
    pl330_queue_remove_tagged(&ch->parent->read_queue, ch->tag);
    pl330_queue_remove_tagged(&ch->parent->write_queue, ch->tag);
    ch->state = pl330_chan_stopped;
}

static void pl330_dmald(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    uint8_t bs = opcode & 0b11;
    uint32_t size, num;
    bool inc;

    if (ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }

    if (bs == 2) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    //DMALD[S|B] missmatch -> nop case
    if ((bs == 1 && ch->request_flag == PL330_BURST) || //TODO: get request flag from ccr
        (bs == 3 && ch->request_flag == PL330_SINGLE)) {
        /* Perform NOP */
        return;
    }
    //DMALD case
    if (bs == 1 && ch->request_flag == PL330_SINGLE) {
        num = 1;
    } else {
        num = ((ch->control >> 4) & 0xf) + 1;//TODO: find out what this does in the ccr
    }
    size = (uint32_t)1 << ((ch->control >> 1) & 0x7);
    inc = !!(ch->control & 0b1);
    //TODO comment put into read queue unless stall
    ch->stall = pl330_queue_put_insn(&ch->parent->read_queue, ch->src,
                                     size, num, inc, 0, ch->tag);
    if (!ch->stall) {
//        trace_pl330_dmald(ch->tag, ch->src, size, num, inc ? 'Y' : 'N');
        ch->src += inc ? size * num - (ch->src & (size - 1)) : 0;//TODO spec does not mention src_inc for DMALD only for DMALDP... strange
    }
}

static void pl330_dmaldp(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    uint8_t periph_id;

    if (args[0] & 7) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= ch->parent->num_periph_req) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if (ch->ns && !(ch->parent->cfg[CFG_PNS] & (1 << periph_id))) {
        pl330_fault(ch, PL330_FAULT_CH_PERIPH_ERR);
        return;
    }
    pl330_dmald(ch, opcode, args, len);
}

static void pl330_dmalp(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    if (ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }

    uint8_t lc = (opcode & 2) >> 1;

    ch->lc[lc] = args[0];
}

static void pl330_dmalpend(pl330::pl330_channel *ch, uint8_t opcode,
                           uint8_t *args, int len)
{
    uint8_t nf = (opcode & 0b1'0000) >> 4;
    uint8_t bs = opcode & 0b11;
    uint8_t lc = (opcode & 0b100) >> 2;

//    trace_pl330_dmalpend(nf, bs, lc, ch->lc[lc], ch->request_flag);

    if (bs == 0b10) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if ((bs == 0b01 && ch->request_flag == PL330_BURST) ||
        (bs == 0b11 && ch->request_flag == PL330_SINGLE)) {
        /* Perform NOP */
        return;
    }
    if (!nf || ch->lc[lc]) {
        if (nf) {
            ch->lc[lc]--;
        }
//        trace_pl330_dmalpiter();
        ch->pc -= args[0];
        ch->pc -= len + 1;//TODO: test if this is correct especially why do we need to subtract insn len+1 shouldnt the pc be at the start of the current insn?!
        /* "ch->pc -= args[0] + len + 1" is incorrect when args[0] == 256 */
    } else {
//        trace_pl330_dmalpfallthrough();
    }
}

//no pl330_dmalpfe needed because it does not result in a dma instruction

static void pl330_dmamov(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    uint8_t rd = args[0] & 0b0111;
    uint32_t im;

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
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
}

static void pl330_dmanop(pl330::pl330_channel *ch, uint8_t opcode,
                         uint8_t *args, int len)
{
    /* NOP is NOP. */
}

static void pl330_dmarmb(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    if (pl330_queue_find_insn(&ch->parent->read_queue, ch->tag, false)) {
        ch->state = vcml::arm::pl330::At_barrier;
        ch->stall = 1;
        return;
    } else {
        ch->state = vcml::arm::pl330::Executing;
    }
}

static void pl330_dmasev(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    uint8_t ev_id;

    if (args[0] & 0b0111) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    ev_id = (args[0] >> 3) & 0b0001'1111;
    if (ev_id >= ch->parent->num_events) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if (ch->ns && !(ch->parent->cfg[CFG_INS] & (1 << ev_id))) {
        pl330_fault(ch, PL330_FAULT_EVENT_ERR);
        return;
    }
    if (ch->parent->inten & (1 << ev_id)) {
        ch->parent->int_status |= (1 << ev_id);
//        trace_pl330_dmasev_evirq(ev_id);
        qemu_irq_raise(ch->parent->irq[ev_id]);
    }
//    trace_pl330_dmasev_event(ev_id);
    ch->parent->ev_status |= (1 << ev_id);
}

static void pl330_dmast(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    uint8_t bs = opcode & 0b11;
    uint32_t size, num;
    bool inc;

    if (ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }

    if (bs == 0b10) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if ((bs == 0b01 && ch->request_flag == PL330_BURST) ||
        (bs == 0b11 && ch->request_flag == PL330_SINGLE)) {
        /* Perform NOP */
        return;
    }
    num = ((ch->ccr >> 18) & 0b1111) + 1;
    size = (uint32_t)1 << ((ch->ccr >> 15) & 0b111);
    inc = !!((ch->ccr >> 14) & 0b1);
    ch->stall = pl330_queue_put_insn(&ch->parent->write_queue, ch->dar,
                                     size, num, inc, 0, ch->tag);
    if (!ch->stall) {
//        trace_pl330_dmast(ch->tag, ch->dar, size, num, inc ? 'Y' : 'N');
        ch->dar += inc ? size * num - (ch->dar & (size - 1)) : 0;
    }
}

static void pl330_dmastp(pl330::pl330_channel *ch, uint8_t opcode,
                         uint8_t *args, int len)
{
    uint8_t periph_id;

    if (ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }
    if (args[0] & 0b111) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    periph_id = (args[0] >> 3) & 0x0001'1111;
    if (periph_id >= ch->parent->num_periph_req) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if (ch->ns && !(ch->parent->cfg[CFG_PNS] & (1 << periph_id))) {
        pl330_fault(ch, PL330_FAULT_CH_PERIPH_ERR);
        return;
    }
    pl330_dmast(ch, opcode, args, len);
}

static void pl330_dmastz(pl330::pl330_channel *ch, uint8_t opcode,
                         uint8_t *args, int len)
{
    uint32_t size, num;
    bool inc;

    num = ((ch->ccr >> 18) & 0b1111) + 1;
    size = (uint32_t)1 << ((ch->ccr >> 15) & 0x7);
    inc = !!((ch->ccr >> 14) & 0b1);
    ch->stall = pl330_queue_put_insn(&ch->parent->write_queue, ch->dar,
                                     size, num, inc, 1, ch->tag);
    if (inc) {
        ch->dar += size * num;
    }
}

static void pl330_dmawfe(pl330::pl330_channel *ch, uint8_t opcode,
                         uint8_t *args, int len)
{
    uint8_t ev_id;
    int i;

    if (args[0] & 0b101) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    ev_id = (args[0] >> 3) & 0x1f;
    if (ev_id >= ch->parent->num_events) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if (ch->ns && !(ch->parent->cfg[CFG_INS] & (1 << ev_id))) {
        pl330_fault(ch, PL330_FAULT_EVENT_ERR);
        return;
    }
    ch->wakeup = ev_id;
    ch->state = vcml::arm::pl330::Waiting_for_event;
    if (~ch->parent->inten & ch->parent->ev_status & 1 << ev_id) {
        ch->state = vcml::arm::pl330::Executing;
        /* If anyone else is currently waiting on the same event, let them
         * clear the ev_status so they pick up event as well
         */
        for (i = 0; i < ch->parent->num_chnls; ++i) {
            pl330::pl330_channel *peer = &ch->parent->chan[i];
            if (peer->state == vcml::arm::pl330::Waiting_for_event &&
                peer->wakeup == ev_id) {
                return;
            }
        }
        ch->parent->ev_status &= ~(1 << ev_id);
//        trace_pl330_dmawfe(ev_id);
    } else {
        ch->stall = 1;
    }
}

static void pl330_dmawfp(pl330::pl330_channel *ch, uint8_t opcode,
                         uint8_t *args, int len)
{
    uint8_t bs = opcode & 0b11;
    uint8_t periph_id;

    if (ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }
    if (args[0] & 0b111) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    periph_id = (args[0] >> 3) & 0x1f;
    if (periph_id >= ch->parent->num_periph_req) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if (ch->ns && !(ch->parent->cfg[CFG_PNS] & (1 << periph_id))) {
        pl330_fault(ch, PL330_FAULT_CH_PERIPH_ERR);
        return;
    }
    switch (bs) {
    case 0b00: /* S */
        ch->request_flag = PL330_SINGLE;
        ch->wfp_sbp = 0;
        break;
    case 0b01: /* P */
        ch->request_flag = PL330_BURST;
        ch->wfp_sbp = 2;
        break;
    case 0b10: /* B */
        ch->request_flag = PL330_BURST;
        ch->wfp_sbp = 1;
        break;
    default:
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }

    if (ch->parent->periph_busy[periph_id]) {
        ch->state = vcml::arm::pl330::Waiting_for_peripheral;
        ch->stall = 1;
    } else if (ch->state == vcml::arm::pl330::Waiting_for_peripheral) {
        ch->state = vcml::arm::pl330::Executing;
    }
}

static void pl330_dmawmb(pl330::pl330_channel *ch, uint8_t opcode,
                         uint8_t *args, int len)
{
    if (pl330_queue_find_insn(&ch->parent->write_queue, ch->tag, false)) {
        ch->state = vcml::arm::pl330::At_barrier;
        ch->stall = 1;
        return;
    } else {
        ch->state = vcml::arm::pl330::Executing;
    }

}

struct insn_descr { // TODO: maybe this only lives in the implementation, comment: i dont like this exec, this is very "C" to me in a bad way
    u32 opcode;
    u32 opmask;
    u32 size;
    void (*exec)(pl330_channel*, u8 opcode, u8* args, int len); //todo: make this generic over channel and manager somehow maybe this should just have a pointer to the pl330 itself and the tag where manager = num_channels?!
};

/* NULL terminated array of the instruction descriptions. */
static const struct insn_descr ch_insn_descr[] = {
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
    { .opcode = 0x01, .opmask = 0xFF, .size = 1, .exec = pl330_dmakill, }, // Kill
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
static const struct insn_descr mn_insn_descr[] = {
    { .opcode = 0x00, .opmask = 0xFF, .size = 1, .exec = pl330_dmaend, }, // End
    { .opcode = 0xA0, .opmask = 0xFD, .size = 6, .exec = pl330_dmago, }, // Go
    { .opcode = 0x01, .opmask = 0xFF, .size = 1, .exec = pl330_dmakill, }, // Kill
    { .opcode = 0x18, .opmask = 0xFF, .size = 1, .exec = pl330_dmanop, }, // NOP
    { .opcode = 0x34, .opmask = 0xFF, .size = 2, .exec = pl330_dmasev, }, // Send Event
    { .opcode = 0x36, .opmask = 0xFF, .size = 2, .exec = pl330_dmawfe, }, // Wait For Event
    { .opcode = 0x00, .opmask = 0x00, .size = 0, .exec = NULL, }
};

/* Instructions which can be issued via debug registers. */
static const struct insn_descr debug_insn_desc[] = {
    { .opcode = 0xA0, .opmask = 0xFD, .size = 6, .exec = pl330_dmago, },
    { .opcode = 0x01, .opmask = 0xFF, .size = 1, .exec = pl330_dmakill, },
    { .opcode = 0x34, .opmask = 0xFF, .size = 2, .exec = pl330_dmasev, },
    { .opcode = 0x00, .opmask = 0x00, .size = 0, .exec = NULL, }
};

static inline insn_descr* fetch_insn(insn_descr const* insns,pl330::channel& channel) {
    //get pc & load insn opcode byte
    u8 opcode = in.read(channel.cpc, 1);
    //iterate over insn_desc to find correct one
    //return insn
    for (u32 i; i< insns.size(); i++) {
        if (insn[i].opcode == opcode)
            return insn
    }
    return NULL;
}

void pl330::execute_cycle() {
    manager_execute_cycle();//todo this is already not cycle accurate because we do both manager and channel thread in a single cycle but they actually alternate via the spec, however qumu just does everything in one cycle and in the wrong order.

    for (u32 i = 0; i < num_channels; i++) {
        u32 rr_tag = (last_rr_channel + i + 1) % num_channels;
        if (!(channels[rr_tag].is_state(channel::state::Stopped))) {
            channel_execute_cycle(channels[rr_tag]);
            last_rr_channel = channels[rr_tag].tag;
            break;
        }
    }
}

void pl330::manager_execute_cycle(){
// do an insn fetch with correct insns array
}

void pl330::channel_execute_cycle(channel& channel){
    //check state
    // fetch one instruction and see if it can be placed in queue, stall if queue was not available
    // if it was placed/executed advance pc?! or rather do that in the functions?!

    //do actual work (the above just transfer commands)
    //check if axi read/write is available and clear one command form the MFIFO
    //if MFIFO has space move read command (or if not available write command) to it
}

pl330::channel::channel(const sc_module_name& nm, mwr::u32 tag) :
    module(nm),
    ftr("ftr", 0x040 + tag * 0x04),
    csr("csr", 0x100 + tag * 0x08),
    cpc("cpc", 0x104 + tag * 0x08),
    sar("sar", 0x400 + tag * 0x20),
    dar("dar", 0x404 + tag * 0x20),
    ccr("ccr", 0x404 + tag * 0x20),
    lc0("lc0", 0x40c + tag * 0x20),
    lc1("lc1", 0x410 + tag * 0x20),
    tag(tag),
    stall(false) {
    if (tag > 7)
        VCML_ERROR("Too many channels specified for pl330");
}

pl330::manager::manager(const sc_module_name& nm) :
    module(nm),
    dsr("dsr",0x000),
    dpc("dpc",0x004),
    fsrd("fsrd",0x030),
    ftrd("ftrd",0x038)
{

}

void pl330::pl330_thread() {
    execute_cycle();
}

pl330::pl330(const sc_module_name& nm):
    peripheral(nm),
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
    in("in"),//todo do i need to configure dmi for this one and "out"?!
    out("out") {
    SC_HAS_PROCESS(pl330);
    SC_THREAD(pl330_thread);
}
