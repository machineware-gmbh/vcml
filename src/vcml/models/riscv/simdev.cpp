/******************************************************************************
 *                                                                            *
 * Copyright 2020 Jan Henrik Weinstock                                        *
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

#include "vcml/models/riscv/simdev.h"

namespace vcml { namespace riscv {

    u32 simdev::write_FINISH(u32 val) {
        u32 status = val & 0xffff;
        u32 code = val >> 16;

        switch (status) {
        case FINISH_FAIL:
            log_info("simulation abort requested by cpu %d", current_cpu());
            exit(code);
            break;

        case FINISH_PASS:
            log_info("simulation exit requested by cpu %d", current_cpu());
            sc_core::sc_stop();
            break;

        case FINISH_RESET:
            log_info("simulation reset requested by cpu %d", current_cpu());
            sc_core::sc_stop();
            break;

        default:
            log_warn("illegal exit request 0x%08x", val);
            break;
        }

        return val;
    }

    simdev::simdev(const sc_module_name& nm):
        peripheral(nm),
        FINISH("FINISH", 0x0, 0),
        IN("IN") {
        FINISH.sync_always();
        FINISH.allow_read_write();
        FINISH.on_write(&simdev::write_FINISH);
    }

    simdev::~simdev() {
        // nothing to do
    }

}}
