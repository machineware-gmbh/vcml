/******************************************************************************
 *                                                                            *
 * Copyright (C) 2022 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This is work is licensed under the terms described in the LICENSE file     *
 * found in the root directory of this source tree.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_COMPONENT_H
#define VCML_COMPONENT_H

#include "vcml/core/types.h"
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
    component(const sc_module_name& nm);
    component(const sc_module_name& nm, bool dmi, unsigned int bus_width);
    virtual ~component();
    VCML_KIND(component);

    virtual void reset();

    virtual void wait_clock_reset();
    virtual void wait_clock_cycle();
    virtual void wait_clock_cycles(u64 num);

    sc_time clock_cycle() const { return clk.cycle(); }
    sc_time clock_cycles(size_t n) const { return clk.cycles(n); }

    hz_t clock_hz() const { return clk.read(); }

    virtual unsigned int transport(tlm_target_socket& socket,
                                   tlm_generic_payload& tx,
                                   const tlm_sbi& sideband) override;

    virtual unsigned int transport(tlm_generic_payload& tx,
                                   const tlm_sbi& sideband, address_space as);

    virtual void handle_clock_update(hz_t oldclk, hz_t newclk);

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

inline component::component(const sc_module_name& nm):
    component(nm, true, 64) {
}

} // namespace vcml

#endif
