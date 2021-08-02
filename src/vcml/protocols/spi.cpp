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

    spi_initiator_socket::spi_initiator_socket():
        spi_bw_transport_if(),
        tlm::tlm_base_initiator_socket<1, spi_fw_transport_if,
                                          spi_bw_transport_if, 1,
                                          sc_core::SC_ONE_OR_MORE_BOUND>(),
        m_parent(dynamic_cast<module*>(get_parent_object())),
        m_stub(nullptr) {
        VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
        bind(*(spi_bw_transport_if*)this);
    }

    spi_initiator_socket::spi_initiator_socket(const char* nm):
        spi_bw_transport_if(),
        tlm::tlm_base_initiator_socket<1, spi_fw_transport_if,
                                          spi_bw_transport_if, 1,
                                          sc_core::SC_ONE_OR_MORE_BOUND>(nm),
        m_parent(dynamic_cast<module*>(get_parent_object())),
        m_stub(nullptr) {
        VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
        bind(*(spi_bw_transport_if*)this);
    }

    spi_initiator_socket::~spi_initiator_socket() {
        if (m_stub != nullptr)
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

    spi_target_socket::spi_target_socket():
        spi_fw_transport_if(),
        tlm::tlm_base_target_socket<1, spi_fw_transport_if,
                                       spi_bw_transport_if, 1,
                                       sc_core::SC_ONE_OR_MORE_BOUND>(),
        m_parent(dynamic_cast<module*>(get_parent_object())),
        m_host(dynamic_cast<spi_host*>(get_parent_object())),
        m_stub(nullptr) {
        VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
        VCML_ERROR_ON(!m_host, "%s declared outside spi_host", name());
        bind(*(spi_fw_transport_if*)this);
    }

    spi_target_socket::spi_target_socket(const char* nm):
        spi_fw_transport_if(),
        tlm::tlm_base_target_socket<1, spi_fw_transport_if,
                                       spi_bw_transport_if, 1,
                                       sc_core::SC_ONE_OR_MORE_BOUND>(nm),
        m_parent(dynamic_cast<module*>(get_parent_object())),
        m_host(dynamic_cast<spi_host*>(get_parent_object())),
        m_stub(nullptr) {
        VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
        VCML_ERROR_ON(!m_host, "%s declared outside spi_host", name());
        bind(*(spi_fw_transport_if*)this);
    }

    spi_target_socket::~spi_target_socket() {
        if (m_stub != nullptr)
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
        spi_bw_transport_if(),
        SPI_OUT("SPI_OUT") {
        SPI_OUT.bind(*this);
    }

    spi_initiator_stub::~spi_initiator_stub() {
        // nothing to do
    }

    void spi_target_stub::spi_transport(spi_payload& payload) {
        // nothing to do
    }

    spi_target_stub::spi_target_stub(const sc_module_name& nm):
        module(nm),
        spi_fw_transport_if(),
        SPI_IN("SPI_IN") {
        SPI_IN.bind(*this);
    }

    spi_target_stub::~spi_target_stub() {
        // nothing to do
    }

}
