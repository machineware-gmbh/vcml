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

void clk_base_target_socket::stub(hz_t hz) {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new clk_initiator_stub(basename(), hz);
    m_stub->clk_out.bind(*this);
}

clk_initiator_socket::clk_initiator_socket(const char* nm, address_space as):
    clk_base_initiator_socket(nm, as),
    m_host(dynamic_cast<clk_host*>(hierarchy_top())),
    m_hz(0),
    m_transport(this) {
    bind(m_transport);
}

void clk_initiator_socket::set(hz_t hz) {
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

clk_initiator_socket& clk_initiator_socket::operator=(hz_t hz) {
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

hz_t clk_target_socket::read() const {
    const clk_bw_transport_if* iface = get_base_port().get_interface(0);
    if (iface == nullptr)
        return 0;

    return const_cast<clk_bw_transport_if*>(iface)->clk_get_hz();
}

sc_time clk_target_socket::cycle() const {
    hz_t hz = read();
    return hz ? sc_time(1.0 / hz, SC_SEC) : SC_ZERO_TIME;
}

clk_initiator_stub::clk_initiator_stub(const char* nm, hz_t hz):
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

static clk_base_initiator_socket* get_initiator_socket(sc_object* port) {
    return dynamic_cast<clk_base_initiator_socket*>(port);
}

static clk_base_target_socket* get_target_socket(sc_object* port) {
    return dynamic_cast<clk_base_target_socket*>(port);
}

static clk_base_initiator_socket* get_initiator_socket(sc_object* array,
                                                       size_t idx) {
    auto* base = dynamic_cast<clk_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<clk_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

static clk_base_target_socket* get_target_socket(sc_object* array,
                                                 size_t idx) {
    auto* base = dynamic_cast<clk_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<clk_target_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

clk_base_initiator_socket& clk_initiator(const sc_object& parent,
                                         const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

clk_base_initiator_socket& clk_initiator(const sc_object& parent,
                                         const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

clk_base_target_socket& clk_target(const sc_object& parent,
                                   const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

clk_base_target_socket& clk_target(const sc_object& parent, const string& port,
                                   size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

void clk_stub(const sc_object& obj, const string& port, hz_t hz) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child);
    auto* tgt = get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid clk socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub(hz);
}

void clk_stub(const sc_object& obj, const string& port, size_t idx, hz_t hz) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    clk_base_initiator_socket* isock = get_initiator_socket(child, idx);
    if (isock) {
        isock->stub();
        return;
    }

    clk_base_target_socket* tsock = get_target_socket(child, idx);
    if (tsock) {
        tsock->stub(hz);
        return;
    }

    VCML_ERROR("%s is not a valid clk socket array", child->name());
}

void clk_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid clk port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid clk port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void clk_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid clk port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid clk port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void clk_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid clk port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid clk port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void clk_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid clk port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid clk port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

} // namespace vcml
