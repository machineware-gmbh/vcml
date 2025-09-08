/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
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
    auto guard = get_hierarchy_scope();
    string name = mkstr("%s_adapter", basename());
    m_adapter = new gpio_target_adapter(name.c_str());
    m_adapter->out.bind(signal);
    bind(m_adapter->in);
}

void gpio_base_initiator_socket::bind_socket(sc_object& obj) {
    if (auto* sig = dynamic_cast<sc_signal_inout_if<bool>*>(&obj)) {
        bind(*sig);
        return;
    }

    using I = gpio_base_initiator_socket;
    using T = gpio_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void gpio_base_initiator_socket::stub_socket(void* unused) {
    stub();
}

void gpio_base_initiator_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
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
    auto guard = get_hierarchy_scope();
    string name = mkstr("%s_adapter", basename());
    m_adapter = new gpio_initiator_adapter(name.c_str());
    m_adapter->in.bind(signal);
    bind(m_adapter->out);
}

void gpio_base_target_socket::bind_socket(sc_object& obj) {
    if (auto* sig = dynamic_cast<sc_signal_inout_if<bool>*>(&obj)) {
        bind(*sig);
        return;
    }

    using I = gpio_base_initiator_socket;
    using T = gpio_base_target_socket;
    bind_generic<I, T>(*this, obj);
}

void gpio_base_target_socket::stub_socket(void* unused) {
    stub();
}

void gpio_base_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new gpio_initiator_stub(basename());
    m_stub->gpio_out.bind(*this);
}

void gpio_initiator_socket::gpio_state_tracker::write(bool val) {
    if (state != val) {
        state = val;
        parent->gpio_transport(*this);
    }
}

bool gpio_initiator_socket::gpio_state_tracker::operator=(bool val) {
    write(val);
    return state;
}

bool gpio_initiator_socket::gpio_state_tracker::operator|=(bool val) {
    write(state | val);
    return state;
}

bool gpio_initiator_socket::gpio_state_tracker::operator&=(bool val) {
    write(state & val);
    return state;
}

bool gpio_initiator_socket::gpio_state_tracker::operator^=(bool val) {
    write(state ^ val);
    return state;
}

gpio_initiator_socket::gpio_initiator_socket(const char* nm, address_space as):
    gpio_base_initiator_socket(nm, as),
    m_host(dynamic_cast<gpio_host*>(hierarchy_top())),
    m_event(nullptr),
    m_state(),
    m_transport(this) {
    bind(m_transport);
}

gpio_initiator_socket::~gpio_initiator_socket() {
    if (m_event)
        delete m_event;
}

const sc_event& gpio_initiator_socket::default_event() {
    if (m_event == nullptr) {
        auto guard = get_hierarchy_scope();
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

gpio_initiator_socket& gpio_initiator_socket::operator|=(bool set) {
    write(read(GPIO_NO_VECTOR) | set, GPIO_NO_VECTOR);
    return *this;
}

gpio_initiator_socket& gpio_initiator_socket::operator&=(bool set) {
    write(read(GPIO_NO_VECTOR) & set, GPIO_NO_VECTOR);
    return *this;
}

gpio_initiator_socket& gpio_initiator_socket::operator^=(bool set) {
    write(read(GPIO_NO_VECTOR) ^ set, GPIO_NO_VECTOR);
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
    bind(m_transport);
}

gpio_target_socket::~gpio_target_socket() {
    if (m_event)
        delete m_event;
}

const sc_event& gpio_target_socket::default_event() {
    if (m_event == nullptr) {
        auto guard = get_hierarchy_scope();
        m_event = new sc_event(mkstr("%s_ev", basename()).c_str());
    }

    return *m_event;
}

void gpio_target_socket::bind(base_type& base) {
    auto* socket = dynamic_cast<gpio_base_target_socket*>(&base);
    VCML_ERROR_ON(!socket, "%s cannot bind to unknown socket type", name());
    if (m_initiator != nullptr)
        m_initiator->bind(*socket);
    else
        m_targets.push_back(socket);
}

void gpio_target_socket::complete_binding(gpio_base_initiator_socket& socket) {
    m_initiator = &socket;
    for (gpio_base_target_socket* target : m_targets)
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
    module(nm), gpio_host(), in("in"), out("out"), m_trigger("trigger") {
    SC_HAS_PROCESS(gpio_target_adapter);
    SC_METHOD(update);
    sensitive << m_trigger;
    dont_initialize();
}

void gpio_target_adapter::update() {
    out = in;
}

void gpio_target_adapter::gpio_transport(const gpio_target_socket& socket,
                                         gpio_payload& tx) {
    m_trigger.notify(SC_ZERO_TIME);
}

void gpio_stub(const sc_object& obj, const string& port) {
    stub(mkstr("%s.%s", obj.name(), port.c_str()));
}

void gpio_stub(const sc_object& obj, const string& port, size_t idx) {
    stub(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx));
}

void gpio_bind(const sc_object& obj1, const string& port1,
               const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void gpio_bind(const sc_object& obj1, const string& port1,
               const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s", obj1.name(), port1.c_str()),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void gpio_bind(const sc_object& obj1, const string& port1, size_t idx1,
               const sc_object& obj2, const string& port2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s", obj2.name(), port2.c_str()));
}

void gpio_bind(const sc_object& obj1, const string& port1, size_t idx1,
               const sc_object& obj2, const string& port2, size_t idx2) {
    vcml::bind(mkstr("%s.%s[%zu]", obj1.name(), port1.c_str(), idx1),
               mkstr("%s.%s[%zu]", obj2.name(), port2.c_str(), idx2));
}

void gpio_bind(const sc_object& obj, const string& port,
               sc_signal_inout_if<bool>& sigif) {
    sc_object* sig = dynamic_cast<sc_object*>(&sigif);
    VCML_REPORT_ON(!sig, "failed to bind to signal: not an object");
    vcml::bind(mkstr("%s.%s", obj.name(), port.c_str()), sig->name());
}

void gpio_bind(const sc_object& obj, const string& port, size_t idx,
               sc_signal_inout_if<bool>& sigif) {
    sc_object* sig = dynamic_cast<sc_object*>(&sigif);
    VCML_REPORT_ON(!sig, "failed to bind to signal: not an object");
    vcml::bind(mkstr("%s.%s[%zu]", obj.name(), port.c_str(), idx),
               sig->name());
}

} // namespace vcml
