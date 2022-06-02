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

#ifndef VCML_PROTOCOLS_TLM_EXMON_H
#define VCML_PROTOCOLS_TLM_EXMON_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

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
