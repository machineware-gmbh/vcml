/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm_sockets.h"

namespace vcml {

void tlm_initiator_socket::invalidate_direct_mem_ptr_int(sc_dt::uint64 start,
                                                         sc_dt::uint64 end) {
    VCML_ERROR_ON(start > end, "invalid dmi invalidation request");
    invalidate_direct_mem_ptr(start, end);
}

void tlm_initiator_socket::invalidate_direct_mem_ptr(u64 start, u64 end) {
    unmap_dmi(start, end);
    m_host->invalidate_direct_mem_ptr(*this, start, end);
}

tlm_initiator_socket::tlm_initiator_socket(const char* nm,
                                           address_space space):
    simple_initiator_socket<tlm_initiator_socket>(nm),
    hierarchy_element(),
    m_tx(),
    m_txd(),
    m_sbi(SBI_NONE),
    m_dmi_cache(),
    m_stub(nullptr),
    m_host(hierarchy_search<tlm_host>()),
    m_parent(hierarchy_search<module>()),
    m_adapter(nullptr),
    trace_all(this, "trace", false),
    trace_errors(this, "trace_errors", false),
    allow_dmi(this, "allow_dmi", true) {
    VCML_ERROR_ON(!m_host, "socket '%s' declared outside tlm_host", nm);
    VCML_ERROR_ON(!m_parent, "socket '%s' declared outside module", nm);

    trace_all.inherit_default();
    trace_errors.inherit_default();
    allow_dmi.inherit_default();

    m_host->register_socket(this);

    register_invalidate_direct_mem_ptr(
        this, &tlm_initiator_socket::invalidate_direct_mem_ptr_int);

    m_tx.set_extension(new sbiext());
    m_txd.set_extension(new sbiext());
}

tlm_initiator_socket::~tlm_initiator_socket() {
    if (m_adapter)
        delete m_adapter;
    if (m_stub)
        delete m_stub;
    if (m_dmi_cache)
        delete m_dmi_cache;
}

u8* tlm_initiator_socket::lookup_dmi_ptr(const range& mem, vcml_access rw) {
    if (!allow_dmi)
        return nullptr;

    tlm_dmi dmi;
    if (dmi_cache().lookup(mem, rw, dmi))
        return dmi_get_ptr(dmi, mem.start);

    tlm_generic_payload tx;
    tlm_command cmd = tlm_command_from_access(rw);
    tx_setup(tx, cmd, mem.start, nullptr, mem.length());
    if (!(*this)->get_direct_mem_ptr(tx, dmi))
        return nullptr;

    map_dmi(dmi);

    // Re-check permission for RW requests
    if (!dmi_check_access(dmi, rw))
        return nullptr;

    // Granted DMI region might be smaller
    if (!mem.inside(dmi))
        return nullptr;

    return dmi_get_ptr(dmi, mem.start);
}

void tlm_initiator_socket::b_transport(tlm_generic_payload& tx, sc_time& t) {
    trace_fw(tx, t);
    (*this)->b_transport(tx, t);
    trace_bw(tx, t);
}

unsigned int tlm_initiator_socket::send(tlm_generic_payload& tx,
                                        const tlm_sbi& info) try {
    unsigned int bytes = 0;
    unsigned int size = tx.get_data_length();
    unsigned int width = tx.get_streaming_width();
    unsigned char* beptr = tx.get_byte_enable_ptr();
    unsigned int belen = tx.get_byte_enable_length();
    u64 addr = tx.get_address();

    if ((width == 0) || (width > size) || (size % width)) {
        tx.set_response_status(TLM_BURST_ERROR_RESPONSE);
        return 0;
    }

    if ((beptr != nullptr) && (belen == 0)) {
        tx.set_response_status(TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return 0;
    }

    tx_reset(tx);
    tx_set_sbi(tx, m_sbi | info);

    if (info.is_debug) {
        sc_time t1(sc_time_stamp());
        bytes = (*this)->transport_dbg(tx);
        sc_time t2(sc_time_stamp());

        if (thctl_is_sysc_thread() && t1 != t2)
            VCML_ERROR("time advanced during debug call");
    } else {
        if (!is_thread())
            VCML_ERROR("non-debug TLM access outside SC_THREAD forbidden");

        if (info.is_sync || m_host->needs_sync())
            m_host->sync();

        sc_time& offset = m_host->local_time();
        sc_time local = sc_time_stamp() + offset;

        b_transport(tx, offset);

        sc_time now = sc_time_stamp() + offset;
        VCML_ERROR_ON(now < local, "b_transport time went backwards");

        if (info.is_sync || m_host->needs_sync())
            m_host->sync();
        bytes = tx.is_response_ok() ? tx.get_data_length() : 0;
    }

    if (info.is_excl && !tx_is_excl(tx))
        bytes = 0;

    if (allow_dmi && tx.is_dmi_allowed()) {
        tlm_dmi dmi;
        tx.set_address(addr);
        if ((*this)->get_direct_mem_ptr(tx, dmi))
            map_dmi(dmi);
    }

    return bytes;
} catch (report& rep) {
    m_parent->log.error(rep);
    throw;
} catch (std::exception& ex) {
    m_parent->log.error(ex);
    throw;
}

tlm_response_status tlm_initiator_socket::access_dmi(tlm_command cmd, u64 addr,
                                                     void* data,
                                                     unsigned int size,
                                                     const tlm_sbi& info) {
    if (info.is_nodmi || info.is_excl)
        return TLM_INCOMPLETE_RESPONSE;

    tlm_dmi dmi;
    tlm_command elevate = info.is_debug ? TLM_READ_COMMAND : cmd;
    if (!dmi_cache().lookup(addr, size, elevate, dmi))
        return TLM_INCOMPLETE_RESPONSE;

    if (info.is_sync && !info.is_debug)
        m_host->sync();

    sc_time latency = SC_ZERO_TIME;
    if (cmd == TLM_READ_COMMAND) {
        memcpy(data, dmi_get_ptr(dmi, addr), size);
        latency += dmi.get_read_latency();
    } else if (cmd == TLM_WRITE_COMMAND) {
        memcpy(dmi_get_ptr(dmi, addr), data, size);
        latency += dmi.get_write_latency();
    }

    if (!info.is_debug) {
        m_host->local_time() += latency;
        if (info.is_sync)
            m_host->sync();
    }

    return TLM_OK_RESPONSE;
}

tlm_response_status tlm_initiator_socket::access(tlm_command cmd, u64 addr,
                                                 void* data, unsigned int size,
                                                 const tlm_sbi& info,
                                                 unsigned int* sz) {
    // TLM protocol sanity checking
    if (!info.is_debug && !is_thread())
        VCML_ERROR("non-debug TLM access outside SC_THREAD forbidden");

    // check if we are allowed to do a DMI access on that address
    if (cmd != TLM_IGNORE_COMMAND && allow_dmi) {
        if (success(access_dmi(cmd, addr, data, size, info))) {
            if (sz != nullptr)
                *sz = size;
            return TLM_OK_RESPONSE;
        }
    }

    // if DMI was not successful, send a regular transaction
    auto& tx = info.is_debug ? m_txd : m_tx;
    tx_setup(tx, cmd, addr, data, size);
    size = send(tx, info);

    // transport_dbg does not always change response status
    tlm_response_status rs = tx.get_response_status();
    if (rs == TLM_INCOMPLETE_RESPONSE && info.is_debug)
        rs = TLM_OK_RESPONSE;

    if (rs == TLM_INCOMPLETE_RESPONSE)
        m_parent->log_warn("got incomplete response from 0x%016llx", addr);

    if (sz != nullptr)
        *sz = size;

    return rs;
}

void tlm_initiator_socket::stub(tlm_response_status r) {
    VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new tlm_target_stub(strcat(basename(), "_stub").c_str(), r);
    base_type::bind(m_stub->in);
}

void tlm_target_socket::b_transport_int(tlm_generic_payload& tx, sc_time& t) {
    b_transport(tx, t);
}

unsigned int tlm_target_socket::transport_dbg_int(tlm_generic_payload& tx) {
    return transport_dbg(tx);
}

bool tlm_target_socket::get_dmi_ptr_int(tlm_generic_payload& tx, tlm_dmi& d) {
    return get_dmi_ptr(tx, d);
}

void tlm_target_socket::b_transport(tlm_generic_payload& tx, sc_time& dt) {
    trace_fw(tx, dt);

    int self = m_next++;
    while (self != m_curr)
        wait_free();

    m_payload = &tx;
    m_sideband = tx_get_sbi(tx);

    tx.set_dmi_allowed(false);

    tlm_dmi dmi;
    if (allow_dmi && m_dmi_cache && m_dmi_cache->lookup(tx, dmi)) {
        if (tx_is_excl(tx) && tx.is_read()) {
            u64 lo = tx.get_address();
            u64 hi = lo + tx_size(tx) - 1;
            (*this)->invalidate_direct_mem_ptr(lo, hi);
        } else {
            tx.set_dmi_allowed(true);
        }
    }

    if (m_exmon.update(tx))
        m_host->b_transport(*this, tx, dt);
    else
        tx.set_response_status(TLM_OK_RESPONSE);

    m_curr++;
    if (m_free_ev)
        m_free_ev->notify();

    m_payload = nullptr;
    m_sideband = SBI_NONE;

    trace_bw(tx, dt);
}

unsigned int tlm_target_socket::transport_dbg(tlm_generic_payload& tx) {
    m_payload = &tx;
    m_sideband = tx_get_sbi(tx) | SBI_DEBUG;

    unsigned int n = m_host->transport_dbg(*this, tx);

    m_payload = nullptr;
    m_sideband = SBI_NONE;

    return n;
}

bool tlm_target_socket::get_dmi_ptr(tlm_generic_payload& tx, tlm_dmi& dmi) {
    dmi.allow_read_write();
    dmi.set_start_address(0);
    dmi.set_end_address((sc_dt::uint64)-1);

    if (!allow_dmi)
        return false;

    if (!(m_dmi_cache && m_dmi_cache->lookup(tx, dmi)) &&
        !m_host->get_direct_mem_ptr(*this, tx, dmi))
        return false;

    return m_exmon.override_dmi(tx, dmi);
}

tlm_target_socket::tlm_target_socket(const char* nm, address_space a):
    simple_target_socket<tlm_target_socket>(nm),
    hierarchy_element(),
    m_curr(0),
    m_next(0),
    m_free_ev(nullptr),
    m_dmi_cache(nullptr),
    m_exmon(),
    m_stub(nullptr),
    m_host(hierarchy_search<tlm_host>()),
    m_parent(hierarchy_search<module>()),
    m_adapter(nullptr),
    m_payload(nullptr),
    m_sideband(SBI_NONE),
    trace_all(this, "trace", false),
    trace_errors(this, "trace_errors", false),
    allow_dmi(this, "allow_dmi", true),
    as(a) {
    VCML_ERROR_ON(!m_host, "socket '%s' declared outside module", nm);

    trace_all.inherit_default();
    trace_errors.inherit_default();
    allow_dmi.inherit_default();

    m_host->register_socket(this);

    register_b_transport(this, &tlm_target_socket::b_transport_int);
    register_transport_dbg(this, &tlm_target_socket::transport_dbg_int);
    register_get_direct_mem_ptr(this, &tlm_target_socket::get_dmi_ptr_int);
}

tlm_target_socket::~tlm_target_socket() {
    if (m_host)
        m_host->unregister_socket(this);
    if (m_adapter)
        delete m_adapter;
    if (m_stub)
        delete m_stub;
    if (m_dmi_cache)
        delete m_dmi_cache;
    if (m_free_ev)
        delete m_free_ev;
}

void tlm_target_socket::unmap_dmi(u64 start, u64 end) {
    if (m_dmi_cache && m_dmi_cache->invalidate(start, end))
        (*this)->invalidate_direct_mem_ptr(start, end);
}

void tlm_target_socket::remap_dmi(const sc_time& rd, const sc_time& wr) {
    if (m_dmi_cache) {
        for (auto dmi : m_dmi_cache->get_entries()) {
            if (dmi.get_read_latency() != rd ||
                dmi.get_write_latency() != wr) {
                (*this)->invalidate_direct_mem_ptr(dmi.get_start_address(),
                                                   dmi.get_end_address());
                dmi.set_read_latency(rd);
                dmi.set_write_latency(wr);
            }
        }
    }
}

void tlm_target_socket::invalidate_dmi() {
    if (m_dmi_cache) {
        for (const tlm_dmi& dmi : m_dmi_cache->get_entries()) {
            (*this)->invalidate_direct_mem_ptr(dmi.get_start_address(),
                                               dmi.get_end_address());
        }
    }
}

void tlm_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
    auto guard = get_hierarchy_scope();
    m_stub = new tlm_initiator_stub(strcat(basename(), "_stub").c_str());
    m_stub->out.bind(*this);
}

template <typename SOCKET = tlm::tlm_base_initiator_socket<>>
static SOCKET* get_initiator_socket(sc_object* obj) {
    return dynamic_cast<SOCKET*>(obj);
}

template <typename SOCKET = tlm::tlm_base_target_socket<>>
static SOCKET* get_target_socket(sc_object* port) {
    return dynamic_cast<SOCKET*>(port);
}

static tlm::tlm_base_initiator_socket<>* get_initiator_socket(sc_object* array,
                                                              size_t idx) {
    auto* main = dynamic_cast<tlm_initiator_array*>(array);
    if (main)
        return &main->get(idx);
    auto* base = dynamic_cast<tlm_base_initiator_array*>(array);
    if (base)
        return &base->get(idx);
    return nullptr;
}

static tlm::tlm_base_target_socket<>* get_target_socket(sc_object* array,
                                                        size_t idx) {
    auto* main = dynamic_cast<tlm_target_array*>(array);
    if (main)
        return &main->get(idx);
    auto* base = dynamic_cast<tlm_base_target_array*>(array);
    if (base)
        return &base->get(idx);
    return nullptr;
}

tlm::tlm_base_initiator_socket<>& tlm_initiator(const sc_object& parent,
                                                const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

tlm::tlm_base_initiator_socket<>& tlm_initiator(const sc_object& parent,
                                                const string& port,
                                                size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_initiator_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid initiator socket", child->name());
    return *sock;
}

tlm::tlm_base_target_socket<>& tlm_target(const sc_object& parent,
                                          const string& port) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

tlm::tlm_base_target_socket<>& tlm_target(const sc_object& parent,
                                          const string& port, size_t idx) {
    sc_object* child = find_child(parent, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", parent.name(), port.c_str());
    auto* sock = get_target_socket(child, idx);
    VCML_ERROR_ON(!sock, "%s is not a valid target socket", child->name());
    return *sock;
}

void tlm_stub(const sc_object& obj, const string& port) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* ini = get_initiator_socket<tlm_initiator_socket>(child);
    auto* tgt = get_target_socket<tlm_target_socket>(child);

    if (!ini && !tgt)
        VCML_ERROR("%s is not a valid tlm socket", child->name());

    if (ini)
        ini->stub();
    if (tgt)
        tgt->stub();
}

void tlm_stub(const sc_object& obj, const string& port, size_t idx) {
    sc_object* child = find_child(obj, port);
    VCML_ERROR_ON(!child, "%s.%s does not exist", obj.name(), port.c_str());

    auto* isock = get_initiator_socket(child, idx);
    auto* ibase = dynamic_cast<tlm_base_initiator_socket*>(isock);
    auto* ifull = dynamic_cast<tlm_initiator_socket*>(isock);

    if (ibase) {
        ibase->stub();
        return;
    }

    if (ifull) {
        ifull->stub();
        return;
    }

    auto* tsock = get_target_socket(child, idx);
    auto* tbase = dynamic_cast<tlm_base_target_socket*>(tsock);
    auto* tfull = dynamic_cast<tlm_target_socket*>(tsock);

    if (tbase) {
        tbase->stub();
        return;
    }

    if (tfull) {
        tfull->stub();
        return;
    }

    VCML_ERROR("%s is not a valid tlm socket array", child->name());
}

void tlm_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid tlm port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid tlm port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void tlm_bind(const sc_object& obj1, const string& port1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid tlm port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid tlm port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void tlm_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid tlm port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid tlm port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

void tlm_bind(const sc_object& obj1, const string& port1, size_t idx1,
              const sc_object& obj2, const string& port2, size_t idx2) {
    auto* p1 = find_child(obj1, port1);
    auto* p2 = find_child(obj2, port2);

    VCML_ERROR_ON(!p1, "%s.%s does not exist", obj1.name(), port1.c_str());
    VCML_ERROR_ON(!p2, "%s.%s does not exist", obj2.name(), port2.c_str());

    auto* i1 = get_initiator_socket(p1, idx1);
    auto* i2 = get_initiator_socket(p2, idx2);
    auto* t1 = get_target_socket(p1, idx1);
    auto* t2 = get_target_socket(p2, idx2);

    VCML_ERROR_ON(!i1 && !t1, "%s is not a valid tlm port", p1->name());
    VCML_ERROR_ON(!i2 && !t2, "%s is not a valid tlm port", p2->name());

    if (i1 && i2)
        i1->bind(*i2);
    else if (i1 && t2)
        i1->bind(*t2);
    else if (t1 && i2)
        i2->bind(*t1);
    else if (t1 && t2)
        t1->bind(*t2);
}

} // namespace vcml
