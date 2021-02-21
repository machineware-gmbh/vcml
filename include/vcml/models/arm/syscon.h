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

#ifndef VCML_ARM_SYSCON_H
#define VCML_ARM_SYSCON_H

#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/component.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

#define VCML_ARM_SYSCON_CLOCK24MHZ (24000000)

namespace vcml { namespace arm {

    class syscon: public peripheral
    {
    private:
        u32 read_CLCK24();

        u16 write_LOCKVAL(u16 val);
        u32 write_FLAGS_S(u32 val);
        u32 write_FLAGS_C(u32 val);
        u32 write_NVFLAGS_S(u32 val);
        u32 write_NVFLAGS_C(u32 val);
        u32 write_SYS_CFGCTRL(u32 val);
        u32 write_SYS_CFGSTAT(u32 val);

        // disabled
        syscon();
        syscon(const syscon&);

    public:
        enum sysid_bits {
            SYSID_VERSATILE = 0x41007004,
            SYSID_VEXPRESS  = 0x1190f500
        };

        enum procid_bits {
            PROCID_VERSATILE = 0x02000000,
            PROCID_VEXPRESS  = 0x0c000191
        };

        enum lockval_bits {
            LOCKVAL_LOCK = 0xa05f,
            LOCKVAL_M    = 0x7fff
        };

        reg<syscon, u32> SYS_ID;
        reg<syscon, u16> LOCKVAL;
        reg<syscon, u32> CFGDATA0;
        reg<syscon, u32> CFGDATA1;
        reg<syscon, u32> FLAGS_S;
        reg<syscon, u32> FLAGS_C;
        reg<syscon, u32> NVFLAGS_S;
        reg<syscon, u32> NVFLAGS_C;
        reg<syscon, u32> CLCK24;
        reg<syscon, u32> PROC_ID;
        reg<syscon, u32> SYS_CFGDATA;
        reg<syscon, u32> SYS_CFGCTRL;
        reg<syscon, u32> SYS_CFGSTAT;

        slave_socket IN;

        syscon(const sc_module_name& name);
        virtual ~syscon();
        VCML_KIND(arm::syscon);

        void setup_versatile();
        void setup_vexpress();
    };

    inline void syscon::setup_versatile() {
        SYS_ID.set_default(SYSID_VERSATILE);
        PROC_ID.set_default(PROCID_VERSATILE);
    }

    inline void syscon::setup_vexpress() {
        SYS_ID.set_default(SYSID_VEXPRESS);
        PROC_ID.set_default(PROCID_VEXPRESS);
    }

}}

#endif
