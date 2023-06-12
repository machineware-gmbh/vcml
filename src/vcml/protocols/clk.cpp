/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/clk.h"

namespace vcml {

ostream& operator<<(ostream& os, const clk_payload& clk) {
    stream_guard guard(os);
    os << std::dec << "CLK [";
    if (clk.oldhz)
        os << clk.oldhz << "Hz";
    else
        os << "off";
    os << "->";
    if (clk.newhz)
        os << clk.newhz << "Hz";
    else
        os << "off";
    return os << "]";
}

clk_host::clk_target_sockets clk_host::all_clk_target_sockets(
    address_space as) const {
    clk_target_sockets sockets;
    for (auto& socket : m_target_sockets)
        if (socket->as == as)
            sockets.push_back(socket);
    return sockets;
}

clk_base_initiator_socket::clk_base_initiator_socket(const char* nm,
                                                     address_space as):
    clk_base_initiator_socket_b(nm, as), m_stub(nullptr) {
}

clk_base_initiator_socket::~clk_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void clk_base_initiator_socket::bind(clk_base_target_socket& socket) {
    clk_base_initiator_socket_b::bind(socket);
    socket.complete_binding(*this);
}

void clk_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new clk_target_stub(basename());
    bind(m_stub->clk_in);
}

clk_base_target_socket::clk_base_target_socket(const char* nm,
                                               address_space space):
    clk_base_target_socket_b(nm, space), m_stub(nullptr) {
}

clk_base_target_socket::~clk_base_target_socket() {
    if (m_stub != nullptr)
        delete m_stub;
}

void clk_base_target_socket::bind(clk_base_initiator_socket& other) {
    clk_base_target_socket_b::bind(other);
    complete_binding(other);
}

void clk_base_target_socket::stub(clock_t hz) {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new clk_initiator_stub(basename(), hz);
    m_stub->clk_out.bind(*this);
}

clk_initiator_socket::clk_initiator_socket(const char* nm, address_space as):
    clk_base_initiator_socket(nm, as),
    m_host(dynamic_cast<clk_host*>(hierarchy_top())),
    m_hz(0),
    m_transport(this) {
    bind(m_transport);
    if (m_host)
        m_host->m_initiator_sockets.push_back(this);
}

clk_initiator_socket::~clk_initiator_socket() {
    if (m_host)
        stl_remove(m_host->m_initiator_sockets, this);
}

void clk_initiator_socket::set(clock_t hz) {
    if (hz < 0)
        hz = 0;

    if (hz != m_hz) {
        clk_payload tx;
        tx.oldhz = m_hz;
        tx.newhz = hz;
        clk_transport(tx);
        m_hz = hz;
    }
}

clk_initiator_socket& clk_initiator_socket::operator=(clock_t hz) {
    set(hz);
    return *this;
}

void clk_initiator_socket::clk_transport(const clk_payload& tx) {
    trace_fw(tx);
    for (int i = 0; i < size(); i++)
        get_interface(i)->clk_transport(tx);
    trace_bw(tx);
}

void clk_target_socket::clk_transport_internal(const clk_payload& tx) {
    trace_fw(tx);
    if (tx.oldhz != tx.newhz)
        clk_transport(tx);
    trace_bw(tx);
}

void clk_target_socket::clk_transport(const clk_payload& tx) {
    m_host->clk_notify(*this, tx);
}

clk_target_socket::clk_target_socket(const char* nm, address_space space):
    clk_base_target_socket(nm, space),
    m_host(hierarchy_search<clk_host>()),
    m_transport(this),
    m_initiator(nullptr),
    m_targets() {
    VCML_ERROR_ON(!m_host, "%s declared outside clk_target", name());
    m_host->m_target_sockets.push_back(this);
    clk_base_target_socket::bind(m_transport);
}

clk_target_socket::~clk_target_socket() {
    stl_remove(m_host->m_target_sockets, this);
}

void clk_target_socket::bind(clk_base_target_socket& socket) {
    if (m_initiator != nullptr)
        m_initiator->bind(socket);
    else
        m_targets.push_back(&socket);
}

void clk_target_socket::complete_binding(clk_base_initiator_socket& socket) {
    m_initiator = &socket;
    for (clk_base_target_socket* target : m_targets) {
        target->bind(socket);
        target->complete_binding(socket);
    }
    m_targets.clear();
}

clock_t clk_target_socket::read() const {
    const clk_bw_transport_if* iface = get_base_port().get_interface(0);
    if (iface == nullptr)
        return 0;

    return const_cast<clk_bw_transport_if*>(iface)->clk_get_hz();
}

sc_time clk_target_socket::cycle() const {
    clock_t hz = read();
    return hz ? sc_time(1.0 / hz, SC_SEC) : SC_ZERO_TIME;
}

clk_initiator_stub::clk_initiator_stub(const char* nm, clock_t hz):
    clk_bw_transport_if(), m_hz(hz), clk_out(mkstr("%s_stub", nm).c_str()) {
    clk_out.bind(*(clk_bw_transport_if*)this);
}

void clk_target_stub::clk_transport(const clk_payload& clk) {
    // nothing to do
}

clk_target_stub::clk_target_stub(const char* nm):
    clk_fw_transport_if(), clk_in(mkstr("%s_stub", nm).c_str()) {
    clk_in.bind(*(clk_fw_transport_if*)this);
}

} // namespace vcml
