/******************************************************************************
 *                                                                            *
 * Copyright 2021 Jan Henrik Weinstock                                        *
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

void clk_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new clk_initiator_stub(basename());
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
        stl_remove_erase(m_host->m_initiator_sockets, this);
}

void clk_initiator_socket::set(clock_t hz) {
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

void clk_target_socket::clk_transport(const clk_payload& tx) {
    trace_fw(tx);

    if (tx.newhz != m_hz) {
        m_host->clk_notify(*this, tx);
        m_hz = tx.newhz;
    }

    trace_bw(tx);
}

clk_target_socket::clk_target_socket(const char* nm, address_space space):
    clk_base_target_socket(nm, space),
    m_host(hierarchy_search<clk_host>()),
    m_hz(0),
    m_transport(this) {
    VCML_ERROR_ON(!m_host, "%s declared outside clk_target", name());
    m_host->m_target_sockets.push_back(this);
    bind(m_transport);
}

clk_target_socket::~clk_target_socket() {
    stl_remove_erase(m_host->m_target_sockets, this);
}

clk_initiator_stub::clk_initiator_stub(const char* nm):
    clk_bw_transport_if(), clk_out(mkstr("%s_stub", nm).c_str()) {
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
