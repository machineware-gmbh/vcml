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

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"

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

    const tlm_generic_payload* m_payload;
    const tlm_sbi* m_sideband;

    unsigned int do_transport(tlm_target_socket& socket,
                              tlm_generic_payload& tx, const tlm_sbi& info);

protected:
    bool in_transaction() const;
    bool in_debug_transaction() const;
    int current_cpu() const;
    int current_privilege() const;
    const tlm_generic_payload& current_transaction() const;
    const tlm_sbi& current_sideband() const;
    size_t current_transaction_size() const;
    range current_transaction_address() const;

public:
    void register_socket(tlm_initiator_socket* socket);
    void register_socket(tlm_target_socket* socket);
    void unregister_socket(tlm_initiator_socket* socket);
    void unregister_socket(tlm_target_socket* socket);

    const vector<tlm_initiator_socket*>& get_tlm_initiator_sockets() const;
    const vector<tlm_target_socket*>& get_tlm_target_sockets() const;

    tlm_initiator_socket* find_tlm_initiator_socket(const string&) const;
    tlm_target_socket* find_tlm_target_socket(const string& name) const;

    vector<tlm_target_socket*> find_tlm_target_sockets(address_space as) const;

    tlm_host(bool allow_dmi = true);
    virtual ~tlm_host() = default;

    sc_time& local_time(sc_process_b* proc = nullptr);
    sc_time local_time_stamp(sc_process_b* proc = nullptr);

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
                             tlm_generic_payload& tx, sc_time& dt);

    virtual unsigned int transport_dbg(tlm_target_socket& origin,
                                       tlm_generic_payload& tx);

    virtual bool get_direct_mem_ptr(tlm_target_socket& origin,
                                    const tlm_generic_payload& tx,
                                    tlm_dmi& dmi);

    virtual void invalidate_direct_mem_ptr(tlm_initiator_socket& origin,
                                           u64 start, u64 end);

    virtual unsigned int transport(tlm_target_socket& socket,
                                   tlm_generic_payload& tx,
                                   const tlm_sbi& info);

    property<bool> allow_dmi;
};

inline bool tlm_host::in_transaction() const {
    return m_payload != nullptr;
}

inline bool tlm_host::in_debug_transaction() const {
    return m_sideband && m_sideband->is_debug;
}

inline int tlm_host::current_cpu() const {
    return m_sideband ? m_sideband->cpuid : -1;
}

inline int tlm_host::current_privilege() const {
    return m_sideband ? m_sideband->privilege : 0;
}

inline const tlm_generic_payload& tlm_host::current_transaction() const {
    VCML_ERROR_ON(!m_payload, "not currently servicing a transaction");
    return *m_payload;
}

inline const tlm_sbi& tlm_host::current_sideband() const {
    VCML_ERROR_ON(!m_sideband, "not currently servicing a transaction");
    return *m_sideband;
}

inline size_t tlm_host::current_transaction_size() const {
    return m_payload ? m_payload->get_data_length() : 0;
}

inline range tlm_host::current_transaction_address() const {
    return m_payload ? range(*m_payload) : range();
}

inline const vector<tlm_initiator_socket*>&
tlm_host::get_tlm_initiator_sockets() const {
    return m_initiator_sockets;
}

inline const vector<tlm_target_socket*>& tlm_host::get_tlm_target_sockets()
    const {
    return m_target_sockets;
}

} // namespace vcml

#endif
