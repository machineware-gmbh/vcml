/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/timers/nrf51.h"

namespace vcml {
namespace timers {

constexpr u64 NSEC_PER_SEC = 1000000000ull;

u32 nrf51::time_to_ticks(const sc_time& t) const {
    return (time_to_ns(t) * (clk >> prescaler)) / NSEC_PER_SEC;
}

sc_time nrf51::ticks_to_time(u32 ticks) const {
    u64 nsec = (ticks * NSEC_PER_SEC) / (clk >> prescaler);
    return sc_time((double)nsec, SC_NS);
}

u32 nrf51::counter_mask() const {
    static const u32 masks[4] = {
        bitmask(16),
        bitmask(8),
        bitmask(24),
        bitmask(32),
    };

    return masks[bitmode & 3];
}

u32 nrf51::current_count() const {
    if (!m_running || is_counter_mode())
        return count;

    u32 ticks = time_to_ticks(sc_time_stamp() - m_start);
    return (count + ticks) & counter_mask();
}

u32 nrf51::next_deadline() const {
    u32 deadline = 0;
    u32 ticks = current_count();
    u32 next;
    for (size_t i = 0; i < 4; i++) {
        if (compare[i] || !(m_inten & bit(16 + i)))
            continue;

        if (ticks > cc[i])
            next = counter_mask() - ticks + cc[i];
        else
            next = cc[i] - ticks;

        if (!deadline || next < deadline)
            deadline = next;
    }

    return deadline;
}

void nrf51::update() {
    count = current_count();
    m_start = sc_time_stamp();

    bool doirq = false;
    for (size_t i = 0; i < 4; i++) {
        if (cc[i] == count) {
            compare[i] = 1;
            if (shorts & bit(i + 0))
                count = 0;
            if (shorts & bit(i + 8))
                m_running = false;
        }

        doirq |= (compare[i] != 0u) && (m_inten & bit(16 + i));
    }

    irq = doirq;
    m_trigger.cancel();

    if (is_timer_mode()) {
        u32 deadline = next_deadline();
        if (deadline)
            m_trigger.notify(ticks_to_time(deadline));
    }
}

void nrf51::write_start(u32 val) {
    if (m_running || val != 1u)
        return;

    m_running = true;
    m_start = sc_time_stamp();

    update();
}

void nrf51::write_stop(u32 val) {
    if (!m_running || val != 1u)
        return;

    count = current_count();
    m_running = false;
}

void nrf51::write_count(u32 val) {
    if (val == 1u && is_counter_mode()) {
        count = (count + 1) & counter_mask();
        update();
    }
}

void nrf51::write_clear(u32 val) {
    if (val == 1u) {
        count = 0;
        m_start = sc_time_stamp();
        update();
    }
}

void nrf51::write_shutdown(u32 val) {
    write_stop(val);
}

void nrf51::write_capture(u32 val, size_t idx) {
    if (val == 1) {
        cc[idx] = current_count();
        update();
    }
}

void nrf51::write_compare(u32 val, size_t idx) {
    if (val == 0) {
        compare[idx] = 0;
        update();
    }
}

void nrf51::write_intenset(u32 val) {
    m_inten |= val;
    update();
}

void nrf51::write_intenclr(u32 val) {
    m_inten &= val;
    update();
}

void nrf51::write_cc(u32 val, size_t idx) {
    cc[idx] = val & counter_mask();
    update();
}

void nrf51::write_shorts(u32 val) {
    shorts = val;
    update();
}

nrf51::nrf51(const sc_module_name& nm):
    peripheral(nm),
    m_running(),
    m_start(),
    m_trigger("trigger"),
    m_inten(),
    start("start", 0x0),
    stop("stop", 0x4),
    count("count", 0x8),
    clear("clear", 0xc),
    shutdown("shutdown", 0x10),
    capture("capture", 0x40),
    compare("compare", 0x140),
    shorts("shorts", 0x200),
    intenset("intenset", 0x304),
    intenclr("intenclr", 0x308),
    mode("mode", 0x504),
    bitmode("bitmode", 0x508),
    prescaler("prescaler", 0x510),
    cc("cc", 0x540),
    in("in"),
    irq("irq") {
    start.sync_always();
    start.allow_read_write();
    start.on_write(&nrf51::write_start);

    stop.sync_always();
    stop.allow_read_write();
    stop.on_write(&nrf51::write_stop);

    count.sync_always();
    count.allow_read_write();
    count.on_write(&nrf51::write_count);

    clear.sync_always();
    clear.allow_read_write();
    clear.on_write(&nrf51::write_clear);

    shutdown.sync_always();
    shutdown.allow_read_write();
    shutdown.on_write(&nrf51::write_shutdown);

    capture.sync_always();
    capture.allow_read_write();
    capture.on_write(&nrf51::write_capture);

    compare.sync_always();
    compare.allow_read_write();
    compare.on_write(&nrf51::write_compare);

    shorts.sync_always();
    shorts.allow_read_write();
    shorts.on_write(&nrf51::write_shorts);

    intenset.sync_always();
    intenset.allow_read_write();
    intenset.on_read([&]() -> u32 { return m_inten; });
    intenset.on_write(&nrf51::write_intenset);

    intenclr.sync_always();
    intenclr.allow_read_write();
    intenclr.on_read([&]() -> u32 { return m_inten; });
    intenclr.on_write(&nrf51::write_intenclr);

    cc.sync_always();
    cc.allow_read_write();
    cc.on_write(&nrf51::write_cc);

    SC_HAS_PROCESS(nrf51);
    SC_METHOD(update);
    sensitive << m_trigger;
    dont_initialize();
}

nrf51::~nrf51() {
    // nothing to do
}

void nrf51::reset() {
    peripheral::reset();

    m_inten = 0;
    m_running = false;
    m_trigger.cancel();

    update();
}

VCML_EXPORT_MODEL(vcml::timers::nrf51, name, args) {
    return new nrf51(name);
}

} // namespace timers
} // namespace vcml
