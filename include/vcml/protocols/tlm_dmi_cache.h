/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_DMI_CACHE_H
#define VCML_PROTOCOLS_DMI_CACHE_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"

namespace vcml {

class tlm_dmi_cache
{
private:
    mutable mutex m_mtx;

    size_t m_limit;
    vector<tlm_dmi> m_entries;

    void insert_locked(const tlm_dmi& dmi);

public:
    size_t get_entry_limit() const { return m_limit; }
    void set_entry_limit(size_t lim) { m_limit = lim; }

    vector<tlm_dmi> get_entries() { return m_entries; }
    const vector<tlm_dmi>& get_entries() const { return m_entries; }

    tlm_dmi_cache();
    virtual ~tlm_dmi_cache();

    void insert(const tlm_dmi& dmi);

    bool invalidate(u64 start, u64 end);
    bool invalidate(const range& r);

    bool lookup(const range& r, vcml_access rwx, tlm_dmi& dmi);
    bool lookup(const range& addr, tlm_command c, tlm_dmi& dmi);
    bool lookup(u64 addr, u64 size, tlm_command c, tlm_dmi& dmi);
    bool lookup(const tlm_generic_payload& tx, tlm_dmi& dmi);
};

inline bool tlm_dmi_cache::lookup(const range& addr, tlm_command command,
                                  tlm_dmi& dmi) {
    return lookup(addr, tlm_command_to_access(command), dmi);
}

inline bool tlm_dmi_cache::lookup(u64 addr, u64 size, tlm_command command,
                                  tlm_dmi& dmi) {
    return lookup({ addr, addr + size - 1 }, command, dmi);
}

inline bool tlm_dmi_cache::lookup(const tlm_generic_payload& tx,
                                  tlm_dmi& dmi) {
    return lookup(tx, tx.get_command(), dmi);
}

inline void dmi_set_access(tlm_dmi& dmi, vcml_access a) {
    switch (a) {
    case VCML_ACCESS_READ:
        dmi.allow_read();
        break;
    case VCML_ACCESS_WRITE:
        dmi.allow_write();
        break;
    case VCML_ACCESS_READ_WRITE:
        dmi.allow_read_write();
        break;
    default:
        dmi.allow_none();
    }
}

inline bool dmi_check_access(const tlm_dmi& dmi, vcml_access acs) {
    switch (acs) {
    case VCML_ACCESS_READ:
        return dmi.is_read_allowed();
    case VCML_ACCESS_WRITE:
        return dmi.is_write_allowed();
    case VCML_ACCESS_READ_WRITE:
        return dmi.is_read_write_allowed();
    case VCML_ACCESS_NONE:
        return true;
    default:
        VCML_ERROR("illegal access mode: %d", acs);
    }
}

inline u64 dmi_get_size(const tlm_dmi& dmi) {
    return dmi.get_end_address() - dmi.get_start_address() + 1;
}

inline unsigned char* dmi_get_ptr(const tlm_dmi& dmi, u64 addr) {
    return dmi.get_dmi_ptr() + addr - dmi.get_start_address();
}

inline void dmi_set_start_address(tlm_dmi& dmi, u64 addr) {
    dmi.set_dmi_ptr(dmi_get_ptr(dmi, addr));
    dmi.set_start_address(addr);
}

} // namespace vcml

#endif
