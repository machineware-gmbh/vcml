/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/models/riscv/aclint.h"

namespace vcml {
namespace riscv {

u64 aclint::get_cycles() const {
    sc_time delta = sc_time_stamp() - m_time_reset;
    return delta / clock_cycle();
}

u64 aclint::read_mtime() {
    return get_cycles();
}

void aclint::write_mtimecmp(u64 val, size_t hart) {
    if (!irq_mtimer.exists(hart))
        return;

    mtimecmp[hart] = val;
    update_timer();
}

u32 aclint::read_msip(size_t hart) {
    if (!irq_mswi.exists(hart))
        return 0;

    return irq_mswi[hart].read() ? 1u : 0u;
}

void aclint::write_msip(u32 val, size_t hart) {
    if (!irq_mswi.exists(hart))
        return;

    const u32 mask = 1u << 0;
    val &= mask;

    log_debug("%sing mswi on hart %zu", val ? "sett" : "clear", hart);

    msip[hart] = val;
    irq_mswi[hart].write(val != 0);
}

u32 aclint::read_ssip(size_t hart) {
    if (!irq_sswi.exists(hart))
        return 0;

    return irq_sswi[hart].read() ? 1u : 0u;
}

void aclint::write_ssip(u32 val, size_t hart) {
    if (!irq_sswi.exists(hart))
        return;

    const u32 mask = 1u << 0;
    val &= mask;

    log_debug("%sing sswi on hart %zu", val ? "sett" : "clear", hart);

    ssip[hart] = val;
    irq_sswi[hart].write(val != 0);
}

void aclint::update_timer() {
    u64 mtime = get_cycles();

    for (auto& [hart, port] : irq_mtimer) {
        u64 mcomp = mtimecmp.get(hart);
        port->write(mtime >= mcomp);

        if (mtime >= mcomp)
            log_debug("triggering hart %zu timer interrupt", hart);
        else if (mcomp != ~0ull)
            m_trigger.notify(clock_cycles(mcomp - mtime));
    }
}

aclint::aclint(const sc_module_name& nm):
    peripheral(nm),
    m_time_reset(),
    m_trigger("triggerev"),
    comp_base("comp_base", 0x0000),
    time_base("time_base", 0x7ff8),
    mtimecmp(ACLINT_AS_MTIMER, "mtimecmp", comp_base, 0),
    mtime(ACLINT_AS_MTIMER, "mtime", time_base),
    msip(ACLINT_AS_MSWI, "msip", 0x0000, 0),
    ssip(ACLINT_AS_SSWI, "ssip", 0x0000, 0),
    irq_mtimer("irq_mtimer", NHARTS),
    irq_mswi("irq_mswi", NHARTS),
    irq_sswi("irq_sswi", NHARTS),
    mtimer("mtimer", ACLINT_AS_MTIMER),
    mswi("mswi", ACLINT_AS_MSWI),
    sswi("sswi", ACLINT_AS_SSWI) {
    mtimecmp.sync_on_write();
    mtimecmp.allow_read_write();
    mtimecmp.on_write(&aclint::write_mtimecmp);

    mtime.sync_on_read();
    mtime.allow_read_only();
    mtime.on_read(&aclint::read_mtime);

    msip.sync_always();
    msip.allow_read_write();
    msip.on_read(&aclint::read_msip);
    msip.on_write(&aclint::write_msip);

    ssip.sync_always();
    ssip.allow_read_write();
    ssip.on_read(&aclint::read_ssip);
    ssip.on_write(&aclint::write_ssip);

    SC_HAS_PROCESS(aclint);
    SC_METHOD(update_timer);
    sensitive << m_trigger;
    dont_initialize();
}

aclint::~aclint() {
    // nothing to do
}

void aclint::reset() {
    peripheral::reset();

    m_time_reset = sc_time_stamp();
}

VCML_EXPORT_MODEL(vcml::riscv::aclint, name, args) {
    return new aclint(name);
}

} // namespace riscv
} // namespace vcml
