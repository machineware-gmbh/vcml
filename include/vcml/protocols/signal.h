/******************************************************************************
 *                                                                            *
 * Copyright (C) 2025 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_SIGNAL_H
#define VCML_PROTOCOLS_SIGNAL_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

#include "vcml/protocols/base.h"

namespace vcml {

struct signal_payload_base {
    virtual ~signal_payload_base() = default;
    virtual string to_string() const = 0;
};

ostream& operator<<(ostream& os, const signal_payload_base& tx);

template <typename T>
struct signal_payload : signal_payload_base {
    T data;
    signal_payload(): signal_payload_base(), data() {}
    template <typename U>
    signal_payload(const U& val): signal_payload_base(), data(val) {}
    virtual string to_string() const override { return mwr::to_string(data); }
};

template <typename T>
class signal_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef signal_payload<T> protocol_types;
    virtual void signal_transport(signal_payload<T>& tx) = 0;
};

template <typename T>
class signal_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef signal_payload<T> protocol_types;
};

template <typename T>
class signal_base_initiator_socket;
template <typename T>
class signal_base_target_socket;
template <typename T>
class signal_initiator_socket;
template <typename T>
class signal_target_socket;
template <typename T>
class signal_initiator_stub;
template <typename T>
class signal_target_stub;
template <typename T>
class signal_initiator_adapter;
template <typename T>
class signal_target_adapter;

template <typename T>
class signal_host
{
public:
    signal_host() = default;
    virtual ~signal_host() = default;
    virtual void signal_transport(const signal_target_socket<T>&,
                                  const T&) = 0;
};

template <typename T>
using signal_base_initiator_socket_b = multi_initiator_socket<
    signal_fw_transport_if<T>, signal_bw_transport_if<T>>;

template <typename T>
using signal_base_target_socket_b = multi_target_socket<
    signal_fw_transport_if<T>, signal_bw_transport_if<T>>;

class signal_socket_if
{
public:
    virtual ~signal_socket_if() = default;
    virtual bool try_bind(sc_object& other) = 0;
    virtual void stub() = 0;
};

template <typename T>
class signal_base_initiator_socket : public signal_base_initiator_socket_b<T>,
                                     public signal_socket_if
{
private:
    signal_target_stub<T>* m_stub;
    signal_target_adapter<T>* m_adapter;

    typedef signal_base_initiator_socket_b<T> base;

public:
    signal_base_initiator_socket(const char* nm,
                                 address_space space = VCML_AS_DEFAULT):
        signal_base_initiator_socket_b<T>(nm, space),
        signal_socket_if(),
        m_stub(),
        m_adapter() {
        // nothing to do
    }

    virtual ~signal_base_initiator_socket() {
        if (m_stub)
            delete m_stub;
        if (m_adapter)
            delete m_adapter;
    }

    VCML_KIND(signal_base_initiator_socket);

    using signal_base_initiator_socket_b<T>::bind;
    virtual void bind(signal_base_target_socket<T>& socket) {
        signal_base_initiator_socket_b<T>::bind(socket);
        socket.complete_binding(*this);
    }

    virtual void bind(sc_signal_inout_if<T>& signal) {
        VCML_ERROR_ON(m_adapter, "socket '%s' already bound", base::name());
        auto guard = base::get_hierarchy_scope();
        string name = mkstr("%s_adapter", base::basename());
        m_adapter = new signal_target_adapter<T>(name.c_str());
        m_adapter->out.bind(signal);
        bind(m_adapter->in);
    }

    virtual bool try_bind(sc_object& obj) override {
        auto* isock = dynamic_cast<signal_base_initiator_socket_b<T>*>(&obj);
        if (isock) {
            bind(*isock);
            return true;
        }

        auto* tsock = dynamic_cast<signal_base_target_socket_b<T>*>(&obj);
        if (tsock) {
            bind(*tsock);
            return true;
        }

        auto* signal = dynamic_cast<sc_signal_inout_if<T>*>(&obj);
        if (signal) {
            bind(*signal);
            return true;
        };

        return false;
    }

    virtual void stub() override {
        VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", base::name());
        auto guard = base::get_hierarchy_scope();
        m_stub = new signal_target_stub<T>(base::basename());
        bind(m_stub->signal_in);
    }

    bool is_adapted() const { return m_adapter != nullptr; }
    bool is_stubbed() const { return m_stub != nullptr; }
};

template <typename T>
class signal_base_target_socket : public signal_base_target_socket_b<T>,
                                  public signal_socket_if
{
private:
    signal_initiator_stub<T>* m_stub;
    signal_initiator_adapter<T>* m_adapter;

    typedef signal_base_target_socket_b<T> base;

public:
    signal_base_target_socket(const char* nm,
                              address_space space = VCML_AS_DEFAULT):
        signal_base_target_socket_b<T>(nm, space),
        signal_socket_if(),
        m_stub(),
        m_adapter() {
        // nothing to do
    }

    virtual ~signal_base_target_socket() {
        if (m_stub)
            delete m_stub;
        if (m_adapter)
            delete m_adapter;
    }

    VCML_KIND(signal_base_target_socket);

    using signal_base_target_socket_b<T>::bind;
    virtual void bind(signal_base_initiator_socket<T>& other) {
        signal_base_target_socket_b<T>::bind(other);
        complete_binding(other);
    }

    virtual void bind(sc_signal_inout_if<T>& signal) {
        VCML_ERROR_ON(m_adapter, "socket '%s' already bound", base::name());
        auto guard = base::get_hierarchy_scope();
        string name = mkstr("%s_adapter", base::basename());
        m_adapter = new signal_initiator_adapter<T>(name.c_str());
        m_adapter->in.bind(signal);
        bind(m_adapter->out);
    }

    virtual bool try_bind(sc_object& obj) override {
        auto* isock = dynamic_cast<signal_base_initiator_socket_b<T>*>(&obj);
        if (isock) {
            isock->bind(*this);
            return true;
        }

        auto* tsock = dynamic_cast<signal_base_target_socket_b<T>*>(&obj);
        if (tsock) {
            bind(*tsock);
            return true;
        }

        auto* signal = dynamic_cast<sc_signal_inout_if<T>*>(&obj);
        if (signal) {
            bind(*signal);
            return true;
        };

        return false;
    }

    virtual void complete_binding(signal_base_initiator_socket<T>& socket) {}
    virtual void stub() override {
        VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", base::name());
        auto guard = base::get_hierarchy_scope();
        m_stub = new signal_initiator_stub<T>(base::basename());
        m_stub->signal_out.bind(*this);
    }

    bool is_adapted() const { return m_adapter != nullptr; }
    bool is_stubbed() const { return m_stub != nullptr; }
};

template <typename T, size_t N = SIZE_MAX>
using signal_base_initiator_array = socket_array<
    signal_base_initiator_socket<T>, N>;

template <typename T, size_t N = SIZE_MAX>
using signal_base_target_array = socket_array<signal_base_target_socket<T>, N>;

template <typename T>
class signal_initiator_socket : public signal_base_initiator_socket<T>
{
public:
    typedef signal_base_initiator_socket<T> base;
    signal_initiator_socket(const char* nm,
                            address_space space = VCML_AS_DEFAULT):
        signal_base_initiator_socket<T>(nm, space),
        m_host(hierarchy_search<signal_host<T>>()),
        m_event(),
        m_state(),
        m_transport(this) {
        base::bind(m_transport);
    }

    virtual ~signal_initiator_socket() {
        if (m_event)
            delete m_event;
    }

    VCML_KIND(signal_initiator_socket);

    const sc_event& default_event() {
        if (m_event == nullptr) {
            auto guard = base::get_hierarchy_scope();
            m_event = new sc_event(mkstr("%s_ev", base::basename()).c_str());
        }

        return *m_event;
    }

    T read() const { return m_state; }
    operator T() const { return read(); }
    void write(const T& state) {
        if (state != m_state) {
            m_state = state;
            signal_payload<T> tx(state);
            signal_transport(tx);
        }
    }

    signal_initiator_socket<T>& operator=(const T& val) {
        write(val);
        return *this;
    }

    signal_initiator_socket<T>& operator|=(const T& val) {
        write(m_state | val);
        return *this;
    }

    signal_initiator_socket<T>& operator&=(const T& val) {
        write(m_state & val);
        return *this;
    }

    signal_initiator_socket<T>& operator^=(const T& val) {
        write(m_state ^ val);
        return *this;
    }

private:
    signal_host<T>* m_host;
    sc_event* m_event;
    T m_state;

    struct signal_bw_transport : public signal_bw_transport_if<T> {
        mutable signal_initiator_socket<T>* socket;
        signal_bw_transport(signal_initiator_socket<T>* s):
            signal_bw_transport_if<T>(), socket(s) {}

        virtual const sc_event& default_event() const override {
            return socket->default_event();
        }
    } m_transport;

    void signal_transport(signal_payload<T>& tx) {
        base_socket::trace_fw<signal_payload_base>(tx);
        for (int i = 0; i < base::size(); i++)
            base::get_interface(i)->signal_transport(tx);
        if (m_event)
            m_event->notify(SC_ZERO_TIME);
        base_socket::trace_bw<signal_payload_base>(tx);
    }
};

template <typename T>
class signal_target_socket : public signal_base_target_socket<T>
{
public:
    typedef signal_base_target_socket<T> base;
    signal_target_socket(const char* nm,
                         address_space space = VCML_AS_DEFAULT):
        signal_base_target_socket<T>(nm, space),
        m_host(hierarchy_search<signal_host<T>>()),
        m_event(nullptr),
        m_state(),
        m_initiator(nullptr),
        m_targets(),
        m_transport(this) {
        VCML_ERROR_ON(!m_host, "%s declared outside signal_host",
                      base::name());
        bind(m_transport);
    }

    virtual ~signal_target_socket() {
        if (m_event)
            delete m_event;
    }

    VCML_KIND(signal_target_socket);

    using base::bind;
    virtual void bind(typename base::base_type& other) override {
        auto* socket = dynamic_cast<signal_base_target_socket<T>*>(&other);
        VCML_ERROR_ON(!socket, "%s cannot bind to unknown socket type",
                      base::name());
        if (m_initiator != nullptr)
            m_initiator->bind(*socket);
        else
            m_targets.push_back(socket);
    }

    virtual void complete_binding(
        signal_base_initiator_socket<T>& socket) override {
        m_initiator = &socket;
        for (signal_base_target_socket<T>* target : m_targets)
            target->bind(socket);
        m_targets.clear();
    }

    const sc_event& default_event() {
        if (m_event == nullptr) {
            auto guard = base::get_hierarchy_scope();
            m_event = new sc_event(mkstr("%s_ev", base::basename()).c_str());
        }

        return *m_event;
    }

    const T& read() const { return m_state; }
    operator T() const { return read(); }

    bool operator==(const signal_target_socket<T>& o) const {
        return this == &o;
    }

    bool operator!=(const signal_target_socket<T>& o) const {
        return this == &o;
    }

private:
    signal_host<T>* m_host;
    sc_event* m_event;
    T m_state;
    signal_base_initiator_socket<T>* m_initiator;
    vector<signal_base_target_socket<T>*> m_targets;

    struct signal_fw_transport : public signal_fw_transport_if<T> {
        mutable signal_target_socket<T>* socket;
        signal_fw_transport(signal_target_socket<T>* s):
            signal_fw_transport_if<T>(), socket(s) {}

        virtual void signal_transport(signal_payload<T>& tx) override {
            socket->signal_transport_internal(tx);
        }

        virtual const sc_event& default_event() const override {
            return socket->default_event();
        }
    } m_transport;

    void signal_transport_internal(signal_payload<T>& tx) {
        base_socket::trace_fw<signal_payload_base>(tx);
        m_state = tx.data;
        signal_transport(tx);
        if (m_event)
            m_event->notify(SC_ZERO_TIME);
        base_socket::trace_bw<signal_payload_base>(tx);
    }

protected:
    virtual void signal_transport(signal_payload<T>& tx) {
        m_host->signal_transport(*this, tx.data);
    }
};

template <typename T, size_t N = SIZE_MAX>
using signal_initiator_array = socket_array<signal_initiator_socket<T>, N>;

template <typename T, size_t N = SIZE_MAX>
using signal_target_array = socket_array<signal_target_socket<T>, N>;

template <typename T>
class signal_initiator_stub : private signal_bw_transport_if<T>
{
public:
    signal_base_initiator_socket<T> signal_out;
    signal_initiator_stub(const char* nm):
        signal_bw_transport_if<T>(), signal_out(mkstr("%s_stub", nm).c_str()) {
        signal_out.bind(*(signal_bw_transport_if<T>*)this);
    }
    virtual ~signal_initiator_stub() = default;
};

template <typename T>
class signal_target_stub : private signal_fw_transport_if<T>
{
private:
    virtual void signal_transport(signal_payload<T>& tx) override {}

public:
    signal_base_target_socket<T> signal_in;
    signal_target_stub(const char* nm):
        signal_fw_transport_if<T>(), signal_in(mkstr("%s_stub", nm).c_str()) {
        signal_in.bind(*(signal_fw_transport_if<T>*)this);
    }
    virtual ~signal_target_stub() = default;
};

template <typename T>
class signal_initiator_adapter : public module
{
public:
    sc_in<T> in;
    signal_initiator_socket<T> out;

    signal_initiator_adapter(const sc_module_name& nm):
        module(nm), in("in"), out("out") {
        SC_HAS_PROCESS(signal_initiator_adapter<T>);
        SC_METHOD(update);
        sensitive << in;
    }
    virtual ~signal_initiator_adapter() = default;
    VCML_KIND(signal_initiator_adapter);

private:
    void update() { out = in; }
};

template <typename T>
class signal_target_adapter : public module, public signal_host<T>
{
public:
    signal_target_socket<T> in;
    sc_out<T> out;

    signal_target_adapter(const sc_module_name& nm):
        module(nm),
        signal_host<T>(),
        in("in"),
        out("out"),
        m_trigger("trigger") {
        SC_HAS_PROCESS(signal_target_adapter);
        SC_METHOD(update);
        sensitive << m_trigger;
        dont_initialize();
    }

    virtual ~signal_target_adapter() = default;
    VCML_KIND(signal_target_adapter);

private:
    sc_event m_trigger;

    void update() { out = in; }

    virtual void signal_transport(const signal_target_socket<T>& socket,
                                  const T& data) override {
        m_trigger.notify(SC_ZERO_TIME);
    }
};

void signal_stub(const sc_object& obj, const string& port);
void signal_stub(const sc_object& obj, const string& port, size_t idx);

void signal_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2);
void signal_bind(const sc_object& obj1, const string& port1,
                 const sc_object& obj2, const string& port2, size_t idx2);
void signal_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2);
void signal_bind(const sc_object& obj1, const string& port1, size_t idx1,
                 const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
