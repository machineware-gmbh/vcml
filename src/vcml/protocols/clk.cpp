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

ostream& operator<<(ostream& os, const clk_desc& clk) {
    stream_guard guard(os);
    os << std::dec << "CLK [";
    if (clk.period == SC_ZERO_TIME) {
        os << "off";
    } else {
        os << clk.period << ", " << clk_get_hz(clk) << "Hz, " << clk.duty_cycle
           << ", " << (clk.polarity ? "pos" : "neg") << "edge first";
    }
    os << "]";
    return os;
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

void clk_base_initiator_socket::bind_socket(sc_object& obj) {
    using I = clk_base_initiator_socket;
    using T = clk_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void clk_base_initiator_socket::stub_socket(void* unused) {
    stub();
}

void clk_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
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

void clk_base_target_socket::bind_socket(sc_object& obj) {
    using I = clk_base_initiator_socket;
    using T = clk_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void clk_base_target_socket::stub_socket(void* hz) {
    if (hz)
        stub(*(hz_t*)hz);
    else
        stub();
}

void clk_base_target_socket::stub(const clk_desc& clk) {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new clk_initiator_stub(basename(), clk);
    m_stub->clk_out.bind(*this);
}

void clk_base_target_socket::stub(hz_t hz) {
    clk_desc clk{};
    clk.polarity = true;
    clk.duty_cycle = 0.5;
    clk_set_hz(clk, hz);
    stub(clk);
}

clk_initiator_socket::clk_initiator_socket(const char* nm, address_space as):
    clk_base_initiator_socket(nm, as),
    m_host(dynamic_cast<clk_host*>(hierarchy_top())),
    m_clk(),
    m_transport(this) {
    bind(m_transport);
}

void clk_initiator_socket::set(const clk_desc& clk) {
    if (clk != m_clk) {
        clk_transport(clk);
        m_clk = clk;
    }
}

clk_initiator_socket& clk_initiator_socket::operator=(clk_desc& clk) {
    set(clk);
    return *this;
}

void clk_initiator_socket::set_hz(hz_t hz) {
    if (hz < 0)
        hz = 0;

    clk_desc clk = m_clk;
    clk_set_hz(clk, hz);
    set(clk);
}

clk_initiator_socket& clk_initiator_socket::operator=(hz_t hz) {
    set_hz(hz);
    return *this;
}

void clk_initiator_socket::clk_transport(const clk_desc& tx) {
    trace_fw(tx);
    for (int i = 0; i < size(); i++)
        get_interface(i)->clk_transport(tx);
    trace_bw(tx);
}

clk_target_socket::clk_target_socket(const char* nm, address_space space):
    clk_base_target_socket(nm, space),
    m_host(hierarchy_search<clk_host>()),
    m_transport(this),
    m_initiator(nullptr),
    m_targets() {
    VCML_ERROR_ON(!m_host, "%s declared outside clk_target", name());
    clk_base_target_socket::bind(m_transport);
}

void clk_target_socket::bind(base_type& base) {
    auto* socket = dynamic_cast<clk_base_target_socket*>(&base);
    VCML_ERROR_ON(!socket, "%s cannot bind to unknown socket type", name());
    if (m_initiator != nullptr)
        m_initiator->bind(*socket);
    else
        m_targets.push_back(socket);
}

void clk_target_socket::complete_binding(clk_base_initiator_socket& socket) {
    m_initiator = &socket;
    for (clk_base_target_socket* target : m_targets) {
        target->bind(socket);
        target->complete_binding(socket);
    }
    m_targets.clear();
}

clk_desc clk_target_socket::get() const {
    const clk_bw_transport_if* iface = get_base_port().get_interface(0);
    if (iface == nullptr)
        return clk_desc{};

    return const_cast<clk_bw_transport_if*>(iface)->clk_query();
}

void clk_target_socket::clk_transport_internal(const clk_desc& tx) {
    trace_fw(tx);
    clk_transport(tx);
    trace_bw(tx);
}

void clk_target_socket::clk_transport(const clk_desc& tx) {
    m_host->clk_notify(*this, tx);
}

clk_initiator_stub::clk_initiator_stub(const char* nm, const clk_desc& clk):
    clk_bw_transport_if(), m_clk(clk), clk_out(mkstr("%s_stub", nm).c_str()) {
    clk_out.bind(*(clk_bw_transport_if*)this);
}

void clk_target_stub::clk_transport(const clk_desc& clk) {
    // nothing to do
}

clk_target_stub::clk_target_stub(const char* nm):
    clk_fw_transport_if(), clk_in(mkstr("%s_stub", nm).c_str()) {
    clk_in.bind(*(clk_fw_transport_if*)this);
}

void clk_stub(const sc_object& obj, const string& port, hz_t hz) {
    stub(mkstr("%s.%s", obj.name(), port.c_str()), &hz);
}

void clk_stub(const sc_object& obj, const string& port, size_t idx, hz_t hz) {
    stub(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx), &hz);
}

void clk_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void clk_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void clk_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void clk_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

} // namespace vcml
