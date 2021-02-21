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

#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/component.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

namespace vcml { namespace arm {

    class sp804timer: public peripheral
    {
    private:
        SC_HAS_PROCESS(sp804timer);
        void update_IRQC();

        // disabled
        sp804timer();
        sp804timer(const sp804timer&);

    public:
        const u32 SP804TIMER_PID = 0x00141804; // Peripheral ID
        const u32 SP804TIMER_CID = 0xB105F00D; // PrimeCell ID

        class timer: public peripheral
        {
        private:
            SC_HAS_PROCESS(timer);

            sc_event    m_ev;
            sc_time     m_prev;
            sc_time     m_next;

            void trigger();
            void schedule(u32 ticks);

            u32 read_VALUE();
            u32 read_RIS();
            u32 read_MIS();

            u32 write_LOAD(u32 val);
            u32 write_CONTROL(u32 val);
            u32 write_INTCLR(u32 val);
            u32 write_BGLOAD(u32 val);

        public:
            enum control_bits: u32 {
                CONTROL_ONESHOT = 1 << 0,
                CONTROL_32BIT   = 1 << 1,
                CONTROL_IRQEN   = 1 << 5,
                CONTROL_PERIOD  = 1 << 6,
                CONTROL_ENABLED = 1 << 7,
                CONTROL_M       = 0xFF,
            };

            enum control_prescale_bits: u32 {
                CTLR_PRESCALE_O = 2,
                CTLR_PRESCALE_M = 3,
            };

            reg<timer, u32> LOAD;    // Load register
            reg<timer, u32> VALUE;   // Current Value register
            reg<timer, u32> CONTROL; // Timer Control register
            reg<timer, u32> INTCLR;  // Interrupt Clear register
            reg<timer, u32> RIS;     // Raw Interrupt Status register
            reg<timer, u32> MIS;     // Masked Interrupt Status register
            reg<timer, u32> BGLOAD;  // Background Load register

            out_port<bool> IRQ;

            bool is_enabled()     const { return CONTROL & CONTROL_ENABLED; }
            bool is_irq_enabled() const { return CONTROL & CONTROL_IRQEN; }
            bool is_32bit()       const { return CONTROL & CONTROL_32BIT; }
            bool is_periodic()    const { return CONTROL & CONTROL_PERIOD; }
            bool is_oneshot()     const { return CONTROL & CONTROL_ONESHOT; }

            int get_prescale_stages() const;
            int get_prescale_divider() const;

            timer(const sc_module_name& nm);
            virtual ~timer();
            VCML_KIND(sp804timer::timer);

            virtual void reset();
        };

        enum timer_address {
            TIMER1_START = 0x00,
            TIMER1_END   = 0x1F,
            TIMER2_START = 0x20,
            TIMER2_END   = 0x3F,
        };

        timer TIMER1;
        timer TIMER2;

        reg<sp804timer, u32> ITCR;   // Integration Test Control Register
        reg<sp804timer, u32> ITOP;   // Integration Test OutPut set register

        reg<sp804timer, u32, 4> PID; // Peripheral ID Register
        reg<sp804timer, u32, 4> CID; // Cell ID Register

        slave_socket IN;

        sc_out<bool>   IRQ1;
        sc_out<bool>   IRQ2;
        out_port<bool> IRQC;

        sp804timer(const sc_module_name& nm);
        virtual ~sp804timer();
        VCML_KIND(arm::sp804timer);

        virtual unsigned int receive(tlm_generic_payload& tx,
                                     const sideband& info) override;
        virtual void reset();
    };

    inline int sp804timer::timer::get_prescale_stages() const {
        return ((CONTROL >> CTLR_PRESCALE_O) & CTLR_PRESCALE_M) << 2;
    }

    inline int sp804timer::timer::get_prescale_divider() const {
        return 1 << get_prescale_stages();
    }

}}

#endif
