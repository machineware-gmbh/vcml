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

    reg_base::reg_base(const char* nm, u64 addr, u64 size, peripheral* host):
        sc_object(nm),
        m_range(addr, addr + size - 1),
        m_access(VCML_ACCESS_READ_WRITE),
        m_rsync(false),
        m_wsync(false),
        m_host(host)
    {
        if (m_host == NULL)
            m_host = dynamic_cast<peripheral*>(get_parent_object());

        VCML_ERROR_ON(!m_host, "register '%s' declared outside peripheral", nm);
        m_host->add_register(this);
    }

    reg_base::~reg_base() {
        m_host->remove_register(this);
    }

    unsigned int reg_base::receive(tlm_generic_payload& tx,
                                   const sideband& info) {
        VCML_ERROR_ON(!m_range.overlaps(tx), "invalid register access");
        range addr = m_range.intersect(tx);

        if (addr.length() == 0) {
            tx.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }

        if (tx.get_streaming_width() != tx.get_data_length()) {
            tx.set_response_status(TLM_BURST_ERROR_RESPONSE);
            return 0;
        }

        if (tx.get_byte_enable_ptr() || tx.get_byte_enable_length()) {
            tx.set_response_status(TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return 0;
        }

        if (tx.is_read() && !is_readable() && !info.is_debug) {
            tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            return 0;
        }

        if (tx.is_write() && !is_writeable() && !info.is_debug) {
            tx.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            return 0;
        }

        if (!info.is_debug && tx.is_read() && m_rsync)
            m_host->sync();

        unsigned char* ptr = tx.get_data_ptr() + addr.start - tx.get_address();
        if (m_host->get_endian() != host_endian()) // i.e. if big endian
            memswap(ptr, addr.length());

        if (tx.is_read())
            do_read(addr, ptr);
        if (tx.is_write())
            do_write(addr, ptr);

        if (m_host->get_endian() != host_endian()) // i.e. swap back
            memswap(ptr, addr.length());

        tx.set_response_status(TLM_OK_RESPONSE);

        if (!info.is_debug && tx.is_write() && m_wsync)
            m_host->sync();

        return addr.length();
    }

}
