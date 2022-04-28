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

#include "vcml/protocols/rst.h"

namespace vcml {

const char* rst_signal_str(rst_signal sig) {
    switch (sig) {
    case RST_PULSE:
        return "RST_PULSE";
    case RST_LEVEL:
        return "RST_LEVEL";
    default:
        VCML_ERROR("invalid reset signal type: %d", sig);
    }
}

ostream& operator<<(ostream& os, const rst_payload& rst) {
    switch (rst.signal) {
    case RST_PULSE:
        os << (rst.reset ? "RST^" : "RST0");
        break;
    case RST_LEVEL:
        os << (rst.reset ? "RST+" : "RST-");
        break;
    }

    return os;
}

rst_host::rst_target_sockets rst_host::all_rst_target_sockets(
    address_space as) const {
    rst_target_sockets sockets;
    for (auto& socket : m_target_sockets)
        if (socket->as == as)
            sockets.push_back(socket);
    return sockets;
}

rst_base_initiator_socket::rst_base_initiator_socket(const char* nm,
                                                     address_space as):
    rst_base_initiator_socket_b(nm, as), m_stub(nullptr) {
}

rst_base_initiator_socket::~rst_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void rst_base_initiator_socket::bind(rst_base_target_socket& socket) {
    rst_base_initiator_socket_b::bind(socket);
    socket.complete_binding(*this);
}

void rst_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new rst_target_stub(basename());
    bind(m_stub->rst_in);
}

rst_base_target_socket::rst_base_target_socket(const char* nm,
                                               address_space space):
    rst_base_target_socket_b(nm, space), m_stub(nullptr) {
}

rst_base_target_socket::~rst_base_target_socket() {
    if (m_stub != nullptr)
        delete m_stub;
}

void rst_base_target_socket::bind(rst_base_initiator_socket& other) {
    rst_base_target_socket_b::bind(other);
    complete_binding(other);
}

void rst_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new rst_initiator_stub(basename());
    m_stub->rst_out.bind(*this);
}

rst_initiator_socket::rst_initiator_socket(const char* nm, address_space as):
    rst_base_initiator_socket(nm, as),
    m_host(dynamic_cast<rst_host*>(hierarchy_top())),
    m_event(nullptr),
    m_state(false),
    m_transport(this) {
    bind(m_transport);
    if (m_host)
        m_host->m_initiator_sockets.push_back(this);
}

rst_initiator_socket::~rst_initiator_socket() {
    if (m_host)
        stl_remove_erase(m_host->m_initiator_sockets, this);
    if (m_event)
        delete m_event;
}

const sc_event& rst_initiator_socket::default_event() {
    if (m_event == nullptr) {
        hierarchy_guard guard(this);
        m_event = new sc_event(mkstr("%s_ev", basename()).c_str());
    }

    return *m_event;
}

void rst_initiator_socket::reset(bool state, rst_signal sig) {
    rst_payload tx;
    tx.reset  = state;
    tx.signal = sig;

    if (sig == RST_PULSE)
        rst_transport(tx);
    else if (state != m_state) {
        m_state = state;
        rst_transport(tx);
    }
}

rst_initiator_socket& rst_initiator_socket::operator=(bool set) {
    reset(set, RST_LEVEL);
    return *this;
}

void rst_initiator_socket::rst_transport(const rst_payload& rst) {
    trace_fw(rst);
    for (int i = 0; i < size(); i++)
        get_interface(i)->rst_transport(rst);
    trace_bw(rst);

    if (m_event)
        m_event->notify(SC_ZERO_TIME);
}

void rst_target_socket::rst_transport(const rst_payload& rst) {
    trace_fw(rst);

    if (rst.signal == RST_PULSE && rst.reset)
        m_target->rst_notify(*this, rst);
    else if (rst.reset != m_state) {
        m_state = rst.reset;
        m_target->rst_notify(*this, rst);
    }

    trace_bw(rst);

    if (m_event)
        m_event->notify(SC_ZERO_TIME);
}

rst_target_socket::rst_target_socket(const char* nm, address_space space):
    rst_base_target_socket(nm, space),
    m_target(hierarchy_search<rst_host>()),
    m_event(nullptr),
    m_state(false),
    m_transport(this),
    m_initiator(nullptr),
    m_targets() {
    VCML_ERROR_ON(!m_target, "%s declared outside rst_target", name());
    m_target->m_target_sockets.push_back(this);
    bind(m_transport);
}

rst_target_socket::~rst_target_socket() {
    stl_remove_erase(m_target->m_target_sockets, this);
    if (m_event)
        delete m_event;
}

const sc_event& rst_target_socket::default_event() {
    if (m_event == nullptr) {
        hierarchy_guard guard(this);
        m_event = new sc_event(mkstr("%s_ev", basename()).c_str());
    }

    return *m_event;
}

void rst_target_socket::bind(rst_target_socket& socket) {
    if (m_initiator != nullptr)
        m_initiator->bind(socket);
    else
        m_targets.push_back(&socket);
}

void rst_target_socket::complete_binding(rst_base_initiator_socket& socket) {
    m_initiator = &socket;
    for (rst_target_socket* target : m_targets)
        target->bind(socket);
    m_targets.clear();
}

rst_initiator_stub::rst_initiator_stub(const char* nm):
    rst_bw_transport_if(), rst_out(mkstr("%s_stub", nm).c_str()) {
    rst_out.bind(*(rst_bw_transport_if*)this);
}

void rst_target_stub::rst_transport(const rst_payload& rst) {
    // nothing to do
}

rst_target_stub::rst_target_stub(const char* nm):
    rst_fw_transport_if(), rst_in(mkstr("%s_stub", nm).c_str()) {
    rst_in.bind(*(rst_fw_transport_if*)this);
}

rst_initiator_adapter::rst_initiator_adapter(const sc_module_name& nm):
    module(nm), rst_in("rst_in"), rst_out("rst_out") {
    SC_HAS_PROCESS(rst_initiator_adapter);
    SC_METHOD(update);
    sensitive << rst_in;
}

void rst_initiator_adapter::update() {
    rst_out.reset(rst_in.read(), RST_LEVEL);
}

rst_target_adapter::rst_target_adapter(const sc_module_name& nm):
    module(nm), rst_host(), rst_in("rst_in"), rst_out("rst_out") {
}

void rst_target_adapter::rst_notify(const rst_target_socket& socket,
                                    const rst_payload& tx) {
    rst_out = tx.reset;
}

} // namespace vcml
