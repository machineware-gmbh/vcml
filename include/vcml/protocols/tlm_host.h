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

#ifndef VCML_PROTOCOLS_TLM_HOST_H
#define VCML_PROTOCOLS_TLM_HOST_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/properties/property.h"
#include "vcml/protocols/tlm_sbi.h"

namespace vcml {

    class tlm_initiator_socket;
    class tlm_target_socket;

    class tlm_host
    {
    private:
        std::unordered_map<sc_process_b*, sc_time> m_offsets;
        vector<tlm_initiator_socket*> m_initiator_sockets;
        vector<tlm_target_socket*> m_target_sockets;

    public:
        void register_socket(tlm_initiator_socket* socket);
        void register_socket(tlm_target_socket* socket);
        void unregister_socket(tlm_initiator_socket* socket);
        void unregister_socket(tlm_target_socket* socket);

        const vector<tlm_initiator_socket*>& get_tlm_initiator_sockets() const;
        const vector<tlm_target_socket*>& get_tlm_target_sockets() const;

        vector<tlm_initiator_socket*>& get_tlm_initiator_sockets();
        vector<tlm_target_socket*>& get_tlm_target_sockets();

        tlm_initiator_socket* find_tlm_initiator_socket(const string&) const;
        tlm_target_socket* find_tlm_target_socket(const string& name) const;

        vector<tlm_target_socket*>
        find_tlm_target_sockets(address_space as) const;

        tlm_host(bool allow_dmi = true);
        virtual ~tlm_host() = default;

        sc_time& local_time(sc_process_b* proc = nullptr);
        sc_time  local_time_stamp(sc_process_b* proc = nullptr);

        bool needs_sync(sc_process_b* proc = nullptr);
        void sync(sc_process_b* proc = nullptr);

        void map_dmi(const tlm_dmi& dmi);
        void map_dmi(unsigned char* ptr, u64 start, u64 end, vcml_access a,
                     const sc_time& read_latency = SC_ZERO_TIME,
                     const sc_time& write_latency = SC_ZERO_TIME);
        void unmap_dmi(const tlm_dmi& dmi);
        void unmap_dmi(u64 start, u64 end);
        void remap_dmi(const sc_time& rdlat, const sc_time& wrlat);

        virtual void invalidate_dmi(u64 start, u64 end);

        virtual void update_local_time(sc_time& local_time);

        virtual void b_transport(tlm_target_socket& origin,
                                 tlm_generic_payload& tx,
                                 sc_time& dt);

        virtual unsigned int transport_dbg(tlm_target_socket& origin,
                                           tlm_generic_payload& tx);

        virtual bool get_direct_mem_ptr(tlm_target_socket& origin,
                                        const tlm_generic_payload& tx,
                                        tlm_dmi& dmi);

        virtual void invalidate_direct_mem_ptr(tlm_initiator_socket& origin,
                                               u64 start, u64 end);

        virtual unsigned int transport(tlm_target_socket& socket,
                                       tlm_generic_payload& tx,
                                       const tlm_sbi& info) = 0;

        property<bool> allow_dmi;
    };

    inline const vector<tlm_initiator_socket*>&
    tlm_host::get_tlm_initiator_sockets() const {
        return m_initiator_sockets;
    }

    inline const vector<tlm_target_socket*>&
    tlm_host::get_tlm_target_sockets() const {
        return m_target_sockets;
    }

    inline vector<tlm_initiator_socket*>&
    tlm_host::get_tlm_initiator_sockets() {
        return m_initiator_sockets;
    }

    inline vector<tlm_target_socket*>&
    tlm_host::get_tlm_target_sockets() {
        return m_target_sockets;
    }

}

#endif
