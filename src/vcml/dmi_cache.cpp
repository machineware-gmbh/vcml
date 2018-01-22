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

#include "vcml/dmi_cache.h"

namespace vcml {

    static bool dmi_is_mergeable(const tlm_dmi& a, const tlm_dmi& b) {
        if (a.get_granted_access() != b.get_granted_access())
            return false;
        if (a.get_read_latency() != b.get_read_latency())
            return false;
        if (a. get_write_latency() != b.get_write_latency())
            return false;

        const range ra(a); const range rb(b);
        if (!ra.overlaps(rb) && !ra.connects(rb))
            return false;

        unsigned char* ptr1 = a.get_dmi_ptr();
        unsigned char* ptr2 = b.get_dmi_ptr();
        if (ptr1 + b.get_start_address() - a.get_start_address() != ptr2)
            return false;
        return true;
    }

    dmi_cache::dmi_cache():
        m_entries() {
        /* nothing to do */
    }

    dmi_cache::~dmi_cache() {
        /* nothing to do */
    }

    void dmi_cache::insert(const tlm_dmi& dmi) {
        vector<tlm_dmi>::iterator it = std::find_if(m_entries.begin(),
            m_entries.end(), [dmi] (const tlm_dmi& entry) -> bool {
            return dmi_is_mergeable(dmi, entry);
        });

        if (it == m_entries.end()) {
            m_entries.push_back(dmi);
            return;
        }

        tlm_dmi merged(*it);
        m_entries.erase(it, it + 1);

        if (dmi.get_end_address() > merged.get_end_address())
            merged.set_end_address(dmi.get_end_address());
        if (dmi.get_start_address() < merged.get_start_address())
            dmi_set_start_address(merged, dmi.get_start_address());

        insert(merged);
    }

    void dmi_cache::invalidate(u64 start, u64 end) {
        invalidate(range(start, end));
    }

    void dmi_cache::invalidate(const range& r) {
        vector<tlm_dmi> entries(m_entries);
        m_entries.clear();

        for (auto dmi : entries) {
            if (!r.overlaps(dmi)) {
                m_entries.push_back(dmi);
                continue;
            }

            if (r.start > 0) {
                tlm_dmi front(dmi);
                front.set_end_address(r.start - 1);
                if (front.get_start_address() < front.get_end_address())
                    m_entries.push_back(front);
            }

            if (r.end != (u64)-1) {
                tlm_dmi back(dmi);
                dmi_set_start_address(back, r.end + 1);
                if (back.get_start_address() < back.get_end_address())
                    m_entries.push_back(back);
            }
        }
    }

    bool dmi_cache::lookup(const range& r, tlm_command c, tlm_dmi& out) const {
        for (auto dmi : m_entries) {
            if (r.inside(dmi) && dmi_check_access(dmi, c)) {
                out = dmi;
                return true;
            }
        }
        return false;
    }

    bool dmi_cache::lookup(u64 addr, u64 size, tlm_command c,
                           tlm_dmi& dmi) const {
        return lookup(range(addr, addr + size - 1), c, dmi);
    }

    bool dmi_cache::lookup(const tlm_generic_payload& tx, tlm_dmi& dmi) const {
        return lookup(range(tx), tx.get_command(), dmi);
    }

}
