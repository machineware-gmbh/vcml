/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_HOST_H
#define VCML_PROTOCOLS_TLM_HOST_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"

#include "vcml/properties/property.h"
#include "vcml/protocols/tlm_sbi.h"

namespace vcml {

class tlm_initiator_socket;
class tlm_target_socket;

class tlm_host
{
private:
    struct proc_data {
        sc_time time;
        tlm_generic_payload* tx;
        const tlm_sbi* sbi;
        proc_data(): time(SC_ZERO_TIME), tx(nullptr), sbi(nullptr) {}
    };

    mutable std::unordered_map<sc_process_b*, proc_data> m_processes;
    vector<tlm_initiator_socket*> m_initiator_sockets;
    vector<tlm_target_socket*> m_target_sockets;

    unsigned int do_transport(tlm_target_socket& socket,
                              tlm_generic_payload& tx, const tlm_sbi& info);

protected:
    bool in_transaction(sc_process_b* proc = current_process()) const;
    bool in_debug_transaction(sc_process_b* proc = current_process()) const;
    bool in_secure_transaction(sc_process_b* proc = current_process()) const;
    int current_cpu(sc_process_b* proc = current_process()) const;
    int current_privilege(sc_process_b* proc = current_process()) const;

    const tlm_generic_payload& current_transaction(
        sc_process_b* proc = current_process()) const;
    const tlm_sbi& current_sideband(
        sc_process_b* proc = current_process()) const;
    size_t current_transaction_size(
        sc_process_b* proc = current_process()) const;
    range current_transaction_address(
        sc_process_b* proc = current_process()) const;

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

    tlm_host(): tlm_host(true, 64) {}
    tlm_host(bool allow_dmi, unsigned int bus_width);
    virtual ~tlm_host() = default;

    sc_time& local_time(sc_process_b* proc = current_process());
    sc_time local_time_stamp(sc_process_b* proc = current_process());

    bool needs_sync(sc_process_b* proc = current_process());
    void sync(sc_process_b* proc = current_process());

    void map_dmi(const tlm_dmi& dmi);
    void map_dmi(unsigned char* ptr, u64 start, u64 end, vcml_access a,
                 const sc_time& read_latency = SC_ZERO_TIME,
                 const sc_time& write_latency = SC_ZERO_TIME);
    void unmap_dmi(const tlm_dmi& dmi);
    void unmap_dmi(u64 start, u64 end);
    void remap_dmi(const sc_time& rdlat, const sc_time& wrlat);

    virtual void invalidate_dmi(u64 start, u64 end);

    virtual void update_local_time(sc_time& local_time, sc_process_b* proc);

    virtual void b_transport(tlm_target_socket& origin,
                             tlm_generic_payload& tx, sc_time& dt);

    virtual unsigned int transport_dbg(tlm_target_socket& origin,
                                       tlm_generic_payload& tx);

    virtual bool get_direct_mem_ptr(tlm_target_socket& origin,
                                    tlm_generic_payload& tx, tlm_dmi& dmi);

    virtual void invalidate_direct_mem_ptr(tlm_initiator_socket& origin,
                                           u64 start, u64 end);

    virtual unsigned int transport(tlm_target_socket& socket,
                                   tlm_generic_payload& tx,
                                   const tlm_sbi& info);

    property<bool> allow_dmi;
};

inline bool tlm_host::in_transaction(sc_process_b* proc) const {
    return m_processes[proc].tx != nullptr;
}

inline bool tlm_host::in_debug_transaction(sc_process_b* proc) const {
    return m_processes[proc].sbi && m_processes[proc].sbi->is_debug;
}

inline bool tlm_host::in_secure_transaction(sc_process_b* proc) const {
    return m_processes[proc].sbi && m_processes[proc].sbi->is_secure;
}

inline int tlm_host::current_cpu(sc_process_b* proc) const {
    return m_processes[proc].sbi ? m_processes[proc].sbi->cpuid : -1;
}

inline int tlm_host::current_privilege(sc_process_b* proc) const {
    return m_processes[proc].sbi ? m_processes[proc].sbi->privilege : 0;
}

inline const tlm_generic_payload& tlm_host::current_transaction(
    sc_process_b* proc) const {
    VCML_ERROR_ON(!m_processes[proc].tx, "no current transaction");
    return *m_processes[proc].tx;
}

inline const tlm_sbi& tlm_host::current_sideband(sc_process_b* proc) const {
    VCML_ERROR_ON(!m_processes[proc].sbi, "no current transaction");
    return *m_processes[proc].sbi;
}

inline size_t tlm_host::current_transaction_size(sc_process_b* proc) const {
    return m_processes[proc].tx ? m_processes[proc].tx->get_data_length() : 0;
}

inline range tlm_host::current_transaction_address(sc_process_b* proc) const {
    return m_processes[proc].tx ? range(*m_processes[proc].tx) : range();
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
