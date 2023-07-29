/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/riscv/clint.h"

namespace vcml {
namespace riscv {

u64 clint::get_cycles() const {
    sc_time delta = sc_time_stamp() - m_time_reset;
    return delta / clock_cycle();
}

u32 clint::read_msip(size_t hart) {
    if (!irq_sw.exists(hart))
        return 0;

    return irq_sw[hart].read() ? 1u : 0u;
}

void clint::write_msip(u32 val, size_t hart) {
    if (!irq_sw.exists(hart))
        return;

    const u32 mask = 1u << 0;
    val &= mask;

    log_debug("%sing interrupt on hart %zu", val ? "sett" : "clear", hart);

    msip[hart] = val;
    irq_sw[hart].write(val != 0);
}

void clint::write_mtimecmp(u64 val, size_t hart) {
    if (!irq_timer.exists(hart))
        return;

    mtimecmp[hart] = val;
    update_timer();
}

u64 clint::read_mtime() {
    return get_cycles();
}

void clint::update_timer() {
    u64 mtime = get_cycles();

    for (auto it : irq_timer) {
        auto hart = it.first;
        auto port = it.second;

        u64 mcomp = mtimecmp.get(hart);
        port->write(mtime >= mcomp);

        if (mtime >= mcomp)
            log_debug("triggering hart %zu timer interrupt", it.first);
        else if (mcomp != ~0ull)
            m_trigger.notify(clock_cycles(mcomp - mtime));
    }
}

clint::clint(const sc_module_name& nm):
    peripheral(nm),
    m_time_reset(),
    m_trigger("triggerev"),
    msip("msip", 0x0000, 0),
    mtimecmp("mtimecmp", 0x4000, 0),
    mtime("mtime", 0xbff8, 0),
    irq_sw("irq_sw", NHARTS),
    irq_timer("irq_timer", NHARTS),
    in("in") {
    msip.sync_always();
    msip.allow_read_write();
    msip.on_read(&clint::read_msip);
    msip.on_write(&clint::write_msip);

    mtimecmp.sync_on_write();
    mtimecmp.allow_read_write();
    mtimecmp.on_write(&clint::write_mtimecmp);

    mtime.sync_on_read();
    mtime.allow_read_only();
    mtime.on_read(&clint::read_mtime);

    SC_HAS_PROCESS(clint);
    SC_METHOD(update_timer);
    sensitive << m_trigger;
    dont_initialize();
}

clint::~clint() {
    // nothing to do
}

void clint::reset() {
    peripheral::reset();

    m_time_reset = sc_time_stamp();
}

VCML_EXPORT_MODEL(vcml::riscv::clint, name, args) {
    return new clint(name);
}

} // namespace riscv
} // namespace vcml
