/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_TIMERS_NRF51_H
#define VCML_TIMERS_NRF51_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

namespace vcml {
namespace timers {

class nrf51 : public peripheral
{
private:
    bool m_running;
    sc_time m_start;
    sc_event m_trigger;
    u32 m_inten;

    bool is_timer_mode() const { return m_running && mode == 0u; }
    bool is_counter_mode() const { return m_running && mode == 1u; }

    u32 time_to_ticks(const sc_time& t) const;
    sc_time ticks_to_time(u32 ticks) const;

    u32 counter_mask() const;
    u32 current_count() const;
    u32 next_deadline() const;

    void update();

    void write_start(u32 val);
    void write_stop(u32 val);
    void write_count(u32 val);
    void write_clear(u32 val);
    void write_shutdown(u32 val);
    void write_capture(u32 val, size_t idx);
    void write_compare(u32 val, size_t idx);
    void write_cc(u32 val, size_t idx);
    void write_shorts(u32 val);
    void write_intenset(u32 val);
    void write_intenclr(u32 val);

public:
    reg<u32> start;
    reg<u32> stop;
    reg<u32> count;
    reg<u32> clear;
    reg<u32> shutdown;
    reg<u32, 4> capture;
    reg<u32, 4> compare;
    reg<u32> shorts;
    reg<u32> intenset;
    reg<u32> intenclr;
    reg<u32> mode;
    reg<u32> bitmode;
    reg<u32> prescaler;
    reg<u32, 4> cc;

    tlm_target_socket in;
    gpio_initiator_socket irq;

    nrf51(const sc_module_name& nm);
    virtual ~nrf51();
    VCML_KIND(timers::nrf51);
    virtual void reset() override;
};

} // namespace timers
} // namespace vcml

#endif
