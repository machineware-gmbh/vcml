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

#include "vcml/models/generic/bus.h"

namespace vcml {
namespace generic {

bool bus::mapping::operator<(const mapping& m) const {
    if (addr.start == m.addr.start)
        return target < m.target;
    return addr.start < m.addr.start;
}

size_t bus::find_target_port(sc_object& peer) const {
    for (const auto& it : m_target_peers)
        if (it.second == &peer)
            return it.first;
    return SIZE_MAX;
}

size_t bus::find_source_port(sc_object& peer) const {
    for (const auto& it : m_source_peers)
        if (it.second == &peer)
            return it.first;
    return SIZE_MAX;
}

const char* bus::target_peer_name(size_t port) const {
    auto it = m_target_peers.find(port);
    if (it == m_target_peers.end())
        return out[port].name();
    return it->second->name();
}

const char* bus::source_peer_name(size_t port) const {
    auto it = m_source_peers.find(port);
    if (it == m_source_peers.end())
        return in[port].name();
    return it->second->name();
}

bool bus::cmd_mmap(const vector<string>& args, ostream& os) {
    stream_guard guard(os);
    os << "Memory map of " << name();

    size_t w = 0, i = 0;
    for (const auto it : m_target_peers)
        w = max(w, strlen(it.second->name()));

    for (const auto& m : m_mappings) {
        os << "\n" << i++ << ": " << m.addr << " -> ";
        os << mwr::pad(target_peer_name(m.target), w);
        os << " " << m.addr + m.offset - m.addr.start;

        if (m.source != SOURCE_ANY)
            os << " (via " << source_peer_name(m.source) << " only)";
    }

    if (m_default.target != TARGET_NONE)
        os << "\ndefault route -> " << target_peer_name(m_default.target);

    return true;
}

const bus::mapping& bus::lookup(tlm_target_socket& s, const range& mem) const {
    size_t port = in.index_of(s);

    for (const auto& m : m_mappings) {
        if (m.addr.includes(mem) && m.source == port)
            return m;
    }

    for (const auto& m : m_mappings) {
        if (m.addr.includes(mem) && m.source == SOURCE_ANY)
            return m;
    }

    return m_default;
}

void bus::map(size_t target, const range& addr, u64 offset, size_t source) {
    for (const auto& m : m_mappings) {
        if (!m.addr.overlaps(addr))
            continue;
        if (m.source != source)
            continue;

        stringstream ss;
        ss << "Cannot map " << target << ":" << addr << " to '"
           << target_peer_name(target) << "', because it overlaps with "
           << m.target << ":" << m.addr << " mapped to '"
           << target_peer_name(m.target) << "'";
        VCML_ERROR("%s", ss.str().c_str());
    }

    mapping m;
    m.target = target;
    m.source = source;
    m.addr = addr;
    m.offset = offset;
    m_mappings.insert(m);
}

void bus::map_default(size_t target, u64 offset) {
    if (m_default.target != TARGET_NONE) {
        VCML_ERROR("default route already mapped to '%s'",
                   target_peer_name(m_default.target));
    }

    m_default.target = target;
    m_default.offset = offset;
}

void bus::b_transport(tlm_target_socket& socket, tlm_generic_payload& tx,
                      sc_time& dt) {
    const mapping& m = lookup(socket, tx);
    if (m.target == TARGET_NONE) {
        tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    u64 addr = tx.get_address();
    tx.set_address(addr - m.addr.start + m.offset);
    out[m.target]->b_transport(tx, dt);
    tx.set_address(addr);
}

unsigned int bus::transport_dbg(tlm_target_socket& origin,
                                tlm_generic_payload& tx) {
    const mapping& m = lookup(origin, tx);
    if (m.target == TARGET_NONE) {
        tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return 0;
    }

    u64 addr = tx.get_address();
    tx.set_address(addr - m.addr.start + m.offset);
    unsigned int n = out[m.target]->transport_dbg(tx);
    tx.set_address(addr);
    return n;
}

bool bus::get_direct_mem_ptr(tlm_target_socket& origin,
                             tlm_generic_payload& tx, tlm_dmi& dmi) {
    const mapping& m = lookup(origin, tx);
    if (m.target == TARGET_NONE)
        return false;

    u64 addr = tx.get_address();
    tx.set_address(addr - m.addr.start + m.offset);
    bool use_dmi = out[m.target]->get_direct_mem_ptr(tx, dmi);
    tx.set_address(addr);

    if (use_dmi) {
        u64 s = dmi.get_start_address() + m.addr.start - m.offset;
        u64 e = dmi.get_end_address() + m.addr.start - m.offset;

        // check if target gave more DMI space than it has address space
        if (s < m.addr.start) {
            log_warn("truncating DMI start from 0x%016llx to 0x%016llx", s,
                     m.addr.start);
            s = m.addr.start;
        }

        if (e > m.addr.end) {
            log_warn("truncating DMI end from 0x%016llx to 0x%016llx", e,
                     m.addr.end);
            e = m.addr.end;
        }

        dmi.set_start_address(s);
        dmi.set_end_address(e);
    }

    return use_dmi;
}

void bus::invalidate_direct_mem_ptr(tlm_initiator_socket& origin, u64 start,
                                    u64 end) {
    VCML_ERROR_ON(start > end, "invalid dmi invalidation request");
    size_t port = out.index_of(origin);
    for (const mapping& m : m_mappings) {
        if (m.target != port)
            continue;

        if (end < m.offset)
            continue;

        u64 s = start > m.offset ? start - m.offset : 0;
        u64 e = end != ~0ull ? end - m.offset : end;

        if (s >= m.addr.length())
            continue;

        if (e >= m.addr.length())
            e = m.addr.length() - 1;

        s += m.addr.start;
        e += m.addr.start;

        for (auto& it : in) {
            if (m.source == SOURCE_ANY || it.first == m.source)
                (*it.second)->invalidate_direct_mem_ptr(s, e);
        }
    }
}

bus::bus(const sc_module_name& nm):
    component(nm), m_mappings(), m_default(), in("in"), out("out") {
    m_default.target = -1;
    m_default.source = -1;
    m_default.addr = range(0ull, ~0ull);
    m_default.offset = 0;
    register_command("mmap", 0, &bus::cmd_mmap, "shows the bus memory map");
}

bus::~bus() {
    // nothing to do
}

} // namespace generic
} // namespace vcml
