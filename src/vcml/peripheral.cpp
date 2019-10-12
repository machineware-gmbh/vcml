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

#include "vcml/peripheral.h"
#include "vcml/register.h"

namespace vcml {

    peripheral::peripheral(const sc_module_name& nm, vcml_endian endian,
                           unsigned int rlatency, unsigned int wlatency):
        component(nm),
        m_current_cpu(SBI_NONE.cpuid),
        m_endian(endian),
        m_rdlatency(SC_ZERO_TIME),
        m_wrlatency(SC_ZERO_TIME),
        m_registers(),
        m_backends(),
        read_latency("read_latency", rlatency),
        write_latency("write_latency", wlatency),
        backends("backends", "null") {
        vector<string> types = split(backends.get(), ' ');
        for (size_t i = 0; i < types.size(); i++) {
            stringstream ss; ss << "backend" << i;
            backend* be = backend::create(types[i], ss.str());
            if (be != NULL)
                m_backends.push_back(be);
        }
    }

    peripheral::~peripheral() {
        for (backend* be : m_backends)
            delete be;
    }

    void peripheral::reset() {
        component::reset();

        for (auto r : m_registers)
            r->reset();
    }

    void peripheral::add_register(reg_base* reg) {
        if (stl_contains(m_registers, reg))
            VCML_ERROR("register '%' already assigned", reg->name());

        for (auto r : m_registers)
            if (r->get_range().overlaps(reg->get_range()))
                VCML_ERROR("register address space already in use");

        m_registers.push_back(reg);
        std::sort(m_registers.begin(), m_registers.end(),
            [] (const reg_base* a, const reg_base* b) -> bool {
                return a->get_address() < b->get_address();
        });
    }

    void peripheral::remove_register(reg_base* reg) {
        if (!stl_contains(m_registers, reg))
            VCML_ERROR("unknown register '%s'", reg->name());
        stl_remove_erase(m_registers, reg);
    }

    void peripheral::map_dmi(unsigned char* ptr, u64 start, u64 end,
                             vcml_access acs) {
        component::map_dmi(ptr, start, end, acs, m_rdlatency, m_wrlatency);
    }

    bool peripheral::bepeek() {
        for (backend* be : m_backends)
            if (be->peek())
                return true;
        return false;
    }

    size_t peripheral::beread(void* buffer, size_t size) {
        for (backend* be : m_backends)
            if (be->peek())
                return be->read(buffer, size);
        return 0;
    }

    size_t peripheral::bewrite(const void* buffer, size_t size) {
        for (backend* be : m_backends)
            be->write(buffer, size);
        return size;
    }

    unsigned int peripheral::transport(tlm_generic_payload& tx,
                                       const sideband& info) {
        sc_dt::uint64 addr = tx.get_address();
        unsigned char* ptr = tx.get_data_ptr();
        unsigned int length = tx.get_data_length();
        unsigned int streaming_width = tx.get_streaming_width();
        unsigned char* be_ptr = tx.get_byte_enable_ptr();
        unsigned int be_length = tx.get_byte_enable_length();
        unsigned int be_index = 0;
        unsigned int nbytes = 0;

        if (streaming_width == 0)
            streaming_width = length;

        unsigned int npulses = length / streaming_width;
        for (unsigned int pulse = 0; pulse < npulses; pulse++) {
            if (be_ptr == NULL) {
                tx.set_data_ptr(ptr + pulse * streaming_width);
                tx.set_data_length(streaming_width);
                nbytes += receive(tx, info);
            } else {
                for (unsigned int byte = 0; byte < streaming_width; byte++) {
                    if (be_ptr[be_index++ % be_length] == 0x00)
                        continue;

                    tx.set_address(addr + byte);
                    tx.set_data_ptr(ptr + pulse * streaming_width + byte);
                    tx.set_data_length(1);
                    tx.set_streaming_width(1);
                    tx.set_byte_enable_ptr(NULL);
                    tx.set_byte_enable_length(0);
                    nbytes += receive(tx, info);
                }
            }

            if (!info.is_debug)
                local_time() += tx.is_read() ? m_rdlatency : m_wrlatency;
        }

        tx.set_address(addr);
        tx.set_data_ptr(ptr);
        tx.set_data_length(length);
        tx.set_streaming_width(streaming_width);
        tx.set_byte_enable_ptr(be_ptr);
        tx.set_byte_enable_length(be_length);

        return nbytes;
    }

    unsigned int peripheral::receive(tlm_generic_payload& tx,
                                     const sideband& info) {
        unsigned int bytes = 0;
        unsigned int nregs = 0;

        set_current_cpu(info.cpuid);

        for (auto reg : m_registers)
            if (reg->get_range().overlaps(tx)) {
                if (!info.is_debug && (needs_sync() || reg->needs_sync(tx)))
                    sync();
                bytes += reg->receive(tx, info);
                nregs ++;
            }

        set_current_cpu(SBI_NONE.cpuid);
        if (nregs > 0) // stop if at least one register took the access
            return bytes;

        tlm_response_status rs = TLM_OK_RESPONSE;
        const range addr(tx);
        if (tx.is_read())
            rs = read(addr, tx.get_data_ptr(), info);
        if (tx.is_write())
            rs = write(addr, tx.get_data_ptr(), info);

        if (rs == TLM_INCOMPLETE_RESPONSE)
            rs = TLM_ADDRESS_ERROR_RESPONSE;
        tx.set_response_status(rs);
        return tx.is_response_ok() ? addr.length() : 0;
    }

    tlm_response_status peripheral::read(const range& addr, void* data,
                                         const sideband& info) {
        return TLM_INCOMPLETE_RESPONSE; // to be overloaded
    }

    tlm_response_status peripheral::write(const range& addr, const void* data,
                                          const sideband& info) {
        return TLM_INCOMPLETE_RESPONSE; // to be overloaded
    }

    void peripheral::handle_clock_update(clock_t oldclk, clock_t newclk) {
        m_rdlatency = clock_cycles(read_latency);
        m_wrlatency = clock_cycles(write_latency);
        component::remap_dmi(m_rdlatency, m_wrlatency);
    }

}
