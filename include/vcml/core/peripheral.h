/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PERIPHERAL_H
#define VCML_PERIPHERAL_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/register.h"
#include "vcml/core/component.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"
#include "vcml/protocols/tlm.h"

namespace vcml {

class reg_base;

class peripheral : public component
{
private:
    int m_current_cpu;

    unordered_map<address_space, reg_bank*> m_registers;

    bool cmd_mmap(const vector<string>& args, ostream& os);

    unsigned int forward_to_regs(tlm_generic_payload& tx, const tlm_sbi& info,
                                 address_space as);

    reg_bank& lookup_address_space(address_space as);
    reg_bank* find_address_space(address_space as) const;

public:
    property<endianess> endian;

    property<unsigned int> read_latency;
    property<unsigned int> write_latency;

    sc_time read_cycles() const { return clock_cycles(read_latency); }
    sc_time write_cycles() const { return clock_cycles(write_latency); }

    void set_little_endian() { endian = ENDIAN_LITTLE; }
    void set_big_endian() { endian = ENDIAN_BIG; }

    bool is_little_endian() const;
    bool is_big_endian() const;
    bool is_host_endian() const;

    template <typename T>
    T to_host_endian(T val) const;
    template <typename T>
    T from_host_endian(T val) const;

    int current_cpu() const { return m_current_cpu; }
    void set_current_cpu(int cpu) { m_current_cpu = cpu; }

    void aligned_accesses_only(bool only = true);
    void aligned_accesses_only(address_space as, bool only = true);

    void natural_accesses_only(bool only = true);
    void natural_accesses_only(address_space as, bool only = true);

    void set_access_size(u64 min, u64 max);
    void set_access_size(address_space as, u64 min, u64 max);

    peripheral(const sc_module_name& nm, endianess e = host_endian(),
               unsigned int read_latency = 0, unsigned int write_latency = 0);
    virtual ~peripheral();

    peripheral() = delete;
    peripheral(const peripheral&) = delete;

    VCML_KIND(peripheral);

    virtual void reset() override;

    void add_register(reg_base* reg, u64 offset, address_space as);
    void remove_register(reg_base* reg);

    vector<reg_base*> get_registers() const;
    vector<reg_base*> get_registers(address_space as) const;

    address_space address_space_of(const reg_base& reg) const;
    address_space address_space_of(const string& reg_name) const;

    u64 offset_of(const reg_base& reg, address_space as) const;
    u64 offset_of(const reg_base& reg) const;
    u64 offset_of(const string& reg_name, address_space as) const;
    u64 offset_of(const string& reg_name) const;

    void map_dmi(const tlm_dmi& dmi, address_space as = VCML_AS_DEFAULT);
    void map_dmi(unsigned char* ptr, u64 start, u64 end, vcml_access a,
                 address_space = VCML_AS_DEFAULT);

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const tlm_sbi& info,
                                   address_space as) override;

    virtual unsigned int receive(tlm_generic_payload& tx, const tlm_sbi& info,
                                 address_space as);

    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info, address_space);
    virtual tlm_response_status read(const range& addr, void* data,
                                     const tlm_sbi& info);

    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info, address_space);
    virtual tlm_response_status write(const range& addr, const void* data,
                                      const tlm_sbi& info);

    virtual void handle_clock_update(hz_t oldclk, hz_t newclk) override;
};

inline bool peripheral::is_little_endian() const {
    return endian == ENDIAN_LITTLE;
}

inline bool peripheral::is_big_endian() const {
    return endian == ENDIAN_BIG;
}

inline bool peripheral::is_host_endian() const {
    return endian == host_endian();
}

template <typename T>
inline T peripheral::to_host_endian(T val) const {
    return is_host_endian() ? val : bswap(val);
}

template <typename T>
inline T peripheral::from_host_endian(T val) const {
    return is_host_endian() ? val : bswap(val);
}

inline void peripheral::aligned_accesses_only(bool only) {
    for (auto& [_, regs] : m_registers)
        regs->aligned_accesses_only(only);
}

inline void peripheral::aligned_accesses_only(address_space as, bool only) {
    if (auto* regs = find_address_space(as))
        regs->aligned_accesses_only(only);
}

inline void peripheral::natural_accesses_only(bool only) {
    for (auto& [_, regs] : m_registers)
        regs->natural_accesses_only(only);
}

inline void peripheral::natural_accesses_only(address_space as, bool only) {
    if (auto* regs = find_address_space(as))
        regs->natural_accesses_only(only);
}

inline void peripheral::set_access_size(u64 min, u64 max) {
    for (auto [_, regs] : m_registers)
        regs->set_access_size(min, max);
}

inline void peripheral::set_access_size(address_space as, u64 min, u64 max) {
    if (auto* regs = find_address_space(as))
        regs->set_access_size(min, max);
}

inline vector<reg_base*> peripheral::get_registers() const {
    return get_registers(VCML_AS_DEFAULT);
}

} // namespace vcml

#endif
