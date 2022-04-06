/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/spi.h"

namespace vcml {

ostream& operator<<(ostream& os, const spi_payload& spi) {
    os << mkstr("[mosi: 0x%02hhx miso: 0x%02hhx]", spi.mosi, spi.miso);
    return os;
}

spi_host::spi_target_sockets spi_host::all_spi_target_sockets(
    address_space as) {
    spi_target_sockets sockets;
    for (auto& socket : m_target_sockets)
        if (socket->as == as)
            sockets.push_back(socket);
    return sockets;
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
    if (m_host)
        m_host->m_initiator_sockets.push_back(this);
}

spi_initiator_socket::~spi_initiator_socket() {
    if (m_host)
        stl_remove_erase(m_host->m_initiator_sockets, this);
}

void spi_initiator_socket::transport(spi_payload& spi) {
    trace_fw(spi);
    (*this)->spi_transport(spi);
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
    m_host->m_target_sockets.push_back(this);
    bind(m_transport);
}

spi_target_socket::~spi_target_socket() {
    if (m_host)
        stl_remove_erase(m_host->m_target_sockets, this);
}

spi_initiator_stub::spi_initiator_stub(const char* nm):
    m_transport(), spi_out(mkstr("%s_stub", nm).c_str()) {
    spi_out.bind(m_transport);
}

spi_target_stub::spi_target_stub(const char* nm):
    m_transport(), spi_in(mkstr("%s_stub", nm).c_str()) {
    spi_in.bind(m_transport);
}

} // namespace vcml
