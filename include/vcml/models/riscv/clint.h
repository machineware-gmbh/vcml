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

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace riscv {

class clint : public peripheral
{
private:
    sc_time m_time_reset;
    sc_event m_trigger;

    u64 get_cycles() const;

    u32 read_msip(size_t hart);
    void write_msip(u32 val, size_t hart);
    void write_mtimecmp(u64 val, size_t hart);
    u64 read_mtime();

    void update_timer();

    // disabled
    clint();
    clint(const clint&);

public:
    static const size_t NHARTS = 4095;

    reg<u32, NHARTS> msip;
    reg<u64, NHARTS> mtimecmp;
    reg<u64> mtime;

    gpio_initiator_socket_array<NHARTS> irq_sw;
    gpio_initiator_socket_array<NHARTS> irq_timer;

    tlm_target_socket in;

    clint(const sc_module_name& nm);
    virtual ~clint();
    VCML_KIND(riscv::clint);

    virtual void reset() override;
};

} // namespace riscv
} // namespace vcml

#endif
