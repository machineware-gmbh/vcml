
#include "vcml/models/dma/pl330.h"
using namespace vcml::arm;

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
    pl330_dmaadxh(ch, args, extract32(opcode, 1, 1), false);//TODO extract extracts the ra bit implement!
}

static void pl330_dmaadnh(pl330::pl330_channel *ch, uint8_t opcode, uint8_t *args, int len)
{
    pl330_dmaadxh(ch, args, extract32(opcode, 1, 1), true);//TODO extract extracts the ra bit implement!
    //TODO check if this does the right theng twos-complement can be tricky and it doesnt look right to me
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
    periph_id = (args[0] >> 3) & 0x1f; //TODO: somehow the periph_id is not allowed to be 0x11111 ?! why according to the spec 31 should be fine
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
    ns = !!(opcode & 2);//TODO:weird !! operator, i guess its a c thing (it converts any non-zero values to '1', zero stays zero)
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
    uint8_t bs = opcode & 3;//TODO ffs koennt ihr euch mal einigen wie ihr bits lesen wollt?! use extract8?!
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
    inc = !!(ch->control & 1);
    //TODO comment put into read queue or stall
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

    ch->lc[lc] = args[0];//TODO aaahhh lc = loopCounter which is a size 2 array
}

static void pl330_dmalpend(pl330::pl330_channel *ch, uint8_t opcode,
                           uint8_t *args, int len)
{
    uint8_t nf = (opcode & 0x10) >> 4;
    uint8_t bs = opcode & 3;
    uint8_t lc = (opcode & 4) >> 2;

//    trace_pl330_dmalpend(nf, bs, lc, ch->lc[lc], ch->request_flag);

    if (bs == 2) {
        pl330_fault(ch, PL330_FAULT_OPERAND_INVALID);
        return;
    }
    if ((bs == 1 && ch->request_flag == PL330_BURST) ||
        (bs == 3 && ch->request_flag == PL330_SINGLE)) {
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
    uint8_t rd = args[0] & 7;
    uint32_t im;

    switch (rd) {
    case 0:
        ch->sar = im;
        break;
    case 1:
        ch->ccr = im;
        break;
    case 2:
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

    if (args[0] & 7) {
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
    uint8_t bs = opcode & 3;
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
    if ((bs == 1 && ch->request_flag == PL330_BURST) ||
        (bs == 3 && ch->request_flag == PL330_SINGLE)) {
        /* Perform NOP */
        return;
    }
    num = ((ch->ccr >> 18) & 0xf) + 1;
    size = (uint32_t)1 << ((ch->ccr >> 15) & 0x7);
    inc = !!((ch->ccr >> 14) & 1);
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
    pl330_dmast(ch, opcode, args, len);
}

static void pl330_dmastz(pl330::pl330_channel *ch, uint8_t opcode,
                         uint8_t *args, int len)
{
    uint32_t size, num;
    bool inc;

    num = ((ch->ccr >> 18) & 0xf) + 1;
    size = (uint32_t)1 << ((ch->ccr >> 15) & 0x7);
    inc = !!((ch->ccr >> 14) & 1);
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

    if (args[0] & 5) {
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
    uint8_t bs = opcode & 3;
    uint8_t periph_id;

    if (ch->is_manager) {
        pl330_fault(ch, PL330_FAULT_UNDEF_INSTR);
        return;
    }
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
    switch (bs) {
    case 0: /* S */
        ch->request_flag = PL330_SINGLE;
        ch->wfp_sbp = 0;
        break;
    case 1: /* P */
        ch->request_flag = PL330_BURST;
        ch->wfp_sbp = 2;
        break;
    case 2: /* B */
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

/* NULL terminated array of the instruction descriptions. */
static const vcml::arm::pl330::insn_descr insn_descr[] = {
    { .opcode = 0x54, .opmask = 0xFD, .size = 3, .exec = pl330_dmaaddh, },
    { .opcode = 0x5c, .opmask = 0xFD, .size = 3, .exec = pl330_dmaadnh, },
    { .opcode = 0x00, .opmask = 0xFF, .size = 1, .exec = pl330_dmaend, },
    { .opcode = 0x35, .opmask = 0xFF, .size = 2, .exec = pl330_dmaflushp, },
    { .opcode = 0xA0, .opmask = 0xFD, .size = 6, .exec = pl330_dmago, },
    { .opcode = 0x04, .opmask = 0xFC, .size = 1, .exec = pl330_dmald, },
    { .opcode = 0x25, .opmask = 0xFD, .size = 2, .exec = pl330_dmaldp, },
    { .opcode = 0x20, .opmask = 0xFD, .size = 2, .exec = pl330_dmalp, },
    /* dmastp  must be before dmalpend in this list, because their maps
     * are overlapping
     */
    { .opcode = 0x29, .opmask = 0xFD, .size = 2, .exec = pl330_dmastp, },
    { .opcode = 0x28, .opmask = 0xE8, .size = 2, .exec = pl330_dmalpend, },
    { .opcode = 0x01, .opmask = 0xFF, .size = 1, .exec = pl330_dmakill, },
    { .opcode = 0xBC, .opmask = 0xFF, .size = 6, .exec = pl330_dmamov, },
    { .opcode = 0x18, .opmask = 0xFF, .size = 1, .exec = pl330_dmanop, },
    { .opcode = 0x12, .opmask = 0xFF, .size = 1, .exec = pl330_dmarmb, },
    { .opcode = 0x34, .opmask = 0xFF, .size = 2, .exec = pl330_dmasev, },
    { .opcode = 0x08, .opmask = 0xFC, .size = 1, .exec = pl330_dmast, },
    { .opcode = 0x0C, .opmask = 0xFF, .size = 1, .exec = pl330_dmastz, },
    { .opcode = 0x36, .opmask = 0xFF, .size = 2, .exec = pl330_dmawfe, },
    { .opcode = 0x30, .opmask = 0xFC, .size = 2, .exec = pl330_dmawfp, },
    { .opcode = 0x13, .opmask = 0xFF, .size = 1, .exec = pl330_dmawmb, },
    { .opcode = 0x00, .opmask = 0x00, .size = 0, .exec = NULL, }
};

/* Instructions which can be issued via debug registers. */
static const vcml::arm::pl330::insn_descr debug_insn_desc[] = {
    { .opcode = 0xA0, .opmask = 0xFD, .size = 6, .exec = pl330_dmago, },
    { .opcode = 0x01, .opmask = 0xFF, .size = 1, .exec = pl330_dmakill, },
    { .opcode = 0x34, .opmask = 0xFF, .size = 2, .exec = pl330_dmasev, },
    { .opcode = 0x00, .opmask = 0x00, .size = 0, .exec = NULL, }
};
