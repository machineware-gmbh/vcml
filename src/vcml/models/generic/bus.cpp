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

bool operator<(const bus_mapping& a, const bus_mapping& b) {
    if (a.addr.start == b.addr.start)
        return a.target < b.target;
    return a.addr.start < b.addr.start;
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

const bus_mapping& bus::lookup(tlm_target_socket& s, const range& mem) const {
    size_t port = in.index_of(s);

    for (const auto& m : m_mappings) {
        if (m.addr.includes(mem) && m.source == port)
            return m;
    }

    for (const auto& m : m_mappings) {
        if (m.addr.includes(mem) && m.source == bus_mapping::SOURCE_ANY)
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

        if (m.source != bus_mapping::SOURCE_ANY)
            os << " (via " << source_peer_name(m.source) << " only)";
    }

    if (m_default.target != bus_mapping::TARGET_NONE)
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
    const bus_mapping& m = lookup(socket, tx);
    if (m.target == bus_mapping::TARGET_NONE) {
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
    const bus_mapping& m = lookup(origin, tx);
    if (m.target == bus_mapping::TARGET_NONE) {
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
    const bus_mapping& m = lookup(origin, tx);
    if (m.target == bus_mapping::TARGET_NONE)
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

    for (const bus_mapping& m : m_mappings) {
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
            if (m.source == bus_mapping::SOURCE_ANY || id == m.source)
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
        ss << "cannot map " << target << ":" << addr << " to '"
           << target_peer_name(target) << "', because it overlaps with "
           << m.target << ":" << m.addr << " mapped to '"
           << target_peer_name(m.target) << "'";
        VCML_REPORT("%s: %s", name(), ss.str().c_str());
    }

    bus_mapping m;
    m.target = target;
    m.source = source;
    m.addr = addr;
    m.offset = offset;
    m_mappings.insert(m);
}

void bus::map_default(size_t target, u64 offset) {
    if (m_default.target != bus_mapping::TARGET_NONE) {
        VCML_REPORT("default route already mapped to '%s'",
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

optional<bus_mapping> bus::get_default_mapping() const {
    if (m_default.target != bus_mapping::TARGET_NONE)
        return m_default;
    return std::nullopt;
}

vector<bus_mapping> bus::get_all_mappings() const {
    vector<bus_mapping> mm;
    mm.reserve(m_mappings.size());
    for (const auto& map : m_mappings)
        mm.push_back(map);
    if (auto defmap = get_default_mapping())
        mm.push_back(*defmap);
    return mm;
}

vector<bus_mapping> bus::get_source_mappings(size_t source) const {
    vector<bus_mapping> mm;
    if (!in.exists(source))
        return mm;

    for (const auto& map : m_mappings) {
        if (map.source == source || map.source == bus_mapping::SOURCE_ANY)
            mm.push_back(map);
    }

    if (m_default.target != bus_mapping::TARGET_NONE)
        mm.push_back(m_default);

    return mm;
}

vector<bus_mapping> bus::get_target_mappings(size_t target) const {
    vector<bus_mapping> mm;
    if (!out.exists(target))
        return mm;

    for (const auto& map : m_mappings) {
        if (map.target == target)
            mm.push_back(map);
    }

    if (m_default.target == target)
        mm.push_back(m_default);

    return mm;
}

VCML_EXPORT_MODEL(vcml::generic::bus, name, args) {
    return new bus(name);
}

} // namespace generic

using initiator_t = tlm::tlm_base_initiator_socket<>;
using target_t = tlm::tlm_base_target_socket<>;

static generic::bus& get_bus(sc_object& obj) {
    auto b = dynamic_cast<generic::bus*>(&obj);
    VCML_REPORT_ON(!b, "%s is not a valid tlm bus", obj.name());
    return *b;
}

static sc_object& get_socket(sc_object& socket) {
    if (auto* arr = dynamic_cast<socket_array_if*>(&socket))
        return *arr->alloc();
    return socket;
}

static sc_object& get_socket(const string& name) {
    sc_object& socket = find_socket(name);
    return get_socket(socket);
}

void stub(sc_object& obj, const range& addr, tlm_response_status rs) {
    auto& bus = get_bus(obj);
    bus.stub(addr, rs);
}

void stub(sc_object& bus, u64 lo, u64 hi, tlm_response_status rs) {
    stub(bus, range(lo, hi), rs);
}

void stub(sc_object& obj, sc_object& socket, const range& addr,
          tlm_response_status rs) {
    auto& bus = get_bus(obj);
    auto& sock = get_socket(socket);
    if (auto* ini = dynamic_cast<initiator_t*>(&sock)) {
        bus.stub(*ini, addr, rs);
        return;
    }

    if (auto* tgt = dynamic_cast<target_t*>(&sock)) {
        bus.stub(*tgt, addr, rs);
        return;
    }

    VCML_REPORT("%s is not a valid tlm port", sock.name());
}

void stub(sc_object& obj, sc_object& socket, u64 lo, u64 hi,
          tlm_response_status rs) {
    stub(obj, socket, range(lo, hi), rs);
}

void stub(sc_object& obj, const string& socket, const range& addr,
          tlm_response_status rs) {
    stub(obj, get_socket(socket), addr, rs);
}

void stub(sc_object& bus, const string& socket, u64 lo, u64 hi,
          tlm_response_status rs) {
    stub(bus, socket, range(lo, hi), rs);
}

void bind(sc_object& obj, sc_object& socket, bool dummy) {
    // dummy is needed to differentiate binding two versions of binding two
    // sc_objects: binding two sockets and binding an initiator to a bus
    auto& bus = get_bus(obj);
    auto& sock = get_socket(socket);
    if (auto* ini = dynamic_cast<initiator_t*>(&sock)) {
        bus.bind(*ini);
        return;
    }

    if (auto* tgt = dynamic_cast<target_t*>(&sock)) {
        bus.bind(*tgt);
        return;
    }

    VCML_REPORT("%s is not a valid tlm port", sock.name());
}

void bind(sc_object& bus, const string& socket) {
    vcml::bind(bus, get_socket(socket), true);
}

void bind(sc_object& obj, sc_object& socket, const range& addr, u64 offset) {
    auto& bus = get_bus(obj);
    auto& sock = get_socket(socket);
    if (auto* ini = dynamic_cast<initiator_t*>(&sock)) {
        bus.bind(*ini, addr, offset);
        return;
    }

    if (auto* tgt = dynamic_cast<target_t*>(&sock)) {
        bus.bind(*tgt, addr, offset);
        return;
    }

    VCML_REPORT("%s is not a valid tlm port", sock.name());
}

void bind(sc_object& bus, const string& socket, const range& addr,
          u64 offset) {
    vcml::bind(bus, get_socket(socket), addr, offset);
}

void bind(sc_object& bus, sc_object& socket, u64 lo, u64 hi, u64 offset) {
    vcml::bind(bus, socket, range(lo, hi), offset);
}

void bind(sc_object& bus, const string& socket, u64 lo, u64 hi, u64 offset) {
    vcml::bind(bus, socket, range(lo, hi), offset);
}

void bind_default(sc_object& obj, sc_object& socket, u64 offset) {
    auto& bus = get_bus(obj);
    auto& sock = get_socket(socket);
    if (auto* ini = dynamic_cast<initiator_t*>(&sock)) {
        bus.bind_default(*ini, offset);
        return;
    }

    if (auto* tgt = dynamic_cast<target_t*>(&sock)) {
        bus.bind_default(*tgt, offset);
        return;
    }

    VCML_REPORT("%s is not a valid tlm port", sock.name());
}

void bind_default(sc_object& bus, const string& socket, u64 offset) {
    bind_default(bus, get_socket(socket), offset);
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              const range& addr, tlm_response_status rs) {
    stub(bus, mkstr("%s.%s", host.name(), port.c_str()), addr, rs);
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, const range& addr, tlm_response_status rs) {
    stub(bus, mkstr("%s.%s[%zu]", host.name(), port.c_str(), idx), addr, rs);
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              u64 lo, u64 hi, tlm_response_status rs) {
    stub(bus, mkstr("%s.%s", host.name(), port.c_str()), lo, hi, rs);
}

void tlm_stub(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, u64 lo, u64 hi, tlm_response_status rs) {
    stub(bus, mkstr("%s.%s[%zu]", host.name(), port.c_str(), idx), lo, hi, rs);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port) {
    vcml::bind(bus, mkstr("%s.%s", host.name(), port.c_str()));
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx) {
    vcml::bind(bus, mkstr("%s.%s[%zu]", host.name(), port.c_str(), idx));
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              const range& addr, u64 offset) {
    vcml::bind(bus, mkstr("%s.%s", host.name(), port.c_str()), addr, offset);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, const range& addr, u64 offset) {
    vcml::bind(bus, mkstr("%s.%s[%zu]", host.name(), port.c_str(), idx), addr,
               offset);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              u64 lo, u64 hi, u64 offset) {
    tlm_bind(bus, host, port, range(lo, hi), offset);
}

void tlm_bind(sc_object& bus, const sc_object& host, const string& port,
              size_t idx, u64 lo, u64 hi, u64 offset) {
    tlm_bind(bus, host, port, idx, range(lo, hi), offset);
}

void tlm_bind_default(sc_object& bus, const sc_object& host,
                      const string& port, u64 offset) {
    bind_default(bus, mkstr("%s.%s", host.name(), port.c_str()), offset);
}

void tlm_bind_default(sc_object& bus, const sc_object& host,
                      const string& port, size_t idx, u64 offset) {
    bind_default(bus, mkstr("%s.%s[%zu]", host.name(), port.c_str(), idx),
                 offset);
}

} // namespace vcml
