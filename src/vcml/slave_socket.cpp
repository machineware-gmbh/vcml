/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
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

#include "vcml/slave_socket.h"

namespace vcml {

    void slave_socket::b_transport(tlm_generic_payload& tx, sc_time& dt) {
        trace_fw(tx, dt);

        int self = m_next++;
        while (self != m_curr)
            sc_core::wait(m_free_ev);

        if (tx_is_excl(tx) && tx.is_read()) {
            unmap_dmi(tx.get_address(),
                      tx.get_address() + tx.get_data_length() - 1);
        }

        tlm_dmi dmi;
        if (m_dmi_cache.lookup(tx, dmi))
            tx.set_dmi_allowed(true);

        if (m_exmon.update(tx)) {
            m_host->b_transport(this, tx, dt);
        } else {
            tx.set_dmi_allowed(false);
            tx.set_response_status(tlm::TLM_OK_RESPONSE);
        }

        m_curr++;
        m_free_ev.notify();

        trace_bw(tx, dt);
    }

    unsigned int slave_socket::transport_dbg(tlm_generic_payload& tx){
        return m_host->transport_dbg(this, tx);
    }

    bool slave_socket::get_direct_mem_ptr(tlm_generic_payload& tx,
                                          tlm_dmi& dmi) {
        dmi.allow_read_write();
        dmi.set_start_address(0);
        dmi.set_end_address((sc_dt::uint64)-1);

        if (!m_dmi_cache.lookup(tx, dmi))
            return false;

        if (!m_host->get_direct_mem_ptr(this, tx, dmi))
            return false;

        return m_exmon.override_dmi(tx, dmi);
    }

    slave_socket::slave_socket(const char* nm, component* host):
        simple_target_socket<slave_socket, 64>(nm),
        m_curr(0),
        m_next(0),
        m_free_ev(concat(nm, "_free").c_str()),
        m_dmi_cache(),
        m_exmon(),
        m_stub(nullptr),
        m_adapter(nullptr),
        m_host(host) {
        if (m_host == nullptr) {
            m_host = dynamic_cast<component*>(get_parent_object());
            VCML_ERROR_ON(!m_host, "socket '%s' declared outside module", nm);
        }

        m_host->register_socket(this);
        register_b_transport(this, &slave_socket::b_transport);
        register_transport_dbg(this, &slave_socket::transport_dbg);
        register_get_direct_mem_ptr(this, &slave_socket::get_direct_mem_ptr);
    }

    slave_socket::~slave_socket() {
        if (m_adapter != nullptr)
            delete m_adapter;
        if (m_stub != nullptr)
            delete m_stub;
    }

    void slave_socket::unmap_dmi(u64 start, u64 end) {
        m_dmi_cache.invalidate(start, end);
        (*this)->invalidate_direct_mem_ptr(start, end);
    }

    void slave_socket::remap_dmi(const sc_time& rdlat, const sc_time& wrlat) {
        for (auto dmi : m_dmi_cache.get_entries()) {
            if (dmi.get_read_latency() != rdlat ||
                dmi.get_write_latency() != wrlat) {
                (*this)->invalidate_direct_mem_ptr(dmi.get_start_address(),
                                                   dmi.get_end_address());
                dmi.set_read_latency(rdlat);
                dmi.set_write_latency(wrlat);
            }
        }
    }

    void slave_socket::invalidate_dmi() {
        for (auto dmi : m_dmi_cache.get_entries()) {
            (*this)->invalidate_direct_mem_ptr(dmi.get_start_address(),
                                               dmi.get_end_address());
        }
    }

    void slave_socket::stub() {
        VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
        m_host->hierarchy_push();
        string nm = concat(basename(), "_stub");
        m_stub = new initiator_stub(nm.c_str());
        m_host->hierarchy_pop();
        m_stub->OUT.bind(*this);
    }

}
