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

#ifndef VCML_PROTOCOLS_GPIO_H
#define VCML_PROTOCOLS_GPIO_H

#include "vcml/core/types.h"
#include "vcml/core/report.h"
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
    friend class gpio_initiator_socket;
    friend class gpio_target_socket;

    typedef vector<gpio_initiator_socket*> gpio_initiator_sockets;
    typedef vector<gpio_target_socket*> gpio_target_sockets;

    const gpio_initiator_sockets& all_gpio_initiator_sockets() const;
    const gpio_target_sockets& all_gpio_target_sockets() const;

    gpio_target_sockets all_gpio_target_sockets(address_space as) const;

    gpio_host() = default;

    virtual ~gpio_host() = default;

    virtual void gpio_transport(const gpio_target_socket&, gpio_payload&) = 0;

private:
    gpio_initiator_sockets m_initiator_sockets;
    gpio_target_sockets m_target_sockets;
};

inline const gpio_host::gpio_initiator_sockets&
gpio_host::all_gpio_initiator_sockets() const {
    return m_initiator_sockets;
}

inline const gpio_host::gpio_target_sockets&
gpio_host::all_gpio_target_sockets() const {
    return m_target_sockets;
}

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
    virtual void bind(sc_signal<bool>& signal);

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
    virtual void bind(sc_signal<bool>& signal);
    virtual void complete_binding(gpio_base_initiator_socket& socket) {}

    bool is_adapted() const { return m_adapter != nullptr; }
    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

template <const size_t MAX_PORTS = SIZE_MAX>
using gpio_base_initiator_socket_array = socket_array<
    gpio_base_initiator_socket, MAX_PORTS>;

template <const size_t MAX_PORTS = SIZE_MAX>
using gpio_base_target_socket_array = socket_array<gpio_base_target_socket,
                                                   MAX_PORTS>;

class gpio_initiator_socket : public gpio_base_initiator_socket
{
public:
    struct gpio_state_tracker : gpio_payload {
        gpio_initiator_socket* parent;
        bool operator=(bool state);
        operator bool() const { return state; }
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
    gpio_state_tracker& operator[](gpio_vector vector);

private:
    gpio_host* m_host;
    sc_event* m_event;
    unordered_map<gpio_vector, gpio_state_tracker> m_state;

    struct gpio_bw_transport : public gpio_bw_transport_if {
        gpio_initiator_socket* socket;
        gpio_bw_transport(gpio_initiator_socket* s):
            gpio_bw_transport_if(), socket(s) {}
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
    virtual void bind(gpio_target_socket& other);
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
    vector<gpio_target_socket*> m_targets;

    struct gpio_fw_transport : public gpio_fw_transport_if {
        gpio_target_socket* socket;
        gpio_fw_transport(gpio_target_socket* s):
            gpio_fw_transport_if(), socket(s) {}

        virtual void gpio_transport(gpio_payload& tx) override {
            socket->gpio_transport(tx);
        }
    } m_transport;

    void gpio_transport(gpio_payload& gpio);
};

template <const size_t MAX_PORTS = SIZE_MAX>
using gpio_initiator_socket_array = socket_array<gpio_initiator_socket,
                                                 MAX_PORTS>;

template <const size_t MAX_PORTS = SIZE_MAX>
using gpio_target_socket_array = socket_array<gpio_target_socket, MAX_PORTS>;

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
    virtual void gpio_transport(const gpio_target_socket& socket,
                                gpio_payload& tx) override;
};

} // namespace vcml

#endif
