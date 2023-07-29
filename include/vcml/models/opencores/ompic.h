/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_OPENCORES_OMPIC_H
#define VCML_OPENCORES_OMPIC_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace opencores {

class ompic : public peripheral
{
private:
    unsigned int m_num_cores;

    u32* m_control;
    u32* m_status;

    u32 read_status(size_t core_idx);
    u32 read_control(size_t core_idx);

    void write_control(u32 val, size_t core_idx);

    // Disabled
    ompic();
    ompic(const ompic&);

public:
    enum control_bits { CTRL_IRQ_GEN = 1 << 30, CTRL_IRQ_ACK = 1 << 31 };

    reg<u32>** control;
    reg<u32>** status;

    gpio_initiator_array irq;
    tlm_target_socket in;

    ompic(const sc_module_name& name, unsigned int num_cores);
    virtual ~ompic();
    VCML_KIND(opencores::ompic);
};

} // namespace opencores
} // namespace vcml

#endif
