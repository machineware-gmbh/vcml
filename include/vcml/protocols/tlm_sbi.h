/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_SBI_H
#define VCML_PROTOCOLS_TLM_SBI_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"

namespace vcml {

struct tlm_sbi {
    bool is_debug;
    bool is_nodmi;
    bool is_sync;
    bool is_insn;
    bool is_excl;
    bool is_lock;
    bool is_secure;

    u64 cpuid;
    u64 privilege;
    u64 asid;

    tlm_sbi(const tlm_sbi&) = default;
    tlm_sbi(tlm_sbi&&) = default;

    tlm_sbi();
    tlm_sbi(bool debug, bool nodmi, bool sync, bool insn, bool excl, bool lock,
            bool secure, u64 cpu = 0, u64 privilege = 0, u64 asid = 0);

    void copy(const tlm_sbi& other);

    tlm_sbi& operator=(const tlm_sbi& other);
    tlm_sbi& operator&=(const tlm_sbi& other);
    tlm_sbi& operator|=(const tlm_sbi& other);
    tlm_sbi& operator^=(const tlm_sbi& other);

    tlm_sbi operator&(const tlm_sbi& other) const;
    tlm_sbi operator|(const tlm_sbi& other) const;
    tlm_sbi operator^(const tlm_sbi& other) const;

    bool operator==(const tlm_sbi& other) const;
    bool operator!=(const tlm_sbi& other) const;
};

inline tlm_sbi tlm_sbi::operator&(const tlm_sbi& other) const {
    tlm_sbi result(*this);
    return result &= other;
}

inline tlm_sbi tlm_sbi::operator|(const tlm_sbi& other) const {
    tlm_sbi result(*this);
    return result |= other;
}

inline bool tlm_sbi::operator==(const tlm_sbi& other) const {
    return is_debug == other.is_debug && is_nodmi == other.is_nodmi &&
           is_sync == other.is_sync && is_insn == other.is_insn &&
           is_excl == other.is_excl && is_lock == other.is_lock &&
           is_secure == other.is_secure && cpuid == other.cpuid &&
           privilege == other.privilege && asid == other.asid;
}

inline bool tlm_sbi::operator!=(const tlm_sbi& other) const {
    return !operator==(other);
}

extern const tlm_sbi SBI_NONE;
extern const tlm_sbi SBI_DEBUG;
extern const tlm_sbi SBI_NODMI;
extern const tlm_sbi SBI_SYNC;
extern const tlm_sbi SBI_INSN;
extern const tlm_sbi SBI_EXCL;
extern const tlm_sbi SBI_LOCK;
extern const tlm_sbi SBI_SECURE;

inline tlm_sbi sbi_cpuid(u64 cpu) {
    return tlm_sbi(false, false, false, false, false, false, false, cpu, 0, 0);
}

inline tlm_sbi sbi_privilege(u64 lvl) {
    return tlm_sbi(false, false, false, false, false, false, false, 0, lvl, 0);
}

inline tlm_sbi sbi_asid(u64 id) {
    return tlm_sbi(false, false, false, false, false, false, false, 0, 0, id);
}

inline tlm_sbi sbi_security(bool sec) {
    return tlm_sbi(false, false, false, false, false, false, sec, 0, 0, 0);
}

class sbiext : public tlm_extension<sbiext>, public tlm_sbi
{
public:
    sbiext() = default;

    virtual tlm_extension_base* clone() const override;
    virtual void copy_from(const tlm_extension_base& ext) override;
};

inline bool tx_has_sbi(const tlm_generic_payload& tx) {
    return tx.get_extension<sbiext>() != nullptr;
}

inline const tlm_sbi& tx_get_sbi(const tlm_generic_payload& tx) {
    return tx_has_sbi(tx) ? *tx.get_extension<sbiext>() : SBI_NONE;
}

inline bool tx_is_debug(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).is_debug;
}

inline bool tx_is_nodmi(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).is_nodmi;
}

inline bool tx_is_sync(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).is_sync;
}

inline bool tx_is_insn(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).is_insn;
}

inline bool tx_is_excl(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).is_excl;
}

inline bool tx_is_lock(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).is_lock;
}

inline bool tx_is_secure(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).is_secure;
}

inline u64 tx_cpuid(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).cpuid;
}

inline u64 tx_privilege(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).privilege;
}

inline u64 tx_asid(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).asid;
}

void tx_set_sbi(tlm_generic_payload& tx, const tlm_sbi& info);

inline void tx_set_cpuid(tlm_generic_payload& tx, u64 id) {
    tx_set_sbi(tx, sbi_cpuid(id));
}

inline void tx_set_privilege(tlm_generic_payload& tx, u64 lvl) {
    tx_set_sbi(tx, sbi_privilege(lvl));
}

inline void tx_set_asid(tlm_generic_payload& tx, u64 asid) {
    tx_set_sbi(tx, sbi_asid(asid));
}

} // namespace vcml

#endif
