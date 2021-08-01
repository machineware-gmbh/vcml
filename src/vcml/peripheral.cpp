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

    bool peripheral::cmd_mmap(const vector<string>& args, ostream& os) {
        os << "Memory map of " << name();
        #define HEX(x) std::setfill('0') << std::setw((x) > ~0u ? 16 : 8) << \
                       std::hex << (x) << std::dec

        auto regs = get_registers();
        std::sort(regs.begin(), regs.end(),
                [](reg_base* a, reg_base* b) -> bool {
            return a->get_range().start < b->get_range().start;
        });

        int i = 0;
        for (auto reg : regs) {
            os << std::endl << i++ << ": "
               << HEX(reg->get_range().start) << ".."
               << HEX(reg->get_range().end) << " -> " << reg->basename();
        }

        #undef HEX
        return true;
    }

    peripheral::peripheral(const sc_module_name& nm, endianess endian,
                           unsigned int rlatency, unsigned int wlatency):
        component(nm),
        m_current_cpu(SBI_NONE.cpuid),
        m_endian(endian),
        m_registers(),
        read_latency("read_latency", rlatency),
        write_latency("write_latency", wlatency) {
        register_command("mmap", 0, this, &peripheral::cmd_mmap,
                         "shows the memory map of this peripheral");
    }

    peripheral::~peripheral() {
        // nothing to do
    }

    void peripheral::reset() {
        component::reset();

        for (auto r : m_registers)
            r->reset();
    }

    void peripheral::add_register(reg_base* reg) {
        if (stl_contains(m_registers, reg))
            VCML_ERROR("register %s already assigned", reg->name());

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
        const sc_time rlat = clock_cycles(read_latency);
        const sc_time wlat = clock_cycles(write_latency);
        component::map_dmi(ptr, start, end, acs, rlat, wlat);
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
            if (!info.is_debug) {
                local_time() += tx.is_read() ? clock_cycles(read_latency)
                                             : clock_cycles(write_latency);
            }

            if (be_ptr == nullptr) {
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
                    tx.set_byte_enable_ptr(nullptr);
                    tx.set_byte_enable_length(0);
                    nbytes += receive(tx, info);
                }
            }
        }

        tx.set_address(addr);
        tx.set_data_ptr(ptr);
        tx.set_data_length(length);
        tx.set_streaming_width(streaming_width);
        tx.set_byte_enable_ptr(be_ptr);
        tx.set_byte_enable_length(be_length);

        // check for quantum overshoot
        if (!info.is_debug && needs_sync())
            sync();

        return nbytes;
    }

    unsigned int peripheral::receive(tlm_generic_payload& tx,
                                     const sideband& info) {
        unsigned int bytes = 0;
        unsigned int nregs = 0;
        unsigned int width = tx.get_streaming_width();

        // no streaming support
        if (width && width != tx.get_data_length()) {
            tx.set_response_status(TLM_BURST_ERROR_RESPONSE);
            return 0;
        }

        // no byte-enable support
        if (tx.get_byte_enable_ptr() || tx.get_byte_enable_length()) {
            tx.set_response_status(TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return 0;
        }

        set_current_cpu(info.cpuid);

        for (auto reg : m_registers)
            if (reg->get_range().overlaps(tx)) {
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
        const sc_time rlat = clock_cycles(read_latency);
        const sc_time wlat = clock_cycles(write_latency);
        component::remap_dmi(rlat, wlat);
    }

}
