/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/core/peripheral.h"
#include "vcml/core/register.h"

namespace vcml {

bool peripheral::cmd_mmap(const vector<string>& args, ostream& os) {
    os << "Memory map of " << name();

    address_space as = VCML_AS_DEFAULT;
    if (!args.empty()) {
        as = from_string<address_space>(args[0]);
        os << " (address space " << as << ")";
    }

#define HEX(x)                                                            \
    std::setfill('0') << std::setw((x) > ~0u ? 16 : 8) << std::hex << (x) \
                      << std::dec

    auto regs = get_registers(as);
    std::sort(regs.begin(), regs.end(), [](reg_base* a, reg_base* b) -> bool {
        return a->get_range().start < b->get_range().start;
    });

    int i = 0;
    for (auto reg : regs) {
        os << std::endl
           << i++ << ": " << HEX(reg->get_range().start) << ".."
           << HEX(reg->get_range().end) << " -> " << reg->basename();
    }

#undef HEX
    return true;
}

peripheral::peripheral(const sc_module_name& nm, endianess default_endian,
                       unsigned int rlatency, unsigned int wlatency):
    component(nm),
    m_current_cpu(SBI_NONE.cpuid),
    m_registers(),
    endian("endian", default_endian),
    read_latency("read_latency", rlatency),
    write_latency("write_latency", wlatency) {
    register_command("mmap", 0, &peripheral::cmd_mmap,
                     "shows the memory map of this peripheral");
}

peripheral::~peripheral() {
    // nothing to do
}

void peripheral::reset() {
    component::reset();

    for (auto& [as, regs] : m_registers)
        for (auto* r : regs)
            r->reset();
}

void peripheral::add_register(reg_base* reg) {
    if (stl_contains(m_registers[reg->as], reg))
        VCML_ERROR("register %s already assigned", reg->name());

    for (auto r : m_registers[reg->as]) {
        if (r->get_range().overlaps(reg->get_range()))
            VCML_ERROR(
                "address space of register %s (%d: %s) already in "
                "use by register %s",
                reg->name(), reg->as, to_string(reg->get_range()).c_str(),
                r->name());
    }

    mwr::stl_insert_sorted(m_registers[reg->as], reg,
                           [](const reg_base* a, const reg_base* b) -> bool {
                               return a->get_address() < b->get_address();
                           });
}

void peripheral::remove_register(reg_base* reg) {
    if (!stl_contains(m_registers[reg->as], reg))
        VCML_ERROR("unknown register '%s'", reg->name());
    stl_remove(m_registers[reg->as], reg);
}

const vector<reg_base*>& peripheral::get_registers(address_space as) const {
    auto it = m_registers.find(as);
    if (it == m_registers.end()) {
        static const vector<reg_base*> none;
        return none;
    }

    return it->second;
}

void peripheral::map_dmi(const tlm_dmi& dmi) {
    tlm_dmi copy(dmi);
    copy.set_read_latency(read_cycles());
    copy.set_write_latency(write_cycles());
    tlm_host::map_dmi(copy);
}

void peripheral::map_dmi(unsigned char* ptr, u64 start, u64 end,
                         vcml_access acs) {
    tlm_host::map_dmi(ptr, start, end, acs, read_cycles(), write_cycles());
}

unsigned int peripheral::transport(tlm_generic_payload& tx,
                                   const tlm_sbi& info, address_space as) {
    sc_dt::uint64 addr = tx.get_address();
    unsigned char* ptr = tx.get_data_ptr();
    unsigned int length = tx.get_data_length();
    unsigned int swidth = tx.get_streaming_width();
    unsigned char* be_ptr = tx.get_byte_enable_ptr();
    unsigned int be_length = tx.get_byte_enable_length();
    unsigned int be_index = 0;
    unsigned int nbytes = 0;

    VCML_ERROR_ON(ptr == nullptr, "transaction data pointer cannot be null");
    VCML_ERROR_ON(length == 0, "transaction data length cannot be zero");

    if (tx.get_response_status() != TLM_INCOMPLETE_RESPONSE)
        VCML_ERROR("invalid in-bound transaction response status");

    if (swidth == 0)
        swidth = length;

    unsigned int npulses = length / swidth;
    for (unsigned int pulse = 0; pulse < npulses && !failed(tx); pulse++) {
        if (!info.is_debug) {
            local_time() += tx.is_read() ? clock_cycles(read_latency)
                                         : clock_cycles(write_latency);
        }

        if (be_ptr == nullptr) {
            tx.set_data_ptr(ptr + pulse * swidth);
            tx.set_data_length(swidth);
            tx.set_response_status(TLM_INCOMPLETE_RESPONSE);
            nbytes += receive(tx, info, as);
        } else {
            for (unsigned int byte = 0; byte < swidth && !failed(tx); byte++) {
                if (be_ptr[be_index++ % be_length]) {
                    tx.set_address(addr + byte);
                    tx.set_data_ptr(ptr + pulse * swidth + byte);
                    tx.set_data_length(1);
                    tx.set_streaming_width(1);
                    tx.set_byte_enable_ptr(nullptr);
                    tx.set_byte_enable_length(0);
                    tx.set_response_status(TLM_INCOMPLETE_RESPONSE);
                    nbytes += receive(tx, info, as);
                }
            }
        }
    }

    if (tx.get_response_status() == TLM_INCOMPLETE_RESPONSE)
        VCML_ERROR("invalid out-bound transaction response status");

    tx.set_address(addr);
    tx.set_data_ptr(ptr);
    tx.set_data_length(length);
    tx.set_streaming_width(swidth);
    tx.set_byte_enable_ptr(be_ptr);
    tx.set_byte_enable_length(be_length);

    // check for quantum overshoot
    if (!info.is_debug && needs_sync())
        sync();

    return nbytes;
}

unsigned int peripheral::receive(tlm_generic_payload& tx, const tlm_sbi& info,
                                 address_space as) {
    unsigned int bytes = 0;
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

    for (auto* reg : m_registers[as]) {
        if (reg->get_range().overlaps(tx)) {
            bytes += reg->receive(tx, info);

            if (success(tx) && reg->is_natural_accesses_only())
                break;

            if (failed(tx))
                break;
        }
    }

    set_current_cpu(SBI_NONE.cpuid);
    if (success(tx) || failed(tx)) // stop if at least one reg took the access
        return bytes;

    tlm_response_status rs = TLM_OK_RESPONSE;
    const range addr(tx);
    if (tx.is_read())
        rs = read(addr, tx.get_data_ptr(), info, as);
    if (tx.is_write())
        rs = write(addr, tx.get_data_ptr(), info, as);

    if (rs == TLM_INCOMPLETE_RESPONSE)
        rs = TLM_ADDRESS_ERROR_RESPONSE;
    tx.set_response_status(rs);
    return tx.is_response_ok() ? addr.length() : 0;
}

tlm_response_status peripheral::read(const range& addr, void* data,
                                     const tlm_sbi& info, address_space as) {
    return read(addr, data, info); // to be overloaded
}

tlm_response_status peripheral::read(const range& addr, void* data,
                                     const tlm_sbi& info) {
    return TLM_INCOMPLETE_RESPONSE; // to be overloaded
}

tlm_response_status peripheral::write(const range& addr, const void* data,
                                      const tlm_sbi& info, address_space as) {
    return write(addr, data, info); // to be overloaded
}

tlm_response_status peripheral::write(const range& addr, const void* data,
                                      const tlm_sbi& info) {
    return TLM_INCOMPLETE_RESPONSE; // to be overloaded
}

void peripheral::handle_clock_update(hz_t oldclk, hz_t newclk) {
    const sc_time rlat = clock_cycles(read_latency);
    const sc_time wlat = clock_cycles(write_latency);
    component::remap_dmi(rlat, wlat);
}

} // namespace vcml
