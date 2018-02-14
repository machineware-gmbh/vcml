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
        simple_initiator_socket<master_socket>(nm),
        m_free(true),
        m_free_ev(concat(nm, "_free").c_str()),
        m_tx(),
        m_txd(),
        m_bank_ext(),
        m_exmem_ext(),
        m_dmi_cache(),
        m_host(host) {
        if (m_host == NULL) {
            m_host = dynamic_cast<component*>(get_parent_object());
            VCML_ERROR_ON(!m_host, "socket '%s' declared outside module", nm);
        }

        m_host->register_socket(this);
        register_invalidate_direct_mem_ptr(this,
                &master_socket::invalidate_direct_mem_ptr);
    }

    master_socket::~master_socket() {
        m_tx.clear_extension<ext_bank>();
        m_tx.clear_extension<ext_exmem>();
        m_txd.clear_extension<ext_bank>();
        m_txd.clear_extension<ext_exmem>();
    }

    unsigned int master_socket::send(tlm_generic_payload& tx, int flags) {
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

        tx.set_extension(&m_bank_ext);
        if (is_excl(flags)) {
            m_exmem_ext.reset();
            tx.set_extension(&m_exmem_ext);
        }

        if (is_debug(flags)) {
            sc_time t1 = sc_time_stamp();
            bytes = (*this)->transport_dbg(tx);
            sc_time t2 = sc_time_stamp();
            VCML_ERROR_ON(t1 != t2, "time advanced during debug call");
        } else {
            if (is_sync(flags))
                m_host->sync();
            (*this)->b_transport(tx, m_host->offset());
            if (is_sync(flags))
                m_host->sync();
            bytes = tx.is_response_ok() ? tx.get_data_length() : 0;
        }

        tx.clear_extension<ext_bank>();

        if (is_excl(flags)) {
            ext_exmem* ext = tx.get_extension<ext_exmem>();
            if (!ext->get_status())
                bytes = 0;
            tx.clear_extension<ext_exmem>();
        }

        if (m_host->allow_dmi && tx.is_dmi_allowed()) {
            tlm_dmi dmi;
            if ((*this)->get_direct_mem_ptr(tx, dmi))
                map_dmi(dmi);
        }

        return bytes;
    }

    tlm_response_status master_socket::access_dmi(tlm_command cmd, u64 addr,
                                                  void* data, unsigned int size,
                                                  int flags) {
        if (flags & (VCML_FLAG_NODMI | VCML_FLAG_EXCL))
            return TLM_INCOMPLETE_RESPONSE;

        tlm_dmi dmi;
        tlm_command elevate = is_debug(flags) ? TLM_READ_COMMAND : cmd;
        if (!m_dmi_cache.lookup(addr, size, elevate, dmi))
            return TLM_INCOMPLETE_RESPONSE;

        if (is_sync(flags) && !is_debug(flags))
            m_host->sync();

        sc_time latency = SC_ZERO_TIME;
        if (cmd == TLM_READ_COMMAND) {
            memcpy(data, dmi_get_ptr(dmi, addr), size);
            latency += dmi.get_read_latency();
        } else if (cmd == TLM_WRITE_COMMAND) {
            memcpy(dmi_get_ptr(dmi, addr), data, size);
            latency += dmi.get_write_latency();
        }

        if (!is_debug(flags)) {
            m_host->offset() += latency;
            if (is_sync(flags))
                m_host->sync();
        }

        return TLM_OK_RESPONSE;
    }

    tlm_response_status master_socket::access(tlm_command cmd, u64 addr,
                                              void* data, unsigned int size,
                                              int flags, unsigned int* bytes) {

        tlm_response_status rs = TLM_INCOMPLETE_RESPONSE;

        // check if we are allowed to do a DMI access on that address
        if ((cmd != TLM_IGNORE_COMMAND) && (m_host->allow_dmi))
            rs = access_dmi(cmd, addr, data, size, flags);

        // if DMI was not successful, send a regular transaction
        if (rs == TLM_INCOMPLETE_RESPONSE) {
            tlm_generic_payload& tx = is_debug(flags) ? m_txd : m_tx;
            tx_setup(tx, cmd, addr, data, size);
            size = send(tx, flags);
            rs = tx.get_response_status();
        }

        if (bytes != NULL)
            *bytes = size;
        return rs;
    }

}
