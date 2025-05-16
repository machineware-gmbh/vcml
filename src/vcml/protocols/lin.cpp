/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/lin.h"

namespace vcml {

const char* lin_status_str(lin_status sts) {
    switch (sts) {
    case LIN_SUCCESS:
        return "LIN_SUCCESS";
    case LIN_INCOMPLETE:
        return "LIN_INCOMPLETE";
    case LIN_CHECKSUM_ERROR:
        return "LIN_CHECKSUM_ERROR";
    case LIN_SYNC_ERROR:
        return "LIN_SYNC_ERROR";
    case LIN_TIMEOUT_ERROR:
        return "LIN_TIMEOUT_ERROR";
    case LIN_PARITY_ERROR:
        return "LIN_PARITY_ERROR";
    case LIN_FRAMING_ERROR:
        return "LIN_FRAMING_ERROR";
    case LIN_BIT_ERROR:
        return "LIN_BIT_ERROR";
    default:
        VCML_ERROR("invalid lin status: %d", sts);
    }
}

ostream& operator<<(ostream& os, lin_status sts) {
    return os << lin_status_str(sts);
}

ostream& operator<<(ostream& os, const lin_payload& tx) {
    os << mkstr("LIN %hhu [", tx.linid);
    for (size_t i = 0; i < tx.size() - 1; i++)
        os << mkstr("%02hhx ", tx.data[i]);
    os << mkstr("%02hhx]", tx.data[tx.size() - 1]);
    os << " (" << tx.status << ")";
    return os;
}

lin_base_initiator_socket::lin_base_initiator_socket(const char* nm,
                                                     address_space space):
    lin_base_initiator_socket_b(nm, space), m_stub(nullptr) {
}

lin_base_initiator_socket::~lin_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void lin_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new lin_target_stub(basename());
    bind(m_stub->lin_in);
}

lin_base_target_socket::lin_base_target_socket(const char* nm,
                                               address_space space):
    lin_base_target_socket_b(nm, space), m_stub(nullptr) {
}

lin_base_target_socket::~lin_base_target_socket() {
    if (m_stub)
        delete m_stub;
}

void lin_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new lin_initiator_stub(basename());
    m_stub->lin_out.bind(*this);
}

lin_initiator_socket::lin_initiator_socket(const char* nm, address_space as):
    lin_base_initiator_socket(nm, as),
    m_host(hierarchy_search<lin_host>()),
    m_transport(this) {
    bind(m_transport);
}

lin_status lin_initiator_socket::send(u8 linid, u8 data[]) {
    VCML_ERROR_ON(linid > 64, "invalid lin id: %hhu", linid);

    lin_payload tx{};
    tx.linid = linid;
    tx.status = LIN_INCOMPLETE;

    if (data)
        memcpy(tx.data, data, lin_payload_size(linid));

    transport(tx);

    if (data)
        memcpy(data, tx.data, lin_payload_size(linid));

    return tx.status;
}

void lin_initiator_socket::transport(lin_payload& tx) {
    trace_fw(tx);

    for (int i = 0; i < size(); i++) {
        get_interface(i)->lin_transport(tx);
        if (tx.status != LIN_INCOMPLETE)
            break;
    }

    if (tx.status == LIN_INCOMPLETE)
        tx.status = LIN_TIMEOUT_ERROR;

    trace_bw(tx);
}

void lin_target_socket::lin_transport(lin_payload& tx) {
    trace_fw(tx);
    m_host->lin_receive(*this, tx);
    trace_bw(tx);
}

lin_target_socket::lin_target_socket(const char* nm, address_space as):
    lin_base_target_socket(nm, as),
    m_host(hierarchy_search<lin_host>()),
    m_transport(this) {
    bind(m_transport);
    VCML_ERROR_ON(!m_host, "socket %s declared outside lin_host", name());
}

lin_initiator_stub::lin_initiator_stub(const char* nm):
    lin_bw_transport_if(),
    lin_out(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    lin_out.bind(*this);
}

void lin_target_stub::lin_transport(lin_payload& tx) {
    // nothing to do
}

lin_target_stub::lin_target_stub(const char* nm):
    lin_fw_transport_if(),
    lin_in(mkstr("%s_stub", nm).c_str(), VCML_AS_DEFAULT) {
    lin_in.bind(*this);
}

static lin_base_initiator_socket* lin_get_initiator_socket(sc_object* port) {
    return dynamic_cast<lin_base_initiator_socket*>(port);
}

static lin_base_target_socket* lin_get_target_socket(sc_object* port) {
    return dynamic_cast<lin_base_target_socket*>(port);
}

static lin_base_initiator_socket* lin_get_inntiator_socket(sc_object* array,
                                                           size_t idx) {
    if (auto* aif = dynamic_cast<socket_array_if*>(array))
        return aif->fetch_as<lin_base_initiator_socket>(idx, true);
    return nullptr;
}

static lin_base_target_socket* lin_get_target_socket(sc_object* array,
                                                     size_t idx) {
    if (auto* aif = dynamic_cast<socket_array_if*>(array))
        return aif->fetch_as<lin_base_target_socket>(idx, true);
    return nullptr;
}

lin_base_initiator_socket& lin_initiator(const sc_object& parent,
                                         const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = lin_get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

lin_base_initiator_socket& lin_initiator(const sc_object& parent,
                                         const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = lin_get_inntiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

lin_base_target_socket& lin_target(const sc_object& parent,
                                   const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = lin_get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

lin_base_target_socket& lin_target(const sc_object& parent, const string& port,
                                   size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = lin_get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

void lin_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = lin_get_initiator_socket(child);
    auto* tgt = lin_get_target_socket(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid lin socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void lin_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    lin_base_initiator_socket* isock = lin_get_inntiator_socket(child, idx);
    if (isock) {
        isock->stub();
        return;
    }

    lin_base_target_socket* tsock = lin_get_target_socket(child, idx);
    if (tsock) {
        tsock->stub();
        return;
    }

    VCML_ERROR("%s is not a valid lin socket array", child->name());
}

void lin_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = lin_get_initiator_socket(p1);
    auto* i2 = lin_get_initiator_socket(p2);
    auto* t1 = lin_get_target_socket(p1);
    auto* t2 = lin_get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid lin port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid lin port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void lin_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = lin_get_initiator_socket(p1);
    auto* i2 = lin_get_inntiator_socket(p2, idx2);
    auto* t1 = lin_get_target_socket(p1);
    auto* t2 = lin_get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid lin port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid lin port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void lin_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = lin_get_inntiator_socket(p1, idx1);
    auto* i2 = lin_get_initiator_socket(p2);
    auto* t1 = lin_get_target_socket(p1, idx1);
    auto* t2 = lin_get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid lin port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid lin port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void lin_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = lin_get_inntiator_socket(p1, idx1);
    auto* i2 = lin_get_inntiator_socket(p2, idx2);
    auto* t1 = lin_get_target_socket(p1, idx1);
    auto* t2 = lin_get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid lin port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid lin port", p2->name());

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
