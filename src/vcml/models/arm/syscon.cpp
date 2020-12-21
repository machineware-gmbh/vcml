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

#include "vcml/models/arm/syscon.h"

namespace vcml { namespace arm {

    syscon::syscon(const sc_module_name& nm):
        peripheral(nm),
        SYS_ID      ("SYS_ID",      0x00),
        LOCKVAL     ("LOCKVAL",     0x20),
        CFGDATA0    ("CFGDATA0",    0x24),
        CFGDATA1    ("CFGDATA1",    0x28),
        FLAGS_S     ("FLAGS_S",     0x30),
        FLAGS_C     ("FLAGS_C",     0x34),
        NVFLAGS_S   ("NVFLAGS_S",   0x38),
        NVFLAGS_C   ("NVFLAGS_C",   0x3c),
        CLCK24      ("CLCK24",      0x5c),
        PROC_ID     ("PROC_ID",     0x84),
        SYS_CFGDATA ("SYS_CFGDATA", 0xa0),
        SYS_CFGCTRL ("SYS_CFGCTRL", 0xa4),
        SYS_CFGSTAT ("SYS_CFGSTAT", 0xa8),
        IN("IN") {
        SYS_ID.allow_read_only();

        LOCKVAL.allow_read_write();
        LOCKVAL.write = &syscon::write_LOCKVAL;

        CFGDATA0.allow_read_write();
        CFGDATA1.allow_read_write();

        FLAGS_S.allow_read_write();
        FLAGS_S.write = &syscon::write_FLAGS_S;
        FLAGS_C.allow_read_write();
        FLAGS_C.write = &syscon::write_FLAGS_C;

        NVFLAGS_S.allow_read_write();
        NVFLAGS_S.write = &syscon::write_NVFLAGS_S;
        NVFLAGS_C.allow_read_write();
        NVFLAGS_C.write = &syscon::write_NVFLAGS_C;

        CLCK24.allow_read_only();
        CLCK24.read = &syscon::read_CLCK24;

        PROC_ID.allow_read_write();

        SYS_CFGDATA.allow_read_write();

        SYS_CFGCTRL.allow_read_write();
        SYS_CFGCTRL.write = &syscon::write_SYS_CFGCTRL;

        SYS_CFGSTAT.allow_read_write();
        SYS_CFGSTAT.write = &syscon::write_SYS_CFGSTAT;
    }

    syscon::~syscon() {
        // nothing to do
    }

    u32 syscon::read_CLCK24() {
        return sc_time_stamp().to_seconds() * VCML_ARM_SYSCON_CLOCK24MHZ;
    }

    u16 syscon::write_LOCKVAL(u16 val) {
        if (val != LOCKVAL_LOCK)
            val &= LOCKVAL_M;
        return val;
    }

    u32 syscon::write_FLAGS_S(u32 val) {
        FLAGS_S |= val;
        FLAGS_C |= val;
        return FLAGS_S;
    }

    u32 syscon::write_FLAGS_C(u32 val) {
        FLAGS_S &= ~val;
        FLAGS_C &= ~val;
        return FLAGS_C;
    }

    u32 syscon::write_NVFLAGS_S(u32 val) {
        NVFLAGS_S |= val;
        NVFLAGS_C |= val;
        return NVFLAGS_S;
    }

    u32 syscon::write_NVFLAGS_C(u32 val) {
        NVFLAGS_S &= ~val;
        NVFLAGS_C &= ~val;
        return NVFLAGS_C;
    }

    u32 syscon::write_SYS_CFGCTRL(u32 val) {
        if (val & (1 << 31))
            log_warning("SYS_CFGCTRL write trigger action not implemented");
        return val & ~((3 << 18) | (1 << 31));
    }

    u32 syscon::write_SYS_CFGSTAT(u32 val) {
        return val & 0x3;
    }

}}
