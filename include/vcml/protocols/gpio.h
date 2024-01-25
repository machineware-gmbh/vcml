/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_GPIO_H
#define VCML_PROTOCOLS_GPIO_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"

#include "vcml/protocols/base.h"

namespace vcml {

typedef size_t gpio_vector;

enum gpio_vectors : gpio_vector {
    GPIO_NO_VECTOR = SIZE_MAX,
};

struct gpio_payload {
    gpio_vector vector;
    bool state;
};

ostream& operator<<(ostream& os, const gpio_payload& gpio);

class gpio_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef gpio_payload protocol_types;
    virtual void gpio_transport(gpio_payload& tx) = 0;
};

class gpio_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef gpio_payload protocol_types;
};

class gpio_base_initiator_socket;
class gpio_base_target_socket;
class gpio_initiator_socket;
class gpio_target_socket;
class gpio_initiator_stub;
class gpio_target_stub;
class gpio_initiator_adapter;
class gpio_target_adapter;

class gpio_host
{
public:
    gpio_host() = default;
    virtual ~gpio_host() = default;
    virtual void gpio_transport(const gpio_target_socket&, gpio_payload&) = 0;
};

typedef multi_initiator_socket<gpio_fw_transport_if, gpio_bw_transport_if>
    gpio_base_initiator_socket_b;

typedef multi_target_socket<gpio_fw_transport_if, gpio_bw_transport_if>
    gpio_base_target_socket_b;

class gpio_base_initiator_socket : public gpio_base_initiator_socket_b
{
private:
    gpio_target_stub* m_stub;
    gpio_target_adapter* m_adapter;

public:
    gpio_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~gpio_base_initiator_socket();
    VCML_KIND(gpio_base_initiator_socket);

    using gpio_base_initiator_socket_b::bind;
    virtual void bind(gpio_base_target_socket& socket);
    virtual void bind(sc_signal_inout_if<bool>& signal);

