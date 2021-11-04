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
#include "vcml/common/systemc.h"
#include "vcml/module.h"

namespace vcml {

    typedef size_t irq_vector;

    enum irq_vectors : irq_vector {
        IRQ_NO_VECTOR = SIZE_MAX,
    };

    struct irq_payload {
        irq_vector vector;
        bool active;
    };

    ostream& operator << (ostream& os, const irq_payload& irq);

    class irq_fw_transport_if: public sc_core::sc_interface
    {
    public:
        typedef irq_payload protocol_types;
        virtual void irq_transport(irq_payload& irq) = 0;
    };

    class irq_bw_transport_if: public sc_core::sc_interface
    {
    public:
        typedef irq_payload protocol_types;
    };

    class irq_initiator_socket;
    class irq_target_socket;
    class irq_initiator_stub;
    class irq_target_stub;

    class irq_host
    {
    public:
        friend class irq_initiator_socket;
        friend class irq_target_socket;

        typedef vector<irq_initiator_socket*> irq_initiator_sockets;
        typedef vector<irq_target_socket*> irq_target_sockets;

        const irq_initiator_sockets& get_irq_initiator_sockets() const;
        const irq_target_sockets& get_irq_target_sockets() const;

        irq_target_sockets get_irq_target_sockets(address_space as) const;

        irq_host() = default;
        virtual ~irq_host() = default;

        virtual void irq_transport(const irq_target_socket&, irq_payload&) = 0;

    private:
        irq_initiator_sockets m_initiator_sockets;
        irq_target_sockets m_target_sockets;
    };

    inline const irq_host::irq_initiator_sockets&
    irq_host::get_irq_initiator_sockets() const {
        return m_initiator_sockets;
    }

    inline const irq_host::irq_target_sockets&
    irq_host::get_irq_target_sockets() const {
        return m_target_sockets;
    }

    typedef tlm::tlm_base_initiator_socket<1, irq_fw_transport_if,
        irq_bw_transport_if, 0, sc_core::SC_ONE_OR_MORE_BOUND>
        irq_base_initiator_socket;

    typedef tlm::tlm_base_target_socket<1, irq_fw_transport_if,
        irq_bw_transport_if, 0, sc_core::SC_ONE_OR_MORE_BOUND>
        irq_base_target_socket;

    class irq_initiator_socket: public irq_base_initiator_socket,
                                private irq_bw_transport_if {
    public:
        struct irq_state_tracker {
            irq_initiator_socket* parent;
            irq_vector vector;
            bool active;
            bool operator = (bool state);
        };

        bool is_stubbed() const { return m_stub != nullptr; }

        irq_initiator_socket(const char* name);
        virtual ~irq_initiator_socket();
        VCML_KIND(irq_initiator_socket);
        virtual sc_core::sc_type_index get_protocol_types() const;
        void stub();

        void interrupt(bool state = true, irq_vector vector = IRQ_NO_VECTOR);
        void interrupt(irq_payload& irq);

        void raise_irq(irq_vector vector = IRQ_NO_VECTOR);
        void lower_irq(irq_vector vector = IRQ_NO_VECTOR);

        irq_initiator_socket& operator = (bool set);
        irq_state_tracker& operator [] (irq_vector vector);

    private:
        module* m_parent;
        irq_host* m_host;
        irq_target_stub* m_stub;
        unordered_map<irq_vector, irq_state_tracker> m_state;
    };

    class irq_target_socket: public irq_base_target_socket,
                             private irq_fw_transport_if {
    public:
        const address_space as;
        bool is_stubbed() const { return m_stub != nullptr; }
        irq_target_socket(const char* nm, address_space as = VCML_AS_DEFAULT);
        virtual ~irq_target_socket();
        VCML_KIND(irq_target_socket);
        virtual sc_core::sc_type_index get_protocol_types() const;
        void stub();

    private:
        module* m_parent;
        irq_host* m_host;
        irq_initiator_stub* m_stub;

        virtual void irq_transport(irq_payload& irq) override;
    };

    template <const size_t MAX = SIZE_MAX>
    using irq_initiator_socket_array = socket_array<irq_initiator_socket, MAX>;

    template <const size_t MAX = SIZE_MAX>
    using irq_target_socket_array = socket_array<irq_target_socket, MAX>;

    class irq_initiator_stub: public module
    {
    public:
        irq_initiator_socket IRQ_OUT;
        irq_initiator_stub(const sc_module_name& name);
        virtual ~irq_initiator_stub();
        VCML_KIND(irq_initiator_stub);
    };

    class irq_target_stub: public module, public irq_host
    {
    protected:
        virtual void irq_transport(const irq_target_socket& socket,
                                   irq_payload& irq) override;
    public:
        irq_target_socket IRQ_IN;
        irq_target_stub(const sc_module_name& name);
        virtual ~irq_target_stub();
        VCML_KIND(irq_target_stub);
    };

}

#endif
