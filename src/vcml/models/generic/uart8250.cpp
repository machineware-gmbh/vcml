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

namespace vcml { namespace generic {

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
            LSR |= LSR_DR;
        }

        while (!m_tx_fifo.empty()) {
            val = m_tx_fifo.front();
            m_tx_fifo.pop();
            serial_out(val);

            LSR |= LSR_THRE;
            if (m_tx_fifo.empty())
                LSR |= LSR_TEMT;
        }

        if (m_tx_fifo.empty() && (IER & IER_THRE)) {
            log_debug("transmitter empty, setting THRE interrupt");
            IRQ = true;
        }

        if (!m_rx_fifo.empty() && (IER & IER_RDA)) {
            log_debug("data received, setting RDA interrupt");
            IRQ = true;
        }
    }

    void uart8250::poll() {
        update();

        // it does not make sense to poll multiple times during
        // a quantum, so if a quantum is set, only update once.
        sc_time cycle = clock_cycle() * m_divisor;
        sc_time quantum = tlm_global_quantum::instance().get();
        next_trigger(max(cycle, quantum));
    }

    u8 uart8250::read_RBR() {
        if (LCR & LCR_DLAB)
            return m_divisor & 0xff;

        if (m_rx_fifo.empty())
            return 0;

        u8 val = m_rx_fifo.front();
        m_rx_fifo.pop();
        if (m_rx_fifo.empty()) {
            LSR &= ~LSR_DR;
            if ((IER & IER_RDA) && IRQ) {
                log_debug("received data fetched, clearing RDA interrupt");
                IRQ = false;
            }
        }

        update();
        return val;
    }

    u8 uart8250::read_IER() {
        if (LCR & LCR_DLAB)
            return m_divisor >> 8;
        return IER;
    }

    u8 uart8250::read_IIR() {
        if (!IRQ.read())
            return IIR_NOIP;

        if (!m_rx_fifo.empty())
            return IIR_RDA;

        log_debug("IIR read, clearing THRE interrupt");
        IRQ = false;
        return IIR_THRE;
    }

    void uart8250::write_THR(u8 val) {
        if (LCR & LCR_DLAB) {
            insert(m_divisor, 0, 8, val);
            calibrate();
            return;
        }

        m_tx_fifo.push(val);
        LSR &= ~LSR_TEMT;
        if (m_tx_fifo.size() == m_tx_size)
            LSR &= ~LSR_THRE;

        THR = val;
        update();
    }

    void uart8250::write_IER(u8 val) {
        if (LCR & LCR_DLAB) {
            insert(m_divisor, 8, 8, val);
            calibrate();
            return;
        }

        VCML_LOG_REG_BIT_CHANGE(IER_RDA,  IER, val);
        VCML_LOG_REG_BIT_CHANGE(IER_THRE, IER, val);
        VCML_LOG_REG_BIT_CHANGE(IER_RLS,  IER, val);
        VCML_LOG_REG_BIT_CHANGE(IER_MST,  IER, val);

        IER = val & 0xF;
        update();
    }

    void uart8250::write_LCR(u8 val) {
        int oldwl = (LCR & 0x3) + 5;
        int newwl = (val & 0x3) + 5;
        if (newwl != oldwl)
            log_debug("word length %d bits", newwl);

        VCML_LOG_REG_BIT_CHANGE(LCR_STP, LCR, val);
        VCML_LOG_REG_BIT_CHANGE(LCR_PEN, LCR, val);
        VCML_LOG_REG_BIT_CHANGE(LCR_EPS, LCR, val);
        VCML_LOG_REG_BIT_CHANGE(LCR_SPB, LCR, val);
        VCML_LOG_REG_BIT_CHANGE(LCR_BCB, LCR, val);
        VCML_LOG_REG_BIT_CHANGE(LCR_DLAB, LCR, val);

        LCR = val;
    }

    void uart8250::write_FCR(u8 val) {
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
        case FCR_IT1:  log_debug("interrupt threshold 1 byte"); break;
        case FCR_IT4:  log_debug("interrupt threshold 4 bytes"); break;
        case FCR_IT8:  log_debug("interrupt threshold 8 bytes"); break;
        case FCR_IT14: log_debug("interrupt threshold 14 bytes"); break;
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
        THR("THR", 0x0, 0x00),
        IER("IER", 0x1, 0x00),
        IIR("IIR", 0x2, IIR_NOIP),
        LCR("LCR", 0x3, 0x00),
        MCR("MCR", 0x4, 0x00),
        LSR("LSR", 0x5, LSR_THRE | LSR_TEMT),
        MSR("MSR", 0x6, 0x00),
        SCR("SCR", 0x7, 0x00),
        IRQ("IRQ"),
        IN("IN") {

        THR.allow_read_write();
        THR.on_read(&uart8250::read_RBR);
        THR.on_write(&uart8250::write_THR);

        IER.allow_read_write();
        IER.on_read(&uart8250::read_IER);
        IER.on_write(&uart8250::write_IER);

        IIR.allow_read_write();
        IIR.on_read(&uart8250::read_IIR);
        IIR.on_write(&uart8250::write_FCR);

        LCR.allow_read_write();
        LCR.on_write(&uart8250::write_LCR);

        LSR.allow_read_only();
        MCR.allow_read_write();
        MSR.allow_read_write();
        SCR.allow_read_write();

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

}}
