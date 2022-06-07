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

#ifndef VCML_ARM_SP804TIMER_H
#define VCML_ARM_SP804TIMER_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"

#include "vcml/ports.h"
#include "vcml/peripheral.h"

namespace vcml {
namespace arm {

class sp804timer : public peripheral
{
private:
    void update_irqc();

    // disabled
    sp804timer();
    sp804timer(const sp804timer&);

public:
    enum amba_ids : u32 {
        AMBA_PID = 0x00141804, // Peripheral ID
        AMBA_CID = 0xb105f00d, // PrimeCell ID
    };

    class timer : public peripheral
    {
    private:
        sc_event m_ev;
        sc_time m_prev;
        sc_time m_next;
        sp804timer* m_timer;

        void trigger();
        void schedule(u32 ticks);

        u32 read_value();
        u32 read_ris();
        u32 read_mis();

        void write_load(u32 val);
        void write_control(u32 val);
        void write_intclr(u32 val);
        void write_bgload(u32 val);

    public:
        enum control_bits : u32 {
            CONTROL_ONESHOT = 1 << 0,
            CONTROL_32BIT = 1 << 1,
            CONTROL_IRQEN = 1 << 5,
            CONTROL_PERIOD = 1 << 6,
            CONTROL_ENABLED = 1 << 7,
            CONTROL_M = 0xff,
        };

        enum control_prescale_bits : u32 {
            CTLR_PRESCALE_O = 2,
            CTLR_PRESCALE_M = 3,
        };

        reg<u32> load;    // Load register
        reg<u32> value;   // Current Value register
        reg<u32> control; // Timer Control register
        reg<u32> intclr;  // Interrupt Clear register
        reg<u32> ris;     // Raw Interrupt Status register
        reg<u32> mis;     // Masked Interrupt Status register
        reg<u32> bgload;  // Background Load register

        gpio_initiator_socket irq;

        bool is_enabled() const { return control & CONTROL_ENABLED; }
        bool is_irq_enabled() const { return control & CONTROL_IRQEN; }
        bool is_32bit() const { return control & CONTROL_32BIT; }
        bool is_periodic() const { return control & CONTROL_PERIOD; }
        bool is_oneshot() const { return control & CONTROL_ONESHOT; }

        int get_prescale_stages() const;
        int get_prescale_divider() const;

        timer(const sc_module_name& nm);
        virtual ~timer();
        VCML_KIND(arm::sp804timer::timer);

        virtual void reset() override;
    };

    enum timer_address : u64 {
        TIMER1_START = 0x00,
        TIMER1_END = 0x1f,
        TIMER2_START = 0x20,
        TIMER2_END = 0x3f,
    };

    timer timer1;
    timer timer2;

    reg<u32> itcr; // Integration Test Control Register
    reg<u32> itop; // Integration Test OutPut set register

    reg<u32, 4> pid; // Peripheral ID Register
    reg<u32, 4> cid; // Cell ID Register

    tlm_target_socket in;

    gpio_base_initiator_socket irq1;
    gpio_base_initiator_socket irq2;
    gpio_initiator_socket irqc;

    sp804timer(const sc_module_name& nm);
    virtual ~sp804timer();
    VCML_KIND(arm::sp804timer);

    virtual unsigned int receive(tlm_generic_payload& tx, const tlm_sbi& info,
                                 address_space as) override;

    virtual void reset() override;
};

inline int sp804timer::timer::get_prescale_stages() const {
    return ((control >> CTLR_PRESCALE_O) & CTLR_PRESCALE_M) << 2;
}

inline int sp804timer::timer::get_prescale_divider() const {
    return 1 << get_prescale_stages();
}

} // namespace arm
} // namespace vcml

#endif
