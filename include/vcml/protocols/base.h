/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_BASE_H
#define VCML_PROTOCOLS_BASE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/version.h"

#include "vcml/tracing/tracer.h"
#include "vcml/properties/property.h"

#if SYSTEMC_VERSION < SYSTEMC_VERSION_2_3_2
#define VCML_PROTO_OVERRIDE
#else
#define VCML_PROTO_OVERRIDE override
#endif

namespace vcml {

class base_socket
{
private:
    sc_object* m_port;

public:
    const address_space as;

    property<bool> trace_all;
    property<bool> trace_errors;

    base_socket() = delete;
    base_socket(sc_object* port, address_space space):
        m_port(port),
        as(space),
        trace_all(port, "trace", false),
        trace_errors(port, "trace_errors", false) {
        trace_all.inherit_default();
        trace_errors.inherit_default();
    }

    virtual ~base_socket() = default;

    virtual const char* version() const { return VCML_VERSION_STRING; }

protected:
    template <typename PAYLOAD>
    void trace_fw(const PAYLOAD& tx, const sc_time& t = SC_ZERO_TIME) {
        if (trace_all)
            tracer::record(TRACE_FW, *m_port, tx, t);
    }

    template <typename PAYLOAD>
    void trace_bw(const PAYLOAD& tx, const sc_time& t = SC_ZERO_TIME) {
        if (trace_all || (trace_errors && failed(tx)))
            tracer::record(TRACE_BW, *m_port, tx, t);
    }
};

template <typename FW, typename BW, unsigned int WIDTH = 1, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class base_initiator_socket
    : public tlm::tlm_base_initiator_socket<WIDTH, FW, BW, N, POL>,
      public base_socket
{
public:
    base_initiator_socket(const char* nm, address_space as = VCML_AS_DEFAULT):
        tlm::tlm_base_initiator_socket<WIDTH, FW, BW, N, POL>(nm),
        base_socket(this, as) {}

    virtual sc_type_index get_protocol_types() const VCML_PROTO_OVERRIDE {
        return typeid(typename FW::protocol_types);
    }

    bool is_bound() const {
        using base = tlm::tlm_base_initiator_socket<WIDTH, FW, BW, N, POL>;
        const sc_core::sc_port_b<FW>& port = base::get_base_port();
        return const_cast<sc_core::sc_port_b<FW>&>(port).bind_count();
    }
};

template <typename FW, typename BW, unsigned int WIDTH = 1, int N = 1,
          sc_core::sc_port_policy POL = sc_core::SC_ONE_OR_MORE_BOUND>
class base_target_socket
    : public tlm::tlm_base_target_socket<WIDTH, FW, BW, N, POL>,
      public base_socket
{
public:
    base_target_socket(const char* nm, address_space space = VCML_AS_DEFAULT):
        tlm::tlm_base_target_socket<WIDTH, FW, BW, N, POL>(nm),
        base_socket(this, space) {}

    virtual sc_type_index get_protocol_types() const VCML_PROTO_OVERRIDE {
        return typeid(typename BW::protocol_types);
    }

    bool is_bound() const {
        using base = tlm::tlm_base_target_socket<WIDTH, FW, BW, N, POL>;
        const sc_core::sc_port_b<BW>& port = base::get_base_port();
        return const_cast<sc_core::sc_port_b<BW>&>(port).bind_count();
    }
};

template <typename FW, typename BW, unsigned int WIDTH = 1>
using multi_initiator_socket = base_initiator_socket<FW, BW, WIDTH, 0>;

template <typename FW, typename BW, unsigned int WIDTH = 1>
using multi_target_socket = base_target_socket<FW, BW, WIDTH, 0>;

template <typename SOCKET>
class socket_array : public sc_object
{
public:
    typedef unordered_map<size_t, SOCKET*> map_type;
    typedef unordered_map<const SOCKET*, size_t> revmap_type;
    typedef typename map_type::iterator iterator;
    typedef typename map_type::const_iterator const_iterator;

private:
    size_t m_next;
    size_t m_max;
    address_space m_space;
    map_type m_sockets;
    revmap_type m_ids;

public:
    socket_array(const char* nm):
        sc_object(nm),
        m_next(0),
        m_max(SIZE_MAX),
        m_space(VCML_AS_DEFAULT),
        m_sockets(),
        m_ids() {
        // nothing to do
    }

    socket_array(const char* nm, size_t max): socket_array(nm) { m_max = max; }
    socket_array(const char* nm, address_space as): socket_array(nm) {
        m_space = as;
    }

    socket_array(const char* nm, size_t max, address_space as):
        socket_array(nm) {
        m_space = as;
        m_max = max;
    }

    virtual ~socket_array() {
        for (auto socket : m_sockets)
            delete socket.second;
    }

    VCML_KIND(socket_array)

    iterator begin() { return m_sockets.begin(); }
    iterator end() { return m_sockets.end(); }

    const_iterator begin() const { return m_sockets.cbegin(); }
    const_iterator end() const { return m_sockets.cend(); }

    SOCKET& get(size_t idx) {
        SOCKET*& socket = m_sockets[idx];
        if (socket)
            return *socket;

        VCML_ERROR_ON(idx >= m_max, "socket out of bounds: %zu", idx);
        hierarchy_guard guard(this);
        string nm = mkstr("%s[%zu]", basename(), idx);
        socket = new SOCKET(nm.c_str(), m_space);
        m_ids[socket] = idx;
        m_next = max(m_next, idx + 1);
        return *socket;
    }

    SOCKET& operator[](size_t idx) { return get(idx); }
    const SOCKET& operator[](size_t idx) const {
        VCML_ERROR_ON(!exists(idx), "socket %zu not found", idx);
        return *m_sockets.at(idx);
    }

    size_t count() const { return m_sockets.size(); }
    bool exists(size_t idx) const { return stl_contains(m_sockets, idx); }
    size_t next_index() const { return m_next; }
    SOCKET& next() { return operator[](next_index()); }

    bool contains(const SOCKET& socket) const {
        return m_ids.find(&socket) != m_ids.end();
    }

    size_t index_of(const SOCKET& socket) const {
        auto it = m_ids.find(&socket);
        if (it == m_ids.end())
            VCML_ERROR("socket %s not part of %s", socket.name(), name());
        return it->second;
    }

    set<size_t> all_keys() const {
        set<size_t> keys;
        for (const auto& socket : m_sockets)
            keys.insert(socket.first);
        return keys;
    }
};

template <typename SOCKET>
bool operator==(const SOCKET& socket, const socket_array<SOCKET>& arr) {
    return arr.contains(socket);
}

} // namespace vcml

#endif
