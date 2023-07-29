/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_RISCV_SIMDEV_H
#define VCML_RISCV_SIMDEV_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"

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
