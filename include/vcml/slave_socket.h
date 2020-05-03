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

#ifndef VCML_SLAVE_SOCKET_H
#define VCML_SLAVE_SOCKET_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/range.h"
#include "vcml/sbi.h"
#include "vcml/exmon.h"
#include "vcml/dmi_cache.h"
#include "vcml/component.h"
#include "vcml/adapters.h"

namespace vcml {

    class slave_socket: public simple_target_socket<slave_socket, 64>
    {
    private:
        int        m_curr;
        int        m_next;
        sc_event   m_free_ev;
        dmi_cache  m_dmi_cache;
        exmon      m_exmon;
        sc_module* m_adapter;
        component* m_host;

        void b_transport(tlm_generic_payload& tx, sc_time& dt);
        unsigned int transport_dbg(tlm_generic_payload& tx);
        bool get_direct_mem_ptr(tlm_generic_payload& tx, tlm_dmi& dmi);

    public:
        slave_socket() = delete;
        slave_socket(const char* name, component* host = nullptr);
        virtual ~slave_socket();

        VCML_KIND(slave_socket);

        dmi_cache& dmi()   { return m_dmi_cache; }
        exmon&     exmem() { return m_exmon; }

        void map_dmi(const tlm_dmi& dmi);
        void unmap_dmi(u64 start, u64 end);
        void remap_dmi(const sc_time& rlat, const sc_time& wlat);
        void invalidate_dmi();

        template <unsigned int WIDTH>
        void bind(tlm_initiator_socket<WIDTH>& other);

        template <unsigned int WIDTH>
        void bind(tlm_target_socket<WIDTH>& other);

        void trace_fw(const tlm_generic_payload& tx, const sc_time& dt) const;
        void trace_bw(const tlm_generic_payload& tx, const sc_time& dt) const;
    };

    inline void slave_socket::map_dmi(const tlm_dmi& dmi) {
        m_dmi_cache.insert(dmi);
    }

    template <unsigned int WIDTH>
    inline void slave_socket::bind(tlm_initiator_socket<WIDTH>& other) {
        typedef bus_width_adapter<WIDTH, 64> adapter_type;
        VCML_ERROR_ON(m_adapter, "socket %s already bound", name());

        m_host->hierarchy_push();
        string nm = concat(name(), "_adapter");
        adapter_type* adapter = new adapter_type(nm.c_str());
        m_host->hierarchy_pop();

        other.bind(adapter->IN);
        adapter->OUT.bind(*this);
        m_adapter = adapter;
    }

    template <>
    inline void slave_socket::bind<64>(tlm_initiator_socket<64>& other) {
        base_type::bind(other);
    }

    template <unsigned int WIDTH>
    inline void slave_socket::bind(tlm_target_socket<WIDTH>& other) {
        typedef bus_width_adapter<WIDTH, 64> adapter_type;
        VCML_ERROR_ON(m_adapter, "socket %s already bound", name());

        m_host->hierarchy_push();
        string nm = concat(name(), "_adapter");
        adapter_type* adapter = new adapter_type(nm.c_str());
        m_host->hierarchy_pop();

        other.bind(adapter->IN);
        adapter->OUT.bind(*this);
        m_adapter = adapter;
    }

    template <>
    inline void slave_socket::bind<64>(tlm_target_socket<64>& other) {
        base_type::bind(other);
    }

    inline void slave_socket::trace_fw(const tlm_generic_payload& tx,
                                       const sc_time& dt) const {
        if (!m_host->trace_errors && m_host->loglvl >= LOG_TRACE)
            logger::trace_fw(name(), tx, dt);
    }

    inline void slave_socket::trace_bw(const tlm_generic_payload& tx,
                                       const sc_time& dt) const {
        if ((!m_host->trace_errors || failed(tx)) &&
            m_host->loglvl >= LOG_TRACE) {
            logger::trace_bw(name(), tx, dt);
        }
    }

}

#endif
