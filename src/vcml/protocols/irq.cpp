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

#include "vcml/protocols/irq.h"

namespace vcml {

ostream& operator<<(ostream& os, const irq_payload& irq) {
    os << "IRQ" << (irq.active ? "+" : "-");
    if (irq.vector != IRQ_NO_VECTOR)
        os << ":" << irq.vector;
    return os;
}

irq_target::irq_target_sockets irq_target::all_irq_target_sockets(
    address_space as) const {
    irq_target_sockets sockets;
    for (auto& socket : m_target_sockets)
        if (socket->as == as)
            sockets.push_back(socket);
    return sockets;
}

irq_base_initiator_socket::irq_base_initiator_socket(const char* nm,
                                                     address_space as):
    irq_base_initiator_socket_b(nm, as), m_stub(nullptr) {
}

irq_base_initiator_socket::~irq_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
}

void irq_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new irq_target_stub(basename());
    bind(m_stub->irq_in);
}

irq_base_target_socket::irq_base_target_socket(const char* nm,
                                               address_space space):
    irq_base_target_socket_b(nm, space), m_stub(nullptr) {
}

irq_base_target_socket::~irq_base_target_socket() {
    if (m_stub != nullptr)
        delete m_stub;
}

void irq_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new irq_initiator_stub(basename());
    m_stub->irq_out.bind(*this);
}

bool irq_initiator_socket::irq_state_tracker::operator=(bool state) {
    if (state == active)
        return state;

    active = state;
    parent->irq_transport(*this);
    return active;
}

irq_initiator_socket::irq_initiator_socket(const char* nm, address_space as):
    irq_base_initiator_socket(nm, as),
    m_host(dynamic_cast<irq_target*>(hierarchy_top())),
    m_state(),
    m_event(nullptr),
    m_transport(this) {
    bind(m_transport);
    if (m_host)
        m_host->m_initiator_sockets.push_back(this);
}

irq_initiator_socket::~irq_initiator_socket() {
    if (m_host)
        stl_remove_erase(m_host->m_initiator_sockets, this);
    if (m_event)
        delete m_event;
}

const sc_event& irq_initiator_socket::default_event() {
    if (m_event == nullptr) {
        hierarchy_guard guard(this);
        m_event = new sc_event(mkstr("%s_ev", basename()).c_str());
    }

    return *m_event;
}

bool irq_initiator_socket::read(irq_vector vector) const {
    return m_state.count(vector) ? m_state.at(vector) : false;
}

void irq_initiator_socket::write(bool state, irq_vector vector) {
    (*this)[vector] = state;
}

void irq_initiator_socket::raise_irq(irq_vector vector) {
    write(true, vector);
}

void irq_initiator_socket::lower_irq(irq_vector vector) {
    write(false, vector);
}

irq_initiator_socket& irq_initiator_socket::operator=(bool set) {
    (*this)[IRQ_NO_VECTOR] = set;
    return *this;
}

irq_initiator_socket::irq_state_tracker& irq_initiator_socket::operator[](
    irq_vector vector) {
    if (m_state.count(vector))
        return m_state[vector];

    irq_state_tracker state;
    state.parent = this;
    state.active = false;
    state.vector = vector;

    return m_state[vector] = state;
}

void irq_initiator_socket::irq_transport(irq_payload& irq) {
    trace_fw(irq);
    for (int i = 0; i < size(); i++)
        get_interface(i)->irq_transport(irq);
    trace_bw(irq);

    if (m_event)
        m_event->notify(SC_ZERO_TIME);
}

void irq_target_socket::irq_transport(irq_payload& irq) {
    trace_fw(irq);
    m_state[irq.vector] = irq.active;
    m_host->irq_transport(*this, irq);
    trace_bw(irq);
    if (m_event)
        m_event->notify(SC_ZERO_TIME);
}

irq_target_socket::irq_target_socket(const char* nm, address_space space):
    irq_base_target_socket(nm, space),
    m_host(hierarchy_search<irq_target>()),
    m_state(),
    m_event(nullptr),
    m_transport(this) {
    VCML_ERROR_ON(!m_host, "%s declared outside irq_target", name());
    m_host->m_target_sockets.push_back(this);
    bind(m_transport);
}

irq_target_socket::~irq_target_socket() {
    stl_remove_erase(m_host->m_target_sockets, this);
    if (m_event)
        delete m_event;
}

const sc_event& irq_target_socket::default_event() {
    if (m_event == nullptr) {
        hierarchy_guard guard(this);
        m_event = new sc_event(mkstr("%s_ev", basename()).c_str());
    }

    return *m_event;
}

bool irq_target_socket::read(irq_vector vector) const {
    return m_state.count(vector) ? m_state.at(vector) : false;
}

irq_initiator_stub::irq_initiator_stub(const char* nm):
    m_transport(), irq_out(mkstr("%s_stub", nm).c_str()) {
    irq_out.bind(m_transport);
}

irq_target_stub::irq_target_stub(const char* nm):
    m_transport(), irq_in(mkstr("%s_stub", nm).c_str()) {
    irq_in.bind(m_transport);
}

irq_initiator_adapter::irq_initiator_adapter(const sc_module_name& nm):
    module(nm), irq_in("irq_in"), irq_out("irq_out") {
    SC_HAS_PROCESS(irq_initiator_adapter);
    SC_METHOD(update);
    sensitive << irq_in;
}

void irq_initiator_adapter::update() {
    irq_out.write(irq_in.read(), IRQ_NO_VECTOR);
}

irq_target_adapter::irq_target_adapter(const sc_module_name& nm):
    module(nm), irq_target(), irq_in("irq_in"), irq_out("irq_out") {
}

void irq_target_adapter::irq_transport(const irq_target_socket& socket,
                                       irq_payload& irq) {
    irq_out = irq.active;
}

} // namespace vcml
