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

#ifndef VCML_PROTOCOLS_TLM_SOCKETS_H
#define VCML_PROTOCOLS_TLM_SOCKETS_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"
#include "vcml/common/thctl.h"

#include "vcml/protocols/tlm_sbi.h"
#include "vcml/protocols/tlm_exmon.h"
#include "vcml/protocols/tlm_stubs.h"
#include "vcml/protocols/tlm_adapters.h"
#include "vcml/protocols/tlm_dmi_cache.h"

#include "vcml/component.h"

namespace vcml {

    class tlm_initiator_socket:
        public simple_initiator_socket<tlm_initiator_socket, 64> {
    private:
        tlm_generic_payload m_tx;
        tlm_generic_payload m_txd;

        tlm_sbi m_sbi;

        tlm_dmi_cache m_dmi_cache;

        tlm_target_stub* m_stub;
        sc_module* m_adapter;
        component* m_host;

        void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end);

    public:
        int  get_cpuid() const  { return m_sbi.cpuid; }
        int  get_level() const  { return m_sbi.level; }

        void set_cpuid(int cpuid);
        void set_level(int level);

        tlm_initiator_socket() = delete;
        tlm_initiator_socket(const char* name, component* host = nullptr);
        virtual ~tlm_initiator_socket();

        VCML_KIND(tlm_initiator_socket);

        u8* lookup_dmi_ptr(const range& addr,
                           vcml_access acs = VCML_ACCESS_READ);
        u8* lookup_dmi_ptr(u64 start, u64 length,
                           vcml_access acs = VCML_ACCESS_READ);

        tlm_dmi_cache& dmi();

        void map_dmi(const tlm_dmi& dmi);
        void unmap_dmi(u64 start, u64 end);

        unsigned int send(tlm_generic_payload& tx,
                          const tlm_sbi& info = SBI_NONE);

        tlm_response_status access_dmi(tlm_command c, u64 addr, void* data,
                                       unsigned int size,
                                       const tlm_sbi& info = SBI_NONE);