    bool is_adapted() const { return m_adapter != nullptr; }
    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class gpio_base_target_socket : public gpio_base_target_socket_b
{
private:
    gpio_initiator_stub* m_stub;
    gpio_initiator_adapter* m_adapter;

public:
    gpio_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~gpio_base_target_socket();
    VCML_KIND(gpio_base_target_socket);

    using gpio_base_target_socket_b::bind;
    virtual void bind(gpio_base_initiator_socket& other);
    virtual void bind(sc_signal_inout_if<bool>& signal);
    virtual void complete_binding(gpio_base_initiator_socket& socket) {}

    bool is_adapted() const { return m_adapter != nullptr; }
    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

using gpio_base_initiator_array = socket_array<gpio_base_initiator_socket>;
using gpio_base_target_array = socket_array<gpio_base_target_socket>;

class gpio_initiator_socket : public gpio_base_initiator_socket
{
public:
    struct gpio_state_tracker : gpio_payload {
        gpio_initiator_socket* parent;
        bool read() const { return state; }
        void write(bool val);
        bool operator=(bool val);
        bool operator|=(bool val);
        bool operator&=(bool val);
        bool operator^=(bool val);
        operator bool() const { return read(); }
    };

    gpio_initiator_socket(const char* n, address_space a = VCML_AS_DEFAULT);
    virtual ~gpio_initiator_socket();
    VCML_KIND(gpio_initiator_socket);

    const sc_event& default_event();

    bool read(gpio_vector vector = GPIO_NO_VECTOR) const;
    operator bool() const { return read(GPIO_NO_VECTOR); }
    void write(bool state, gpio_vector vector = GPIO_NO_VECTOR);

    void raise(gpio_vector vector = GPIO_NO_VECTOR);
    void lower(gpio_vector vector = GPIO_NO_VECTOR);
    void pulse(gpio_vector vector = GPIO_NO_VECTOR);

    gpio_initiator_socket& operator=(bool set);
    gpio_initiator_socket& operator|=(bool set);
    gpio_initiator_socket& operator&=(bool set);
    gpio_initiator_socket& operator^=(bool set);
    gpio_state_tracker& operator[](gpio_vector vector);

private:
    gpio_host* m_host;
    sc_event* m_event;
    unordered_map<gpio_vector, gpio_state_tracker> m_state;

    struct gpio_bw_transport : public gpio_bw_transport_if {
        mutable gpio_initiator_socket* socket;
        gpio_bw_transport(gpio_initiator_socket* s):
            gpio_bw_transport_if(), socket(s) {}

        virtual const sc_event& default_event() const override {
            return socket->default_event();
        }
    } m_transport;

    void gpio_transport(gpio_payload& tx);
};

class gpio_target_socket : public gpio_base_target_socket
{
public:
    gpio_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~gpio_target_socket();
    VCML_KIND(gpio_target_socket);

    using gpio_base_target_socket::bind;
    virtual void bind(base_type& other) override;
    virtual void complete_binding(gpio_base_initiator_socket& socket) override;

    const sc_event& default_event();

    bool read(gpio_vector vector = GPIO_NO_VECTOR) const;
    bool operator[](gpio_vector vector) const { return read(vector); }
    operator bool() const { return read(GPIO_NO_VECTOR); }

    bool operator==(const gpio_target_socket& o) const;
    bool operator!=(const gpio_target_socket& o) const;

private:
    gpio_host* m_host;
    sc_event* m_event;
    unordered_map<gpio_vector, bool> m_state;
    gpio_base_initiator_socket* m_initiator;
    vector<gpio_base_target_socket*> m_targets;

    struct gpio_fw_transport : public gpio_fw_transport_if {
        mutable gpio_target_socket* socket;
        gpio_fw_transport(gpio_target_socket* s):
            gpio_fw_transport_if(), socket(s) {}

        virtual void gpio_transport(gpio_payload& tx) override {
            socket->gpio_transport_internal(tx);
        }

        virtual const sc_event& default_event() const override {
            return socket->default_event();
        }
    } m_transport;

    void gpio_transport_internal(gpio_payload& gpio);

protected:
    virtual void gpio_transport(gpio_payload& gpio);
};

using gpio_initiator_array = socket_array<gpio_initiator_socket>;
using gpio_target_array = socket_array<gpio_target_socket>;

class gpio_initiator_stub : private gpio_bw_transport_if
{
public:
    gpio_base_initiator_socket gpio_out;
    gpio_initiator_stub(const char* nm);
    virtual ~gpio_initiator_stub() = default;
};

class gpio_target_stub : private gpio_fw_transport_if
{
private:
    virtual void gpio_transport(gpio_payload& tx) override;

public:
    gpio_base_target_socket gpio_in;
    gpio_target_stub(const char* nm);
    virtual ~gpio_target_stub() = default;
};

class gpio_initiator_adapter : public module
{
public:
    sc_in<bool> in;
    gpio_initiator_socket out;

    gpio_initiator_adapter(const sc_module_name& nm);
    virtual ~gpio_initiator_adapter() = default;
    VCML_KIND(gpio_initiator_adapter);

private:
    void update();
};

class gpio_target_adapter : public module, public gpio_host
{
public:
    gpio_target_socket in;
    sc_out<bool> out;

    gpio_target_adapter(const sc_module_name& nm);
    virtual ~gpio_target_adapter() = default;
    VCML_KIND(gpio_target_adapter);

private:
    sc_event m_trigger;

    void update();

    virtual void gpio_transport(const gpio_target_socket& socket,
                                gpio_payload& tx) override;
};

gpio_base_initiator_socket& gpio_initiator(const sc_object& parent,
                                           const string& port);
gpio_base_initiator_socket& gpio_initiator(const sc_object& parent,
                                           const string& port, size_t idx);

gpio_base_target_socket& gpio_target(const sc_object& parent,
                                     const string& port);
gpio_base_target_socket& gpio_target(const sc_object& parent,
                                     const string& port, size_t idx);

void gpio_stub(const sc_object& obj, const string& port);
void gpio_stub(const sc_object& obj, const string& port, size_t idx);

void gpio_bind(const sc_object& obj1, const string& port1,
               const sc_object& obj2, const string& port2);
void gpio_bind(const sc_object& obj1, const string& port1,
               const sc_object& obj2, const string& port2, size_t idx2);
void gpio_bind(const sc_object& obj1, const string& port1, size_t idx1,
               const sc_object& obj2, const string& port2);
void gpio_bind(const sc_object& obj1, const string& port1, size_t idx1,
               const sc_object& obj2, const string& port2, size_t idx2);

void gpio_bind(const sc_object& obj, const string& port,
               sc_signal_inout_if<bool>& sig);
void gpio_bind(const sc_object& obj, const string& port, size_t idx,
               sc_signal_inout_if<bool>& sig);

} // namespace vcml

#endif
