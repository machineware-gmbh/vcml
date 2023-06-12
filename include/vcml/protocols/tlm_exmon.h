/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_PROTOCOLS_TLM_EXMON_H
#define VCML_PROTOCOLS_TLM_EXMON_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"

#include "vcml/protocols/tlm_sbi.h"
#include "vcml/protocols/tlm_dmi_cache.h"

namespace vcml {

struct exlock {
    int cpu;
    range addr;
};

class tlm_exmon
{
private:
    vector<exlock> m_locks;

public:
    const vector<exlock> get_locks() const { return m_locks; }

    tlm_exmon() = default;
    virtual ~tlm_exmon() = default;

    bool has_lock(int cpu, const range& r) const;
    bool add_lock(int cpu, const range& r);

    void break_locks(int cpu);
    void break_locks(const range& r);

    bool update(tlm_generic_payload& tx);

    bool override_dmi(const tlm_generic_payload& tx, tlm_dmi& dmi);
};

} // namespace vcml

#endif
