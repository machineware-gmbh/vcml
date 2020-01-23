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

        ticks &= is_32bit() ? 0xFFFFFFFF : 0x0000FFFF;
        clock_t effclk = m_parent->clock / get_prescale_divider();
        sc_time delta((double)ticks / (double)effclk, SC_SEC);

        m_prev = sc_time_stamp();
        m_next = m_prev + delta;
        m_ev.notify(delta);
    }

    u32 sp804timer::timer::read_CVAL() {
        if (!is_enabled())
            return LOAD;

        double delta = (sc_time_stamp() - m_prev) / (m_next - m_prev);
        return LOAD * (1.0 - delta);
    }

    u32 sp804timer::timer::read_RISR() {
        return IRQ.read() ? 0x1 : 0x0;
    }

    u32 sp804timer::timer::read_MISR() {
        return is_irq_enabled() ? read_RISR() : 0x0;
    }

    u32 sp804timer::timer::write_LOAD(u32 val) {
        LOAD = val;
        BGLR = val;
        schedule(val);
        return val;
    }

    u32 sp804timer::timer::write_CTLR(u32 val) {
        if (((val >> CTLR_PRESCALE_O) & CTLR_PRESCALE_M) == 3)
            log_warn("invalid prescaler value defined");
        CTLR = val & CTLR_M;
        schedule(LOAD);
        return CTLR;
    }

    u32 sp804timer::timer::write_ICLR(u32 val) {
        IRQ = false;
        return 0;
    }

    u32 sp804timer::timer::write_BGLR(u32 val) {
        LOAD = val;
        BGLR = val;
        return val;
    }

    sp804timer::timer::timer(const sc_module_name& nm):
        vcml::peripheral(nm),
        m_parent(NULL),
        m_ev("event"),
        m_prev(SC_ZERO_TIME),
        m_next(SC_ZERO_TIME),
        LOAD("LOAD", 0x00, 0x00000000),
        CVAL("CVAL", 0x04, 0xFFFFFFFF),
        CTLR("CTLR", 0x08, 0x00000020),
        ICLR("ICLR", 0x0C, 0x00000000),
        RISR("RISR", 0x10, 0x00000000),
        MISR("MISR", 0x14, 0x00000000),
        BGLR("BGLR", 0x18, 0x00000000),
        IRQ("IRQ") {
        m_parent = dynamic_cast<sp804timer*>(get_parent_object());
        VCML_ERROR_ON(!m_parent, "invalid timer parent specified");

        LOAD.allow_read_write();
        LOAD.write = &timer::write_LOAD;

        CVAL.allow_read();
        CVAL.read = &timer::read_CVAL;

        CTLR.allow_read_write();
        CTLR.write = &timer::write_CTLR;

        ICLR.allow_write();
        ICLR.write = &timer::write_ICLR;

        RISR.allow_read();
        RISR.read = &timer::read_RISR;

        MISR.allow_read();
        MISR.read = &timer::read_MISR;

        BGLR.allow_read_write();
        BGLR.write = &timer::write_BGLR;

        SC_METHOD(trigger);
        sensitive << m_ev;
        dont_initialize();
    }

    sp804timer::timer::~timer() {
        // nothing to do
    }

    void sp804timer::timer::reset() {
        LOAD = 0x00000000;
        CVAL = 0xFFFFFFFF;
        CTLR = 0x00000020;
        ICLR = 0x00000000;
        RISR = 0x00000000;
        MISR = 0x00000000;
        BGLR = 0x00000000;
        IRQ  = false;
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
        IRQC("IRQC"),
        clock("clock", VCML_ARM_SP804TIMER_CLK) {

        ITCR.allow_read_write();
        ITOP.allow_read();

        PID.allow_read();
        CID.allow_read();

        TIMER1.IRQ.bind(IRQ1);
        TIMER2.IRQ.bind(IRQ2);

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
        ITCR = 0x00000000;
        ITOP = 0x00000000;

        for (unsigned int i = 0; i < PID.num(); i++)
            PID[i] = (VCML_ARM_SP804TIMER_PID >> (i * 8)) & 0xFF;

        for (unsigned int i = 0; i < CID.num(); i++)
            CID[i] = (VCML_ARM_SP804TIMER_CID >> (i * 8)) & 0xFF;

        IRQC = false;
    }

}}
