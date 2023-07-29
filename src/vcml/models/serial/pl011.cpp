/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/serial/pl011.h"

namespace vcml {
namespace serial {

void pl011::serial_receive(u8 data) {
    if (is_enabled() || is_rx_enabled()) {
        if (m_fifo.size() < m_fifo_size)
            m_fifo.push(data);
        update();
    }
}

void pl011::update() {
    // update flags
    fr &= ~(FR_RXFE | FR_RXFF | FR_TXFF);
    fr |= FR_TXFE; // tx FIFO is always empty
    if (m_fifo.empty())
        fr |= FR_RXFE;
    if (m_fifo.size() >= m_fifo_size)
        fr |= FR_RXFF;

    // update interrupts
    if (m_fifo.empty())
        ris &= ~RIS_RX;
    else
        ris |= RIS_RX;
    mis = ris & imsc;
    if (mis != 0 && !irq.read())
        log_debug("raising interrupt");
    if (mis == 0 && irq.read())
        log_debug("clearing interrupt");
    irq.write(mis != 0);
}

u16 pl011::read_dr() {
    u16 val = 0;
    if (!m_fifo.empty()) {
        val = m_fifo.front();
        m_fifo.pop();
    }

    dr = val;
    rsr = (val >> RSR_O) & RSR_M;

    update();
    return val;
}

void pl011::write_dr(u16 val) {
    if (!is_tx_enabled())
        return;

    // Upper 8 bits of DR are used for encoding transmission errors, but
    // since those are not simulated, we just set them to zero.
    dr = val & 0x00ff;
    ris |= RIS_TX;
    serial_tx.send(dr);
    update();
}

void pl011::write_rsr(u8 val) {
    //  A write to this register clears the framing, parity, break,
    //  and overrun errors. The data value is not important.
}

void pl011::write_ibrd(u16 val) {
    ibrd = val & LCR_IBRD_M;
}

void pl011::write_fbrd(u16 val) {
    fbrd = val & LCR_FBRD_M;
}

void pl011::write_lcr(u8 val) {
    if ((val & LCR_FEN) && !(lcr & LCR_FEN))
        log_debug("FIFO enabled");
    if (!(val & LCR_FEN) && (lcr & LCR_FEN))
        log_debug("FIFO disabled");

    m_fifo_size = (val & LCR_FEN) ? FIFOSIZE : 1;

    lcr = val & LCR_H_M;
}

void pl011::write_cr(u16 val) {
    if (!is_enabled() && (val & CR_UARTEN))
        log_debug("device enabled");
    if (is_enabled() && !(val & CR_UARTEN))
        log_debug("device disabled");
    if (!is_tx_enabled() && (val & CR_TXE))
        log_debug("transmitter enabled");
    if (is_tx_enabled() && !(val & CR_TXE))
        log_debug("transmitter disabled");
    if (!is_rx_enabled() && (val & CR_RXE))
        log_debug("receiver enabled");
    if (is_rx_enabled() && !(val & CR_RXE))
        log_debug("receiver disabled");

    cr = val;
}

void pl011::write_ifls(u16 val) {
    ifls = val & 0x3f; // TODO implement interrupt FIFO level select
}

void pl011::write_imsc(u16 val) {
    imsc = val & RIS_M;
    update();
}

void pl011::write_icr(u16 val) {
    ris &= ~(val & RIS_M);
    icr = 0;
    update();
}

pl011::pl011(const sc_module_name& nm):
    peripheral(nm),
    serial_host(),
    m_fifo_size(),
    m_fifo(),
    dr("dr", 0x000, 0x0),
    rsr("rsr", 0x004, 0x0),
    fr("fr", 0x018, FR_TXFE | FR_RXFE),
    ilpr("ilpr", 0x020, 0x0),
    ibrd("ibrd", 0x024, 0x0),
    fbrd("fbrd", 0x028, 0x0),
    lcr("lcr", 0x02C, 0x0),
    cr("cr", 0x030, CR_TXE | CR_RXE),
    ifls("ifls", 0x034, 0x0),
    imsc("imsc", 0x038, 0x0),
    ris("ris", 0x03C, 0x0),
    mis("mis", 0x040, 0x0),
    icr("icr", 0x044, 0x0),
    dmac("dmac", 0x048, 0x0),
    pid("pid", 0xFE0, 0x00000000),
    cid("cid", 0xFF0, 0x00000000),
    in("in"),
    irq("irq"),
    serial_tx("serial_tx"),
    serial_rx("serial_rx") {
    dr.sync_always();
    dr.allow_read_write();
    dr.on_read(&pl011::read_dr);
    dr.on_write(&pl011::write_dr);

    rsr.sync_always();
    rsr.allow_read_write();
    rsr.on_write(&pl011::write_rsr);

    fr.sync_always();
    fr.allow_read_only();

    ilpr.sync_never();
    ilpr.allow_read_write(); // not implemented

    ibrd.sync_always();
    ibrd.allow_read_write();
    ibrd.on_write(&pl011::write_ibrd);

    fbrd.sync_always();
    fbrd.allow_read_write();
    fbrd.on_write(&pl011::write_fbrd);

    lcr.sync_always();
    lcr.allow_read_write();
    lcr.on_write(&pl011::write_lcr);

    cr.sync_always();
    cr.allow_read_write();
    cr.on_write(&pl011::write_cr);

    ifls.sync_always();
    ifls.allow_read_write();
    ifls.on_write(&pl011::write_ifls);

    imsc.sync_always();
    imsc.allow_read_write();
    imsc.on_write(&pl011::write_imsc);

    ris.sync_always();
    ris.allow_read_only();

    mis.sync_always();
    mis.allow_read_only();

    icr.sync_always();
    icr.allow_write_only();
    icr.on_write(&pl011::write_icr);

    dmac.sync_never();
    dmac.allow_read_write(); // not implemented

    pid.sync_never();
    pid.allow_read_only();

    cid.sync_never();
    cid.allow_read_only();
}

pl011::~pl011() {
    // nothing to do
}

void pl011::reset() {
    peripheral::reset();

    for (unsigned int i = 0; i < pid.count(); i++)
        pid[i] = (AMBA_PID >> (i * 8)) & 0xff;

    for (unsigned int i = 0; i < cid.count(); i++)
        cid[i] = (AMBA_CID >> (i * 8)) & 0xff;

    while (!m_fifo.empty())
        m_fifo.pop();

    irq = false;
}

VCML_EXPORT_MODEL(vcml::serial::pl011, name, args) {
    return new pl011(name);
}

} // namespace serial
} // namespace vcml
