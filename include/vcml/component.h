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

#ifndef VCML_COMPONENT_H
#define VCML_COMPONENT_H

#include "vcml/common/types.h"
#include "vcml/common/report.h"
#include "vcml/common/systemc.h"
#include "vcml/common/range.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/protocols/tlm.h"

#include "vcml/ports.h"
#include "vcml/module.h"

namespace vcml {

    class component: public module, public tlm_host
    {
    private:
        clock_t m_curclk;

        bool cmd_reset(const vector<string>& args, ostream& os);

        void do_reset();

        void clock_handler();
        void reset_handler();

    public:
        in_port<clock_t> CLOCK;
        in_port<bool>    RESET;

        component() = delete;
        component(const component&) = delete;
        component(const sc_module_name& nm, bool allow_dmi = true);
        virtual ~component();
        VCML_KIND(component);

        virtual void reset();

        virtual void wait_clock_reset();
        virtual void wait_clock_cycle();
        virtual void wait_clock_cycles(u64 num);

        sc_time clock_cycle() const;
        sc_time clock_cycles(u64 num) const;

        double clock_hz() const;

        virtual unsigned int transport(tlm_target_socket& socket,
                                       tlm_generic_payload& tx,
                                       const tlm_sbi& sideband) override;

        virtual unsigned int transport(tlm_generic_payload& tx,
                                       const tlm_sbi& sideband,
                                       address_space as);

        virtual void handle_clock_update(clock_t oldclk, clock_t newclk);
    };

    inline sc_time component::clock_cycles(u64 num) const {
        return clock_cycle() * num;
    }

    inline double component::clock_hz() const {
        const sc_time c = clock_cycle();
        return c == SC_ZERO_TIME ? 0.0 : sc_time(1.0, SC_SEC) / c;
    }

}

#endif
