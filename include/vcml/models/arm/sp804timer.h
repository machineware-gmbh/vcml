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

#include "vcml/common/includes.h"
#include "vcml/common/types.h"
#include "vcml/common/utils.h"
#include "vcml/common/report.h"

#include "vcml/range.h"
#include "vcml/ports.h"
#include "vcml/command.h"
#include "vcml/register.h"
#include "vcml/component.h"
#include "vcml/peripheral.h"
#include "vcml/slave_socket.h"

#define VCML_ARM_SP804TIMER_CLK 1000000    // 1MHz
#define VCML_ARM_SP804TIMER_PID 0x00141804 // Peripheral ID
#define VCML_ARM_SP804TIMER_CID 0xB105F00D // PrimeCell ID

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
        class timer: public peripheral
        {
        private:
            SC_HAS_PROCESS(timer);

            sp804timer* m_parent;

            sc_event    m_ev;
            sc_time     m_prev;
            sc_time     m_next;

            void trigger();
            void schedule(u32 ticks);

            u32 read_CVAL();
            u32 read_RISR();
            u32 read_MISR();

            u32 write_LOAD(u32 val);
            u32 write_CTLR(u32 val);
            u32 write_ICLR(u32 val);
            u32 write_BGLR(u32 val);

        public:
            enum ctrl_status {
                CTLR_ENABLED = 1 << 7,
                CTLR_PERIOD  = 1 << 6,
                CTLR_IRQEN   = 1 << 5,
                CTLR_32BIT   = 1 << 1,
                CTLR_ONESHOT = 1 << 0,
                CTLR_M       = 0xFF
            };

            enum ctrl_prescale {
                CTLR_PRESCALE_O = 2,
                CTLR_PRESCALE_M = 3,
            };

            reg<timer, u32> LOAD; // Load register
            reg<timer, u32> CVAL; // Current Value register
            reg<timer, u32> CTLR; // Timer Control register
            reg<timer, u32> ICLR; // Interrupt Clear register
            reg<timer, u32> RISR; // Raw Interrupt Status register
            reg<timer, u32> MISR; // Masked Interrupt Status register
            reg<timer, u32> BGLR; // Background Load register

            out_port<bool> IRQ;

            bool is_enabled()     const { return CTLR & CTLR_ENABLED; }
            bool is_irq_enabled() const { return CTLR & CTLR_IRQEN; }
            bool is_32bit()       const { return CTLR & CTLR_32BIT; }
            bool is_periodic()    const { return CTLR & CTLR_PERIOD; }
            bool is_oneshot()     const { return CTLR & CTLR_ONESHOT; }

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

        property<clock_t> clock;

        sp804timer(const sc_module_name& nm);
        virtual ~sp804timer();
        VCML_KIND(arm::sp804timer);

        virtual unsigned int receive(tlm_generic_payload& tx,
                                     const sideband& info) override;
        virtual void reset();
    };

    inline int sp804timer::timer::get_prescale_stages() const {
        return ((CTLR >> CTLR_PRESCALE_O) & CTLR_PRESCALE_M) << 2;
    }

    inline int sp804timer::timer::get_prescale_divider() const {
        return 1 << get_prescale_stages();
    }

}}

#endif
