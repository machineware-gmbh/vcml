/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/spi.h"

namespace vcml {

ostream& operator<<(ostream& os, const spi_payload& spi) {
    os << mkstr("[mosi: 0x%02hhx miso: 0x%02hhx]", spi.mosi, spi.miso);
    return os;
}

spi_base_initiator_socket::spi_base_initiator_socket(const char* nm,
                                                     address_space a):
    spi_base_initiator_socket_b(nm, a), m_stub(nullptr) {
}

spi_base_initiator_socket::~spi_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void spi_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new spi_target_stub(basename());
    bind(m_stub->spi_in);
}

spi_base_target_socket::spi_base_target_socket(const char* n, address_space a):
    spi_base_target_socket_b(n, a), m_stub(nullptr) {
}

spi_base_target_socket::~spi_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void spi_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new spi_initiator_stub(basename());
    m_stub->spi_out.bind(*this);
}

spi_initiator_socket::spi_initiator_socket(const char* nm, address_space a):
    spi_base_initiator_socket(nm, a),
    m_host(dynamic_cast<spi_host*>(hierarchy_top())),
    m_transport(this) {
    bind(m_transport);
}

void spi_initiator_socket::transport(spi_payload& spi) {
    trace_fw(spi);

    for (int i = 0; i < size(); i++)
        get_interface(i)->spi_transport(spi);

    trace_bw(spi);
}

void spi_target_socket::spi_transport(spi_payload& spi) {
    trace_fw(spi);
    m_host->spi_transport(*this, spi);
    trace_bw(spi);
}

spi_target_socket::spi_target_socket(const char* nm, address_space as):
    spi_base_target_socket(nm, as),
    m_host(hierarchy_search<spi_host>()),
    m_transport(this) {
    VCML_ERROR_ON(!m_host, "%s declared outside spi_host", name());
    bind(m_transport);
}

spi_initiator_stub::spi_initiator_stub(const char* nm):
    spi_bw_transport_if(), spi_out(mkstr("%s_stub", nm).c_str()) {
    spi_out.bind(*this);
}

void spi_target_stub::spi_transport(spi_payload& spi) {
    // nothing to do
}

spi_target_stub::spi_target_stub(const char* nm):
    spi_fw_transport_if(), spi_in(mkstr("%s_stub", nm).c_str()) {
    spi_in.bind(*this);
}

static spi_base_initiator_socket* get_initiator_socket(sc_object* port) {
    return dynamic_cast<spi_base_initiator_socket*>(port);
}

static spi_base_target_socket* get_target_socket(sc_object* port) {
    return dynamic_cast<spi_base_target_socket*>(port);
}

static spi_base_initiator_socket* get_initiator_socket(sc_object* array,
                                                       size_t idx) {
    auto* base = dynamic_cast<spi_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<spi_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

static spi_base_target_socket* get_target_socket(sc_object* array,
                                                 size_t idx) {
    auto* base = dynamic_cast<spi_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    auto* main = dynamic_cast<spi_target_array*>(array);
    if (main)
        return &main->get(idx);
    return nullptr;
}

spi_base_initiator_socket& spi_initiator(const sc_object& parent,
                                         const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

spi_base_initiator_socket& spi_initiator(const sc_object& parent,
                                         const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

spi_base_target_socket& spi_target(const sc_object& parent,
                                   const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

spi_base_target_socket& spi_target(const sc_object& parent, const string& port,
                                   size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

void spi_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket(child);
    auto* tgt = get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid spi socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void spi_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    spi_base_initiator_socket* isock = get_initiator_socket(child, idx);
    if (isock) {
        isock->stub();
        return;
    }

    spi_base_target_socket* tsock = get_target_socket(child, idx);
    if (tsock) {
        tsock->stub();
        return;
    }

    VCML_ERROR("%s is not a valid spi socket array", child->name());
}

void spi_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid spi port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid spi port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void spi_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid spi port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid spi port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void spi_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid spi port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid spi port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void spi_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid spi port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid spi port", p2->name());

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
