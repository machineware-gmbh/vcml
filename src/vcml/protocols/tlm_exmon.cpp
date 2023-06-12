/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm_exmon.h"

namespace vcml {

bool tlm_exmon::has_lock(int cpu, const range& r) const {
    for (auto lock : m_locks)
        if (lock.cpu == cpu && lock.addr.includes(r))
            return true;
    return false;
}

bool tlm_exmon::add_lock(int cpu, const range& r) {
    assert(cpu >= 0);
    break_locks(cpu);
    m_locks.push_back({ cpu, r });
    return true;
}

void tlm_exmon::break_locks(int cpu) {
    assert(cpu >= 0);
    m_locks.erase(std::remove_if(m_locks.begin(), m_locks.end(),
                                 [cpu](const exlock& lock) -> bool {
                                     return lock.cpu == cpu;
                                 }),
                  m_locks.end());
}

void tlm_exmon::break_locks(const range& r) {
    m_locks.erase(std::remove_if(m_locks.begin(), m_locks.end(),
                                 [r](const exlock& lock) -> bool {
                                     return lock.addr.overlaps(r);
                                 }),
                  m_locks.end());
}

bool tlm_exmon::update(tlm_generic_payload& tx) {
    for (auto lock : m_locks)
        if (lock.addr.overlaps(tx))
            tx.set_dmi_allowed(false);

    bool proceed = true;
    sbiext* ex = tx.get_extension<sbiext>();
    if (ex != nullptr && ex->is_excl) {
        if (tx.is_read())
            add_lock(ex->cpuid, tx);
        if (tx.is_write())
            ex->is_excl = has_lock(ex->cpuid, tx);
        proceed = ex->is_excl;
    }

    if (tx.is_write())
        break_locks(tx); // increase range to invalidate entire cache line?

    return proceed;
}

bool tlm_exmon::override_dmi(const tlm_generic_payload& tx, tlm_dmi& dmi) {
    for (auto lock : m_locks) {
        if (lock.addr.includes(tx.get_address())) {
            dmi.set_start_address(0);
            dmi.set_end_address((sc_dt::uint64)-1);
            dmi.allow_read_write();
            return false;
        }
    }

    for (auto lock : m_locks) {
        if (lock.addr.end < tx.get_address() &&
            dmi.get_start_address() <= lock.addr.end) {
            dmi_set_start_address(dmi, lock.addr.end + 1);
        }
        if (lock.addr.start > tx.get_address() &&
            dmi.get_end_address() >= lock.addr.start) {
            dmi.set_end_address(lock.addr.start - 1);
        }
    }

    return true;
}

} // namespace vcml
