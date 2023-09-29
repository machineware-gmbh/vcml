/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/cdns.h"

namespace vcml {
namespace serial {

enum cr_bits : u32 {
    CR_SRR = bit(0),
    CR_SRT = bit(1),
    CR_RE = bit(2),
    CR_RD = bit(3),
    CR_TE = bit(4),
    CR_TD = bit(5),
    CR_RT = bit(6),
    CR_SB = bit(7),
    CR_TB = bit(8),

    CR_RESET = CR_RD | CR_TD | CR_TB,
    CR_MASK = CR_RE | CR_RD | CR_TE | CR_TD | CR_RT | CR_SB | CR_TB,
};

using MR_CHLN = field<1, 2, u32>;
using MR_PARITY = field<3, 3, u32>;
using MR_SBITS = field<6, 2, u32>;
using MR_CM = field<8, 2, u32>;

enum channel_mode : u32 {
    CM_NORMAL = 0,
    CM_ECHO = 1,
    CM_LOCAL_LOOPBACK = 2,
    CM_REMOTE_LOOPBACK = 3,
};

static const char* mode_name(u32 mode) {
    switch (mode) {
    case CM_NORMAL:
        return "normal";
    case CM_ECHO:
        return "echo";
    case CM_LOCAL_LOOPBACK:
        return "local-loopback";
    case CM_REMOTE_LOOPBACK:
        return "remote-loopback";
    default:
        return "invalid";
    }
}

enum mr_bits : u32 {
    MR_CL = bit(0),
    MR_MASK = MR_CL | MR_CHLN::MASK | MR_PARITY::MASK | MR_SBITS::MASK |
              MR_CM::MASK,
};

enum irq_bits : u32 {
    IRQ_RTR = bit(0), // receiver reached trigger level
    IRQ_RE = bit(1),  // receiver empty
    IRQ_RF = bit(2),  // receiver full
    IRQ_TE = bit(3),  // transmitter empty
    IRQ_TF = bit(4),  // transmitter full
    IRQ_RO = bit(5),  // receiver overflow
    IRQ_RFR = bit(6), // receiver frame error
    IRQ_RP = bit(7),  // receiver parity error
    IRQ_RT = bit(8),  // receiver timeout
    IRQ_MS = bit(9),  // modem status indicator
    IRQ_TI = bit(10), // transmitter reacher trigger level
    IRQ_NF = bit(11), // transmitter nearly full
    IRQ_TO = bit(12), // transmitter overflow
    IRQ_MASK = IRQ_RTR | IRQ_RE | IRQ_RF | IRQ_TE | IRQ_TF | IRQ_RO | IRQ_RFR |
               IRQ_RP | IRQ_RT | IRQ_MS | IRQ_TI | IRQ_NF | IRQ_TO,
};

using BRGR_BAUDGEN = field<0, 16, u32>;
using BDIV_BAUDDIV = field<0, 8, u32>;
using RTOR_TIMEOUT = field<0, 8, u32>;
using RTRIG_RFIFOLVL = field<0, 6, u32>;

enum sr_bits : u32 {
    SR_RTS = bit(0),
    SR_RES = bit(1),
    SR_RFS = bit(2),
    SR_TES = bit(3),
    SR_TFS = bit(4),
    SR_RSS = bit(10),
    SR_TSS = bit(11),
    SR_RDS = bit(12),
    SR_FTS = bit(13),
    SR_FS = bit(14),
};

static serial_parity decode_parity(u32 mr) {
    switch (get_field<MR_PARITY>(mr)) {
    case 0:
        return SERIAL_PARITY_EVEN;
    case 1:
        return SERIAL_PARITY_ODD;
    default:
        return SERIAL_PARITY_NONE;
    }
}

static serial_bits decode_data_bits(u32 mr) {
    switch (get_field<MR_CHLN>(mr)) {
    case 2:
        return SERIAL_7_BITS;
    case 3:
        return SERIAL_6_BITS;
    default:
        return SERIAL_8_BITS;
    }
}

static serial_stop decode_stop_bits(u32 mr) {
    if (get_field<MR_SBITS>(mr) == 3)
        return SERIAL_STOP_1;
    else
        return SERIAL_STOP_2;
}

static void clear_fifo(queue<u8>& ff) {
    while (!ff.empty())
        ff.pop();
}

void cdns::push_rxff(u8 val) {
    if ((cr & CR_RD) || !(cr & CR_RE))
        return;

    if (m_rxff.size() < rxff_size)
        m_rxff.push(val);
    else
        isr |= IRQ_RO;

    // we are supposed to wait for a rtor * baud cycles after we have received
    // a character before raising the receiver timeout interrupt in order to
    // allow the rxfifo to collect some data first and to prevent spamming the
    // cpu with interrupts; but since we do not know how fast the simulation
    // will be running in the end, we just notify the cpu directly.
    if (rtor > 0u)
        isr |= IRQ_RT;

    update_irq();
}

void cdns::push_txff(u8 val) {
    if ((cr & CR_TD) || !(cr & CR_TE))
        return;

    if (m_txff.size() < txff_size)
        m_txff.push(val);
    else
        isr |= IRQ_TO;

    update_irq();

    m_txev.notify(SC_ZERO_TIME);
}

void cdns::write_cr(u32 val) {
    if (val & CR_SRR)
        clear_fifo(m_rxff);
    if (val & CR_SRT)
        clear_fifo(m_txff);

    cr = (val & CR_MASK) | (cr & ~CR_MASK);
}

void cdns::write_mr(u32 val) {
    u32 prev_mode = mr.get_field<MR_CM>();
    u32 next_mode = get_field<MR_CM>(val);
    if (prev_mode != next_mode)
        log_info("switching to mode %s", mode_name(next_mode));

    mr = (val & MR_MASK) | (mr & ~MR_MASK);

    hz_t baud = clk;
    if (mr & MR_CL)
        baud /= 8;

    hz_t divider = bdiv + 1;
    if (brgr > 0u)
        divider *= brgr;

    serial_tx.set_baud(baud / divider);
    serial_tx.set_parity(decode_parity(mr));
    serial_tx.set_data_width(decode_data_bits(mr));
    serial_tx.set_stop_bits(decode_stop_bits(mr));
}

void cdns::write_ier(u32 val) {
    imr |= (val & IRQ_MASK);
    update_irq();
}

void cdns::write_idr(u32 val) {
    imr &= ~(val & IRQ_MASK);
    update_irq();
}

void cdns::write_isr(u32 val) {
    isr &= ~(val & IRQ_MASK);
    update_irq();
}

void cdns::write_txrx(u32 val) {
    u32 mode = mr.get_field<MR_CM>();
    if (mode == CM_NORMAL)
        push_txff(val);
    if (mode == CM_LOCAL_LOOPBACK)
        push_rxff(val);
}

u32 cdns::read_txrx() {
    if (m_rxff.empty())
        return 0;

    u8 data = m_rxff.front();
    m_rxff.pop();
    update_irq();

    return data;
}

void cdns::tx_thread() {
    while (true) {
        while (m_txff.empty())
            wait(m_txev);

        do {
            serial_tx.send(m_txff.front());
            m_txff.pop();
            update_irq();
        } while (!m_txff.empty());
    }
}

void cdns::update_irq() {
    sr.set_bit<SR_RES>(m_rxff.empty());
    sr.set_bit<SR_RTS>(m_rxff.size() >= rtrig);
    sr.set_bit<SR_RFS>(m_rxff.size() == rxff_size);

    sr.set_bit<SR_TES>(m_txff.empty());
    sr.set_bit<SR_FTS>(m_txff.size() >= ttrig);
    sr.set_bit<SR_TFS>(m_txff.size() == txff_size);

    if (sr & SR_RTS)
        isr |= IRQ_RTR;
    if (sr & SR_RES)
        isr |= IRQ_RE;
    if (sr & SR_RFS)
        isr |= IRQ_RF;
    if (sr & SR_FTS)
        isr |= IRQ_TI;
    if (sr & SR_TES)
        isr |= IRQ_TE;
    if (sr & SR_TFS)
        isr |= IRQ_TF;

    irq = isr & imr;
}

void cdns::serial_receive(u8 data) {
    u32 mode = mr.get_field<MR_CM>();
    if (mode == CM_NORMAL || mode == CM_ECHO)
        push_rxff(data);
    if (mode == CM_REMOTE_LOOPBACK || mode == CM_ECHO)
        push_txff(data);
}

cdns::cdns(const sc_module_name& nm):
    peripheral(nm),
    m_rxff(),
    m_txff(),
    m_txev("txev"),
    rxff_size("rxff_size", 16),
    txff_size("txff_size", 16),
    cr("cr", 0x0, CR_RESET),
    mr("mr", 0x4, 0),
    ier("ier", 0x8, 0),
    idr("idr", 0xc, 0),
    imr("imr", 0x10, 0),
    isr("isr", 0x14, 0),
    brgr("brgr", 0x18, 4747),
    rtor("rtor", 0x1c),
    rtrig("rtrig", 0x20, rxff_size),
    mcr("mcr", 0x24),
    msr("msr", 0x28),
    sr("sr", 0x2c),
    txrx("txrx", 0x30),
    bdiv("bdiv", 0x34, 15),
    fdel("fdel", 0x38),
    pmin("pmin", 0x3c),
    pwid("pwid", 0x40),
    ttrig("ttrig", 0x44, txff_size),
    in("in"),
    irq("irq"),
    serial_tx("serial_tx"),
    serial_rx("serial_rx") {
    cr.sync_always();
    cr.allow_read_write();
    cr.on_write(&cdns::write_cr);

    mr.sync_always();
    mr.allow_read_write();
    mr.on_write(&cdns::write_mr);

    ier.sync_always();
    ier.allow_write_only();
    ier.on_write(&cdns::write_ier);

    idr.sync_always();
    idr.allow_write_only();
    idr.on_write(&cdns::write_idr);

    imr.sync_always();
    imr.allow_read_only();

    isr.sync_always();
    isr.allow_read_write();
    isr.on_write(&cdns::write_isr);

    brgr.sync_always();
    brgr.allow_read_write();
    brgr.on_write_mask(BRGR_BAUDGEN::MASK);

    rtor.sync_always();
    rtor.allow_read_write();
    rtor.on_write_mask(RTOR_TIMEOUT::MASK);

    rtrig.sync_always();
    rtrig.allow_read_write();
    rtrig.on_write_mask(RTRIG_RFIFOLVL::MASK);

    mcr.sync_always();
    mcr.allow_read_write();

    msr.sync_always();
    msr.allow_read_write();

    sr.sync_always();
    sr.allow_read_only();

    txrx.sync_always();
    txrx.allow_read_write();
    txrx.on_read(&cdns::read_txrx);
    txrx.on_write(&cdns::write_txrx);

    bdiv.sync_always();
    bdiv.allow_read_write();
    bdiv.on_write_mask(BDIV_BAUDDIV::MASK);

    fdel.sync_always();
    fdel.allow_read_only();

    pmin.sync_always();
    pmin.allow_read_only();

    pwid.sync_always();
    pwid.allow_read_only();

    ttrig.sync_always();
    ttrig.allow_read_only();

    SC_HAS_PROCESS(cdns);
    SC_THREAD(tx_thread);
    sensitive << m_txev;
}

cdns::~cdns() {
    // nothing
}

void cdns::reset() {
    peripheral::reset();

    clear_fifo(m_rxff);
    clear_fifo(m_txff);

    update_irq();
}

} // namespace serial
} // namespace vcml
