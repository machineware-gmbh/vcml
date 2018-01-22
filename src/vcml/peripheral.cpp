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
                           const sc_time& rlatency, const sc_time& wlatency):
        component(nm),
        m_endian(endian),
        m_registers(),
        m_backend(NULL),
        read_latency("read_latency", rlatency),
        write_latency("write_latency", wlatency),
        backend_type("backend", "null") {
        m_backend = backend::create(backend_type, "backend");
        if (!m_backend)
            m_backend = backend::create("null", "backend");
    }

    peripheral::~peripheral() {
        if (m_backend)
            delete m_backend;
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
        component::map_dmi(ptr, start, end, acs, read_latency, write_latency);
    }

    unsigned int peripheral::transport(tlm_generic_payload& tx, sc_time& dt,
                                       int flags) {
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
                nbytes += receive(tx, flags);
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
                    nbytes += receive(tx, flags);
                }
            }

            if (!is_debug(flags))
                dt += tx.is_read() ? read_latency : write_latency;
        }

        tx.set_address(addr);
        tx.set_data_ptr(ptr);
        tx.set_data_length(length);
        tx.set_streaming_width(streaming_width);
        tx.set_byte_enable_ptr(be_ptr);
        tx.set_byte_enable_length(be_length);

        return nbytes;
    }

    unsigned int peripheral::receive(tlm_generic_payload& tx, int flags) {
        for (auto reg : m_registers)
            if (reg->get_range().overlaps(tx))
                return reg->receive(tx, flags);

        const range addr(tx);
        tlm_response_status rs = TLM_OK_RESPONSE;
        if (tx.is_read())
            rs = read(addr, tx.get_data_ptr(), flags);
        if (tx.is_write())
            rs = write(addr, tx.get_data_ptr(), flags);

        if (rs == TLM_INCOMPLETE_RESPONSE)
            rs = TLM_ADDRESS_ERROR_RESPONSE;
        tx.set_response_status(rs);
        return tx.is_response_ok() ? addr.length() : 0;
    }

    tlm_response_status peripheral::read(const range& addr, void* data,
                                         int flags) {
        return TLM_INCOMPLETE_RESPONSE; // to be overloaded
    }

    tlm_response_status peripheral::write(const range& addr, const void* data,
                                          int flags) {
        return TLM_INCOMPLETE_RESPONSE; // to be overloaded
    }

}
