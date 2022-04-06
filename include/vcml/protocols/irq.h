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

#ifndef VCML_PROTOCOLS_IRQ_H
#define VCML_PROTOCOLS_IRQ_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/ports.h"
#include "vcml/module.h"

#include "vcml/protocols/base.h"

namespace vcml {

typedef size_t irq_vector;

enum irq_vectors : irq_vector {
    IRQ_NO_VECTOR = SIZE_MAX,
};

struct irq_payload {
    irq_vector vector;
    bool active;
};

ostream& operator<<(ostream& os, const irq_payload& irq);

class irq_fw_transport_if : public sc_core::sc_interface
{
public:
    typedef irq_payload protocol_types;
    virtual void irq_transport(irq_payload& irq) = 0;
};

class irq_bw_transport_if : public sc_core::sc_interface
{
public:
    typedef irq_payload protocol_types;
};

class irq_initiator_socket;
class irq_target_socket;
class irq_initiator_stub;
class irq_target_stub;

class irq_target
{
public:
    friend class irq_initiator_socket;
    friend class irq_target_socket;

    typedef vector<irq_initiator_socket*> irq_initiator_sockets;
    typedef vector<irq_target_socket*> irq_target_sockets;

    const irq_initiator_sockets& all_irq_initiator_sockets() const;
    const irq_target_sockets& all_irq_target_sockets() const;

    irq_target_sockets all_irq_target_sockets(address_space as) const;

    irq_target() = default;

    virtual ~irq_target() = default;

    virtual void irq_transport(const irq_target_socket&, irq_payload&) = 0;

private:
    irq_initiator_sockets m_initiator_sockets;
    irq_target_sockets m_target_sockets;
};

inline const irq_target::irq_initiator_sockets&
irq_target::all_irq_initiator_sockets() const {
    return m_initiator_sockets;
}

inline const irq_target::irq_target_sockets&
irq_target::all_irq_target_sockets() const {
    return m_target_sockets;
}

typedef multi_initiator_socket<irq_fw_transport_if, irq_bw_transport_if>
    irq_base_initiator_socket_b;

typedef multi_target_socket<irq_fw_transport_if, irq_bw_transport_if>
    irq_base_target_socket_b;

class irq_base_initiator_socket : public irq_base_initiator_socket_b
{
private:
    irq_target_stub* m_stub;

public:
    irq_base_initiator_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~irq_base_initiator_socket();
    VCML_KIND(irq_base_initiator_socket);

    virtual sc_core::sc_type_index get_protocol_types() const override {
        return typeid(irq_bw_transport_if);
    }

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

class irq_base_target_socket : public irq_base_target_socket_b
{
private:
    irq_initiator_stub* m_stub;

public:
    irq_base_target_socket(const char*, address_space = VCML_AS_DEFAULT);
    virtual ~irq_base_target_socket();
    VCML_KIND(irq_base_target_socket);

    virtual sc_core::sc_type_index get_protocol_types() const override {
        return typeid(irq_fw_transport_if);
    }

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();
};

template <const size_t MAX_PORTS = SIZE_MAX>
using irq_base_initiator_socket_array = socket_array<irq_base_initiator_socket,
                                                     MAX_PORTS>;

template <const size_t MAX_PORTS = SIZE_MAX>
using irq_base_target_socket_array = socket_array<irq_base_target_socket,
                                                  MAX_PORTS>;

class irq_initiator_socket : public irq_base_initiator_socket
{
public:
    struct irq_state_tracker : irq_payload {
        irq_initiator_socket* parent;
        bool operator=(bool state);
        operator bool() const { return active; }
    };

    irq_initiator_socket(const char* n, address_space a = VCML_AS_DEFAULT);
    virtual ~irq_initiator_socket();
    VCML_KIND(irq_initiator_socket);

    const sc_event& default_event();

    bool read(irq_vector vector = IRQ_NO_VECTOR) const;
    operator bool() const { return read(IRQ_NO_VECTOR); }
    void write(bool state, irq_vector vector = IRQ_NO_VECTOR);

    void raise_irq(irq_vector vector = IRQ_NO_VECTOR);
    void lower_irq(irq_vector vector = IRQ_NO_VECTOR);

    irq_initiator_socket& operator=(bool set);
    irq_state_tracker& operator[](irq_vector vector);

private:
    irq_target* m_host;
    unordered_map<irq_vector, irq_state_tracker> m_state;
    sc_event* m_event;

    struct irq_bw_transport : public irq_bw_transport_if {
        irq_initiator_socket* socket;
        irq_bw_transport(irq_initiator_socket* s):
            irq_bw_transport_if(), socket(s) {}
    } m_transport;

    void irq_transport(irq_payload& irq);
};

class irq_target_socket : public irq_base_target_socket
{
public:
    irq_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
    virtual ~irq_target_socket();
    VCML_KIND(irq_target_socket);

    const sc_event& default_event();

    bool read(irq_vector vector = IRQ_NO_VECTOR) const;
    operator bool() const { return read(IRQ_NO_VECTOR); }

private:
    irq_target* m_host;
    unordered_map<irq_vector, bool> m_state;
    sc_event* m_event;

    struct irq_fw_transport : public irq_fw_transport_if {
        irq_target_socket* socket;
        irq_fw_transport(irq_target_socket* s):
            irq_fw_transport_if(), socket(s) {}

        virtual void irq_transport(irq_payload& irq) override {
            socket->irq_transport(irq);
        }
    } m_transport;

    void irq_transport(irq_payload& irq);
};

template <const size_t MAX_PORTS = SIZE_MAX>
using irq_initiator_socket_array = socket_array<irq_initiator_socket,
                                                MAX_PORTS>;

template <const size_t MAX_PORTS = SIZE_MAX>
using irq_target_socket_array = socket_array<irq_target_socket, MAX_PORTS>;

class irq_initiator_stub : private irq_bw_transport_if
{
public:
    irq_base_initiator_socket irq_out;
    irq_initiator_stub(const char* nm);
    virtual ~irq_initiator_stub() = default;
};

class irq_target_stub : private irq_fw_transport_if
{
private:
    virtual void irq_transport(irq_payload& irq) override;

public:
    irq_base_target_socket irq_in;
    irq_target_stub(const char* nm);
    virtual ~irq_target_stub() = default;
};

class irq_initiator_adapter : public module
{
public:
    in_port<bool> irq_in;
    irq_initiator_socket irq_out;

    irq_initiator_adapter(const sc_module_name& nm);
    virtual ~irq_initiator_adapter() = default;

private:
    void update();
};

class irq_target_adapter : public module, public irq_target
{
public:
    irq_target_socket irq_in;
    out_port<bool> irq_out;

    irq_target_adapter(const sc_module_name& nm);
    virtual ~irq_target_adapter() = default;

private:
    virtual void irq_transport(const irq_target_socket& socket,
                               irq_payload& irq) override;
};

} // namespace vcml

#endif
