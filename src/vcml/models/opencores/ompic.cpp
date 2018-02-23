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

#define OMPIC_DATA(x) ((x) & 0xffff)
#define OMPIC_DEST(x) (((x) >> 16) & 0x3fff)

namespace vcml { namespace opencores {

    u32 ompic::read_STATUS(unsigned int core_id) {
        u32 val = m_status[core_id];
        if (IRQ[core_id].read())
            val |= 1 << 30;
        return val;
    }

    u32 ompic::read_CONTROL(unsigned int core_id) {
        return m_control[core_id];
    }

    u32 ompic::write_CONTROL(u32 val, unsigned int core_id)
    {
        u32 self = static_cast<uint32_t>(core_id);
        u32 dest = OMPIC_DEST(val);
        u32 data = OMPIC_DATA(val);

        m_control[core_id] = val;

        //log_debug("CTRL %d -> %d: 0x%08x", self, dest, val);

        if (val & CTRL_IRQ_GEN) {
            m_status[dest] = self << 16 | data;
            //log_debug("core %d triggers irq on core %d", self, dest);
            if (IRQ[dest].read())
                log_warning("irq already pending on core %d", dest);
            IRQ[dest] = true;
        }

        if (val & CTRL_IRQ_ACK) {
            //log_debug("reset irq for core %d", core_id);
            if (!IRQ[self].read())
                log_warning("no irq pending for core %d", core_id);
            IRQ[self] = false;
        }

        return val;
    }

    ompic::ompic(const sc_core::sc_module_name& nm, unsigned int num_cores):
        peripheral(nm),
        m_num_cores(num_cores),
        m_control(NULL),
        m_status(NULL),
        CONTROL(NULL),
        STATUS(NULL),
        IRQ("IRQ"),
        IN("IN") {
        VCML_ERROR_ON(num_cores == 0, "number of cores must not be zero");

        CONTROL = new reg<ompic, u32>*[m_num_cores];
        STATUS  = new reg<ompic, u32>*[m_num_cores];

        m_control = new uint32_t[m_num_cores]();
        m_status  = new uint32_t[m_num_cores]();

        stringstream ss;
        for (unsigned int core = 0; core < num_cores; core++) {
            ss.str("");
            ss << "CONTROL" << core;

            CONTROL[core] = new reg<ompic, u32>(ss.str().c_str(), core * 8);
            CONTROL[core]->allow_read_write();
            CONTROL[core]->tagged_read  = &ompic::read_CONTROL;
            CONTROL[core]->tagged_write = &ompic::write_CONTROL;
            CONTROL[core]->tag = core;

            ss.str("");
            ss << "STATUS" << core;

            STATUS[core] = new reg<ompic, u32>(ss.str().c_str(), core * 8 + 4);
            STATUS[core]->allow_read();
            STATUS[core]->tagged_read = &ompic::read_STATUS;
            STATUS[core]->tag = core;
        }
    }

    ompic::~ompic() {
        for (unsigned int core = 0; core < m_num_cores; core++) {
            delete CONTROL [core];
            delete STATUS  [core];
        }

        delete [] CONTROL;
        delete [] STATUS;
        delete [] m_control;
        delete [] m_status;
    }

}}
