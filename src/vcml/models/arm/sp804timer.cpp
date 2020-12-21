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

#include "vcml/models/arm/sp804timer.h"

namespace vcml { namespace arm {

    void sp804timer::timer::trigger() {
        IRQ = false;

        if (!is_enabled())
            return;

        if (is_irq_enabled())
            IRQ = true;

        if (!is_oneshot())
            schedule(is_periodic() ? LOAD : 0xFFFFFFFF);
    }

    void sp804timer::timer::schedule(u32 ticks) {
        m_ev.cancel();

        if (!is_enabled())
            return;

        if (!is_32bit())
            ticks &= 0xffff;

        clock_t effclk = CLOCK / get_prescale_divider();
        sc_time delta((double)ticks / (double)effclk, SC_SEC);

        m_prev = sc_time_stamp();
        m_next = m_prev + delta;
        m_ev.notify(delta);
    }

    u32 sp804timer::timer::read_VALUE() {
        if (!is_enabled())
            return LOAD;

        double delta = (sc_time_stamp() - m_prev) / (m_next - m_prev);
        return LOAD * (1.0 - delta);
    }

    u32 sp804timer::timer::read_RIS() {
        return IRQ.read() ? 0x1 : 0x0;
    }

    u32 sp804timer::timer::read_MIS() {
        return is_irq_enabled() ? read_RIS() : 0x0;
    }

    u32 sp804timer::timer::write_LOAD(u32 val) {
        LOAD = val;
        BGLOAD = val;
        schedule(val);
        return val;
    }

    u32 sp804timer::timer::write_CONTROL(u32 val) {
        if (((val >> CTLR_PRESCALE_O) & CTLR_PRESCALE_M) == 3)
            log_warn("invalid prescaler value defined");
        CONTROL = val & (u32)CONTROL_M;
        schedule(LOAD);
        return CONTROL;
    }

    u32 sp804timer::timer::write_INTCLR(u32 val) {
        IRQ = false;
        return 0;
    }

    u32 sp804timer::timer::write_BGLOAD(u32 val) {
        LOAD = val;
        BGLOAD = val;
        return val;
    }

    sp804timer::timer::timer(const sc_module_name& nm):
        peripheral(nm),
        m_ev("event"),
        m_prev(SC_ZERO_TIME),
        m_next(SC_ZERO_TIME),
        LOAD("LOAD", 0x00, 0x00000000),
        VALUE("VALUE", 0x04, 0xFFFFFFFF),
        CONTROL("CONTROL", 0x08, 0x00000020),
        INTCLR("INTCLR", 0x0C, 0x00000000),
        RIS("RIS", 0x10, 0x00000000),
        MIS("MIS", 0x14, 0x00000000),
        BGLOAD("BGLOAD", 0x18, 0x00000000),
        IRQ("IRQ") {

        LOAD.sync_always();
        LOAD.allow_read_write();
        LOAD.write = &timer::write_LOAD;

        VALUE.sync_always();
        VALUE.allow_read_only();
        VALUE.read = &timer::read_VALUE;

        CONTROL.sync_always();
        CONTROL.allow_read_write();
        CONTROL.write = &timer::write_CONTROL;

        INTCLR.sync_always();
        INTCLR.allow_write_only();
        INTCLR.write = &timer::write_INTCLR;

        RIS.sync_always();
        RIS.allow_read_only();
        RIS.read = &timer::read_RIS;

        MIS.sync_always();
        MIS.allow_read_only();
        MIS.read = &timer::read_MIS;

        BGLOAD.sync_always();
        BGLOAD.allow_read_write();
        BGLOAD.write = &timer::write_BGLOAD;

        SC_METHOD(trigger);
        sensitive << m_ev;
        dont_initialize();
    }

    sp804timer::timer::~timer() {
        // nothing to do
    }

    void sp804timer::timer::reset() {
        peripheral::reset();
        m_ev.cancel();
    }

    void sp804timer::update_IRQC() {
        IRQC = IRQ1.read() || IRQ2.read();
    }

    sp804timer::sp804timer(const sc_module_name& nm):
        peripheral(nm),
        TIMER1("TIMER1"),
        TIMER2("TIMER2"),
        ITCR("ITCR", 0xF00, 0x00000000),
        ITOP("ITOP", 0xF04, 0x00000000),
        PID("PID", 0xFE0),
        CID("CID", 0xFF0),
        IN("IN"),
        IRQ1("IRQ1"),
        IRQ2("IRQ2"),
        IRQC("IRQC") {

        ITCR.sync_never();
        ITCR.allow_read_write();

        ITOP.sync_never();
        ITOP.allow_read_only();

        PID.sync_never();
        PID.allow_read_only();

        CID.sync_never();
        CID.allow_read_only();

        TIMER1.IRQ.bind(IRQ1);
        TIMER2.IRQ.bind(IRQ2);

        TIMER1.CLOCK.bind(CLOCK);
        TIMER2.CLOCK.bind(CLOCK);

        TIMER1.RESET.bind(RESET);
        TIMER2.RESET.bind(RESET);

        SC_METHOD(update_IRQC);
        sensitive << IRQ1 << IRQ2;
        dont_initialize();

        reset();
    }

    sp804timer::~sp804timer() {
        // nothing to do
    }

    unsigned int sp804timer::receive(tlm_generic_payload& tx,
                                     const sideband& info) {
        u64 addr = tx.get_address();

        // coverity[unsigned_compare]
        if ((addr >= TIMER1_START) && (addr <= TIMER1_END)) {
            tx.set_address(addr - TIMER1_START);
            unsigned int bytes = TIMER1.receive(tx, info);
            tx.set_address(addr);
            return bytes;
        }

        if ((addr >= TIMER2_START) && (addr <= TIMER2_END)) {
            tx.set_address(addr - TIMER2_START);
            unsigned int bytes = TIMER2.receive(tx, info);
            tx.set_address(addr);
            return bytes;
        }

        return peripheral::receive(tx, info);
    }

    void sp804timer::reset() {
        peripheral::reset();

        for (unsigned int i = 0; i < PID.count(); i++)
            PID[i] = (SP804TIMER_PID >> (i * 8)) & 0xFF;

        for (unsigned int i = 0; i < CID.count(); i++)
            CID[i] = (SP804TIMER_CID >> (i * 8)) & 0xFF;

        IRQC = false;
    }

}}
