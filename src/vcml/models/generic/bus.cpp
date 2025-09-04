/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
    if (it != m_target_peers.end())
        return it->second->name();

    auto& socket = out[port];
    if (socket.is_stubbed())
        return "stubbed";
    return out[port].name();
}

const char* bus::source_peer_name(size_t port) const {
    auto it = m_source_peers.find(port);
    if (it == m_source_peers.end())
        return in[port].name();
    return it->second->name();
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

void bus::handle_bus_error(tlm_generic_payload& tx) const {
    if (lenient) {
        if (tx.is_read())
            memset(tx.get_data_ptr(), 0, tx.get_data_length());
        tx.set_response_status(TLM_OK_RESPONSE);
        log_warn("ignoring %s access to unmapped area [%llx..%llx]",
                 tx.is_read() ? "read" : "write", tx.get_address(),
                 tx.get_address() + tx_size(tx) - 1);
    } else {
        tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
    }
}

void bus::do_mmap(ostream& os) {
    stream_guard guard(os);

    size_t l = 0;
    size_t z = m_mappings.size();
    do {
        l++;
    } while (z /= 10);

    size_t w = 0;
    for (const auto it : m_target_peers)
        w = max(w, strlen(it.second->name()));

    size_t y = 8;
    for (const auto& m : m_mappings) {
        if (m.addr.end > ~0u)
            y = 16;
    }

    size_t i = 0;
    for (const auto& m : m_mappings) {
        os << "\n[";
        os << std::dec << std::setw(l) << std::setfill(' ') << i++ << "] ";
        os << std::hex << std::setw(y) << m.addr << " -> ";
        os << mwr::pad(target_peer_name(m.target), w);
        os << " " << std::setw(y) << m.addr + m.offset - m.addr.start;

        if (m.source != SOURCE_ANY)
            os << " (via " << source_peer_name(m.source) << " only)";
    }

    if (m_default.target != TARGET_NONE)
        os << "\ndefault route -> " << target_peer_name(m_default.target);
}

bool bus::cmd_mmap(const vector<string>& args, ostream& os) {
    stream_guard guard(os);
    os << "Memory map of " << name();
    do_mmap(os);
    return true;
}

void bus::end_of_elaboration() {
    if (loglvl == LOG_DEBUG) {
        stringstream ss;
        do_mmap(ss);
        log_debug("%s", ss.str().c_str());
    }
}

void bus::b_transport(tlm_target_socket& socket, tlm_generic_payload& tx,
                      sc_time& dt) {
    const mapping& m = lookup(socket, tx);
    if (m.target == TARGET_NONE) {
        handle_bus_error(tx);
        return;
    }

    u64 addr = tx.get_address();
    tx.set_address(addr - m.addr.start + m.offset);
    out[m.target].b_transport(tx, dt);
    tx.set_address(addr);
}

unsigned int bus::transport_dbg(tlm_target_socket& origin,
                                tlm_generic_payload& tx) {
    const mapping& m = lookup(origin, tx);
    if (m.target == TARGET_NONE) {
        handle_bus_error(tx);
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
    if (port == m_default.target) {
        u64 s = start > m_default.offset ? start - m_default.offset : 0;
        u64 e = end != ~0ull ? end - m_default.offset : end;

        for (auto& [id, port] : in)
            (*port)->invalidate_direct_mem_ptr(s, e);

        return;
    }

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

        for (auto& [id, port] : in) {
            if (m.source == SOURCE_ANY || id == m.source)
                (*port)->invalidate_direct_mem_ptr(s, e);
        }
    }
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

bus::bus(const sc_module_name& nm):
    component(nm),
    m_mappings(),
    m_default(),
    lenient("lenient", false),
    in("in"),
    out("out") {
    m_default.target = -1;
    m_default.source = -1;
    m_default.addr = range(0ull, ~0ull);
    m_default.offset = 0;
    register_command("mmap", 0, &bus::cmd_mmap, "shows the bus memory map");
}

bus::~bus() {
    // nothing to do
}

VCML_EXPORT_MODEL(vcml::generic::bus, name, args) {
    return new bus(name);
}

} // namespace generic

using initiator_t = tlm::tlm_base_initiator_socket<>;
using target_t = tlm::tlm_base_target_socket<>;
using socket_t = variant<initiator_t*, target_t*>;

static socket_t get_socket(const sc_object& host, const string& port) {
    sc_object* child = vcml::find_child(host, port);
    VCML_ERROR_ON(!child, "%s.%s does not exsist", host.name(), port.c_str());
    auto* ini = dynamic_cast<initiator_t*>(child);
    if (ini)
        return ini;

    auto* tgt = dynamic_cast<target_t*>(child);
    if (tgt)
        return tgt;

    VCML_ERROR("%s is not a valid tlm socket", child->name());
}

static socket_t get_socket(const sc_object& host, const string& port,
                           size_t idx) {
    sc_object* child = vcml::find_child(host, port);
    VCML_ERROR_ON(!child, "%s.%s does not exsist", host.name(), port.c_str());
    if (auto* aif = dynamic_cast<socket_array_if*>(child)) {
        auto* base = aif->fetch(idx, true);
        auto* ini = dynamic_cast<initiator_t*>(base);
        if (ini)
            return ini;

        auto* tgt = dynamic_cast<target_t*>(base);
        if (tgt)
            return tgt;
    }

    VCML_ERROR("%s[%zu] is not a valid tlm socket", child->name(), idx);
}

static generic::bus& get_bus(sc_object& obj) {
    auto b = dynamic_cast<generic::bus*>(&obj);
    VCML_ERROR_ON(!b, "%s is not a valid tlm bus", obj.name());
    return *b;
}

void stub(sc_object& obj, const sc_object& host, const string& port,
          const range& addr, tlm_response_status rs) {
    auto& bus = get_bus(obj);
    auto socket = get_socket(host, port);
    if (std::holds_alternative<initiator_t*>(socket))
        bus.stub(*std::get<initiator_t*>(socket), addr, rs);
    else
        bus.stub(*std::get<target_t*>(socket), addr, rs);
}

void stub(sc_object& obj, const sc_object& host, const string& port,
          size_t idx, const range& addr, tlm_response_status rs) {
    auto& bus = get_bus(obj);
    auto socket = get_socket(host, port, idx);
    if (std::holds_alternative<initiator_t*>(socket))
        bus.stub(*std::get<initiator_t*>(socket), addr, rs);
    else
        bus.stub(*std::get<target_t*>(socket), addr, rs);
}

void stub(sc_object& bus, const sc_object& host, const string& port, u64 lo,
          u64 hi, tlm_response_status rs) {
    stub(bus, host, port, range(lo, hi), rs);
}

void stub(sc_object& bus, const sc_object& host, const string& port,
          size_t idx, u64 lo, u64 hi, tlm_response_status rs) {
    stub(bus, host, port, idx, range(lo, hi), rs);
}

void bind(sc_object& obj, const sc_object& host, const string& port) {
    auto& bus = get_bus(obj);
    auto socket = get_socket(host, port);
    if (std::holds_alternative<initiator_t*>(socket))
        bus.bind(*std::get<initiator_t*>(socket));
    else
        bus.bind(*std::get<target_t*>(socket));
}

void bind(sc_object& obj, const sc_object& host, const string& port,
          size_t idx) {
    auto& bus = get_bus(obj);
    auto socket = get_socket(host, port, idx);
    if (std::holds_alternative<initiator_t*>(socket))
        bus.bind(*std::get<initiator_t*>(socket));
    else
        bus.bind(*std::get<target_t*>(socket));
}

void bind(sc_object& obj, const sc_object& host, const string& port,
          const range& addr, u64 offset) {
    auto& bus = get_bus(obj);
    auto socket = get_socket(host, port);
    if (std::holds_alternative<initiator_t*>(socket))
        bus.bind(*std::get<initiator_t*>(socket), addr, offset);
    else
        bus.bind(*std::get<target_t*>(socket), addr, offset);
}

void bind(sc_object& obj, const sc_object& host, const string& port,
          size_t idx, const range& addr, u64 offset) {
    auto& bus = get_bus(obj);
    auto socket = get_socket(host, port, idx);
    if (std::holds_alternative<initiator_t*>(socket))
        bus.bind(*std::get<initiator_t*>(socket), addr, offset);
    else
        bus.bind(*std::get<target_t*>(socket), addr, offset);
}

void bind(sc_object& bus, const sc_object& host, const string& port, u64 lo,
          u64 hi, u64 offset) {
    vcml::bind(bus, host, port, range(lo, hi), offset);
}

void bind(sc_object& bus, const sc_object& host, const string& port,
          size_t idx, u64 lo, u64 hi, u64 offset) {
    vcml::bind(bus, host, port, idx, range(lo, hi), offset);
}

void bind_default(sc_object& obj, const sc_object& host, const string& port,
                  u64 offset) {
    auto& bus = get_bus(obj);
    auto socket = get_socket(host, port);
    if (std::holds_alternative<initiator_t*>(socket))
        bus.bind_default(*std::get<initiator_t*>(socket), offset);
    else
        bus.bind_default(*std::get<target_t*>(socket), offset);
}

void bind_default(sc_object& obj, const sc_object& host, const string& port,
                  size_t idx, u64 offset) {
    auto& bus = get_bus(obj);
    auto socket = get_socket(host, port, idx);
    if (std::holds_alternative<initiator_t*>(socket))
        bus.bind_default(*std::get<initiator_t*>(socket), offset);
    else
        bus.bind_default(*std::get<target_t*>(socket), offset);
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              const range& addr, tlm_response_status rs) {
    stub(bus, host, port, addr, rs);
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, const range& addr, tlm_response_status rs) {
    stub(bus, host, port, idx, addr, rs);
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              u64 lo, u64 hi, tlm_response_status rs) {
    stub(bus, host, port, range(lo, hi), rs);
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, u64 lo, u64 hi, tlm_response_status rs) {
    stub(bus, host, port, idx, range(lo, hi), rs);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port) {
    bind(bus, host, port);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx) {
    bind(bus, host, port, idx);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              const range& addr, u64 offset) {
    bind(bus, host, port, addr, offset);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, const range& addr, u64 offset) {
    bind(bus, host, port, idx, addr, offset);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              u64 lo, u64 hi, u64 offset) {
    bind(bus, host, port, lo, hi, offset);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, u64 lo, u64 hi, u64 offset) {
    bind(bus, host, port, idx, lo, hi, offset);
}

void tlm_bind_default(sc_object& bus, const sc_object& host,
                      const string& port, u64 offset) {
    bind_default(bus, host, port, offset);
}

void tlm_bind_default(sc_object& bus, const sc_object& host,
                      const string& port, size_t idx, u64 offset) {
    bind_default(bus, host, port, idx, offset);
}

} // namespace vcml
