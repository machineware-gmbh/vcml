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

#include "vcml/core/types.h"
#include "vcml/core/report.h"
#include "vcml/core/systemc.h"
#include "vcml/core/range.h"
#include "vcml/core/module.h"

#include "vcml/logging/logger.h"
#include "vcml/properties/property.h"

#include "vcml/protocols/tlm.h"
#include "vcml/protocols/gpio.h"
#include "vcml/protocols/clk.h"

namespace vcml {

class component : public module,
                  public tlm_host,
                  public clk_host,
                  public gpio_host
{
private:
    sc_event m_clkrst_ev;

    bool cmd_reset(const vector<string>& args, ostream& os);

    void do_reset();

public:
    clk_target_socket clk;
    gpio_target_socket rst;

    component() = delete;
    component(const component&) = delete;
    component(const sc_module_name& nm, bool allow_dmi = true);
    virtual ~component();
    VCML_KIND(component);

    virtual void reset();

    virtual void wait_clock_reset();
    virtual void wait_clock_cycle();
    virtual void wait_clock_cycles(u64 num);

    sc_time clock_cycle() const { return clk.cycle(); }
    sc_time clock_cycles(size_t n) const { return clk.cycles(n); }

    double clock_hz() const { return clk.read(); }

    virtual unsigned int transport(tlm_target_socket& socket,
                                   tlm_generic_payload& tx,
                                   const tlm_sbi& sideband) override;

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const tlm_sbi& sideband, address_space as);

    virtual void handle_clock_update(clock_t oldclk, clock_t newclk);

protected:
    virtual void clk_notify(const clk_target_socket& socket,
                            const clk_payload& tx) override;

    virtual void gpio_transport(const gpio_target_socket& socket,
                                gpio_payload& tx) override;

    virtual void gpio_notify(const gpio_target_socket& socket, bool state,
                             gpio_vector vector);
    virtual void gpio_notify(const gpio_target_socket& socket, bool state);
    virtual void gpio_notify(const gpio_target_socket& socket);
};

} // namespace vcml

#endif
