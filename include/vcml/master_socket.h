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

#ifndef VCML_MASTER_SOCKET_H
#define VCML_MASTER_SOCKET_H

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"
#include "vcml/txext.h"
#include "vcml/dmi_cache.h"
#include "vcml/component.h"

namespace vcml {

    class master_socket: public simple_initiator_socket<master_socket>
    {
    private:
        bool m_free;
        sc_event m_free_ev;

        tlm_generic_payload m_tx;
        tlm_generic_payload m_txd;

        ext_bank  m_bank_ext;
        ext_exmem m_exmem_ext;

        component* m_host;

        void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end);

    public:
        inline int  get_bank() const   { return m_bank_ext.get_bank(); }
        inline void set_bank(int bank) { m_bank_ext.set_bank(bank); }
        inline int  get_exid() const   { return m_exmem_ext.get_id(); }
        inline void set_exid(int id)   { m_exmem_ext.set_id(id); }

        master_socket(const char* name, component* host = NULL);
        virtual ~master_socket();

        VCML_KIND(master_socket);

        unsigned int send(tlm_generic_payload& tx, int flags = VCML_FLAG_NONE);

        tlm_response_status access_dmi(tlm_command c, u64 addr, void* data,
                                       unsigned int size,
                                       int flags = VCML_FLAG_NONE);

        tlm_response_status access (tlm_command cmd, u64 addr, void* data,
                                    unsigned int size,
                                    int flags = VCML_FLAG_NONE,
                                    unsigned int* nbytes = NULL);

        tlm_response_status read   (u64 addr, void* data,
                                    unsigned int size,
                                    int flags = VCML_FLAG_NONE,
                                    unsigned int* nbytes = NULL);

        tlm_response_status write  (u64 addr, const void* data,
                                    unsigned int size,
                                    int flags = VCML_FLAG_NONE,
                                    unsigned int* nbytes = NULL);

        template <typename T>
        tlm_response_status read   (u64 addr, T& data,
                                    int flags = VCML_FLAG_NONE,
                                    unsigned int* nbytes = NULL);

        template <typename T>
        tlm_response_status write  (u64 addr, const T& data,
                                    int flags = VCML_FLAG_NONE,
                                    unsigned int* nbytes = NULL);
    };

    inline tlm_response_status master_socket::read(u64 addr, void* data,
            unsigned int size, int flags, unsigned int* bytes) {
        return access(TLM_READ_COMMAND, addr, data, size, flags, bytes);
    }

    inline tlm_response_status master_socket::write(u64 addr, const void* data,
            unsigned int size, int flags, unsigned int* bytes) {
        void* ptr = const_cast<void*>(data);
        return access(TLM_WRITE_COMMAND, addr, ptr, size, flags, bytes);
    }

    template <typename T>
    inline tlm_response_status master_socket::read(u64 addr, T& data,
            int flags, unsigned int* nbytes) {
        return read(addr, &data, sizeof(T), flags, nbytes);
    }

    template <typename T>
    inline tlm_response_status master_socket::write (u64 addr, const T& data,
            int flags, unsigned int* nbytes) {
        return write(addr, &data, sizeof(T), flags, nbytes);
    }

    static inline void tx_setup(tlm_generic_payload& tx, tlm_command cmd,
                                u64 addr, void* data, unsigned int size) {
        tx.set_command(cmd);
        tx.set_address(addr);
        tx.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        tx.set_data_length(size);
        tx.set_streaming_width(size);
        tx.set_byte_enable_ptr(NULL);
        tx.set_byte_enable_length(0);
        tx.set_response_status(TLM_INCOMPLETE_RESPONSE);
        tx.set_dmi_allowed(false);
    }

}

#endif
