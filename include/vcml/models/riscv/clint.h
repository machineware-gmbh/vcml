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

#ifndef VCML_RISCV_CLINT_H
#define VCML_RISCV_CLINT_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/irq.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

namespace vcml { namespace riscv {

    class clint: public peripheral
    {
    private:
        sc_time m_time_reset;
        sc_event m_trigger;

        u64 get_cycles() const;

        u32 read_MSIP(unsigned int hart);
        u32 write_MSIP(u32 val, unsigned int hart);
        u64 write_MTIMECMP(u64 val, unsigned int hart);
        u64 read_MTIME();

        void update_timer();

        // disabled
        clint();
        clint(const clint&);

    public:
        static const int NHARTS = 4095;

        reg<clint, u32, NHARTS> MSIP;
        reg<clint, u64, NHARTS> MTIMECMP;
        reg<clint, u64> MTIME;

        irq_initiator_socket_array<NHARTS> IRQ_SW;
        irq_initiator_socket_array<NHARTS> IRQ_TIMER;

        tlm_target_socket IN;

        clint(const sc_module_name& nm);
        virtual ~clint();

        SC_HAS_PROCESS(clint);
        VCML_KIND(clint);

        virtual void reset() override;
    };

}}

#endif
