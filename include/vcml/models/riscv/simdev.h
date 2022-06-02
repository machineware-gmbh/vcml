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

#ifndef VCML_RISCV_SIMDEV_H
#define VCML_RISCV_SIMDEV_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/tlm.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

namespace vcml {
namespace riscv {

class simdev : public peripheral
{
private:
    void write_finish(u32 val);

public:
    enum : u32 {
        FINISH_FAIL = 0x3333,
        FINISH_PASS = 0x5555,
        FINISH_RESET = 0x7777,
    };

    reg<u32> finish;

    tlm_target_socket in;

    simdev() = delete;
    simdev(const simdev&) = delete;

    simdev(const sc_module_name& nm);
    virtual ~simdev();
    VCML_KIND(sifive::simdev);
};

} // namespace riscv
} // namespace vcml

#endif
