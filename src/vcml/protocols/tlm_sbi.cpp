/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#include "vcml/protocols/tlm_sbi.h"

namespace vcml {

tlm_sbi::tlm_sbi():
    is_debug(false),
    is_nodmi(false),
    is_sync(false),
    is_insn(false),
    is_excl(false),
    is_lock(false),
    is_secure(false),
    atype(SBI_ATYPE_UX),
    cpuid(0),
    privilege(0),
    asid(0) {
    // nothing to do
}

tlm_sbi::tlm_sbi(bool debug, bool nodmi, bool sync, bool insn, bool excl,
                 bool lock, bool secure, u64 addrt, u64 cpu, u64 lvl, u64 id):
    is_debug(debug),
    is_nodmi(nodmi),
    is_sync(sync),
    is_insn(insn),
    is_excl(excl),
    is_lock(lock),
    is_secure(secure),
    atype(addrt),
    cpuid(cpu),
    privilege(lvl),
    asid(id) {
    // nothing to do
}

void tlm_sbi::copy(const tlm_sbi& other) {
    is_debug = other.is_debug;
    is_nodmi = other.is_nodmi;
    is_sync = other.is_sync;
    is_insn = other.is_insn;
    is_excl = other.is_excl;
    is_lock = other.is_lock;
    is_secure = other.is_secure;
    atype = other.atype;
    cpuid = other.cpuid;
    privilege = other.privilege;
    asid = other.asid;
}

tlm_sbi& tlm_sbi::operator=(const tlm_sbi& other) {
    copy(other);
    return *this;
}

static void sbi_copy_integer_props(tlm_sbi& dest, const tlm_sbi& src) {
    if (dest.cpuid == SBI_CPUID_DEFAULT)
        dest.cpuid = src.cpuid;
    else if (src.cpuid != SBI_CPUID_DEFAULT && src.cpuid != dest.cpuid)
        VCML_ERROR("multiple initializations given for sbi.cpuid");

    if (dest.privilege == SBI_PRIVILEGE_NONE)
        dest.privilege = src.privilege;
    else if (src.privilege != SBI_PRIVILEGE_NONE &&
             src.privilege != dest.privilege) {
        VCML_ERROR("multiple initializations given for sbi.privilege");
    }

    if (dest.asid == SBI_ASID_GLOBAL)
        dest.asid = src.asid;
    else if (src.asid != SBI_ASID_GLOBAL && src.asid != dest.asid)
        VCML_ERROR("multiple initializations given for sbi.asid");

    if (dest.atype == SBI_ATYPE_UX)
        dest.atype = src.atype;
    else if (dest.atype != SBI_ATYPE_UX && dest.atype != src.atype)
        VCML_ERROR("multiple initializations given for sbi.atype");
}

tlm_sbi& tlm_sbi::operator&=(const tlm_sbi& other) {
    is_debug &= other.is_debug;
    is_nodmi &= other.is_nodmi;
    is_sync &= other.is_sync;
    is_insn &= other.is_insn;
    is_excl &= other.is_excl;
    is_lock &= other.is_lock;
    is_secure &= other.is_secure;
    sbi_copy_integer_props(*this, other);
    return *this;
}

tlm_sbi& tlm_sbi::operator|=(const tlm_sbi& other) {
    is_debug |= other.is_debug;
    is_nodmi |= other.is_nodmi;
    is_sync |= other.is_sync;
    is_insn |= other.is_insn;
    is_excl |= other.is_excl;
    is_lock |= other.is_lock;
    is_secure |= other.is_secure;
    sbi_copy_integer_props(*this, other);
    return *this;
}

tlm_sbi& tlm_sbi::operator^=(const tlm_sbi& other) {
    is_debug ^= other.is_debug;
    is_nodmi ^= other.is_nodmi;
    is_sync ^= other.is_sync;
    is_insn ^= other.is_insn;
    is_excl ^= other.is_excl;
    is_lock ^= other.is_lock;
    is_secure ^= other.is_secure;
    sbi_copy_integer_props(*this, other);
    return *this;
}

const tlm_sbi SBI_NONE = { 0, 0, 0, 0, 0, 0, 0 };
const tlm_sbi SBI_DEBUG = { 1, 0, 0, 0, 0, 0, 0 };
const tlm_sbi SBI_NODMI = { 0, 1, 0, 0, 0, 0, 0 };
const tlm_sbi SBI_SYNC = { 0, 0, 1, 0, 0, 0, 0 };
const tlm_sbi SBI_INSN = { 0, 0, 0, 1, 0, 0, 0 };
const tlm_sbi SBI_EXCL = { 0, 0, 0, 0, 1, 0, 0 };
const tlm_sbi SBI_LOCK = { 0, 0, 0, 0, 0, 1, 0 };
const tlm_sbi SBI_SECURE = { 0, 0, 0, 0, 0, 0, 1 };
const tlm_sbi SBI_TRANSLATED = { 0, 0, 0, 0, 0, 0, 0, SBI_ATYPE_TX };
const tlm_sbi SBI_TR_REQ = { 0, 0, 0, 0, 0, 0, 0, SBI_ATYPE_RQ };

tlm_extension_base* sbiext::clone() const {
    return new sbiext(*this);
}

void sbiext::copy_from(const tlm_extension_base& ext) {
    VCML_ERROR_ON(typeid(this) != typeid(ext), "cannot copy extension");
    const sbiext& other = (const sbiext&)ext;
    copy(other);
}

void tx_set_sbi(tlm_generic_payload& tx, const tlm_sbi& info) {
    if (!tx_has_sbi(tx) && info == SBI_NONE)
        return;
    if (!tx_has_sbi(tx))
        tx.set_extension<sbiext>(new sbiext());
    sbiext* ext = tx.get_extension<sbiext>();
    ext->copy(info);
}

} // namespace vcml
