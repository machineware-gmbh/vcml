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

#include "vcml/models/opencores/ompic.h"

#define OMPIC_DATA(x) ((x)&0xffff)
#define OMPIC_DEST(x) (((x) >> 16) & 0x3fff)

namespace vcml {
namespace opencores {

u32 ompic::read_status(size_t core_idx) {
    VCML_ERROR_ON(core_idx >= m_num_cores, "core_id >= num_cores");
    u32 val = m_status[core_idx];
    if (irq[core_idx].read())
        val |= CTRL_IRQ_GEN;
    return val;
}

u32 ompic::read_control(size_t core_idx) {
    VCML_ERROR_ON(core_idx >= m_num_cores, "core_id >= num_cores");
    return m_control[core_idx];
}

void ompic::write_control(u32 val, size_t core_idx) {
    VCML_ERROR_ON(core_idx >= m_num_cores, "core_id >= num_cores");

    u32 self = static_cast<uint32_t>(core_idx);
    u32 dest = OMPIC_DEST(val);
    u32 data = OMPIC_DATA(val);

    if (dest >= m_num_cores) {
        log_warn("illegal interrupt request ignored");
        log_warn(" core: cpu%d", self);
        log_warn(" dest: cpu%d", dest);
        log_warn(" data: 0x%04x", data);
        return;
    }

    m_control[core_idx] = val;
    if (val & CTRL_IRQ_GEN) {
        m_status[dest] = self << 16 | data;
        log_debug("cpu%d triggers interrupt on cpu%d (data: 0x%04x)", self,
                  dest, data);
        if (irq[dest].read())
            log_debug("interrupt already pending for cpu%d", dest);
        irq[dest] = true;
    }

    if (val & CTRL_IRQ_ACK) {
        log_debug("cpu%d acknowledges interrupt", self);
        if (!irq[self].read())
            log_debug("no pending interrupt for cpu%d", self);
        irq[self] = false;
    }
}

ompic::ompic(const sc_core::sc_module_name& nm, unsigned int num_cores):
    peripheral(nm),
    m_num_cores(num_cores),
    m_control(nullptr),
    m_status(nullptr),
    control(nullptr),
    status(nullptr),
    irq("irq"),
    in("in") {
    VCML_ERROR_ON(num_cores == 0, "number of cores must not be zero");

    control = new reg<u32>*[m_num_cores];
    status  = new reg<u32>*[m_num_cores];

    m_control = new uint32_t[m_num_cores]();
    m_status  = new uint32_t[m_num_cores]();

    for (unsigned int core = 0; core < num_cores; core++) {
        control[core] = new reg<u32>(mkstr("control%u", core), core * 8);
        control[core]->allow_read_write();
        control[core]->on_read(&ompic::read_control);
        control[core]->on_write(&ompic::write_control);
        control[core]->tag = core;

        status[core] = new reg<u32>(mkstr("status%u", core), core * 8 + 4);
        status[core]->allow_read_only();
        status[core]->on_read(&ompic::read_status);
        status[core]->tag = core;
    }
}

ompic::~ompic() {
    for (unsigned int core = 0; core < m_num_cores; core++) {
        delete control[core];
        delete status[core];
    }

    delete[] control;
    delete[] status;
    delete[] m_control;
    delete[] m_status;
}

} // namespace opencores
} // namespace vcml
