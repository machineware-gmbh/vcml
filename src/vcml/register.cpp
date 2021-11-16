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

#include "vcml/register.h"
#include "vcml/peripheral.h"

namespace vcml {

    int reg_base::current_cpu() const {
        return m_host->current_cpu();
    }

    reg_base::reg_base(address_space a, const char* nm, u64 addr, u64 size):
        sc_object(nm),
        m_range(addr, addr + size - 1),
        m_access(VCML_ACCESS_READ_WRITE),
        m_rsync(false),
        m_wsync(false),
        m_host(dynamic_cast<peripheral*>(hierarchy_top())),
        as(a),
        tag() {
        VCML_ERROR_ON(!m_host, "register '%s' declared outside peripheral", nm);
        m_host->add_register(this);
    }

    reg_base::~reg_base() {
        m_host->remove_register(this);
    }

    void reg_base::do_receive(tlm_generic_payload& tx, const tlm_sbi& info) {
        if (tx.is_read() && !is_readable() && !info.is_debug) {
            tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        if (tx.is_write() && !is_writeable() && !info.is_debug) {
            tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        unsigned char* ptr = tx.get_data_ptr();
        if (m_host->get_endian() != host_endian()) // i.e. if big endian
            memswap(ptr, tx.get_data_length());

        if (tx.is_read())
            do_read(tx, ptr);
        if (tx.is_write())
            do_write(tx, ptr);

        if (m_host->get_endian() != host_endian()) // i.e. swap back
            memswap(ptr, tx.get_data_length());

        tx.set_response_status(TLM_OK_RESPONSE);
    }

    unsigned int reg_base::receive(tlm_generic_payload& tx,
                                   const tlm_sbi& info) {
        u64 addr = tx.get_address();
        u64 size = tx.get_data_length();
        u64 strw = tx.get_streaming_width();
        u8* data = tx.get_data_ptr();

        VCML_ERROR_ON(strw != size, "invalid transaction streaming setup");
        VCML_ERROR_ON(!m_range.overlaps(tx), "invalid register access");

        range span = m_range.intersect(tx);

        tx.set_address(span.start - m_range.start);
        tx.set_data_ptr(data + span.start - addr);
        tx.set_streaming_width(span.length());
        tx.set_data_length(span.length());

        if (!info.is_debug) {
            if (tx.is_read() && m_rsync)
                m_host->sync();
            if (tx.is_write() && m_wsync)
                m_host->sync();
        }

        m_host->trace_fw(*this, tx, m_host->local_time());
        do_receive(tx, info);
        m_host->trace_bw(*this, tx, m_host->local_time());

        tx.set_address(addr);
        tx.set_data_length(size);
        tx.set_streaming_width(strw);
        tx.set_data_ptr(data);

        return tx.is_response_ok() ? span.length() : 0;
    }

}
