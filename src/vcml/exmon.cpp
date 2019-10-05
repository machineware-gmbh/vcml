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

#include "vcml/exmon.h"

namespace vcml {

    exmon::exmon():
        m_locks() {
        /* nothing to do */
    }

    exmon::~exmon() {
        /* nothing to do */
    }

    bool exmon::has_lock(int cpu, const range& r) const {
        for (auto lock : m_locks)
            if (lock.cpu == cpu && lock.addr.includes(r))
                return true;
        return false;
    }

    bool exmon::add_lock(int cpu, const range& r) {
        assert(cpu >= 0);
        break_locks(cpu);
        m_locks.push_back({cpu, r});
        return true;
    }

    void exmon::break_locks(int cpu) {
        assert(cpu >= 0);
        m_locks.erase(std::remove_if(m_locks.begin(), m_locks.end(),
            [cpu] (const exlock& lock) -> bool {
                return lock.cpu == cpu;
        }), m_locks.end());
    }

    void exmon::break_locks(const range& r) {
        m_locks.erase(std::remove_if(m_locks.begin(), m_locks.end(),
            [r] (const exlock& lock) -> bool {
                return lock.addr.overlaps(r);
        }), m_locks.end());
    }

    bool exmon::update(tlm_generic_payload& tx) {
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

    bool exmon::override_dmi(const tlm_generic_payload& tx, tlm_dmi& dmi) {
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

}
