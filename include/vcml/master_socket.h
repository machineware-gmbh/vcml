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

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"

#include "vcml/range.h"
#include "vcml/sbi.h"
#include "vcml/dmi_cache.h"
#include "vcml/component.h"
#include "vcml/adapters.h"

namespace vcml {

    class master_socket: public simple_initiator_socket<master_socket, 64>
    {
    private:
        bool m_free;
        sc_event m_free_ev;

        tlm_generic_payload m_tx;
        tlm_generic_payload m_txd;

        sideband m_sbi;

        dmi_cache m_dmi_cache;

        sc_module* m_adapter;
        component* m_host;

        void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end);

    public:
        int  get_cpuid() const  { return m_sbi.cpuid; }
        int  get_level() const  { return m_sbi.level; }

        void set_cpuid(int cpuid);
        void set_level(int level);

        master_socket() = delete;
        master_socket(const char* name, component* host = nullptr);
        virtual ~master_socket();

        VCML_KIND(master_socket);

        u8* lookup_dmi_ptr(const range& addr,
                           vcml_access acs = VCML_ACCESS_READ);

        dmi_cache& dmi();

        void map_dmi(const tlm_dmi& dmi);
        void unmap_dmi(u64 start, u64 end);

        unsigned int send(tlm_generic_payload& tx,
                          const sideband& info = SBI_NONE);

        tlm_response_status access_dmi(tlm_command c, u64 addr, void* data,
                                       unsigned int size,
                                       const sideband& info = SBI_NONE);

        tlm_response_status access (tlm_command cmd, u64 addr, void* data,
                                    unsigned int size,
                                    const sideband& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        tlm_response_status read   (u64 addr, void* data,
                                    unsigned int size,
                                    const sideband& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        tlm_response_status write  (u64 addr, const void* data,
                                    unsigned int size,
                                    const sideband& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        template <typename T>
        tlm_response_status readw  (u64 addr, T& data,
                                    const sideband& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        template <typename T>
        tlm_response_status writew (u64 addr, const T& data,
                                    const sideband& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        template <unsigned int WIDTH>
        void bind(tlm_initiator_socket<WIDTH>& other);

        template <unsigned int WIDTH>
        void bind(tlm_target_socket<WIDTH>& other);

        void trace_fw(const tlm_generic_payload& tx, const sc_time& dt) const;
        void trace_bw(const tlm_generic_payload& tx, const sc_time& dt) const;
    };

    inline void master_socket::set_cpuid(int cpuid) {
        m_sbi.cpuid = cpuid;
        VCML_ERROR_ON(m_sbi.cpuid != cpuid, "cpuid too large");
    }

    inline void master_socket::set_level(int level) {
        m_sbi.level = level;
        VCML_ERROR_ON(m_sbi.level != level, "level too large");
    }

    inline dmi_cache& master_socket::dmi() {
        return m_dmi_cache;
    }

    inline void master_socket::map_dmi(const tlm_dmi& dmi) {
        m_dmi_cache.insert(dmi);
    }

    inline void master_socket::unmap_dmi(u64 start, u64 end) {
        m_dmi_cache.invalidate(start, end);
    }

    inline tlm_response_status master_socket::read(u64 addr, void* data,
            unsigned int size, const sideband& info, unsigned int* bytes) {
        return access(TLM_READ_COMMAND, addr, data, size, info, bytes);
    }

    inline tlm_response_status master_socket::write(u64 addr, const void* data,
            unsigned int size, const sideband& info, unsigned int* bytes) {
        void* ptr = const_cast<void*>(data);
        return access(TLM_WRITE_COMMAND, addr, ptr, size, info, bytes);
    }

    template <typename T>
    inline tlm_response_status master_socket::readw(u64 addr, T& data,
            const sideband& info, unsigned int* nbytes) {
        return read(addr, &data, sizeof(T), info, nbytes);
    }

    template <typename T>
    inline tlm_response_status master_socket::writew(u64 addr, const T& data,
            const sideband& info, unsigned int* nbytes) {
        return write(addr, &data, sizeof(T), info, nbytes);
    }

    template <unsigned int WIDTH>
    inline void master_socket::bind(tlm_initiator_socket<WIDTH>& other) {
        typedef bus_width_adapter<64, WIDTH> adapter_type;
        VCML_ERROR_ON(m_adapter, "socket %s already bound", name());

        m_host->hierarchy_push();
        string nm = concat(name(), "_adapter");
        adapter_type* adapter = new adapter_type(nm.c_str());
        m_host->hierarchy_pop();

        base_type::bind(adapter->IN);
        adapter->OUT.bind(other);
        m_adapter = adapter;
    }

    template <>
    inline void master_socket::bind<64>(tlm_initiator_socket<64>& other) {
        base_type::bind(other);
    }

    template <unsigned int WIDTH>
    inline void master_socket::bind(tlm_target_socket<WIDTH>& other) {
        typedef bus_width_adapter<64, WIDTH> adapter_type;
        VCML_ERROR_ON(m_adapter, "socket %s already bound", name());

        m_host->hierarchy_push();
        string nm = concat(name(), "_adapter");
        adapter_type* adapter = new adapter_type(nm.c_str());
        m_host->hierarchy_pop();

        base_type::bind(adapter->IN);
        adapter->OUT.bind(other);
        m_adapter = adapter;
    }

    template <>
    inline void master_socket::bind<64>(tlm_target_socket<64>& other) {
        base_type::bind(other);
    }

    inline void master_socket::trace_fw(const tlm_generic_payload& tx,
                                        const sc_time& dt) const {
        if (!m_host->trace_errors && m_host->loglvl >= LOG_TRACE)
            logger::trace_fw(name(), tx, dt);
    }

    inline void master_socket::trace_bw(const tlm_generic_payload& tx,
                                        const sc_time& dt) const {
        if ((!m_host->trace_errors || failed(tx)) &&
            m_host->loglvl >= LOG_TRACE) {
            logger::trace_bw(name(), tx, dt);
        }
    }

    static inline void tx_setup(tlm_generic_payload& tx, tlm_command cmd,
                                u64 addr, void* data, unsigned int size) {
        tx.set_command(cmd);
        tx.set_address(addr);
        tx.set_data_ptr(reinterpret_cast<unsigned char*>(data));
        tx.set_data_length(size);
        tx.set_streaming_width(size);
        tx.set_byte_enable_ptr(nullptr);
        tx.set_byte_enable_length(0);
        tx.set_response_status(TLM_INCOMPLETE_RESPONSE);
        tx.set_dmi_allowed(false);
    }

}

#endif
