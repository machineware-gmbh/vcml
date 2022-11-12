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
    simple_initiator_socket<tlm_initiator_socket, 64>(nm),
    m_tx(),
    m_txd(),
    m_sbi(SBI_NONE),
    m_dmi_cache(),
    m_stub(nullptr),
    m_host(hierarchy_search<tlm_host>()),
    m_parent(hierarchy_search<module>()),
    m_adapter(nullptr),
    trace(this, "trace", false),
    trace_errors(this, "trace_errors", false),
    allow_dmi(this, "allow_dmi", true) {
    VCML_ERROR_ON(!m_host, "socket '%s' declared outside tlm_host", nm);
    VCML_ERROR_ON(!m_parent, "socket '%s' declared outside module", nm);

    trace.inherit_default();
    trace_errors.inherit_default();
    allow_dmi.inherit_default();

    m_host->register_socket(this);

    register_invalidate_direct_mem_ptr(
        this, &tlm_initiator_socket::invalidate_direct_mem_ptr_int);

    m_tx.set_extension(new sbiext());
    m_txd.set_extension(new sbiext());
}

tlm_initiator_socket::~tlm_initiator_socket() {
    if (m_adapter != nullptr)
        delete m_adapter;
    if (m_stub != nullptr)
        delete m_stub;
}

u8* tlm_initiator_socket::lookup_dmi_ptr(const range& mem, vcml_access rw) {
    if (!allow_dmi)
        return nullptr;

    tlm_dmi dmi;
    if (m_dmi_cache.lookup(mem, rw, dmi))
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

unsigned int tlm_initiator_socket::send(tlm_generic_payload& tx,
                                        const tlm_sbi& info) try {
    unsigned int bytes = 0;
    unsigned int size = tx.get_data_length();
    unsigned int width = tx.get_streaming_width();
    unsigned char* beptr = tx.get_byte_enable_ptr();
    unsigned int belen = tx.get_byte_enable_length();

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

        trace_fw(tx, offset);
        (*this)->b_transport(tx, offset);
        trace_bw(tx, offset);

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
    if (!m_dmi_cache.lookup(addr, size, elevate, dmi))
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

    // if DMI was not successful, send a regular transaction; debug
    // transactions can be arbitrarily wide; none debug transactions
    // must be split up if they are wider than socket bus width.

    if (info.is_debug) {
        tx_setup(m_txd, cmd, addr, data, size);
        size = send(m_txd, info);

        // transport_dbg does not always change response status
        tlm_response_status rs = m_txd.get_response_status();
        if (rs == TLM_INCOMPLETE_RESPONSE)
            rs = TLM_OK_RESPONSE;

        if (sz != nullptr)
            *sz = size;

        return rs;
    }

    unsigned int done = 0;
    while (done < size) {
        unsigned int burstsz = min(size - done, get_bus_width() / 8);
        tx_setup(m_tx, cmd, addr + done, (u8*)data + done, burstsz);

        unsigned int bytes = send(m_tx, info);
        done += bytes;

        if (m_tx.get_response_status() == TLM_INCOMPLETE_RESPONSE) {
            m_parent->log_warn(
                "received incomplete response from target at 0x%016llx", addr);
            break;
        }

        if (bytes == 0 || failed(m_tx))
            break;
    }

    if (sz != nullptr)
        *sz = done;

    return m_tx.get_response_status();
}

void tlm_initiator_socket::stub(tlm_response_status r) {
    VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
    hierarchy_guard guard(m_parent);
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

    if (tx_size(tx) > get_bus_width() / 8) {
        tx.set_response_status(TLM_BURST_ERROR_RESPONSE);
        trace_bw(tx, dt);
        return;
    }

    int self = m_next++;
    while (self != m_curr)
        sc_core::wait(m_free_ev);

    m_payload = &tx;
    m_sideband = tx_get_sbi(tx);

    if (tx_is_excl(tx) && tx.is_read())
        unmap_dmi(tx);

    tx.set_dmi_allowed(false);

    if (allow_dmi) {
        tlm_dmi dmi;
        if (m_dmi_cache.lookup(tx, dmi))
            tx.set_dmi_allowed(true);
    }

    if (m_exmon.update(tx))
        m_host->b_transport(*this, tx, dt);
    else
        tx.set_response_status(TLM_OK_RESPONSE);

    m_curr++;
    m_free_ev.notify();

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

    if (!m_dmi_cache.lookup(tx, dmi) &&
        !m_host->get_direct_mem_ptr(*this, tx, dmi))
        return false;

    return m_exmon.override_dmi(tx, dmi);
}

tlm_target_socket::tlm_target_socket(const char* nm, address_space a):
    simple_target_socket<tlm_target_socket, 64>(nm),
    m_curr(0),
    m_next(0),
    m_free_ev(strcat(nm, "_free").c_str()),
    m_dmi_cache(),
    m_exmon(),
    m_stub(nullptr),
    m_host(hierarchy_search<tlm_host>()),
    m_parent(hierarchy_search<module>()),
    m_adapter(nullptr),
    m_payload(nullptr),
    m_sideband(SBI_NONE),
    trace(this, "trace", false),
    trace_errors(this, "trace_errors", false),
    allow_dmi(this, "allow_dmi", true),
    as(a) {
    VCML_ERROR_ON(!m_host, "socket '%s' declared outside module", nm);

    trace.inherit_default();
    trace_errors.inherit_default();
    allow_dmi.inherit_default();

    m_host->register_socket(this);

    register_b_transport(this, &tlm_target_socket::b_transport_int);
    register_transport_dbg(this, &tlm_target_socket::transport_dbg_int);
    register_get_direct_mem_ptr(this, &tlm_target_socket::get_dmi_ptr_int);
}

tlm_target_socket::~tlm_target_socket() {
    if (m_host != nullptr)
        m_host->unregister_socket(this);
    if (m_adapter != nullptr)
        delete m_adapter;
    if (m_stub != nullptr)
        delete m_stub;
}

void tlm_target_socket::unmap_dmi(u64 start, u64 end) {
    m_dmi_cache.invalidate(start, end);
    (*this)->invalidate_direct_mem_ptr(start, end);
}

void tlm_target_socket::remap_dmi(const sc_time& rd, const sc_time& wr) {
    for (auto dmi : m_dmi_cache.get_entries()) {
        if (dmi.get_read_latency() != rd || dmi.get_write_latency() != wr) {
            (*this)->invalidate_direct_mem_ptr(dmi.get_start_address(),
                                               dmi.get_end_address());
            dmi.set_read_latency(rd);
            dmi.set_write_latency(wr);
        }
    }
}

void tlm_target_socket::invalidate_dmi() {
    for (const tlm_dmi& dmi : m_dmi_cache.get_entries()) {
        (*this)->invalidate_direct_mem_ptr(dmi.get_start_address(),
                                           dmi.get_end_address());
    }
}

void tlm_target_socket::stub() {
    VCML_ERROR_ON(m_stub, "socket %s already stubbed", name());
    hierarchy_guard guard(m_parent);
    m_stub = new tlm_initiator_stub(strcat(basename(), "_stub").c_str());
    m_stub->out.bind(*this);
}

} // namespace vcml
