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

#include "vcml/protocols/gpio.h"

namespace vcml {

ostream& operator<<(ostream& os, const gpio_payload& tx) {
    stream_guard guard(os);
    os << "GPIO";
    if (tx.vector != GPIO_NO_VECTOR)
        os << ":" << tx.vector;
    os << (tx.state ? "+" : "-");
    return os;
}

gpio_host::gpio_target_sockets gpio_host::all_gpio_target_sockets(
    address_space as) const {
    gpio_target_sockets sockets;
    for (auto& socket : m_target_sockets)
        if (socket->as == as)
            sockets.push_back(socket);
    return sockets;
}

gpio_base_initiator_socket::gpio_base_initiator_socket(const char* nm,
                                                       address_space as):
    gpio_base_initiator_socket_b(nm, as), m_stub(nullptr), m_adapter(nullptr) {
}

gpio_base_initiator_socket::~gpio_base_initiator_socket() {
    if (m_stub)
        delete m_stub;
    if (m_adapter)
        delete m_adapter;
}

void gpio_base_initiator_socket::bind(gpio_base_target_socket& socket) {
    gpio_base_initiator_socket_b::bind(socket);
    socket.complete_binding(*this);
}

void gpio_base_initiator_socket::bind(sc_signal_inout_if<bool>& signal) {
    VCML_ERROR_ON(m_adapter, "socket '%s' already bound", name());
    hierarchy_guard guard(this);
    string name = mkstr("%s_adapter", basename());
    m_adapter = new gpio_target_adapter(name.c_str());
    m_adapter->out.bind(signal);
    bind(m_adapter->in);
}

void gpio_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new gpio_target_stub(basename());
    bind(m_stub->gpio_in);
}

gpio_base_target_socket::gpio_base_target_socket(const char* nm,
                                                 address_space space):
    gpio_base_target_socket_b(nm, space), m_stub(nullptr), m_adapter(nullptr) {
}

gpio_base_target_socket::~gpio_base_target_socket() {
    if (m_stub != nullptr)
        delete m_stub;
    if (m_adapter != nullptr)
        delete m_adapter;
}

void gpio_base_target_socket::bind(gpio_base_initiator_socket& other) {
    gpio_base_target_socket_b::bind(other);
    complete_binding(other);
}

void gpio_base_target_socket::bind(sc_signal_inout_if<bool>& signal) {
    VCML_ERROR_ON(m_adapter, "socket '%s' already bound", name());
    hierarchy_guard guard(this);
    string name = mkstr("%s_adapter", basename());
    m_adapter = new gpio_initiator_adapter(name.c_str());
    m_adapter->in.bind(signal);
    bind(m_adapter->out);
}

void gpio_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    hierarchy_guard guard(this);
    m_stub = new gpio_initiator_stub(basename());
    m_stub->gpio_out.bind(*this);
}

bool gpio_initiator_socket::gpio_state_tracker::operator=(bool st) {
    if (state == st)
        return state;

    state = st;
    parent->gpio_transport(*this);
    return state;
}

gpio_initiator_socket::gpio_initiator_socket(const char* nm, address_space as):
    gpio_base_initiator_socket(nm, as),
    m_host(dynamic_cast<gpio_host*>(hierarchy_top())),
    m_event(nullptr),
    m_state(),
    m_transport(this) {
    bind(m_transport);
    if (m_host)
        m_host->m_initiator_sockets.push_back(this);
}

gpio_initiator_socket::~gpio_initiator_socket() {
    if (m_host)
        stl_remove(m_host->m_initiator_sockets, this);
    if (m_event)
        delete m_event;
}

const sc_event& gpio_initiator_socket::default_event() {
    if (m_event == nullptr) {
        hierarchy_guard guard(this);
        m_event = new sc_event(mkstr("%s_ev", basename()).c_str());
    }

    return *m_event;
}

bool gpio_initiator_socket::read(gpio_vector vector) const {
    return m_state.count(vector) ? m_state.at(vector) : false;
}

void gpio_initiator_socket::write(bool state, gpio_vector vector) {
    (*this)[vector] = state;
}

void gpio_initiator_socket::raise(gpio_vector vector) {
    write(true, vector);
}

void gpio_initiator_socket::lower(gpio_vector vector) {
    write(false, vector);
}

