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
    u32 mosi = spi.mosi & spi.mask;
    u32 miso = spi.miso & spi.mask;
    int len = (fls(spi.mask) + 4) / 4;
    os << mkstr("[mosi: 0x%0*x miso: 0x%0*x]", len, mosi, len, miso);
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
    auto guard = get_hierarchy_scope();
    m_stub = new spi_target_stub(basename());
    bind(m_stub->spi_in);
}

void spi_base_initiator_socket::bind_socket(sc_object& obj) {
    using I = spi_base_initiator_socket_b;
    using T = spi_base_target_socket_b;
    bind_generic<I, T>(*this, obj);
}

void spi_base_initiator_socket::stub_socket(void* data) {
    stub();
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
    auto guard = get_hierarchy_scope();
    m_stub = new spi_initiator_stub(basename());
    m_stub->spi_out.bind(*this);
}

void spi_base_target_socket::bind_socket(sc_object& obj) {
    using I = spi_base_initiator_socket_b;
    using T = spi_base_target_socket_b;
    bind_generic<I, T>(*this, obj);
}

void spi_base_target_socket::stub_socket(void* data) {
    stub();
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

void spi_stub(const sc_object& obj, const string& port) {
    stub(mkstr("%s.%s", obj.name(), port.c_str()));
}

void spi_stub(const sc_object& obj, const string& port, size_t idx) {
    stub(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx));
}

void spi_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void spi_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void spi_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void spi_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

} // namespace vcml
