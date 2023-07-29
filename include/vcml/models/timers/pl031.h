/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TIMERS_PL031_H
#define VCML_TIMERS_PL031_H

#include "vcml/core/types.h"
#include "vcml/core/range.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace timers {

class pl031 : public peripheral
{
private:
    u32 m_offset;
    sc_event m_notify;

    u32 read_dr();

    void write_mr(u32 val);
    void write_lr(u32 val);
    void write_dr(u32 val);
    void write_cr(u32 val);
    void write_imsc(u32 val);
    void write_icr(u32 val);

    void update();

public:
    enum amba_ids : u32 {
        AMBA_PID = 0x00141031, // Peripheral ID
        AMBA_CID = 0xb105f00d, // PrimeCell ID
    };

    reg<u32> dr;
    reg<u32> mr;
    reg<u32> lr;
    reg<u32> cr;
    reg<u32> imsc;
    reg<u32> ris;
    reg<u32> mis;
    reg<u32> icr;

    reg<u32, 4> pid;
    reg<u32, 4> cid;

    tlm_target_socket in;
    gpio_initiator_socket irq;

    pl031(const sc_module_name& nm);
    virtual ~pl031();
    VCML_KIND(timers::pl031);
    virtual void reset() override;
};

} // namespace timers
} // namespace vcml

#endif
