/******************************************************************************
 *                                                                            *
 * Copyright (C) 2023 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_RISCV_ACLINT_H
#define VCML_RISCV_ACLINT_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace riscv {

class aclint : public peripheral
{
private:
    sc_time m_time_reset;
    sc_event m_trigger;

    u64 get_cycles() const;

    u64 read_mtime();
    void write_mtimecmp(u64 val, size_t hart);

    u32 read_msip(size_t hart);
    void write_msip(u32 val, size_t hart);

    u32 read_ssip(size_t hart);
    void write_ssip(u32 val, size_t hart);

    void update_timer();

    // disabled
    aclint();
    aclint(const aclint&);

public:
    enum aclint_address_space : address_space {
        ACLINT_AS_MTIMER = VCML_AS_DEFAULT,
        ACLINT_AS_MSWI,
        ACLINT_AS_SSWI,
    };

    property<u64> comp_base;
    property<u64> time_base;

    static constexpr size_t NHARTS = 4095;

    reg<u64, NHARTS> mtimecmp;
    reg<u64> mtime;

    reg<u32, NHARTS> msip;
    reg<u32, NHARTS> ssip;

    gpio_initiator_array irq_mtimer;
    gpio_initiator_array irq_mswi;
    gpio_initiator_array irq_sswi;

    tlm_target_socket mtimer;
    tlm_target_socket mswi;
    tlm_target_socket sswi;

    aclint(const sc_module_name& nm);
    virtual ~aclint();
    VCML_KIND(riscv::aclint);

    virtual void reset() override;
};

} // namespace riscv
} // namespace vcml

#endif
