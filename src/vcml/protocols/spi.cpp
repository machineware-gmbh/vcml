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

    ostream& operator << (ostream& os, const spi_payload& spi) {
        os << mkstr("[mosi: 0x%02hhx miso: 0x%02hhx]", spi.mosi, spi.miso);
        return os;
    }

    const spi_host::spi_target_sockets
    spi_host::get_spi_target_sockets(address_space as) const {
        spi_target_sockets sockets;
        for (auto& socket : m_target_sockets)
            if (socket->as == as)
                sockets.push_back(socket);
        return sockets;
    }

    spi_host::spi_target_sockets
    spi_host::get_spi_target_sockets(address_space as) {
        spi_target_sockets sockets;
        for (auto& socket : m_target_sockets)
            if (socket->as == as)
                sockets.push_back(socket);
        return sockets;
    }

    spi_initiator_socket::spi_initiator_socket(const char* nm):
        spi_base_initiator_socket(nm),
        spi_bw_transport_if(),
        m_parent(dynamic_cast<module*>(get_parent_object())),
        m_host(dynamic_cast<spi_host*>(get_parent_object())),
        m_stub(nullptr) {
        VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
        bind(*(spi_bw_transport_if*)this);
        if (m_host)
            m_host->m_initiator_sockets.push_back(this);
    }

    spi_initiator_socket::~spi_initiator_socket() {
        if (m_host)
            stl_remove_erase(m_host->m_initiator_sockets, this);
        if (m_stub)
            delete m_stub;
    }

    sc_core::sc_type_index spi_initiator_socket::get_protocol_types() const {
        return typeid(spi_fw_transport_if);
    }

    void spi_initiator_socket::transport(spi_payload& spi) {
        m_parent->trace_fw(*this, spi);
        (*this)->spi_transport(spi);
        m_parent->trace_bw(*this, spi);
    }

    void spi_initiator_socket::stub() {
        VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
        hierarchy_guard guard(m_parent);
        m_stub = new spi_target_stub(mkstr("%s_stub", basename()).c_str());
        bind(m_stub->SPI_IN);
    }

    void spi_target_socket::spi_transport(spi_payload& spi) {
        m_parent->trace_fw(*this, spi);
        m_host->spi_transport(*this, spi);
        m_parent->trace_bw(*this, spi);
    }

    spi_target_socket::spi_target_socket(const char* nm, address_space a):
        spi_base_target_socket(nm),
        spi_fw_transport_if(),
        m_parent(dynamic_cast<module*>(get_parent_object())),
        m_host(dynamic_cast<spi_host*>(get_parent_object())),
        m_stub(nullptr), as(a) {
        VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
        VCML_ERROR_ON(!m_host, "%s declared outside spi_host", name());
        m_host->m_target_sockets.push_back(this);
        bind(*(spi_fw_transport_if*)this);
    }

    spi_target_socket::~spi_target_socket() {
        stl_remove_erase(m_host->m_target_sockets, this);
        if (m_stub)
            delete m_stub;
    }

    sc_core::sc_type_index spi_target_socket::get_protocol_types() const {
        return typeid(spi_bw_transport_if);
    }

    void spi_target_socket::stub() {
        VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
        hierarchy_guard guard(m_parent);
        m_stub = new spi_initiator_stub(mkstr("%s_stub", basename()).c_str());
        m_stub->SPI_OUT.bind(*this);
    }

    spi_initiator_stub::spi_initiator_stub(const sc_module_name& nm):
        module(nm),
        SPI_OUT("SPI_OUT") {
    }

    spi_initiator_stub::~spi_initiator_stub() {
        // nothing to do
    }

    void spi_target_stub::spi_transport(const spi_target_socket& socket,
                                        spi_payload& spi) {
        trace_fw(SPI_IN, spi);
        // nothing to do
        trace_bw(SPI_IN, spi);
    }

    spi_target_stub::spi_target_stub(const sc_module_name& nm):
        module(nm),
        spi_host(),
        SPI_IN("SPI_IN") {
    }

    spi_target_stub::~spi_target_stub() {
        // nothing to do
    }

}
