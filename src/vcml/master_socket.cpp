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

#include "vcml/master_socket.h"

namespace vcml {

    void master_socket::invalidate_direct_mem_ptr(sc_dt::uint64 start,
                                                  sc_dt::uint64 end) {
        unmap_dmi(start, end);
        m_host->invalidate_direct_mem_ptr(this, start, end);
    }

    master_socket::master_socket(const char* nm, component* host):
        simple_initiator_socket<master_socket, 64>(nm),
        m_free(true),
        m_free_ev(concat(nm, "_free").c_str()),
        m_tx(),
        m_txd(),
        m_sbi(SBI_NONE),
        m_dmi_cache(),
        m_host(host) {
        if (m_host == NULL) {
            m_host = dynamic_cast<component*>(get_parent_object());
            VCML_ERROR_ON(!m_host, "socket '%s' declared outside module", nm);
        }

        m_host->register_socket(this);
        register_invalidate_direct_mem_ptr(this,
                &master_socket::invalidate_direct_mem_ptr);

        m_tx.set_extension(new sbiext());
        m_txd.set_extension(new sbiext());
    }

    master_socket::~master_socket() {
        // nothing to do
    }

    unsigned int master_socket::send(tlm_generic_payload& tx,
                                     const sideband& info) try {
        unsigned int   bytes = 0;
        unsigned int   size  = tx.get_data_length();
        unsigned int   width = tx.get_streaming_width();
        unsigned char* beptr = tx.get_byte_enable_ptr();
        unsigned int   belen = tx.get_byte_enable_length();

        if ((width == 0) || (width > size) || (size % width)) {
            tx.set_response_status(TLM_BURST_ERROR_RESPONSE);
            return 0;
        }

        if ((beptr != NULL) && (belen == 0)) {
            tx.set_response_status(TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return 0;
        }

        tx.set_response_status(TLM_INCOMPLETE_RESPONSE);
        tx.set_dmi_allowed(false);
        tx_set_sbi(tx, m_sbi | info);

        if (info.is_debug) {
            sc_time t1 = sc_time_stamp();
            bytes = (*this)->transport_dbg(tx);
            sc_time t2 = sc_time_stamp();
            VCML_ERROR_ON(t1 != t2, "time advanced during debug call");
        } else {
            if (info.is_sync)
                m_host->sync();
            (*this)->b_transport(tx, m_host->offset());
            if (info.is_sync)
                m_host->sync();
            bytes = tx.is_response_ok() ? tx.get_data_length() : 0;
        }

        if (info.is_excl && !tx_is_excl(tx))
            bytes = 0;

        if (m_host->allow_dmi && tx.is_dmi_allowed()) {
            tlm_dmi dmi;
            if ((*this)->get_direct_mem_ptr(tx, dmi))
                map_dmi(dmi);
        }

        return bytes;
    } catch (report& rep) {
        logger::log(rep);
        throw;
    } catch (std::exception& ex) {
        throw;
    }

    tlm_response_status master_socket::access_dmi(tlm_command cmd, u64 addr,
                                                  void* data, unsigned int size,
                                                  const sideband& info) {
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
            m_host->offset() += latency;
            if (info.is_sync)
                m_host->sync();
        }

        return TLM_OK_RESPONSE;
    }

    tlm_response_status master_socket::access(tlm_command cmd, u64 addr,
                                              void* data, unsigned int size,
                                              const sideband& info,
                                              unsigned int* bytes) {

        tlm_response_status rs = TLM_INCOMPLETE_RESPONSE;

        // check if we are allowed to do a DMI access on that address
        if ((cmd != TLM_IGNORE_COMMAND) && (m_host->allow_dmi))
            rs = access_dmi(cmd, addr, data, size, info);

        // if DMI was not successful, send a regular transaction
        if (rs == TLM_INCOMPLETE_RESPONSE) {
            tlm_generic_payload& tx = info.is_debug ? m_txd : m_tx;
            tx_setup(tx, cmd, addr, data, size);
            size = send(tx, info);
            rs = tx.get_response_status();

            // transport_dbg does not change response status
            if (rs == TLM_INCOMPLETE_RESPONSE && info.is_debug)
                rs = TLM_OK_RESPONSE;
        }

        if (rs == TLM_INCOMPLETE_RESPONSE)
            log_warn("got incomplete response from target at 0x%016llx", addr);

        if (bytes != nullptr)
            *bytes = size;
        return rs;
    }

}
