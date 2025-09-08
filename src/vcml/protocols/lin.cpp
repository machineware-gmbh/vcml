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

void lin_base_initiator_socket::bind_socket(sc_object& obj) {
    using I = lin_base_initiator_socket;
    using T = lin_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void lin_base_initiator_socket::stub_socket(void* data) {
    stub();
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

void lin_base_target_socket::bind_socket(sc_object& obj) {
    using I = lin_base_initiator_socket;
    using T = lin_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void lin_base_target_socket::stub_socket(void* data) {
    stub();
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

void lin_stub(const sc_object& obj, const string& port) {
    stub(mkstr("%s.%s", obj.name(), port.c_str()));
}

void lin_stub(const sc_object& obj, const string& port, size_t idx) {
    stub(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx));
}

void lin_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void lin_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void lin_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void lin_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

} // namespace vcml
