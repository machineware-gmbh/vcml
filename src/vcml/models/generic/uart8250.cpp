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

    void uart8250::update_divisor() {
        u32 divisor = m_divisor_msb << 8 | m_divisor_lsb;
        if (divisor == 0) {
            log_warn("zero baud divisor specified, reverting to default");
            divisor = clock / (16 * VCML_GENERIC_UART8250_DEFAULT_BAUD);
        }

        u32 baud = clock / (divisor * 16);
        log_debug("setup divisor %u (%u baud)", divisor, baud);
    }

    void uart8250::update() {
        u8 val;

        if ((m_rx_fifo.size() < m_rx_size) && beread(val)) {
            m_rx_fifo.push(val);
            LSR |= LSR_DR;
            if ((IER & IER_RDA) && !IRQ) {
                log_debug("data received, setting RDA interrupt");
                IRQ = true;
            }
        }

        while (!m_tx_fifo.empty()) {
            val = m_tx_fifo.front();
            m_tx_fifo.pop();
            bewrite(val);

            LSR |= LSR_THRE;
            if (m_tx_fifo.empty())
                LSR |= LSR_TEMT;
        }

        if (m_tx_fifo.empty() && (IER & IER_THRE) && !IRQ) {
            log_debug("transmitter empty, setting THRE interrupt");
            IRQ = true;
        }
    }

    void uart8250::poll() {
        update();

        // it does not make sense to poll multiple times during
        // a quantum, so if a quantum is set, only update once.
        u32 divisor = m_divisor_msb << 8 | m_divisor_lsb;
        sc_time cycle = sc_time(1.0 / clock, SC_SEC) * divisor;
        sc_time quantum = tlm_global_quantum::instance().get();
        next_trigger(max(cycle, quantum));
    }

    u8 uart8250::read_RBR() {
        if (LCR & LCR_DLAB)
            return m_divisor_lsb;

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
            return m_divisor_msb;
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

    u8 uart8250::write_THR(u8 val) {
        if (LCR & LCR_DLAB) {
            m_divisor_lsb = val;
            update_divisor();
            return THR;
        }

        m_tx_fifo.push(val);
        LSR &= ~LSR_TEMT;
        if (m_tx_fifo.size() == m_tx_size)
            LSR &= ~LSR_THRE;

        update();

        return val;
    }

    u8 uart8250::write_IER(u8 val) {
        if (LCR & LCR_DLAB) {
            m_divisor_msb = val;
            update_divisor();
            return IER;
        }

        VCML_LOG_REG_BIT_CHANGE(IER_RDA,  IER, val);
        VCML_LOG_REG_BIT_CHANGE(IER_THRE, IER, val);
        VCML_LOG_REG_BIT_CHANGE(IER_RLS,  IER, val);
        VCML_LOG_REG_BIT_CHANGE(IER_MST,  IER, val);

        IER = val & 0xF;
        update();
        return val;
    }

    u8 uart8250::write_LCR(u8 val) {
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

        return val;
    }

    u8 uart8250::write_FCR(u8 val) {
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
        default: break;
        }

        return IIR;
    }

    SC_HAS_PROCESS(uart8250);

    uart8250::uart8250(const sc_module_name& nm):
        peripheral(nm),
        m_rx_size(1),
        m_tx_size(1),
        m_rx_fifo(),
        m_tx_fifo(),
        m_divisor_msb(0),
        m_divisor_lsb(0),
        THR("THR", 0x0, 0x00),
        IER("IER", 0x1, 0x00),
        IIR("IIR", 0x2, IIR_NOIP),
        LCR("LCR", 0x3, 0x00),
        MCR("MCR", 0x4, 0x00),
        LSR("LSR", 0x5, LSR_THRE | LSR_TEMT),
        MSR("MSR", 0x6, 0x00),
        SCR("SCR", 0x7, 0x00),
        IRQ("IRQ"),
        IN("IN"),
        clock("clock", 3686400) { // 3.6864MHz

        u16 divider = clock / (16 * VCML_GENERIC_UART8250_DEFAULT_BAUD);
        m_divisor_msb = divider >> 8;
        m_divisor_lsb = divider & 0xf;

        THR.allow_read_write();
        THR.read = &uart8250::read_RBR;
        THR.write = &uart8250::write_THR;

        IER.allow_read_write();
        IER.read = &uart8250::read_IER;
        IER.write = &uart8250::write_IER;

        IIR.allow_read_write();
        IIR.read = &uart8250::read_IIR;
        IIR.write = &uart8250::write_FCR;

        LCR.allow_read_write();
        LCR.write = &uart8250::write_LCR;

        LSR.allow_read();
        MCR.allow_read_write();
        MSR.allow_read_write();
        SCR.allow_read_write();

        SC_METHOD(poll);
    }

    uart8250::~uart8250() {
        /* nothing to do */
    }

}}
