/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/generic/uart8250.h"

namespace vcml {
namespace generic {

void uart8250::calibrate() {
    if (m_divisor == 0) {
        log_warn("zero baud divisor specified, reverting to default");
        m_divisor = clock_hz() / (16 * DEFAULT_BAUD);
    }

    u32 baud = clock_hz() / (m_divisor * 16);
    log_debug("setup divisor %u (%u baud)", m_divisor, baud);
}

void uart8250::update() {
    u8 val;

    if ((m_rx_fifo.size() < m_rx_size) && serial_in(val)) {
        m_rx_fifo.push(val);
        lsr |= LSR_DR;
    }

    while (!m_tx_fifo.empty()) {
        val = m_tx_fifo.front();
        m_tx_fifo.pop();
        serial_out(val);

        lsr |= LSR_THRE;
        if (m_tx_fifo.empty())
            lsr |= LSR_TEMT;
    }

    if (m_tx_fifo.empty() && (ier & IER_THRE)) {
        log_debug("transmitter empty, setting THRE interrupt");
        irq = true;
    }

    if (!m_rx_fifo.empty() && (ier & IER_RDA)) {
        log_debug("data received, setting RDA interrupt");
        irq = true;
    }
}

void uart8250::poll() {
    update();

    // it does not make sense to poll multiple times during
    // a quantum, so if a quantum is set, only update once.
    sc_time cycle   = clock_cycle() * m_divisor;
    sc_time quantum = tlm_global_quantum::instance().get();
    next_trigger(max(cycle, quantum));
}

u8 uart8250::read_rbr() {
    if (lcr & LCR_DLAB)
        return m_divisor & 0xff;

    if (m_rx_fifo.empty())
        return 0;

    u8 val = m_rx_fifo.front();
    m_rx_fifo.pop();
    if (m_rx_fifo.empty()) {
        lsr &= ~LSR_DR;
        if ((ier & IER_RDA) && irq) {
            log_debug("received data fetched, clearing RDA interrupt");
            irq = false;
        }
    }

    update();
    return val;
}

u8 uart8250::read_ier() {
    if (lcr & LCR_DLAB)
        return m_divisor >> 8;
    return ier;
}

u8 uart8250::read_iir() {
    if (!irq.read())
        return IIR_NOIP;

    if (!m_rx_fifo.empty())
        return IIR_RDA;

    log_debug("IIR read, clearing THRE interrupt");
    irq = false;
    return IIR_THRE;
}

void uart8250::write_thr(u8 val) {
    if (lcr & LCR_DLAB) {
        insert(m_divisor, 0, 8, val);
        calibrate();
        return;
    }

    m_tx_fifo.push(val);
    lsr &= ~LSR_TEMT;
    if (m_tx_fifo.size() == m_tx_size)
        lsr &= ~LSR_THRE;

    thr = val;
    update();
}

void uart8250::write_ier(u8 val) {
    if (lcr & LCR_DLAB) {
        insert(m_divisor, 8, 8, val);
        calibrate();
        return;
    }

    VCML_LOG_REG_BIT_CHANGE(IER_RDA, ier, val);
    VCML_LOG_REG_BIT_CHANGE(IER_THRE, ier, val);
    VCML_LOG_REG_BIT_CHANGE(IER_RLS, ier, val);
    VCML_LOG_REG_BIT_CHANGE(IER_MST, ier, val);

    ier = val & 0xF;
    update();
}

void uart8250::write_lcr(u8 val) {
    int oldwl = (lcr & 0x3) + 5;
    int newwl = (val & 0x3) + 5;
    if (newwl != oldwl)
        log_debug("word length %d bits", newwl);

    VCML_LOG_REG_BIT_CHANGE(LCR_STP, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_PEN, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_EPS, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_SPB, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_BCB, lcr, val);
    VCML_LOG_REG_BIT_CHANGE(LCR_DLAB, lcr, val);

    lcr = val;
}

void uart8250::write_fcr(u8 val) {
    log_debug("FIFOs %sabled", val & FCR_FE ? "en" : "dis");

    if (val & FCR_CRF) {
        while (!m_rx_fifo.empty())
            m_rx_fifo.pop();
        log_debug("receiver FIFO cleared");
    }

    if (val & FCR_CTF) {
        while (!m_tx_fifo.empty())
            m_tx_fifo.pop();
        log_debug("transmitter FIFO cleared");
    }

    if (val & FCR_DMA)
        log_debug("FCR_DMA bit set");

    switch (val & 0b11000000) {
    case FCR_IT1:
        log_debug("interrupt threshold 1 byte");
        break;
    case FCR_IT4:
        log_debug("interrupt threshold 4 bytes");
        break;
    case FCR_IT8:
        log_debug("interrupt threshold 8 bytes");
        break;
    case FCR_IT14:
        log_debug("interrupt threshold 14 bytes");
        break;
    default:
        break;
    }
}

uart8250::uart8250(const sc_module_name& nm):
    peripheral(nm),
    serial::port(),
    m_rx_size(1),
    m_tx_size(1),
    m_rx_fifo(),
    m_tx_fifo(),
    m_divisor(1),
    thr("thr", 0x0, 0x00),
    ier("ier", 0x1, 0x00),
    iir("iir", 0x2, IIR_NOIP),
    lcr("lcr", 0x3, 0x00),
    mcr("mcr", 0x4, 0x00),
    lsr("lsr", 0x5, LSR_THRE | LSR_TEMT),
    msr("msr", 0x6, 0x00),
    scr("scr", 0x7, 0x00),
    irq("irq"),
    in("in") {
    thr.allow_read_write();
    thr.on_read(&uart8250::read_rbr);
    thr.on_write(&uart8250::write_thr);

    ier.allow_read_write();
    ier.on_read(&uart8250::read_ier);
    ier.on_write(&uart8250::write_ier);

    iir.allow_read_write();
    iir.on_read(&uart8250::read_iir);
    iir.on_write(&uart8250::write_fcr);

    lcr.allow_read_write();
    lcr.on_write(&uart8250::write_lcr);

    lsr.allow_read_only();
    mcr.allow_read_write();
    msr.allow_read_write();
    scr.allow_read_write();

    SC_HAS_PROCESS(uart8250);
    SC_METHOD(poll);
}

uart8250::~uart8250() {
    // nothing to do
}

void uart8250::reset() {
    peripheral::reset();
    m_divisor = clock_hz() / (16 * DEFAULT_BAUD);
}

} // namespace generic
} // namespace vcml
