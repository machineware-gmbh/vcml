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

#include "vcml/models/arm/pl011uart.h"

namespace vcml { namespace arm {

    SC_HAS_PROCESS(pl011uart);

    void pl011uart::poll() {
        if (!is_enabled() || !is_rx_enabled()) {
            next_trigger(m_enable);
            return;
        }

        backend* be = get_backend();
        if (be && be->peek()) {
            if (m_fifo.size() < m_fifo_size) {
                u8 val = 0;
                be->read(val);
                m_fifo.push((u16)val);
            }

            update();
        }

        sc_time cycle = sc_time(1.0 / clock, SC_SEC);
        sc_time quantum = tlm_global_quantum::instance().get();
        next_trigger(max(cycle, quantum));
    }

    void pl011uart::update() {
        // update flags
        FR &= ~(FR_RXFE | FR_RXFF | FR_TXFF);
        FR |= FR_TXFE; // tx FIFO is always empty
        if (m_fifo.empty())
            FR |= FR_RXFE;
        if (m_fifo.size() >= m_fifo_size)
            FR |= FR_RXFF;

        // update interrupts
        if (m_fifo.empty())
            RIS &= ~RIS_RX;
        else
            RIS |= RIS_RX;
        MIS = RIS & IMSC;
        if (MIS != 0 && !IRQ.read())
            log_debug("raising interrupt");
        if (MIS == 0 && IRQ.read())
            log_debug("clearing interrupt");
        IRQ.write(MIS != 0);
    }

    u16 pl011uart::read_DR() {
        u16 val = 0;
        if (!m_fifo.empty()) {
            val = m_fifo.front();
            m_fifo.pop();
        }

        DR  = val;
        RSR = (val >> RSR_O) & RSR_M;

        update();
        return val;
    }

    u16 pl011uart::write_DR(u16 val) {
        if (!is_tx_enabled())
            return DR;

        // Upper 8 bits of DR are used for encoding transmission errors, but
        // since those are not simulated, we just set them to zero.
        u8 val8 = val & 0xFF;
        get_backend()->write(val8);
        RIS |= RIS_TX;
        update();
        return val8;
    }

    u8 pl011uart::write_RSR(u8 val) {
        //  A write to this register clears the framing, parity, break,
        //  and overrun errors. The data value is not important.
        return 0;
    }

    u16 pl011uart::write_IBRD(u16 val) {
        return val & LCR_IBRD_M;
    }

    u16 pl011uart::write_FBRD(u16 val) {
        return val & LCR_FBRD_M;
    }

    u8 pl011uart::write_LCR(u8 val) {
        if ((val & LCR_FEN) && !(LCR & LCR_FEN))
            log_debug("FIFO enabled");
        if (!(val & LCR_FEN) && (LCR & LCR_FEN))
            log_debug("FIFO disabled");

        m_fifo_size = (val & LCR_FEN) ? VCML_ARM_PL011UART_FIFOSIZE : 1;
        return val & LCR_H_M;
    }

    u16 pl011uart::write_CR(u16 val) {
        if (!is_enabled() && (val & CR_UARTEN))
            log_debug("enabled");
        if (is_enabled() && !(val & CR_UARTEN))
            log_debug("disabled");
        if (!is_tx_enabled() && (val & CR_TXE))
            log_debug("transmitter enabled");
        if (is_tx_enabled() && !(val & CR_TXE))
            log_debug("transmitter disabled");
        if (!is_rx_enabled() && (val & CR_RXE))
            log_debug("receiver enabled");
        if (is_rx_enabled() && !(val & CR_RXE))
            log_debug("receiver disabled");

        m_enable.notify(SC_ZERO_TIME);
        return val;
    }

    u16 pl011uart::write_IFLS(u16 val) {
        return val & 0x3F; // TODO implement interrupt FIFO level select
    }

    u16 pl011uart::write_IMSC(u16 val) {
        IMSC = val & RIS_M;
        update();
        return IMSC;
    }

    u16 pl011uart::write_ICR(u16 val) {
        RIS &= ~(val & RIS_M);
        update();
        return 0;
    }

    pl011uart::pl011uart(const sc_module_name& nm):
        peripheral(nm),
        m_fifo_size(),
        m_fifo(),
        m_enable("enable"),
        DR   ("DR",    0x000, 0x0),
        RSR  ("RSR",   0x004, 0x0),
        FR   ("FR",    0x018, FR_TXFE | FR_RXFE),
        ILPR ("IPLR",  0x020, 0x0),
        IBRD ("IBRD",  0x024, 0x0),
        FBRD ("FBRD",  0x028, 0x0),
        LCR  ("LCR",   0x02C, 0x0),
        CR   ("CR",    0x030, CR_TXE | CR_RXE),
        IFLS ("IFLS",  0x034, 0x0),
        IMSC ("IMSC",  0x038, 0x0),
        RIS  ("RIS",   0x03C, 0x0),
        MIS  ("MIS",   0x040, 0x0),
        ICR  ("ICR",   0x044, 0x0),
        DMAC ("DMACR", 0x048, 0x0),
        PID  ("PID0",  0xFE0, 0x00000000),
        CID  ("CID0",  0xFF0, 0x00000000),
        clock("clock", VCML_ARM_PL011UART_CLK),
        IN("IN"),
        IRQ("IRQ") {
        DR.allow_read_write();
        DR.read = &pl011uart::read_DR;
        DR.write = &pl011uart::write_DR;

        RSR.allow_read_write();
        RSR.write = &pl011uart::write_RSR;

        FR.allow_read();

        ILPR.allow_read_write(); // not implemented

        IBRD.allow_read_write();
        IBRD.write = &pl011uart::write_IBRD;

        FBRD.allow_read_write();
        FBRD.write = &pl011uart::write_FBRD;

        LCR.allow_read_write();
        LCR.write = &pl011uart::write_LCR;

        CR.allow_read_write();
        CR.write = &pl011uart::write_CR;

        IFLS.allow_read_write();
        IFLS.write = &pl011uart::write_IFLS;

        IMSC.allow_read_write();
        IMSC.write = &pl011uart::write_IMSC;

        RIS.allow_read();
        MIS.allow_read();

        ICR.allow_write();
        ICR.write = &pl011uart::write_ICR;

        DMAC.allow_read_write(); // not implemented

        PID.allow_read();
        CID.allow_read();

        SC_METHOD(poll);

        reset();
    }

    pl011uart::~pl011uart() {
        // nothing to do
    }

    void pl011uart::reset() {
        DR   = 0x0;
        RSR  = 0x0;
        FR   = FR_TXFE | FR_RXFE;
        ILPR = 0x0;
        IBRD = 0x0;
        FBRD = 0x0;
        LCR  = 0x0;
        CR   = CR_TXE | CR_RXE;
        IFLS = 0x0;
        IMSC = 0x0;
        RIS  = 0x0;
        MIS  = 0x0;
        ICR  = 0x0;
        DMAC = 0x0;

        for (unsigned int i = 0; i < PID.num(); i++)
            PID[i] = (VCML_ARM_PL011UART_PID >> (i * 8)) & 0xFF;

        for (unsigned int i = 0; i < CID.num(); i++)
            CID[i] = (VCML_ARM_PL011UART_CID >> (i * 8)) & 0xFF;
    }

}}
