/******************************************************************************
 *                                                                            *
 * Copyright (C) 2026 MachineWare GmbH                                        *
 * All Rights Reserved                                                        *
 *                                                                            *
 * This work is licensed under the terms described in the LICENSE file found  *
 * in the root directory of this source tree.                                 *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_GPIO_BUTTONS_H
#define VCML_GPIO_BUTTONS_H

#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/module.h"
#include "vcml/core/model.h"

#include "vcml/protocols/gpio.h"

namespace vcml {
namespace gpio {

class buttons : public module, public gpio_host
{
private:
    bool cmd_push(const vector<string>& args, ostream& os);
    bool cmd_release(const vector<string>& args, ostream& os);
    bool cmd_pulse(const vector<string>& args, ostream& os);

public:
    property<bool> pressed_state;

    gpio_initiator_array<> gpio_in;

    buttons(const sc_module_name& name);
    virtual ~buttons() = default;
    VCML_KIND(gpio::buttons);

protected:
    virtual void gpio_transport(const gpio_target_socket& socket,
                                gpio_payload& tx) override;
};

} // namespace gpio
} // namespace vcml

#endif
