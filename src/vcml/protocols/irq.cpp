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

    ostream& operator << (ostream& os, const irq_payload& irq) {
        os << (irq.active ? "+" : "-") << "IRQ";
        if (irq.vector != IRQ_NO_VECTOR)
            os << ":" << irq.vector;
        return os;
    }

    irq_target::irq_target_sockets
    irq_target::get_irq_target_sockets(address_space as) const {
        irq_target_sockets sockets;
        for (auto& socket : m_target_sockets)
            if (socket->as == as)
                sockets.push_back(socket);
        return sockets;
    }

    bool irq_initiator_socket::irq_state_tracker::operator = (bool state) {
        if (state == active)
            return state;

        active = state;
        parent->interrupt(state, vector);
        return active;
    }

    irq_initiator_socket::irq_initiator_socket(const char* nm):
        irq_base_initiator_socket(nm),
        m_parent(dynamic_cast<module*>(hierarchy_top())),
        m_host(dynamic_cast<irq_target*>(hierarchy_top())),
        m_stub(nullptr), m_state(), m_transport(this) {
        VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
        bind(m_transport);
        if (m_host)
            m_host->m_initiator_sockets.push_back(this);
    }

    irq_initiator_socket::~irq_initiator_socket() {
        if (m_host)
            stl_remove_erase(m_host->m_initiator_sockets, this);
        if (m_stub)
            delete m_stub;
    }

    void irq_initiator_socket::stub() {
        VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
        hierarchy_guard guard(m_parent);
        m_stub = new irq_target_stub(mkstr("%s_stub", basename()).c_str());
        bind(m_stub->IRQ_IN);
    }

    void irq_initiator_socket::interrupt(bool state, irq_vector vector) {
        irq_payload irq;
        irq.active = state;
        irq.vector = vector;
        interrupt(irq);
    }

    void irq_initiator_socket::interrupt(irq_payload& irq) {
        m_parent->trace_fw(*this, irq);
        for (int i = 0; i < size(); i++)
            get_interface(i)->irq_transport(irq);
        m_parent->trace_bw(*this, irq);
    }

    void irq_initiator_socket::raise_irq(irq_vector vector) {
        interrupt(true, vector);
    }

    void irq_initiator_socket::lower_irq(irq_vector vector) {
        interrupt(false, vector);
    }

    irq_initiator_socket& irq_initiator_socket::operator = (bool set) {
        (*this)[IRQ_NO_VECTOR] = set;
        return *this;
    }

    irq_initiator_socket::irq_state_tracker&
    irq_initiator_socket::operator [] (irq_vector vector) {
        if (m_state.count(vector))
            return m_state[vector];

        irq_state_tracker state;
        state.parent = this;
        state.active = false;
        state.vector = vector;
        return m_state[vector] = state;
    }

    void irq_target_socket::irq_transport(irq_payload& irq) {
        m_parent->trace_fw(*this, irq);
        m_host->irq_transport(*this, irq);
        m_parent->trace_bw(*this, irq);
    }

    irq_target_socket::irq_target_socket(const char* nm, address_space _as):
        irq_base_target_socket(nm), as(_as),
        m_parent(dynamic_cast<module*>(hierarchy_top())),
        m_host(dynamic_cast<irq_target*>(hierarchy_top())),
        m_stub(nullptr), m_transport(this) {
        VCML_ERROR_ON(!m_parent, "%s declared outside module", name());
        VCML_ERROR_ON(!m_host, "%s declared outside irq_host", name());
        m_host->m_target_sockets.push_back(this);
        bind(m_transport);
    }

    irq_target_socket::~irq_target_socket() {
        stl_remove_erase(m_host->m_target_sockets, this);
        if (m_stub)
            delete m_stub;
    }

    void irq_target_socket::stub() {
        VCML_ERROR_ON(m_stub, "socket '%s' already stubbed", name());
        hierarchy_guard guard(m_parent);
        m_stub = new irq_initiator_stub(mkstr("%s_stub", basename()).c_str());
        m_stub->IRQ_OUT.bind(*this);
    }

    irq_initiator_stub::irq_initiator_stub(const sc_module_name& nm):
        module(nm),
        IRQ_OUT("IRQ_OUT") {
    }

    irq_initiator_stub::~irq_initiator_stub() {
        // nothing to do
    }

    void irq_target_stub::irq_transport(const irq_target_socket& socket,
        irq_payload& irq) {
        trace_fw(IRQ_IN, irq);
        // nothing to do
        trace_bw(IRQ_IN, irq);
    }

    irq_target_stub::irq_target_stub(const sc_module_name& nm):
        module(nm),
        irq_target(),
        IRQ_IN("IRQ_IN") {
    }

    irq_target_stub::~irq_target_stub() {
        // nothing to do
    }

}