void gpio_initiator_socket::pulse(gpio_vector vector) {
    write(!read(vector), vector);
    write(!read(vector), vector);
}

gpio_initiator_socket& gpio_initiator_socket::operator=(bool set) {
    write(set, GPIO_NO_VECTOR);
    return *this;
}

gpio_initiator_socket::gpio_state_tracker& gpio_initiator_socket::operator[](
    gpio_vector vector) {
    if (m_state.count(vector))
        return m_state[vector];

    gpio_state_tracker state;
    state.parent = this;
    state.state = false;
    state.vector = vector;

    return m_state[vector] = state;
}

void gpio_initiator_socket::gpio_transport(gpio_payload& tx) {
    trace_fw(tx);
    for (int i = 0; i < size(); i++)
        get_interface(i)->gpio_transport(tx);
    if (m_event)
        m_event->notify(SC_ZERO_TIME);
    trace_bw(tx);
}

gpio_target_socket::gpio_target_socket(const char* nm, address_space space):
    gpio_base_target_socket(nm, space),
    m_host(hierarchy_search<gpio_host>()),
    m_event(nullptr),
    m_state(),
    m_initiator(nullptr),
    m_targets(),
    m_transport(this) {
    VCML_ERROR_ON(!m_host, "%s declared outside gpio_host", name());
    m_host->m_target_sockets.push_back(this);
    bind(m_transport);
}

gpio_target_socket::~gpio_target_socket() {
    stl_remove(m_host->m_target_sockets, this);
    if (m_event)
        delete m_event;
}

const sc_event& gpio_target_socket::default_event() {
    if (m_event == nullptr) {
        hierarchy_guard guard(this);
        m_event = new sc_event(mkstr("%s_ev", basename()).c_str());
    }

    return *m_event;
}

void gpio_target_socket::bind(gpio_target_socket& socket) {
    if (m_initiator != nullptr)
        m_initiator->bind(socket);
    else
        m_targets.push_back(&socket);
}

void gpio_target_socket::complete_binding(gpio_base_initiator_socket& socket) {
    m_initiator = &socket;
    for (gpio_target_socket* target : m_targets)
        target->bind(socket);
    m_targets.clear();
}

bool gpio_target_socket::read(gpio_vector vector) const {
    return m_state.count(vector) ? m_state.at(vector) : false;
}

bool gpio_target_socket::operator==(const gpio_target_socket& other) const {
    return this == &other;
}

bool gpio_target_socket::operator!=(const gpio_target_socket& other) const {
    return !(operator==(other));
}

void gpio_target_socket::gpio_transport_internal(gpio_payload& tx) {
    trace_fw(tx);
    if (m_state.count(tx.vector) == 0 || m_state[tx.vector] != tx.state) {
        m_state[tx.vector] = tx.state;
        gpio_transport(tx);
        if (m_event)
            m_event->notify(SC_ZERO_TIME);
    }
    trace_bw(tx);
}

void gpio_target_socket::gpio_transport(gpio_payload& tx) {
    m_host->gpio_transport(*this, tx);
}

gpio_initiator_stub::gpio_initiator_stub(const char* nm):
    gpio_bw_transport_if(), gpio_out(mkstr("%s_stub", nm).c_str()) {
    gpio_out.bind(*(gpio_bw_transport_if*)this);
}

void gpio_target_stub::gpio_transport(gpio_payload& tx) {
    // nothing to do
}

gpio_target_stub::gpio_target_stub(const char* nm):
    gpio_fw_transport_if(), gpio_in(mkstr("%s_stub", nm).c_str()) {
    gpio_in.bind(*(gpio_fw_transport_if*)this);
}

gpio_initiator_adapter::gpio_initiator_adapter(const sc_module_name& nm):
    module(nm), in("in"), out("out") {
    SC_HAS_PROCESS(gpio_initiator_adapter);
    SC_METHOD(update);
    sensitive << in;
}

void gpio_initiator_adapter::update() {
    out.write(in.read(), GPIO_NO_VECTOR);
}

gpio_target_adapter::gpio_target_adapter(const sc_module_name& nm):
    module(nm), gpio_host(), in("in"), out("out") {
}

void gpio_target_adapter::gpio_transport(const gpio_target_socket& socket,
                                         gpio_payload& tx) {
    out = tx.state;
}

} // namespace vcml
