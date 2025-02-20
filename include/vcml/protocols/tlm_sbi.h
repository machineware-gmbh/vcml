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

enum sbi_defaults : u64 {
    SBI_CPUID_DEFAULT = 0,
    SBI_PRIVILEGE_NONE = 0,
    SBI_ASID_GLOBAL = ~0ull,
    SBI_ATYPE_UX = 0,
    SBI_ATYPE_RQ = 1,
    SBI_ATYPE_TX = 2,
};

//
// tlm_sbi
//
// Sideband information for TLM generic payloads, normally attached through the
// tlm optional extension mechanism using vcml::tlm_sbi_ext
//
// tlm_sbi.is_debug (bool, target-read-only, default off)
//   request originates from a debugger and must not have any side effects,
//   e.g. must not call wait() or notify()
//
// tlm_sbi.is_nodmi (bool, target-read-only, default off)
//   set by the initiator to indicate that the target must not use DMI pointers
//   to fullfull the request; the corresponding transaction should be forwarded
//   to its next target using b_transport.
//
// tlm_sbi.is_sync (bool, target-read-only, default off)
//   set by the initiator to request that the transaction should be executed
//   synchronously and not ahead of simulation time (e.g. sc_time in
//   b_transport should be SC_ZERO_TIME)
//
// tlm_sbi.is_insn (bool, target-read-only, default off)
//   set by the initiatiro to indicate if a request is being used by for
//   instruction execution execution, e.g. a instruction fetch or prefetch by a
//   processor using a transaction read command
//
// tlm_sbi.is_excl (bool, target-read-write, default off)
//   set by initiator to indicate exclusive load (if corresponding transaction
//   is a read command) or an exclusive store (if it is a write command);
//   cleared by the target if the exclusive store operation failed, e.g. due to
//   a missing exclusive monitor lock.
//
// tlm_sbi.is_lock (bool, target-read-only, default off)
//   set by the initiator to indicate a locked bus transaction; busses must
//   block any requests from other initiators until they see another
//   transaction from this initiator with is_lock set to false.
//
// tlm_sbi.is_secure (bool, target-read-only, default off)
//   set by the initiator to indicate that this request originates from a
//   secure context
//
// tlm_sbi.atype (enum, target-read-only, default 0:SBI_ATYPE_UX)
//   set by the initiator to indicate the type of the address in the associated
//   TLM generic payload for use with IOMMUs; possible values are:
//   - SBI_ATYPE_UX: regular untranslated address
//   - SBI_ATYPE_RQ: request to return a translated address
//   - SBI_ATYPE_TX: pretranslated address, do not translate
//
// tlm_sbi.cpuid (integer, target-read-only, default 0)
//   set by the initiator to its own unique identfication number.
//
// tlm_sbi.privilege (integer, target-read-only, default 0)
//   set by the initiator to indicate the privilege level it is operating at; a
//   default privilege level of 0 means "no privilege" (e.g. EL0 in ARM and
//   U-Mode in RISCV)
//
// tlm_sbi.asid (integer, target-read-only, default -1)
//   set by the initiator to indicate the application space ID of the address
//   in the associated TLM generic payload, e.g. for SR-IOV:
//   - for processors, the ASID may refer to the currently active process
//   - for devices the ASID may refer to the currently active virtual function

struct tlm_sbi {
    u64 is_debug : 1;
    u64 is_nodmi : 1;
    u64 is_sync : 1;
    u64 is_insn : 1;
    u64 is_excl : 1;
    u64 is_lock : 1;
    u64 is_secure : 1;
    u64 atype : 2;

    u64 cpuid;
    u64 privilege;
    u64 asid;

    tlm_sbi(const tlm_sbi&) = default;
    tlm_sbi(tlm_sbi&&) = default;

    tlm_sbi();
    tlm_sbi(bool debug, bool nodmi, bool sync, bool insn, bool excl, bool lock,
            bool secure, u64 atype = SBI_ATYPE_UX, u64 cpu = 0,
            u64 privilege = SBI_PRIVILEGE_NONE, u64 asid = SBI_ASID_GLOBAL);

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
           is_secure == other.is_secure && atype == other.atype &&
           cpuid == other.cpuid && privilege == other.privilege &&
           asid == other.asid;
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
extern const tlm_sbi SBI_TRANSLATED;
extern const tlm_sbi SBI_TR_REQ;

inline tlm_sbi sbi_cpuid(u64 cpu) {
    return tlm_sbi(0, 0, 0, 0, 0, 0, 0, 0, cpu);
}

inline tlm_sbi sbi_privilege(u64 lvl) {
    return tlm_sbi(0, 0, 0, 0, 0, 0, 0, 0, 0, lvl);
}

inline tlm_sbi sbi_asid(u64 id) {
    return tlm_sbi(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, id);
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

inline bool tx_is_translated(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).atype == SBI_ATYPE_TX;
}

inline bool tx_is_tr_req(const tlm_generic_payload& tx) {
    return tx_get_sbi(tx).atype == SBI_ATYPE_RQ;
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
