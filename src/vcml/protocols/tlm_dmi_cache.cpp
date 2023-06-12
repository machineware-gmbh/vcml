/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm_dmi_cache.h"

namespace vcml {

static bool dmi_is_mergeable(const tlm_dmi& a, const tlm_dmi& b) {
    if (a.get_granted_access() != b.get_granted_access())
        return false;
    if (a.get_read_latency() != b.get_read_latency())
        return false;
    if (a.get_write_latency() != b.get_write_latency())
        return false;

    const range ra(a);
    const range rb(b);
    if (!ra.overlaps(rb) && !ra.connects(rb))
        return false;

    unsigned char* ptr1 = a.get_dmi_ptr();
    unsigned char* ptr2 = b.get_dmi_ptr();
    if (ptr1 + b.get_start_address() - a.get_start_address() != ptr2)
        return false;
    return true;
}

static tlm_dmi dmi_merge(const tlm_dmi& a, const tlm_dmi& b) {
    assert(dmi_is_mergeable(a, b));

    tlm_dmi result = a;
    if (b.get_end_address() > result.get_end_address())
        result.set_end_address(b.get_end_address());
    if (b.get_start_address() < result.get_start_address())
        dmi_set_start_address(result, b.get_start_address());

    return result;
}

tlm_dmi_cache::tlm_dmi_cache(): m_limit(16), m_entries() {
    // nothing to do
}

tlm_dmi_cache::~tlm_dmi_cache() {
    // nothing to do
}

void tlm_dmi_cache::insert_locked(const tlm_dmi& dmi) {
    tlm_dmi merged(dmi);
    while (true) {
        vector<tlm_dmi>::iterator it = std::find_if(
            m_entries.begin(), m_entries.end(),
            [merged](const tlm_dmi& entry) -> bool {
                return dmi_is_mergeable(merged, entry);
            });

        if (it == m_entries.end()) {
            m_entries.insert(m_entries.begin(), merged);
            break;
        }

        merged = dmi_merge(merged, *it);
        m_entries.erase(it, it + 1);
    };

    if (m_entries.size() > m_limit)
        m_entries.resize(m_limit);
}

void tlm_dmi_cache::insert(const tlm_dmi& dmi) {
    lock_guard<mutex> guard(m_mtx);
    insert_locked(dmi);
}

bool tlm_dmi_cache::invalidate(u64 start, u64 end) {
    return invalidate(range(start, end));
}

bool tlm_dmi_cache::invalidate(const range& r) {
    lock_guard<mutex> guard(m_mtx);
    vector<tlm_dmi> entries(m_entries.rbegin(), m_entries.rend());
    m_entries.clear();

    size_t invalidations = 0;

    for (const tlm_dmi& dmi : entries) {
        if (!r.overlaps(dmi)) {
            insert_locked(dmi);
            continue;
        }

        invalidations++;

        if (r.start > 0) {
            tlm_dmi front(dmi);
            front.set_end_address(r.start - 1);
            if (front.get_start_address() < front.get_end_address())
                insert_locked(front);
        }

        if (r.end != (u64)-1) {
            tlm_dmi back(dmi);
            dmi_set_start_address(back, r.end + 1);
            if (back.get_start_address() < back.get_end_address())
                insert_locked(back);
        }
    }

    return invalidations > 0;
}

bool tlm_dmi_cache::lookup(const range& r, vcml_access rwx, tlm_dmi& out) {
    lock_guard<mutex> guard(m_mtx);
    for (unsigned int i = 0; i < m_entries.size(); i++) {
        if (r.inside(m_entries[i]) && dmi_check_access(m_entries[i], rwx)) {
            std::swap(m_entries[i], m_entries[0]);
            out = m_entries[0];
            return true;
        }
    }

    return false;
}

} // namespace vcml
