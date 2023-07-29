/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_RISCV_CLINT_H
#define VCML_RISCV_CLINT_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

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

    gpio_initiator_array irq_sw;
    gpio_initiator_array irq_timer;

    tlm_target_socket in;

    clint(const sc_module_name& nm);
    virtual ~clint();
    VCML_KIND(riscv::clint);

    virtual void reset() override;
};

} // namespace riscv
} // namespace vcml

#endif
