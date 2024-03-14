/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_SOCKETS_H
#define VCML_PROTOCOLS_TLM_SOCKETS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/thctl.h"
#include "vcml/core/module.h"

#include "vcml/protocols/base.h"
#include "vcml/protocols/tlm_sbi.h"
#include "vcml/protocols/tlm_exmon.h"
#include "vcml/protocols/tlm_stubs.h"
#include "vcml/protocols/tlm_adapters.h"
#include "vcml/protocols/tlm_dmi_cache.h"
#include "vcml/protocols/tlm_host.h"
#include "vcml/protocols/tlm_base.h"

#include "vcml/properties/property.h"
#include "vcml/tracing/tracer.h"

namespace vcml {

class tlm_initiator_socket
    : public simple_initiator_socket<tlm_initiator_socket>,
      public hierarchy_element
{
private:
    tlm_generic_payload m_tx;
    tlm_generic_payload m_txd;
    tlm_sbi m_sbi;
    tlm_dmi_cache* m_dmi_cache;
    tlm_target_stub* m_stub;
    tlm_host* m_host;
    module* m_parent;
    module* m_adapter;

    void trace_fw(const tlm_generic_payload& tx, const sc_time& t);
    void trace_bw(const tlm_generic_payload& tx, const sc_time& t);

    void invalidate_direct_mem_ptr_int(sc_dt::uint64 start, sc_dt::uint64 end);

protected:
    virtual void invalidate_direct_mem_ptr(u64 start, u64 end);

public:
    property<bool> trace_all;
    property<bool> trace_errors;
    property<bool> allow_dmi;

    int get_cpuid() const { return m_sbi.cpuid; }
    int get_privilege() const { return m_sbi.privilege; }

    void set_cpuid(u64 cpuid);
    void set_privilege(u64 level);

    tlm_initiator_socket() = delete;
    tlm_initiator_socket(const char* n, address_space a = VCML_AS_DEFAULT);
    virtual ~tlm_initiator_socket();

    VCML_KIND(tlm_initiator_socket);

    u8* lookup_dmi_ptr(const range& addr, vcml_access rw = VCML_ACCESS_READ);
    u8* lookup_dmi_ptr(u64 start, u64 length,
                       vcml_access rw = VCML_ACCESS_READ);

    tlm_dmi_cache& dmi_cache();

    void map_dmi(const tlm_dmi& dmi);
    void unmap_dmi(u64 start, u64 end);

    void b_transport(tlm_generic_payload& tx, sc_time& t);

    unsigned int send(tlm_generic_payload& tx, const tlm_sbi& info = SBI_NONE);

    tlm_response_status access_dmi(tlm_command c, u64 addr, void* data,
                                   unsigned int size,
                                   const tlm_sbi& info = SBI_NONE);

    tlm_response_status access(tlm_command cmd, u64 addr, void* data,
                               unsigned int size,
                               const tlm_sbi& info = SBI_NONE,
                               unsigned int* nbytes = nullptr);

    tlm_response_status read(u64 addr, void* data, unsigned int size,
                             const tlm_sbi& info = SBI_NONE,
                             unsigned int* nbytes = nullptr);

    tlm_response_status write(u64 addr, const void* data, unsigned int size,
                              const tlm_sbi& info = SBI_NONE,
                              unsigned int* nbytes = nullptr);

    template <typename T>
    tlm_response_status readw(u64 addr, T& data,
                              const tlm_sbi& info = SBI_NONE,
                              unsigned int* nbytes = nullptr);

    template <typename T>
    tlm_response_status writew(u64 addr, const T& data,
                               const tlm_sbi& info = SBI_NONE,
                               unsigned int* nbytes = nullptr);

    template <unsigned int WIDTH>
    void bind(tlm::tlm_base_initiator_socket_b<WIDTH>& other);

    template <unsigned int WIDTH>
    void bind(tlm::tlm_base_target_socket_b<WIDTH>& other);

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub(tlm_response_status resp = TLM_ADDRESS_ERROR_RESPONSE);
};

inline void tlm_initiator_socket::trace_fw(const tlm_generic_payload& tx,
                                           const sc_time& t) {
    if (trace_all)
        tracer::record(TRACE_FW, *this, tx, t);
}

inline void tlm_initiator_socket::trace_bw(const tlm_generic_payload& tx,
                                           const sc_time& t) {
    if (trace_all || (trace_errors && failed(tx)))
        tracer::record(TRACE_BW, *this, tx, t);
}

inline void tlm_initiator_socket::set_cpuid(u64 cpuid) {
    m_sbi.cpuid = cpuid;
}

inline void tlm_initiator_socket::set_privilege(u64 level) {
    m_sbi.privilege = level;
}

inline u8* tlm_initiator_socket::lookup_dmi_ptr(u64 addr, u64 size,
                                                vcml_access rw) {
    return lookup_dmi_ptr({ addr, addr + size - 1 }, rw);
}

inline tlm_dmi_cache& tlm_initiator_socket::dmi_cache() {
    if (!m_dmi_cache)
        m_dmi_cache = new tlm_dmi_cache();
    return *m_dmi_cache;
}

inline void tlm_initiator_socket::map_dmi(const tlm_dmi& dmi) {
    if (m_dmi_cache)
        m_dmi_cache->insert(dmi);
}

inline void tlm_initiator_socket::unmap_dmi(u64 start, u64 end) {
    if (m_dmi_cache)
        m_dmi_cache->invalidate(start, end);
}

inline tlm_response_status tlm_initiator_socket::read(u64 addr, void* data,
                                                      unsigned int size,
                                                      const tlm_sbi& info,
                                                      unsigned int* bytes) {
    return access(TLM_READ_COMMAND, addr, data, size, info, bytes);
}

inline tlm_response_status tlm_initiator_socket::write(u64 addr,
                                                       const void* data,
                                                       unsigned int size,
                                                       const tlm_sbi& info,
                                                       unsigned int* bytes) {
    void* ptr = const_cast<void*>(data);
    return access(TLM_WRITE_COMMAND, addr, ptr, size, info, bytes);
}

template <typename T>
inline tlm_response_status tlm_initiator_socket::readw(u64 addr, T& data,
                                                       const tlm_sbi& info,
                                                       unsigned int* nbytes) {
    return read(addr, &data, sizeof(T), info, nbytes);
}

template <typename T>
inline tlm_response_status tlm_initiator_socket::writew(u64 addr,
                                                        const T& data,
                                                        const tlm_sbi& info,
                                                        unsigned int* nbytes) {
    return write(addr, &data, sizeof(T), info, nbytes);
}

template <unsigned int WIDTH>
inline void tlm_initiator_socket::bind(
    tlm::tlm_base_initiator_socket_b<WIDTH>& socket) {
    typedef tlm_bus_width_adapter<32, WIDTH> adapter_type;
    VCML_ERROR_ON(m_adapter, "socket %s already bound", name());
    string nm = strcat(basename(), "_adapter");

    auto guard = get_hierarchy_scope();
    adapter_type* adapter = new adapter_type(nm.c_str());
    base_type::bind(adapter->in);
    adapter->out.bind(socket);
    m_adapter = adapter;
}

template <>
inline void tlm_initiator_socket::bind<32>(
    tlm::tlm_base_initiator_socket_b<32>& socket) {
    base_type::bind(socket);
}

template <unsigned int WIDTH>
inline void tlm_initiator_socket::bind(
    tlm::tlm_base_target_socket_b<WIDTH>& socket) {
    typedef tlm_bus_width_adapter<32, WIDTH> adapter_type;
    VCML_ERROR_ON(m_adapter, "socket %s already bound", name());
    string nm = strcat(basename(), "_adapter");

    auto guard = get_hierarchy_scope();
    adapter_type* adapter = new adapter_type(nm.c_str());
    base_type::bind(adapter->in);
    adapter->out.bind(socket);
    m_adapter = adapter;
}

template <>
inline void tlm_initiator_socket::bind<32>(
    tlm::tlm_base_target_socket_b<32>& other) {
    base_type::bind(other);
}

class tlm_target_socket : public simple_target_socket<tlm_target_socket>,
                          public hierarchy_element
{
private:
    int m_curr;
    int m_next;
    sc_event* m_free_ev;
    tlm_dmi_cache* m_dmi_cache;
    tlm_exmon m_exmon;
    tlm_initiator_stub* m_stub;
    tlm_host* m_host;
    module* m_parent;
    module* m_adapter;

    tlm_generic_payload* m_payload;
    tlm_sbi m_sideband;

    void wait_free();

    void trace_fw(const tlm_generic_payload& tx, const sc_time& t);
    void trace_bw(const tlm_generic_payload& tx, const sc_time& t);

    void b_transport_int(tlm_generic_payload& tx, sc_time& dt);
    unsigned int transport_dbg_int(tlm_generic_payload& tx);
    bool get_dmi_ptr_int(tlm_generic_payload& tx, tlm_dmi& dmi);

protected:
    virtual void b_transport(tlm_generic_payload& tx, sc_time& dt);
    virtual unsigned int transport_dbg(tlm_generic_payload& tx);
    virtual bool get_dmi_ptr(tlm_generic_payload& tx, tlm_dmi& dmi);

public:
    property<bool> trace_all;
    property<bool> trace_errors;
    property<bool> allow_dmi;

    const address_space as;

    tlm_target_socket() = delete;
    tlm_target_socket(const char* name, address_space a = VCML_AS_DEFAULT);
    virtual ~tlm_target_socket();

    VCML_KIND(tlm_target_socket);

    tlm_dmi_cache& dmi_cache();
    tlm_exmon& exmon() { return m_exmon; }

    void map_dmi(const tlm_dmi& dmi);
    void unmap_dmi(const range& mem);
    void unmap_dmi(u64 start, u64 end);
    void remap_dmi(const sc_time& rlat, const sc_time& wlat);
    void invalidate_dmi();

    template <unsigned int WIDTH>
    void bind(tlm::tlm_base_initiator_socket<WIDTH>& other);

    template <unsigned int WIDTH>
    void bind(tlm::tlm_base_target_socket<WIDTH>& other);

    template <unsigned int WIDTH>
    tlm::tlm_target_socket<WIDTH>& adapt();

    bool is_stubbed() const { return m_stub != nullptr; }
    void stub();

    bool in_transaction() const;
    bool in_debug_transaction() const;

    const tlm_generic_payload& current_transaction() const;
    const tlm_sbi& current_sideband() const;

    size_t current_transaction_size() const;
    range current_transaction_address() const;
};

inline void tlm_target_socket::wait_free() {
    if (!m_free_ev) {
        auto guard = get_hierarchy_scope();
        m_free_ev = new sc_event(mkstr("%s_free", basename()).c_str());
    }

    sc_core::wait(*m_free_ev);
}

inline void tlm_target_socket::trace_fw(const tlm_generic_payload& tx,
                                        const sc_time& t) {
    if (trace_all)
        tracer::record(TRACE_FW, *this, tx, t);
}

inline void tlm_target_socket::trace_bw(const tlm_generic_payload& tx,
                                        const sc_time& t) {
    if (trace_all || (trace_errors && failed(tx)))
        tracer::record(TRACE_BW, *this, tx, t);
}

inline tlm_dmi_cache& tlm_target_socket::dmi_cache() {
    if (!m_dmi_cache)
        m_dmi_cache = new tlm_dmi_cache();
    return *m_dmi_cache;
}

inline void tlm_target_socket::map_dmi(const tlm_dmi& dmi) {
    dmi_cache().insert(dmi);
}

inline void tlm_target_socket::unmap_dmi(const range& mem) {
    unmap_dmi(mem.start, mem.end);
}

template <unsigned int WIDTH>
inline void tlm_target_socket::bind(
    tlm::tlm_base_initiator_socket<WIDTH>& socket) {
    typedef tlm_bus_width_adapter<WIDTH, 32> adapter_type;
    VCML_ERROR_ON(m_adapter, "socket %s already bound", name());
    const string nm = strcat(basename(), "_adapter");

    auto guard = get_hierarchy_scope();
    adapter_type* adapter = new adapter_type(nm.c_str());
    socket.bind(adapter->in);
    adapter->out.bind(*this);
    m_adapter = adapter;
}

template <>
inline void tlm_target_socket::bind<32>(
    tlm::tlm_base_initiator_socket<32>& socket) {
    base_type::bind(socket);
}

template <unsigned int WIDTH>
inline void tlm_target_socket::bind(
    tlm::tlm_base_target_socket<WIDTH>& socket) {
    typedef tlm_bus_width_adapter<WIDTH, 32> adapter_type;
    VCML_ERROR_ON(m_adapter, "socket %s already bound", name());
    const string nm = strcat(basename(), "_adapter");

    auto guard = get_hierarchy_scope();
    adapter_type* adapter = new adapter_type(nm.c_str());
    socket.bind(adapter->in);
    adapter->out.bind(*this);
    m_adapter = adapter;
}

template <>
inline void tlm_target_socket::bind<32>(
    tlm::tlm_base_target_socket<32>& socket) {
    socket.tlm::tlm_base_target_socket<32>::bind(*this);
}

template <unsigned int WIDTH>
inline tlm::tlm_target_socket<WIDTH>& tlm_target_socket::adapt() {
    typedef tlm_bus_width_adapter<WIDTH, 32> adapter_type;
    adapter_type* adapter = dynamic_cast<adapter_type*>(m_adapter);
    VCML_ERROR_ON(m_adapter && !adapter, "socket %s already bound", name());
    if (adapter == nullptr) {
        const string nm = strcat(basename(), "_adapter");
        auto guard = get_hierarchy_scope();
        adapter = new adapter_type(nm.c_str());
        adapter->out.bind(*this);
        m_adapter = adapter;
    }

    return adapter->in;
}

template <>
inline tlm::tlm_target_socket<32>& tlm_target_socket::adapt() {
    return *this;
}

inline bool tlm_target_socket::in_transaction() const {
    return m_payload != nullptr;
}

inline bool tlm_target_socket::in_debug_transaction() const {
    return m_payload ? m_sideband.is_debug : false;
}

inline const tlm_generic_payload& tlm_target_socket::current_transaction()
    const {
    VCML_ERROR_ON(!m_payload, "socket not currently servicing a transaction");
    return *m_payload;
}

inline const tlm_sbi& tlm_target_socket::current_sideband() const {
    VCML_ERROR_ON(!m_payload, "socket not currently servicing a transaction");
    return m_sideband;
}

inline size_t tlm_target_socket::current_transaction_size() const {
    return m_payload ? m_payload->get_data_length() : 0;
}

inline range tlm_target_socket::current_transaction_address() const {
    return m_payload ? range(*m_payload) : range();
}

using tlm_initiator_array = socket_array<tlm_initiator_socket>;
using tlm_target_array = socket_array<tlm_target_socket>;

tlm::tlm_base_initiator_socket<>& tlm_initiator(const sc_object& parent,
                                                const string& port);
tlm::tlm_base_initiator_socket<>& tlm_initiator(const sc_object& parent,
                                                const string& port,
                                                size_t idx);

tlm::tlm_base_target_socket<>& tlm_target(const sc_object& parent,
                                          const string& port);
tlm::tlm_base_target_socket<>& tlm_target(const sc_object& parent,
                                          const string& port, size_t idx);

void tlm_stub(const sc_object& obj, const string& port);
void tlm_stub(const sc_object& obj, const string& port, size_t idx);

void tlm_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2);
void tlm_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2);
void tlm_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2);
void tlm_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2);

} // namespace vcml

#endif