        tlm_response_status access (tlm_command cmd, u64 addr, void* data,
                                    unsigned int size,
                                    const tlm_sbi& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        tlm_response_status read   (u64 addr, void* data,
                                    unsigned int size,
                                    const tlm_sbi& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        tlm_response_status write  (u64 addr, const void* data,
                                    unsigned int size,
                                    const tlm_sbi& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        template <typename T>
        tlm_response_status readw  (u64 addr, T& data,
                                    const tlm_sbi& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        template <typename T>
        tlm_response_status writew (u64 addr, const T& data,
                                    const tlm_sbi& info = SBI_NONE,
                                    unsigned int* nbytes = nullptr);

        template <unsigned int WIDTH>
        void bind(tlm::tlm_initiator_socket<WIDTH>& other);

        template <unsigned int WIDTH>
        void bind(tlm::tlm_target_socket<WIDTH>& other);

        void stub(tlm_response_status resp = TLM_ADDRESS_ERROR_RESPONSE);
    };

    inline void tlm_initiator_socket::set_cpuid(int cpuid) {
        m_sbi.cpuid = cpuid;
        VCML_ERROR_ON(m_sbi.cpuid != cpuid, "cpuid too large");
    }

    inline void tlm_initiator_socket::set_level(int level) {
        m_sbi.level = level;
        VCML_ERROR_ON(m_sbi.level != level, "level too large");
    }

    inline u8* tlm_initiator_socket::lookup_dmi_ptr(u64 addr, u64 size,
        vcml_access a) {
        return lookup_dmi_ptr({addr, addr + size - 1}, a);
    }

    inline tlm_dmi_cache& tlm_initiator_socket::dmi() {
        return m_dmi_cache;
    }

    inline void tlm_initiator_socket::map_dmi(const tlm_dmi& dmi) {
        m_dmi_cache.insert(dmi);
    }

    inline void tlm_initiator_socket::unmap_dmi(u64 start, u64 end) {
        m_dmi_cache.invalidate(start, end);
    }

    inline tlm_response_status tlm_initiator_socket::read(u64 addr, void* data,
            unsigned int size, const tlm_sbi& info, unsigned int* bytes) {
        return access(TLM_READ_COMMAND, addr, data, size, info, bytes);
    }

    inline tlm_response_status tlm_initiator_socket::write(u64 addr, const void* data,
            unsigned int size, const tlm_sbi& info, unsigned int* bytes) {
        void* ptr = const_cast<void*>(data);
        return access(TLM_WRITE_COMMAND, addr, ptr, size, info, bytes);
    }

    template <typename T>
    inline tlm_response_status tlm_initiator_socket::readw(u64 addr, T& data,
            const tlm_sbi& info, unsigned int* nbytes) {
        return read(addr, &data, sizeof(T), info, nbytes);
    }

    template <typename T>
    inline tlm_response_status tlm_initiator_socket::writew(u64 addr, const T& data,
            const tlm_sbi& info, unsigned int* nbytes) {
        return write(addr, &data, sizeof(T), info, nbytes);
    }

    template <unsigned int WIDTH>
    inline void tlm_initiator_socket::bind(tlm::tlm_initiator_socket<WIDTH>& other) {
        typedef tlm_bus_width_adapter<64, WIDTH> adapter_type;
        VCML_ERROR_ON(m_adapter, "socket %s already bound", name());

        m_host->hierarchy_push();
        string nm = concat(basename(), "_adapter");
        adapter_type* adapter = new adapter_type(nm.c_str());
        m_host->hierarchy_pop();

        base_type::bind(adapter->IN);
        adapter->OUT.bind(other);
        m_adapter = adapter;
    }

    template <>
    inline void tlm_initiator_socket::bind<64>(
        tlm::tlm_initiator_socket<64>& other) {
        base_type::bind(other);
    }

    template <unsigned int WIDTH>
    inline void tlm_initiator_socket::bind(
        tlm::tlm_target_socket<WIDTH>& other) {
        typedef tlm_bus_width_adapter<64, WIDTH> adapter_type;
        VCML_ERROR_ON(m_adapter, "socket %s already bound", name());

        m_host->hierarchy_push();
        string nm = concat(basename(), "_adapter");
        adapter_type* adapter = new adapter_type(nm.c_str());
        m_host->hierarchy_pop();

        base_type::bind(adapter->IN);
        adapter->OUT.bind(other);
        m_adapter = adapter;
    }

    template <>
    inline void tlm_initiator_socket::bind<64>(
        tlm::tlm_target_socket<64>& other) {
        base_type::bind(other);
    }

    inline void tx_setup(tlm_generic_payload& tx, tlm_command cmd, u64 addr,
                         void* data, unsigned int size) {
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

    class tlm_slave_socket: public simple_target_socket<tlm_slave_socket, 64>
    {
    private:
        int           m_curr;
        int           m_next;
        sc_event      m_free_ev;
        tlm_dmi_cache m_dmi_cache;
        tlm_exmon     m_exmon;
        tlm_initiator_stub* m_stub;
        sc_module*    m_adapter;
        component*    m_host;

        void b_transport(tlm_generic_payload& tx, sc_time& dt);
        unsigned int transport_dbg(tlm_generic_payload& tx);
        bool get_direct_mem_ptr(tlm_generic_payload& tx, tlm_dmi& dmi);

    public:
        tlm_slave_socket() = delete;
        tlm_slave_socket(const char* name, component* host = nullptr);
        virtual ~tlm_slave_socket();

        VCML_KIND(tlm_slave_socket);

        tlm_dmi_cache& dmi()   { return m_dmi_cache; }
        tlm_exmon&     exmem() { return m_exmon; }

        void map_dmi(const tlm_dmi& dmi);
        void unmap_dmi(u64 start, u64 end);
        void remap_dmi(const sc_time& rlat, const sc_time& wlat);
        void invalidate_dmi();

        template <unsigned int WIDTH>
        void bind(tlm::tlm_initiator_socket<WIDTH>& other);

        template <unsigned int WIDTH>
        void bind(tlm::tlm_target_socket<WIDTH>& other);

        void stub();
    };

    inline void tlm_slave_socket::map_dmi(const tlm_dmi& dmi) {
        m_dmi_cache.insert(dmi);
    }

    template <unsigned int WIDTH>
    inline void tlm_slave_socket::bind(tlm::tlm_initiator_socket<WIDTH>& tgt) {
        typedef tlm_bus_width_adapter<WIDTH, 64> adapter_type;
        VCML_ERROR_ON(m_adapter, "socket %s already bound", name());

        hierarchy_guard guard(m_host);
        string nm = concat(basename(), "_adapter");
        adapter_type* adapter = new adapter_type(nm.c_str());
        tgt.bind(adapter->IN);
        adapter->OUT.bind(*this);
        m_adapter = adapter;
    }

    template <>
    inline void tlm_slave_socket::bind<64>(tlm::tlm_initiator_socket<64>& s) {
        base_type::bind(s);
    }

    template <unsigned int WIDTH>
    inline void tlm_slave_socket::bind(tlm_target_socket<WIDTH>& other) {
        typedef tlm_bus_width_adapter<WIDTH, 64> adapter_type;
        VCML_ERROR_ON(m_adapter, "socket %s already bound", name());

        hierarchy_guard guard(m_host);
        string nm = concat(basename(), "_adapter");
        adapter_type* adapter = new adapter_type(nm.c_str());
        other.bind(adapter->IN);
        adapter->OUT.bind(*this);
        m_adapter = adapter;
    }

    template <>
    inline void tlm_slave_socket::bind<64>(tlm_target_socket<64>& other) {
        base_type::bind(other);
    }

}



#endif
