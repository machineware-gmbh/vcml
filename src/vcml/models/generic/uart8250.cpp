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

    SC_HAS_PROCESS(uart8250);

    uart8250::uart8250(const sc_module_name& nm):
        peripheral(nm),
        m_divisor_msb(0),
        m_divisor_lsb(0),
        m_rx_ready(false),
        m_tx_empty(true),
        m_size(1),
        m_fifo(),
        THR("THR", 0x0, 0x00),
        IER("IER", 0x1, 0x00),
        IIR("IIR", 0x2, 0x00),
        LCR("LCR", 0x3, 0x00),
        MCR("MCR", 0x4, 0x00),
        LSR("LSR", 0x5, 0x00),
        MSR("MSR", 0x6, 0x00),
        SCR("SCR", 0x7, 0x00),
        IRQ("IRQ"),
        IN("IN"),
        clock("clock", 20000000) { // 20MHz polling frequency for input

        THR.allow_read_write();
        THR.read = &uart8250::read_RBR;
        THR.write = &uart8250::write_THR;

        IER.allow_read_write();
        IER.read = &uart8250::read_IER;
        IER.write = &uart8250::write_IER;

        IIR.allow_read_write();
        IIR.read = &uart8250::read_IIR;
        IIR.write = &uart8250::write_FCR;

        LSR.allow_read();
        LSR.read = &uart8250::read_LSR;
        LSR = LSR_THRE | LSR_TEMT;

        LCR.allow_read_write();
        MCR.allow_read_write();
        MSR.allow_read_write();
        SCR.allow_read_write();

        SC_METHOD(poll);
    }

    uart8250::~uart8250() {
        /* nothing to do */
    }


    void uart8250::update() {
        u8 val;
        if (beread(val)) {
            if (m_fifo.size() < m_size)
                m_fifo.push(val);
            else
                log_debug("FIFO full, dropping character 0x%02x", (int)val);
        }

        if ((m_rx_ready = !m_fifo.empty()))
            LSR |= LSR_DR;
        else
            LSR &= ~LSR_DR;

        IIR = IIR_NOIP;
        if ((m_tx_empty) && (IER & IER_THRE))
            IIR = IIR_THRE;
        if ((m_rx_ready) && (IER & IER_RDA))
            IIR = IIR_RDA;
        IRQ = (IIR != IIR_NOIP);
    }

    void uart8250::poll() {
        update();

        // it does not make sense to poll multiple times during
        // a quantum, so if a quantum is set, only update once.
        sc_time cycle = sc_time(1.0 / clock, SC_SEC);
        sc_time quantum = tlm_global_quantum::instance().get();
        next_trigger(max(cycle, quantum));
    }

    u8 uart8250::read_RBR() {
        if (LCR & LCR_DLAB)
            return m_divisor_lsb;

        if (m_fifo.empty())
            return 0;

        u8 val = m_fifo.front();
        m_fifo.pop();
        update();
        return val;
    }

    u8 uart8250::read_IER() {
        if (LCR & LCR_DLAB)
            return m_divisor_msb;
        return IER;
    }

    u8 uart8250::read_IIR() {
        u8 val = IIR;
        m_tx_empty = false;
        update();
        return val;
    }

    u8 uart8250::read_LSR() {
        if (m_rx_ready)
            LSR |= LSR_DR;
        else
            LSR &= ~LSR_DR;
        return LSR;
    }

    u8 uart8250::write_THR(u8 val) {
        if (LCR & LCR_DLAB) {
            m_divisor_lsb = val;
            return THR;
        }

        bewrite(val);
        m_tx_empty = true;
        update();
        return val;
    }

    u8 uart8250::write_IER(u8 val) {
        if (LCR & LCR_DLAB) {
            m_divisor_msb = val;
            return IER;
        }

        IER = val;
        update();
        return val;
    }

    u8 uart8250::write_FCR(u8 val) {
        return val;
    }

}}
