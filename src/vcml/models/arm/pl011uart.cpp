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

        u8 val;
        if (serial_in(val)) {
            if (m_fifo.size() < m_fifo_size)
                m_fifo.push((u16)val);
            update();
        }

        sc_time cycle = clock_cycle();
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

    void pl011uart::write_DR(u16 val) {
        if (!is_tx_enabled())
            return;

        // Upper 8 bits of DR are used for encoding transmission errors, but
        // since those are not simulated, we just set them to zero.
        DR = val & 0x00ff;
        RIS |= RIS_TX;
        serial_out(DR);
        update();
    }

    void pl011uart::write_RSR(u8 val) {
        //  A write to this register clears the framing, parity, break,
        //  and overrun errors. The data value is not important.
        return;
    }

    void pl011uart::write_IBRD(u16 val) {
        IBRD = val & LCR_IBRD_M;
    }

    void pl011uart::write_FBRD(u16 val) {
        FBRD = val & LCR_FBRD_M;
    }

    void pl011uart::write_LCR(u8 val) {
        if ((val & LCR_FEN) && !(LCR & LCR_FEN))
            log_debug("FIFO enabled");
        if (!(val & LCR_FEN) && (LCR & LCR_FEN))
            log_debug("FIFO disabled");

        m_fifo_size = (val & LCR_FEN) ? FIFOSIZE : 1;
        LCR = val & LCR_H_M;
    }

    void pl011uart::write_CR(u16 val) {
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

        m_enable.notify(SC_ZERO_TIME);
        CR = val;
    }

    void pl011uart::write_IFLS(u16 val) {
        IFLS = val & 0x3f; // TODO implement interrupt FIFO level select
    }

    void pl011uart::write_IMSC(u16 val) {
        IMSC = val & RIS_M;
        update();
    }

    void pl011uart::write_ICR(u16 val) {
        RIS &= ~(val & RIS_M);
        ICR = 0;
        update();
    }

    pl011uart::pl011uart(const sc_module_name& nm):
        peripheral(nm),
        serial::port(),
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
        PID  ("PID",   0xFE0, 0x00000000),
        CID  ("CID",   0xFF0, 0x00000000),
        IN("IN"),
        IRQ("IRQ") {
        DR.sync_always();
        DR.allow_read_write();
        DR.on_read(&pl011uart::read_DR);
        DR.on_write(&pl011uart::write_DR);

        RSR.sync_always();
        RSR.allow_read_write();
        RSR.on_write(&pl011uart::write_RSR);

        FR.sync_always();
        FR.allow_read_only();

        ILPR.sync_never();
        ILPR.allow_read_write(); // not implemented

        IBRD.sync_always();
        IBRD.allow_read_write();
        IBRD.on_write(&pl011uart::write_IBRD);

        FBRD.sync_always();
        FBRD.allow_read_write();
        FBRD.on_write(&pl011uart::write_FBRD);

        LCR.sync_always();
        LCR.allow_read_write();
        LCR.on_write(&pl011uart::write_LCR);

        CR.sync_always();
        CR.allow_read_write();
        CR.on_write(&pl011uart::write_CR);

        IFLS.sync_always();
        IFLS.allow_read_write();
        IFLS.on_write(&pl011uart::write_IFLS);

        IMSC.sync_always();
        IMSC.allow_read_write();
        IMSC.on_write(&pl011uart::write_IMSC);

        RIS.sync_always();
        RIS.allow_read_only();

        MIS.sync_always();
        MIS.allow_read_only();

        ICR.sync_always();
        ICR.allow_write_only();
        ICR.on_write(&pl011uart::write_ICR);

        DMAC.sync_never();
        DMAC.allow_read_write(); // not implemented

        PID.sync_never();
        PID.allow_read_only();

        CID.sync_never();
        CID.allow_read_only();

        SC_METHOD(poll);

        reset();
    }

    pl011uart::~pl011uart() {
        // nothing to do
    }

    void pl011uart::reset() {
        peripheral::reset();

        for (unsigned int i = 0; i < PID.count(); i++)
            PID[i] = (AMBA_PID >> (i * 8)) & 0xFF;

        for (unsigned int i = 0; i < CID.count(); i++)
            CID[i] = (AMBA_CID >> (i * 8)) & 0xFF;

        while (!m_fifo.empty())
            m_fifo.pop();

        IRQ = false;
    }

}}
