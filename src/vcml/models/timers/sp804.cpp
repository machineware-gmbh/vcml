/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/timers/sp804.h"

namespace vcml {
namespace timers {

void sp804::timer::trigger() {
    irq = false;

    if (is_enabled()) {
        if (is_irq_enabled())
            irq = true;

        if (!is_oneshot())
            schedule(is_periodic() ? load : 0xFFFFFFFF);
    }

    m_timer->update_irqc();
}

void sp804::timer::schedule(u32 ticks) {
    m_ev.cancel();

    if (!is_enabled())
        return;

    if (!is_32bit())
        ticks &= 0xffff;

    hz_t effclk = clk / get_prescale_divider();
    sc_time delta((double)ticks / (double)effclk, SC_SEC);

    m_prev = sc_time_stamp();
    m_next = m_prev + delta;
    m_ev.notify(delta);
}

u32 sp804::timer::read_value() {
    if (!is_enabled())
        return load;

    double delta = (sc_time_stamp() - m_prev) / (m_next - m_prev);
    return load * (1.0 - delta);
}

u32 sp804::timer::read_ris() {
    return irq.read() ? 0x1 : 0x0;
}

u32 sp804::timer::read_mis() {
    return is_irq_enabled() ? read_ris() : 0x0;
}

void sp804::timer::write_load(u32 val) {
    load = val;
    bgload = val;
    schedule(val);
}

void sp804::timer::write_control(u32 val) {
    if (((val >> CTLR_PRESCALE_O) & CTLR_PRESCALE_M) == 3)
        log_warn("invalid prescaler value defined");
    control = val & (u32)CONTROL_M;
    schedule(load);
}

void sp804::timer::write_intclr(u32 val) {
    irq = false;
    m_timer->update_irqc();
}

void sp804::timer::write_bgload(u32 val) {
    load = val;
    bgload = val;
}

sp804::timer::timer(const sc_module_name& nm):
    peripheral(nm),
    m_ev("event"),
    m_prev(SC_ZERO_TIME),
    m_next(SC_ZERO_TIME),
    m_timer(dynamic_cast<sp804*>(get_parent_object())),
    load("load", 0x00, 0x00000000),
    value("value", 0x04, 0xffffffff),
    control("control", 0x08, 0x00000020),
    intclr("intclr", 0x0C, 0x00000000),
    ris("ris", 0x10, 0x00000000),
    mis("mis", 0x14, 0x00000000),
    bgload("bgload", 0x18, 0x00000000),
    irq("irq") {
    load.sync_always();
    load.allow_read_write();
    load.on_write(&timer::write_load);

    value.sync_always();
    value.allow_read_only();
    value.on_read(&timer::read_value);

    control.sync_always();
    control.allow_read_write();
    control.on_write(&timer::write_control);

    intclr.sync_always();
    intclr.allow_write_only();
    intclr.on_write(&timer::write_intclr);

    ris.sync_always();
    ris.allow_read_only();
    ris.on_read(&timer::read_ris);

    mis.sync_always();
    mis.allow_read_only();
    mis.on_read(&timer::read_mis);

    bgload.sync_always();
    bgload.allow_read_write();
    bgload.on_write(&timer::write_bgload);

    SC_HAS_PROCESS(timer);
    SC_METHOD(trigger);
    sensitive << m_ev;
    dont_initialize();
}

sp804::timer::~timer() {
    // nothing to do
}

void sp804::timer::reset() {
    peripheral::reset();
    m_ev.cancel();
}

void sp804::update_irqc() {
    irqc = timer1.irq || timer2.irq;
}

sp804::sp804(const sc_module_name& nm):
    peripheral(nm),
    timer1("timer1"),
    timer2("timer2"),
    itcr("itcr", 0xf00, 0x00000000),
    itop("itop", 0xf04, 0x00000000),
    pid("pid", 0xfe0),
    cid("cid", 0xff0),
    in("in"),
    irq1("irq1"),
    irq2("irq2"),
    irqc("irqc") {
    itcr.sync_never();
    itcr.allow_read_write();

    itop.sync_never();
    itop.allow_read_only();

    pid.sync_never();
    pid.allow_read_only();

    cid.sync_never();
    cid.allow_read_only();

    timer1.irq.bind(irq1);
    timer2.irq.bind(irq2);

    clk.bind(timer1.clk);
    clk.bind(timer2.clk);

    rst.bind(timer1.rst);
    rst.bind(timer2.rst);
}

sp804::~sp804() {
    // nothing to do
}

unsigned int sp804::receive(tlm_generic_payload& tx, const tlm_sbi& info,
                            address_space as) {
    u64 addr = tx.get_address();

    if ((addr >= TIMER1_START) && (addr <= TIMER1_END)) {
        tx.set_address(addr - TIMER1_START);
        unsigned int bytes = timer1.receive(tx, info, as);
        tx.set_address(addr);
        return bytes;
    }

    if ((addr >= TIMER2_START) && (addr <= TIMER2_END)) {
        tx.set_address(addr - TIMER2_START);
        unsigned int bytes = timer2.receive(tx, info, as);
        tx.set_address(addr);
        return bytes;
    }

    return peripheral::receive(tx, info, as);
}

void sp804::reset() {
    peripheral::reset();

    for (unsigned int i = 0; i < pid.count(); i++)
        pid[i] = (AMBA_PID >> (i * 8)) & 0xff;

    for (unsigned int i = 0; i < cid.count(); i++)
        cid[i] = (AMBA_CID >> (i * 8)) & 0xff;

    irqc = false;
}

VCML_EXPORT_MODEL(vcml::timers::sp804, name, args) {
    return new sp804(name);
}

} // namespace timers
} // namespace vcml
